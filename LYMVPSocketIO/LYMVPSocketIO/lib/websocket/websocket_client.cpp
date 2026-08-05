#include "websocket_client.h"
#include "websocket_frame.h"
#include "websocket_handshake.h"

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
#include <sstream>
#include <iostream>

namespace ws {

struct URLInfo {
    std::string scheme;
    std::string host;
    int port = 0;
    std::string path;
    bool use_tls = false;
};

static bool parseURL(const std::string& url, URLInfo& info) {
    size_t scheme_end = url.find("://");
    if (scheme_end == std::string::npos) return false;
    
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
    
    return true;
}

struct WebSocketClient::Impl {
    WebSocketClient* parent_;
    struct event_base* base_ = nullptr;
    struct bufferevent* bev_ = nullptr;
    struct event* ping_timer_ = nullptr;
    bool own_base_ = false;
    
    std::string recv_buffer_;
    bool handshake_sent_ = false;
    std::string client_key_;
    
    bool fragmented_ = false;
    OpCode fragment_opcode_ = OpCode::Text;
    std::vector<uint8_t> fragment_buffer_;
    
    URLInfo url_info_;
    
    Impl(WebSocketClient* parent) : parent_(parent) {}
    
    ~Impl() {
        cleanup();
    }
    
    void cleanup() {
        if (ping_timer_) {
            event_free(ping_timer_);
            ping_timer_ = nullptr;
        }
        if (bev_) {
            bufferevent_free(bev_);
            bev_ = nullptr;
        }
        if (own_base_ && base_) {
            event_base_free(base_);
            base_ = nullptr;
            own_base_ = false;
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
        
        if (parent_->state_ == WebSocketState::Handshaking) {
            handleHandshakeRead(input, len);
        } else {
            handleFrameRead(input, len);
        }
    }
    
    void handleHandshakeRead(struct evbuffer* input, size_t len) {
        char* data = new char[len + 1];
        evbuffer_copyout(input, data, len);
        data[len] = '\0';
        
        std::string response(data, len);
        delete[] data;
        
        size_t header_end = response.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            return;
        }
        
        HandshakeResponse resp = WebSocketHandshake::parseResponse(response);
        if (!resp.success) {
            parent_->handleError("Handshake failed: " + resp.error);
            parent_->disconnect(CloseCode::ProtocolError, resp.error);
            return;
        }
        
        if (!WebSocketHandshake::verifyAccept(client_key_, resp.accept_key)) {
            parent_->handleError("Handshake failed: invalid Sec-WebSocket-Accept");
            parent_->disconnect(CloseCode::ProtocolError, "Invalid accept key");
            return;
        }
        
        evbuffer_drain(input, header_end + 4);
        
        parent_->setState(WebSocketState::Connected);
        parent_->handleConnect();
        
        startPingTimer();
        
        size_t remaining = evbuffer_get_length(input);
        if (remaining > 0) {
            handleFrameRead(input, remaining);
        }
    }
    
    void handleFrameRead(struct evbuffer* input, size_t len) {
        while (evbuffer_get_length(input) > 0) {
            size_t buf_len = evbuffer_get_length(input);
            std::vector<uint8_t> buf(buf_len);
            evbuffer_copyout(input, buf.data(), buf_len);
            
            WebSocketFrame frame;
            size_t consumed = 0;
            FrameParseResult result = WebSocketFrameCoder::parse(buf.data(), buf_len, frame, consumed);
            
            if (result == FrameParseResult::Incomplete) {
                break;
            }
            
            if (result == FrameParseResult::Error) {
                parent_->handleError("Frame parse error");
                parent_->disconnect(CloseCode::ProtocolError, "Frame parse error");
                return;
            }
            
            evbuffer_drain(input, consumed);
            processFrame(frame);
        }
    }
    
    void processFrame(const WebSocketFrame& frame) {
        if (frame.isControlFrame()) {
            switch (frame.opcode) {
                case OpCode::Close:
                    handleCloseFrame(frame);
                    break;
                case OpCode::Ping:
                    sendPongFrame(frame.payload);
                    if (parent_->on_ping_) {
                        parent_->on_ping_(frame.payload);
                    }
                    break;
                case OpCode::Pong:
                    if (parent_->on_pong_) {
                        parent_->on_pong_(frame.payload);
                    }
                    break;
                default:
                    break;
            }
            return;
        }
        
        if (!frame.fin) {
            if (!fragmented_) {
                fragmented_ = true;
                fragment_opcode_ = frame.opcode;
                fragment_buffer_ = frame.payload;
            } else {
                fragment_buffer_.insert(fragment_buffer_.end(), 
                                        frame.payload.begin(), frame.payload.end());
            }
            return;
        }
        
        std::vector<uint8_t> payload;
        OpCode opcode = frame.opcode;
        
        if (fragmented_) {
            fragment_buffer_.insert(fragment_buffer_.end(), 
                                    frame.payload.begin(), frame.payload.end());
            payload = std::move(fragment_buffer_);
            opcode = fragment_opcode_;
            fragmented_ = false;
        } else {
            payload = frame.payload;
        }
        
        switch (opcode) {
            case OpCode::Text:
                if (parent_->on_text_) {
                    parent_->on_text_(std::string(payload.begin(), payload.end()));
                }
                break;
            case OpCode::Binary:
                if (parent_->on_binary_) {
                    parent_->on_binary_(payload);
                }
                break;
            default:
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
        
        if (parent_->state_ == WebSocketState::Connected) {
            sendCloseFrame(1000, "");
        }
        
        parent_->setState(WebSocketState::Closed);
        parent_->handleDisconnect(reason, static_cast<CloseCode>(code));
    }
    
    void onEvent(short what) {
        if (what & BEV_EVENT_CONNECTED) {
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
                } else {
                    error_msg = "Connection closed";
                }
                parent_->handleError(error_msg);
                parent_->setState(WebSocketState::Disconnected);
                parent_->handleDisconnect(error_msg, CloseCode::AbnormalClosure);
            }
        }
    }
    
    bool connectTCP() {
        if (!base_) {
            base_ = event_base_new();
            own_base_ = true;
        }
        
        bev_ = bufferevent_socket_new(base_, -1, BEV_OPT_CLOSE_ON_FREE);
        if (!bev_) {
            return false;
        }
        
        bufferevent_setcb(bev_, readcb, nullptr, eventcb, this);
        bufferevent_enable(bev_, EV_READ | EV_WRITE);
        
        struct sockaddr_in sin;
        std::memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port = htons(url_info_.port);
        
        if (inet_pton(AF_INET, url_info_.host.c_str(), &sin.sin_addr) <= 0) {
            struct hostent* he = gethostbyname(url_info_.host.c_str());
            if (!he) {
                return false;
            }
            std::memcpy(&sin.sin_addr, he->h_addr, he->h_length);
        }
        
        if (bufferevent_socket_connect(bev_, 
            reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin)) < 0) {
            return false;
        }
        
        return true;
    }
    
    void sendHandshake() {
        client_key_ = WebSocketHandshake::generateKey();
        
        HandshakeRequest req;
        req.host = url_info_.host;
        req.port = url_info_.port;
        req.path = url_info_.path;
        req.key = client_key_;
        req.protocols = parent_->protocols_;
        req.extra_headers = parent_->extra_headers_;
        req.use_tls = url_info_.use_tls;
        
        std::string request = WebSocketHandshake::buildRequest(req);
        bufferevent_write(bev_, request.data(), request.size());
        
        parent_->setState(WebSocketState::Handshaking);
        handshake_sent_ = true;
    }
    
    void sendFrame(const std::vector<uint8_t>& frame_data) {
        if (!bev_ || parent_->state_ != WebSocketState::Connected) return;
        bufferevent_write(bev_, frame_data.data(), frame_data.size());
    }
    
    void sendPingFrame() {
        if (parent_->state_ != WebSocketState::Connected) return;
        auto data = WebSocketFrameCoder::encodePing({});
        sendFrame(data);
    }
    
    void sendPongFrame(const std::vector<uint8_t>& payload) {
        auto data = WebSocketFrameCoder::encodePong(payload);
        sendFrame(data);
    }
    
    void sendCloseFrame(uint16_t code, const std::string& reason) {
        auto data = WebSocketFrameCoder::encodeClose(code, reason);
        sendFrame(data);
    }
    
    void startPingTimer() {
        if (parent_->ping_interval_ <= 0) return;
        
        struct timeval tv;
        tv.tv_sec = parent_->ping_interval_;
        tv.tv_usec = 0;
        
        ping_timer_ = event_new(base_, -1, EV_PERSIST, pingTimerCallback, this);
        evtimer_add(ping_timer_, &tv);
    }
};

WebSocketClient::WebSocketClient() : impl_(std::make_unique<Impl>(this)) {}

WebSocketClient::~WebSocketClient() {
    disconnect(CloseCode::GoingAway, "Client destroyed");
}

void WebSocketClient::setURL(const std::string& url) {
    url_ = url;
}

void WebSocketClient::setProtocols(const std::vector<std::string>& protocols) {
    protocols_ = protocols;
}

void WebSocketClient::addHeader(const std::string& key, const std::string& value) {
    extra_headers_[key] = value;
}

void WebSocketClient::connect() {
    if (state_ != WebSocketState::Disconnected && state_ != WebSocketState::Closed) {
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
        handleError("Failed to connect");
        setState(WebSocketState::Disconnected);
        handleDisconnect("Connection failed", CloseCode::AbnormalClosure);
        return;
    }
}

void WebSocketClient::disconnect(CloseCode code, const std::string& reason) {
    if (state_ == WebSocketState::Disconnected || state_ == WebSocketState::Closed || 
        state_ == WebSocketState::Closing) {
        return;
    }
    
    if (state_ == WebSocketState::Connected) {
        setState(WebSocketState::Closing);
        impl_->sendCloseFrame(static_cast<uint16_t>(code), reason);
    }
    
    if (state_ != WebSocketState::Connected) {
        setState(WebSocketState::Disconnected);
        if (on_disconnect_) {
            on_disconnect_(reason, code);
        }
        impl_->cleanup();
    }
}

void WebSocketClient::sendText(const std::string& text) {
    if (state_ != WebSocketState::Connected) return;
    auto frame = WebSocketFrameCoder::encodeText(text);
    impl_->sendFrame(frame);
}

void WebSocketClient::sendBinary(const std::vector<uint8_t>& data) {
    if (state_ != WebSocketState::Connected) return;
    auto frame = WebSocketFrameCoder::encodeBinary(data);
    impl_->sendFrame(frame);
}

void WebSocketClient::sendPing(const std::vector<uint8_t>& data) {
    if (state_ != WebSocketState::Connected) return;
    auto frame = WebSocketFrameCoder::encodePing(data);
    impl_->sendFrame(frame);
}

void WebSocketClient::sendPong(const std::vector<uint8_t>& data) {
    if (state_ != WebSocketState::Connected) return;
    auto frame = WebSocketFrameCoder::encodePong(data);
    impl_->sendFrame(frame);
}

void WebSocketClient::setExternalEventLoop(void* event_base_ptr) {
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
    state_ = state;
}

void WebSocketClient::handleConnect() {
    if (on_connect_) on_connect_();
}

void WebSocketClient::handleDisconnect(const std::string& reason, CloseCode code) {
    if (on_disconnect_) on_disconnect_(reason, code);
}

void WebSocketClient::handleText(const std::string& text) {
    if (on_text_) on_text_(text);
}

void WebSocketClient::handleBinary(const std::vector<uint8_t>& data) {
    if (on_binary_) on_binary_(data);
}

void WebSocketClient::handleError(const std::string& error) {
    if (on_error_) on_error_(error);
}

}
