#include "websocket_client.h"
#include "websocket_frame.h"
#include "websocket_handshake.h"
#include "websocket_logger.h"
#include "websocket_deflater.h"

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/dns.h>

#ifdef WS_USE_OPENSSL
#include <event2/bufferevent_ssl.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>

#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/location.h"

namespace ws {

struct URLInfo {
    std::string scheme;
    std::string host;
    int port = 0;
    std::string path;
    bool use_tls = false;
};

static bool parseURL(const std::string& url, URLInfo& info) {
    RTC_LOG(LS_VERBOSE) << "Parsing URL: " << url;
    
    size_t scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        RTC_LOG(LS_ERROR) << "Invalid URL, missing scheme: " << url;
        return false;
    }
    
    info.scheme = url.substr(0, scheme_end);
    std::string rest = url.substr(scheme_end + 3);
    
    info.use_tls = (info.scheme == "wss" || info.scheme == "https");
    
    size_t path_start = rest.find('/');
    std::string host_port;
    if (path_start == std::string::npos) {
        host_port = rest;
        info.path = "/";
    } else {
        host_port = rest.substr(0, path_start);
        info.path = rest.substr(path_start);
    }
    
    size_t colon = host_port.find(':');
    if (colon != std::string::npos) {
        info.host = host_port.substr(0, colon);
        info.port = std::stoi(host_port.substr(colon + 1));
    } else {
        info.host = host_port;
        info.port = info.use_tls ? 443 : 80;
    }
    
    RTC_LOG(LS_VERBOSE) << "URL parsed: host=" << info.host 
                        << " port=" << info.port 
                        << " path=" << info.path
                        << " tls=" << (info.use_tls ? "yes" : "no");
    return true;
}

struct WebSocketClient::Impl {
    WebSocketClient* parent_;
    struct event_base* base_ = nullptr;
    struct bufferevent* bev_ = nullptr;
    struct event* ping_timer_ = nullptr;
    struct event* notify_event_ = nullptr;
    int notify_fd_[2] = {-1, -1};
    bool own_base_ = false;
    std::unique_ptr<rtc::Thread> event_thread_;
    std::atomic<bool> running_{false};
    
#ifdef WS_USE_OPENSSL
    SSL_CTX* ssl_ctx_ = nullptr;
    SSL* ssl_ = nullptr;
#endif
    
    webrtc::Mutex send_mtx_;
    std::deque<std::vector<uint8_t>> send_queue_;
    
    std::string recv_buffer_;
    bool handshake_sent_ = false;
    std::string client_key_;
    
    bool fragmented_ = false;
    OpCode fragment_opcode_ = OpCode::Text;
    std::vector<uint8_t> fragment_buffer_;
    
    bool rsv1_compressed_ = false;
    
    std::unique_ptr<WebSocketDeflater> deflater_;
    bool enable_deflate_ = false;
    DeflateConfig deflate_config_;
    
    URLInfo url_info_;
    
    Impl(WebSocketClient* parent) : parent_(parent) {}
    
    ~Impl() {
        cleanup();
    }
    
    void cleanupResources() {
        RTC_LOG(LS_INFO) << "Cleaning up resources";
        
        if (ping_timer_) {
            event_free(ping_timer_);
            ping_timer_ = nullptr;
            RTC_LOG(LS_VERBOSE) << "Ping timer freed";
        }
        if (notify_event_) {
            event_free(notify_event_);
            notify_event_ = nullptr;
            RTC_LOG(LS_VERBOSE) << "Notify event freed";
        }
        if (bev_) {
            bufferevent_free(bev_);
            bev_ = nullptr;
            RTC_LOG(LS_VERBOSE) << "Bufferevent freed";
        }
#ifdef WS_USE_OPENSSL
        if (ssl_) {
            SSL_free(ssl_);
            ssl_ = nullptr;
            RTC_LOG(LS_VERBOSE) << "SSL freed";
        }
        if (ssl_ctx_) {
            SSL_CTX_free(ssl_ctx_);
            ssl_ctx_ = nullptr;
            RTC_LOG(LS_VERBOSE) << "SSL_CTX freed";
        }
#endif
        if (notify_fd_[0] >= 0) {
            close(notify_fd_[0]);
            notify_fd_[0] = -1;
        }
        if (notify_fd_[1] >= 0) {
            close(notify_fd_[1]);
            notify_fd_[1] = -1;
        }
        if (own_base_ && base_) {
            event_base_free(base_);
            base_ = nullptr;
            own_base_ = false;
            RTC_LOG(LS_VERBOSE) << "Event base freed";
        }
    }
    
    void cleanup() {
        RTC_LOG(LS_INFO) << "Cleaning up WebSocket client";
        running_ = false;
        if (base_ && own_base_) {
            event_base_loopbreak(base_);
            if (notify_fd_[1] >= 0) {
                char c = 1;
                write(notify_fd_[1], &c, 1);
            }
        }
        if (event_thread_) {
            event_thread_->Stop();
            event_thread_.reset();
        }
        cleanupResources();
        RTC_LOG(LS_INFO) << "WebSocket client cleanup complete";
    }
    
    void queueSend(std::vector<uint8_t>&& data) {
        {
            webrtc::MutexLock lock(&send_mtx_);
            send_queue_.push_back(std::move(data));
        }
        if (notify_fd_[1] >= 0) {
            char c = 1;
            write(notify_fd_[1], &c, 1);
        }
    }
    
    static void onNotify(evutil_socket_t fd, short events, void* ctx) {
        (void)events;
        auto* impl = static_cast<Impl*>(ctx);
        char buf[64];
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) {}
        
        std::deque<std::vector<uint8_t>> tmp;
        {
            webrtc::MutexLock lock(&impl->send_mtx_);
            tmp.swap(impl->send_queue_);
        }
        
        RTC_LOG(LS_VERBOSE) << "Sending " << tmp.size() << " queued frames";
        
        for (auto& frame : tmp) {
            if (impl->bev_) {
                bufferevent_write(impl->bev_, frame.data(), frame.size());
            }
        }
    }
    
    static void readcb(struct bufferevent *bev, void *ctx) {
        auto* impl = static_cast<Impl*>(ctx);
        impl->onRead();
    }
    
    static void eventcb(struct bufferevent *bev, short what, void *ctx) {
        auto* impl = static_cast<Impl*>(ctx);
        impl->onEvent(what);
    }
    
    static void pingTimerCallback(evutil_socket_t fd, short event, void *ctx) {
        auto* impl = static_cast<Impl*>(ctx);
        impl->sendPingFrame();
    }
    
    void onRead() {
        struct evbuffer* input = bufferevent_get_input(bev_);
        size_t len = evbuffer_get_length(input);
        if (len == 0) return;
        
        RTC_LOG(LS_VERBOSE) << "Received " << len << " bytes of data";
        
        if (parent_->state_ == WebSocketState::Handshaking) {
            handleHandshakeRead(input, len);
        } else {
            handleFrameRead(input, len);
        }
    }
    
    void handleHandshakeRead(struct evbuffer* input, size_t len) {
        RTC_LOG(LS_INFO) << "Processing handshake response, " << len << " bytes";
        
        char* data = new char[len + 1];
        evbuffer_copyout(input, data, len);
        data[len] = '\0';
        
        std::string response(data, len);
        delete[] data;
        
        size_t header_end = response.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            RTC_LOG(LS_VERBOSE) << "Handshake response incomplete, waiting for more data";
            return;
        }
        
        HandshakeResponse resp = WebSocketHandshake::parseResponse(response);
        if (!resp.success) {
            RTC_LOG(LS_ERROR) << "Handshake failed: " << resp.error;
            parent_->handleError("Handshake failed: " + resp.error);
            parent_->disconnect(CloseCode::ProtocolError, resp.error);
            return;
        }
        
        if (!WebSocketHandshake::verifyAccept(client_key_, resp.accept_key)) {
            RTC_LOG(LS_ERROR) << "Handshake failed: invalid Sec-WebSocket-Accept";
            parent_->handleError("Handshake failed: invalid Sec-WebSocket-Accept");
            parent_->disconnect(CloseCode::ProtocolError, "Invalid accept key");
            return;
        }
        
        evbuffer_drain(input, header_end + 4);
        
        RTC_LOG(LS_INFO) << "Handshake successful, WebSocket connected";
        
#ifdef WS_USE_ZLIB
        auto it = resp.headers.find("sec-websocket-extensions");
        if (it == resp.headers.end()) {
            it = resp.headers.find("Sec-WebSocket-Extensions");
        }
        if (it != resp.headers.end()) {
            DeflateConfig negotiated_config;
            if (WebSocketDeflater::parseExtensionHeader(it->second, negotiated_config)) {
                deflater_ = std::make_unique<WebSocketDeflater>();
                if (deflater_->init(negotiated_config)) {
                    RTC_LOG(LS_INFO) << "permessage-deflate negotiated successfully";
                } else {
                    RTC_LOG(LS_WARNING) << "Failed to initialize deflater, compression disabled";
                    deflater_.reset();
                }
            } else {
                RTC_LOG(LS_INFO) << "permessage-deflate not supported by server";
            }
        } else {
            RTC_LOG(LS_INFO) << "Server did not respond with permessage-deflate";
        }
#endif
        
        parent_->setState(WebSocketState::Connected);
        parent_->handleConnect();
        
        startPingTimer();
        
        size_t remaining = evbuffer_get_length(input);
        if (remaining > 0) {
            RTC_LOG(LS_VERBOSE) << remaining << " bytes remaining after handshake, processing frames";
            handleFrameRead(input, remaining);
        }
    }
    
    void handleFrameRead(struct evbuffer* input, size_t len) {
        RTC_LOG(LS_VERBOSE) << "Processing " << len << " bytes of frame data";
        
        int frames_processed = 0;
        while (evbuffer_get_length(input) > 0) {
            size_t buf_len = evbuffer_get_length(input);
            std::vector<uint8_t> buf(buf_len);
            evbuffer_copyout(input, buf.data(), buf_len);
            
            WebSocketFrame frame;
            size_t consumed = 0;
            FrameParseResult result = WebSocketFrameCoder::parse(buf.data(), buf_len, frame, consumed);
            
            if (result == FrameParseResult::Incomplete) {
                RTC_LOG(LS_VERBOSE) << "Incomplete frame, waiting for more data";
                break;
            }
            
            if (result == FrameParseResult::Error) {
                RTC_LOG(LS_ERROR) << "Frame parse error";
                parent_->handleError("Frame parse error");
                parent_->disconnect(CloseCode::ProtocolError, "Frame parse error");
                return;
            }
            
            evbuffer_drain(input, consumed);
            processFrame(frame);
            frames_processed++;
        }
        
        if (frames_processed > 0) {
            RTC_LOG(LS_VERBOSE) << "Processed " << frames_processed << " frames";
        }
    }
    
    void processFrame(const WebSocketFrame& frame) {
        if (frame.isControlFrame()) {
            switch (frame.opcode) {
                case OpCode::Close:
                    RTC_LOG(LS_INFO) << "Received close frame";
                    handleCloseFrame(frame);
                    break;
                case OpCode::Ping:
                    RTC_LOG(LS_VERBOSE) << "Received ping frame, payload size: " << frame.payload.size();
                    sendPongFrame(frame.payload);
                    if (parent_->on_ping_) {
                        parent_->on_ping_(frame.payload);
                    }
                    break;
                case OpCode::Pong:
                    RTC_LOG(LS_VERBOSE) << "Received pong frame, payload size: " << frame.payload.size();
                    if (parent_->on_pong_) {
                        parent_->on_pong_(frame.payload);
                    }
                    break;
                default:
                    RTC_LOG(LS_WARNING) << "Unknown control frame opcode: " 
                                        << static_cast<int>(frame.opcode);
                    break;
            }
            return;
        }
        
        if (!frame.fin) {
            if (!fragmented_) {
                fragmented_ = true;
                fragment_opcode_ = frame.opcode;
                fragment_buffer_ = frame.payload;
                rsv1_compressed_ = frame.rsv1;
                RTC_LOG(LS_VERBOSE) << "Start of fragmented message, opcode: " 
                                    << static_cast<int>(frame.opcode)
                                    << " first fragment size: " << frame.payload.size()
                                    << " compressed=" << (frame.rsv1 ? "yes" : "no");
            } else {
                fragment_buffer_.insert(fragment_buffer_.end(), 
                                        frame.payload.begin(), frame.payload.end());
                RTC_LOG(LS_VERBOSE) << "Continuation fragment, total size now: " 
                                    << fragment_buffer_.size();
            }
            return;
        }
        
        std::vector<uint8_t> payload;
        OpCode opcode = frame.opcode;
        bool compressed = frame.rsv1;
        
        if (fragmented_) {
            fragment_buffer_.insert(fragment_buffer_.end(), 
                                    frame.payload.begin(), frame.payload.end());
            payload = std::move(fragment_buffer_);
            opcode = fragment_opcode_;
            compressed = rsv1_compressed_;
            fragmented_ = false;
            RTC_LOG(LS_VERBOSE) << "Final fragment received, total message size: " << payload.size();
        } else {
            payload = frame.payload;
        }
        
#ifdef WS_USE_ZLIB
        if (compressed && deflater_ && deflater_->isEnabled()) {
            std::vector<uint8_t> decompressed;
            if (deflater_->decompress(payload.data(), payload.size(), decompressed)) {
                RTC_LOG(LS_VERBOSE) << "Decompressed frame: " << payload.size() << " -> " << decompressed.size();
                payload = std::move(decompressed);
            } else {
                RTC_LOG(LS_ERROR) << "Failed to decompress frame";
                parent_->handleError("Frame decompression failed");
                parent_->disconnect(CloseCode::InvalidPayloadData, "Decompression failed");
                return;
            }
        }
#endif
        
        switch (opcode) {
            case OpCode::Text:
                RTC_LOG(LS_VERBOSE) << "Received text frame, size: " << payload.size();
                if (parent_->on_text_) {
                    parent_->on_text_(std::string(payload.begin(), payload.end()));
                }
                break;
            case OpCode::Binary:
                RTC_LOG(LS_VERBOSE) << "Received binary frame, size: " << payload.size();
                if (parent_->on_binary_) {
                    parent_->on_binary_(payload);
                }
                break;
            default:
                RTC_LOG(LS_WARNING) << "Unknown data frame opcode: " 
                                    << static_cast<int>(opcode);
                break;
        }
    }
    
    void handleCloseFrame(const WebSocketFrame& frame) {
        uint16_t code = 1000;
        std::string reason;
        
        if (frame.payload.size() >= 2) {
            std::memcpy(&code, frame.payload.data(), 2);
            code = ntohs(code);
            if (frame.payload.size() > 2) {
                reason.assign(frame.payload.begin() + 2, frame.payload.end());
            }
        }
        
        RTC_LOG(LS_INFO) << "Close frame received: code=" << code << " reason=" << reason;
        
        if (parent_->state_ == WebSocketState::Connected) {
            sendCloseFrame(1000, "");
        }
        
        parent_->setState(WebSocketState::Closed);
        parent_->handleDisconnect(reason, static_cast<CloseCode>(code));
    }
    
    void onEvent(short what) {
        if (what & BEV_EVENT_CONNECTED) {
            RTC_LOG(LS_INFO) << "TCP connection established";
            if (parent_->state_ == WebSocketState::Connecting) {
                sendHandshake();
            }
        } else if (what & (BEV_EVENT_ERROR | BEV_EVENT_EOF)) {
            if (parent_->state_ != WebSocketState::Closed && 
                parent_->state_ != WebSocketState::Disconnected) {
                std::string error_msg;
                if (what & BEV_EVENT_ERROR) {
                    int err = EVUTIL_SOCKET_ERROR();
                    error_msg = evutil_socket_error_to_string(err);
                    RTC_LOG(LS_ERROR) << "Socket error: " << error_msg << " (" << err << ")";
                } else {
                    error_msg = "Connection closed";
                    RTC_LOG(LS_INFO) << "Connection closed by peer";
                }
                parent_->handleError(error_msg);
                parent_->setState(WebSocketState::Disconnected);
                parent_->handleDisconnect(error_msg, CloseCode::AbnormalClosure);
            }
        }
    }
    
    bool connectTCP() {
        RTC_LOG(LS_INFO) << "Connecting to " << url_info_.host << ":" << url_info_.port
                         << " TLS=" << (url_info_.use_tls ? "yes" : "no");
        
        if (!base_) {
            base_ = event_base_new();
            own_base_ = true;
            RTC_LOG(LS_VERBOSE) << "Created new event base";
        }
        
        if (pipe(notify_fd_) < 0) {
            RTC_LOG(LS_ERROR) << "Failed to create notify pipe: " << strerror(errno);
            return false;
        }
        evutil_make_socket_nonblocking(notify_fd_[0]);
        evutil_make_socket_nonblocking(notify_fd_[1]);
        
        notify_event_ = event_new(base_, notify_fd_[0], EV_READ | EV_PERSIST, onNotify, this);
        event_add(notify_event_, nullptr);
        
#ifdef WS_USE_OPENSSL
        if (url_info_.use_tls) {
            RTC_LOG(LS_INFO) << "Creating SSL context for TLS connection";
            
            ssl_ctx_ = SSL_CTX_new(TLS_client_method());
            if (!ssl_ctx_) {
                RTC_LOG(LS_ERROR) << "Failed to create SSL_CTX";
                return false;
            }
            
            if (parent_->self_signed_ssl_) {
                RTC_LOG(LS_INFO) << "Self-signed SSL enabled, skipping certificate verification";
                SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_NONE, nullptr);
            } else {
                SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_PEER, nullptr);
            }
            
            SSL_CTX_set_options(ssl_ctx_, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION);
            
            ssl_ = SSL_new(ssl_ctx_);
            if (!ssl_) {
                RTC_LOG(LS_ERROR) << "Failed to create SSL object";
                return false;
            }
            
            SSL_set_tlsext_host_name(ssl_, url_info_.host.c_str());
            
            bev_ = bufferevent_openssl_socket_new(base_, -1, ssl_,
                                                  BUFFEREVENT_SSL_CONNECTING,
                                                  BEV_OPT_CLOSE_ON_FREE);
            if (!bev_) {
                RTC_LOG(LS_ERROR) << "Failed to create SSL bufferevent";
                return false;
            }
            
            RTC_LOG(LS_INFO) << "SSL bufferevent created successfully";
        } else
#endif
        {
            bev_ = bufferevent_socket_new(base_, -1, BEV_OPT_CLOSE_ON_FREE);
            if (!bev_) {
                RTC_LOG(LS_ERROR) << "Failed to create bufferevent";
                return false;
            }
        }
        
        bufferevent_setcb(bev_, readcb, nullptr, eventcb, this);
        bufferevent_enable(bev_, EV_READ | EV_WRITE);
        
        struct sockaddr_in sin;
        std::memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port = htons(url_info_.port);
        
        if (inet_pton(AF_INET, url_info_.host.c_str(), &sin.sin_addr) <= 0) {
            RTC_LOG(LS_VERBOSE) << "Resolving hostname: " << url_info_.host;
            struct hostent* he = gethostbyname(url_info_.host.c_str());
            if (!he) {
                RTC_LOG(LS_ERROR) << "Failed to resolve hostname: " << url_info_.host;
                return false;
            }
            std::memcpy(&sin.sin_addr, he->h_addr, he->h_length);
        }
        
        if (bufferevent_socket_connect(bev_, 
            reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin)) < 0) {
            RTC_LOG(LS_ERROR) << "Failed to connect socket";
            return false;
        }
        
        if (own_base_ && !running_) {
            running_ = true;
            event_thread_ = rtc::Thread::Create();
            event_thread_->SetName("WebSocketEventLoop", this);
            event_thread_->Start();
            event_thread_->PostTask(RTC_FROM_HERE, [this]() {
                while (running_ && base_) {
                    event_base_dispatch(base_);
                    if (running_) {
                        rtc::Thread::Current()->SleepMs(10);
                    }
                }
                cleanupResources();
            });
            RTC_LOG(LS_VERBOSE) << "Event loop thread started";
        }
        
        return true;
    }
    
    void sendHandshake() {
        RTC_LOG(LS_INFO) << "Sending WebSocket handshake request";
        
        client_key_ = WebSocketHandshake::generateKey();
        
        HandshakeRequest req;
        req.host = url_info_.host;
        req.port = url_info_.port;
        req.path = url_info_.path;
        req.key = client_key_;
        req.protocols = parent_->protocols_;
        req.extra_headers = parent_->extra_headers_;
        req.use_tls = url_info_.use_tls;
        
#ifdef WS_USE_ZLIB
        enable_deflate_ = true;
        deflate_config_ = DeflateConfig();
        std::string ext_header = WebSocketDeflater::buildExtensionHeader(deflate_config_);
        req.extra_headers["Sec-WebSocket-Extensions"] = ext_header;
        RTC_LOG(LS_INFO) << "Requesting permessage-deflate extension";
#endif
        
        std::string request = WebSocketHandshake::buildRequest(req);
        bufferevent_write(bev_, request.data(), request.size());
        
        parent_->setState(WebSocketState::Handshaking);
        handshake_sent_ = true;
        
        RTC_LOG(LS_VERBOSE) << "Handshake request sent, key=" << client_key_;
    }
    
    void sendFrame(const std::vector<uint8_t>& frame_data) {
        if (parent_->state_ != WebSocketState::Connected && 
            parent_->state_ != WebSocketState::Handshaking &&
            parent_->state_ != WebSocketState::Connecting) {
            RTC_LOG(LS_WARNING) << "Attempting to send frame in state: " 
                                << static_cast<int>(parent_->state_.load());
            return;
        }
        if (bev_ && own_base_ && running_) {
            queueSend(std::vector<uint8_t>(frame_data));
        } else if (bev_) {
            bufferevent_write(bev_, frame_data.data(), frame_data.size());
        }
    }
    
    void sendPingFrame() {
        if (parent_->state_ != WebSocketState::Connected) return;
        RTC_LOG(LS_VERBOSE) << "Sending ping frame";
        auto data = WebSocketFrameCoder::encodePing({});
        sendFrame(data);
    }
    
    void sendPongFrame(const std::vector<uint8_t>& payload) {
        RTC_LOG(LS_VERBOSE) << "Sending pong frame, size: " << payload.size();
        auto data = WebSocketFrameCoder::encodePong(payload);
        sendFrame(data);
    }
    
    void sendCloseFrame(uint16_t code, const std::string& reason) {
        RTC_LOG(LS_INFO) << "Sending close frame: code=" << code << " reason=" << reason;
        auto data = WebSocketFrameCoder::encodeClose(code, reason);
        sendFrame(data);
    }
    
    void startPingTimer() {
        if (parent_->ping_interval_ <= 0) {
            RTC_LOG(LS_INFO) << "Ping interval disabled";
            return;
        }
        
        RTC_LOG(LS_INFO) << "Starting ping timer, interval=" << parent_->ping_interval_ << "s";
        
        struct timeval tv;
        tv.tv_sec = parent_->ping_interval_;
        tv.tv_usec = 0;
        
        ping_timer_ = event_new(base_, -1, EV_PERSIST, pingTimerCallback, this);
        evtimer_add(ping_timer_, &tv);
    }
};

WebSocketClient::WebSocketClient() : impl_(std::make_unique<Impl>(this)) {
    RTC_LOG(LS_INFO) << "WebSocketClient created";
}

WebSocketClient::~WebSocketClient() {
    RTC_LOG(LS_INFO) << "WebSocketClient destroyed";
    disconnect(CloseCode::GoingAway, "Client destroyed");
}

void WebSocketClient::setURL(const std::string& url) {
    RTC_LOG(LS_VERBOSE) << "Setting URL: " << url;
    url_ = url;
}

void WebSocketClient::setProtocols(const std::vector<std::string>& protocols) {
    RTC_LOG(LS_VERBOSE) << "Setting protocols, count: " << protocols.size();
    protocols_ = protocols;
}

void WebSocketClient::addHeader(const std::string& key, const std::string& value) {
    RTC_LOG(LS_VERBOSE) << "Adding header: " << key << "=" << value;
    extra_headers_[key] = value;
}

void WebSocketClient::connect() {
    RTC_LOG(LS_INFO) << "WebSocket connect called, URL: " << url_;
    
    if (state_ != WebSocketState::Disconnected && state_ != WebSocketState::Closed) {
        RTC_LOG(LS_WARNING) << "Connect called in invalid state: " 
                            << static_cast<int>(state_.load());
        return;
    }
    
    URLInfo info;
    if (!parseURL(url_, info)) {
        handleError("Invalid URL: " + url_);
        return;
    }
    impl_->url_info_ = info;
    
    setState(WebSocketState::Connecting);
    
    if (!impl_->connectTCP()) {
        RTC_LOG(LS_ERROR) << "Failed to establish TCP connection";
        handleError("Failed to connect");
        setState(WebSocketState::Disconnected);
        handleDisconnect("Connection failed", CloseCode::AbnormalClosure);
        return;
    }
    
    RTC_LOG(LS_INFO) << "Connection initiated";
}

void WebSocketClient::disconnect(CloseCode code, const std::string& reason) {
    RTC_LOG(LS_INFO) << "WebSocket disconnect called: code=" 
                     << static_cast<int>(code) << " reason=" << reason;
    
    if (state_ == WebSocketState::Disconnected || state_ == WebSocketState::Closed || 
        state_ == WebSocketState::Closing) {
        RTC_LOG(LS_VERBOSE) << "Disconnect called in state: " 
                            << static_cast<int>(state_.load()) << ", ignoring";
        return;
    }
    
    if (state_ == WebSocketState::Connected) {
        setState(WebSocketState::Closing);
        impl_->sendCloseFrame(static_cast<uint16_t>(code), reason);
    }
    
    setState(WebSocketState::Disconnected);
    if (on_disconnect_) {
        on_disconnect_(reason, code);
    }
    
    if (impl_->own_base_ && impl_->running_) {
        event_base_loopbreak(impl_->base_);
        if (impl_->notify_fd_[1] >= 0) {
            char c = 1;
            write(impl_->notify_fd_[1], &c, 1);
        }
        impl_->running_ = false;
        if (impl_->event_thread_) {
            impl_->event_thread_->Stop();
            impl_->event_thread_.reset();
        }
        impl_->cleanupResources();
    } else {
        impl_->cleanup();
    }
    
    RTC_LOG(LS_INFO) << "WebSocket disconnected";
}

void WebSocketClient::sendText(const std::string& text) {
    if (state_ != WebSocketState::Connected) {
        RTC_LOG(LS_WARNING) << "sendText called but not connected, state: " 
                            << static_cast<int>(state_.load());
        return;
    }
    RTC_LOG(LS_VERBOSE) << "Sending text frame, size: " << text.size();
    
    std::vector<uint8_t> payload(text.begin(), text.end());
    bool compressed = false;
    
#ifdef WS_USE_ZLIB
    if (impl_->deflater_ && impl_->deflater_->isEnabled()) {
        std::vector<uint8_t> compressed_payload;
        if (impl_->deflater_->compress(payload.data(), payload.size(), compressed_payload)) {
            if (compressed_payload.size() < payload.size()) {
                payload = std::move(compressed_payload);
                compressed = true;
                RTC_LOG(LS_VERBOSE) << "Compressed text frame: " << text.size() << " -> " << payload.size();
            }
        }
    }
#endif
    
    WebSocketFrame frame;
    frame.fin = true;
    frame.rsv1 = compressed;
    frame.opcode = OpCode::Text;
    frame.payload = std::move(payload);
    
    auto encoded = WebSocketFrameCoder::encode(frame, true);
    impl_->sendFrame(encoded);
}

void WebSocketClient::sendBinary(const std::vector<uint8_t>& data) {
    if (state_ != WebSocketState::Connected) {
        RTC_LOG(LS_WARNING) << "sendBinary called but not connected, state: " 
                            << static_cast<int>(state_.load());
        return;
    }
    RTC_LOG(LS_VERBOSE) << "Sending binary frame, size: " << data.size();
    
    std::vector<uint8_t> payload = data;
    bool compressed = false;
    
#ifdef WS_USE_ZLIB
    if (impl_->deflater_ && impl_->deflater_->isEnabled()) {
        std::vector<uint8_t> compressed_payload;
        if (impl_->deflater_->compress(payload.data(), payload.size(), compressed_payload)) {
            if (compressed_payload.size() < payload.size()) {
                payload = std::move(compressed_payload);
                compressed = true;
                RTC_LOG(LS_VERBOSE) << "Compressed binary frame: " << data.size() << " -> " << payload.size();
            }
        }
    }
#endif
    
    WebSocketFrame frame;
    frame.fin = true;
    frame.rsv1 = compressed;
    frame.opcode = OpCode::Binary;
    frame.payload = std::move(payload);
    
    auto encoded = WebSocketFrameCoder::encode(frame, true);
    impl_->sendFrame(encoded);
}

void WebSocketClient::sendPing(const std::vector<uint8_t>& data) {
    if (state_ != WebSocketState::Connected) {
        RTC_LOG(LS_WARNING) << "sendPing called but not connected, state: " 
                            << static_cast<int>(state_.load());
        return;
    }
    RTC_LOG(LS_VERBOSE) << "Sending ping, size: " << data.size();
    auto frame = WebSocketFrameCoder::encodePing(data);
    impl_->sendFrame(frame);
}

void WebSocketClient::sendPong(const std::vector<uint8_t>& data) {
    if (state_ != WebSocketState::Connected) {
        RTC_LOG(LS_WARNING) << "sendPong called but not connected, state: " 
                            << static_cast<int>(state_.load());
        return;
    }
    RTC_LOG(LS_VERBOSE) << "Sending pong, size: " << data.size();
    auto frame = WebSocketFrameCoder::encodePong(data);
    impl_->sendFrame(frame);
}

void WebSocketClient::setExternalEventLoop(void* event_base_ptr) {
    RTC_LOG(LS_INFO) << "Setting external event loop";
    if (impl_->own_base_) {
        impl_->cleanup();
    }
    impl_->base_ = static_cast<struct event_base*>(event_base_ptr);
    impl_->own_base_ = false;
}

void* WebSocketClient::getEventBase() const {
    return impl_->base_;
}

void WebSocketClient::setState(WebSocketState state) {
    if (state_ != state) {
        RTC_LOG(LS_INFO) << "State change: " << static_cast<int>(state_.load()) 
                         << " -> " << static_cast<int>(state);
        state_ = state;
    }
}

void WebSocketClient::handleConnect() {
    RTC_LOG(LS_INFO) << "WebSocket connected successfully";
    if (on_connect_) on_connect_();
}

void WebSocketClient::handleDisconnect(const std::string& reason, CloseCode code) {
    RTC_LOG(LS_INFO) << "WebSocket disconnected: code=" 
                     << static_cast<int>(code) << " reason=" << reason;
    if (on_disconnect_) on_disconnect_(reason, code);
}

void WebSocketClient::handleText(const std::string& text) {
    RTC_LOG(LS_VERBOSE) << "Handling text message, size: " << text.size();
    if (on_text_) on_text_(text);
}

void WebSocketClient::handleBinary(const std::vector<uint8_t>& data) {
    RTC_LOG(LS_VERBOSE) << "Handling binary message, size: " << data.size();
    if (on_binary_) on_binary_(data);
}

void WebSocketClient::handleError(const std::string& error) {
    RTC_LOG(LS_ERROR) << "WebSocket error: " << error;
    if (on_error_) on_error_(error);
}

}
