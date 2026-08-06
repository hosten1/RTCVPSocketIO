#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <cstring>
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include "websocket_frame.h"
#include "websocket_handshake.h"
#include "sio_packet_builder.h"
#include "sio_packet.h"

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace sio;
using namespace ws;

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name) do { \
    std::cout << "TEST: " << name << " ... "; \
} while(0)

#define PASS() do { \
    std::cout << "PASS" << std::endl; \
    g_tests_passed++; \
} while(0)

#define FAIL(msg) do { \
    std::cout << "FAIL: " << msg << std::endl; \
    g_tests_failed++; \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { FAIL(#cond " is false"); return; } \
} while(0)

#define ASSERT_FALSE(cond) do { \
    if ((cond)) { FAIL(#cond " is true"); return; } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if (!((a) == (b))) { FAIL(#a " != " #b); return; } \
} while(0)

#define ASSERT_STR_EQ(a, b) do { \
    if (!((a) == (b))) { FAIL(std::string(#a " != " #b) + " (" + (a) + " vs " + (b) + ")"); return; } \
} while(0)

// ─── Mock WebSocket Server ────────────────────────────────────────

class MockSocketIOServer {
public:
    using ConnectHandler = std::function<void()>;
    using MessageHandler = std::function<void(const std::string&)>;
    using BinaryHandler = std::function<void(const std::vector<uint8_t>&)>;
    using DisconnectHandler = std::function<void()>;

    MockSocketIOServer() : port_(0), base_(nullptr), listener_(nullptr), client_bev_(nullptr), connected_(false), notify_event_(nullptr) {
        notify_fd_[0] = -1;
        notify_fd_[1] = -1;
    }

    ~MockSocketIOServer() {
        stop();
        if (notify_event_) {
            event_free(notify_event_);
            notify_event_ = nullptr;
        }
        if (notify_fd_[0] >= 0) {
            close(notify_fd_[0]);
            notify_fd_[0] = -1;
        }
        if (notify_fd_[1] >= 0) {
            close(notify_fd_[1]);
            notify_fd_[1] = -1;
        }
    }

    int start(int port = 0) {
        base_ = event_base_new();
        if (!base_) return -1;

        if (pipe(notify_fd_) < 0) {
            return -1;
        }

        notify_event_ = event_new(base_, notify_fd_[0], EV_READ | EV_PERSIST, &on_notify, this);
        event_add(notify_event_, nullptr);

        struct sockaddr_in sin;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        sin.sin_port = htons(port);

        listener_ = evconnlistener_new_bind(base_, &on_accept, this,
            LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
            (struct sockaddr*)&sin, sizeof(sin));

        if (!listener_) {
            event_base_free(base_);
            base_ = nullptr;
            return -1;
        }

        struct sockaddr_storage addr;
        socklen_t len = sizeof(addr);
        if (getsockname(evconnlistener_get_fd(listener_), (struct sockaddr*)&addr, &len) == 0) {
            port_ = ntohs(((struct sockaddr_in*)&addr)->sin_port);
        }

        server_thread_ = std::thread([this]() {
            event_base_dispatch(base_);
        });

        return port_;
    }

    void stop() {
        if (base_ && server_thread_.joinable()) {
            event_base_loopbreak(base_);
            if (notify_fd_[1] >= 0) {
                char c = 1;
                write(notify_fd_[1], &c, 1);
            }
            server_thread_.join();
        }
        if (client_bev_) {
            bufferevent_free(client_bev_);
            client_bev_ = nullptr;
        }
        if (listener_) {
            evconnlistener_free(listener_);
            listener_ = nullptr;
        }
        if (base_) {
            event_base_free(base_);
            base_ = nullptr;
        }
    }

    int port() const { return port_; }

    bool isConnected() const { return connected_.load(); }

    void waitForConnection(int timeout_ms = 2000) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this]() { return connected_.load(); });
    }

    void waitForMessage(int timeout_ms = 2000) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this]() { return !last_message_.empty(); });
    }

    std::string getLastMessage() {
        std::lock_guard<std::mutex> lock(mtx_);
        std::string msg = last_message_;
        last_message_.clear();
        return msg;
    }

    std::vector<uint8_t> getLastBinary() {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<uint8_t> bin = last_binary_;
        last_binary_.clear();
        return bin;
    }

    void sendText(const std::string& text) {
        if (!client_bev_) return;
        auto frame = WebSocketFrameCoder::encodeText(text, false);
        queueSend(std::move(frame));
    }

    void sendBinary(const std::vector<uint8_t>& data) {
        if (!client_bev_) return;
        auto frame = WebSocketFrameCoder::encodeBinary(data, false);
        queueSend(std::move(frame));
    }

    void setOnConnect(ConnectHandler cb) { on_connect_ = std::move(cb); }
    void setOnMessage(MessageHandler cb) { on_message_ = std::move(cb); }
    void setOnBinary(BinaryHandler cb) { on_binary_ = std::move(cb); }
    void setOnDisconnect(DisconnectHandler cb) { on_disconnect_ = std::move(cb); }

private:
    void queueSend(std::vector<uint8_t>&& data) {
        {
            std::lock_guard<std::mutex> lock(send_mtx_);
            send_queue_.push_back(std::move(data));
        }
        char c = 1;
        write(notify_fd_[1], &c, 1);
    }

    static void on_notify(evutil_socket_t fd, short events, void* ctx) {
        (void)events;
        auto* server = static_cast<MockSocketIOServer*>(ctx);
        char buf[64];
        read(fd, buf, sizeof(buf));
        
        std::deque<std::vector<uint8_t>> tmp;
        {
            std::lock_guard<std::mutex> lock(server->send_mtx_);
            tmp.swap(server->send_queue_);
        }
        for (auto& frame : tmp) {
            bufferevent_write(server->client_bev_, frame.data(), frame.size());
        }
    }

    struct ServerSendData {
        struct bufferevent* bev;
        std::vector<uint8_t>* data;
    };

    static void server_send_callback(evutil_socket_t fd, short what, void* arg) {
        (void)fd;
        (void)what;
        auto* sd = static_cast<ServerSendData*>(arg);
        bufferevent_write(sd->bev, sd->data->data(), sd->data->size());
        delete sd->data;
        delete sd;
    }

    static void on_accept(struct evconnlistener* listener, evutil_socket_t fd,
                          struct sockaddr* addr, int socklen, void* ctx) {
        auto* server = static_cast<MockSocketIOServer*>(ctx);
        server->handleAccept(fd);
    }

    void handleAccept(evutil_socket_t fd) {
        if (client_bev_) {
            bufferevent_free(client_bev_);
        }
        client_bev_ = bufferevent_socket_new(base_, fd, BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(client_bev_, &on_read, nullptr, &on_event, this);
        bufferevent_enable(client_bev_, EV_READ | EV_WRITE);
    }

    static void on_read(struct bufferevent* bev, void* ctx) {
        auto* server = static_cast<MockSocketIOServer*>(ctx);
        server->handleRead(bev);
    }

    static void on_event(struct bufferevent* bev, short events, void* ctx) {
        auto* server = static_cast<MockSocketIOServer*>(ctx);
        server->handleEvent(bev, events);
    }

    void handleRead(struct bufferevent* bev) {
        struct evbuffer* input = bufferevent_get_input(bev);
        size_t len = evbuffer_get_length(input);
        if (len == 0) return;

        std::vector<uint8_t> data(len);
        evbuffer_remove(input, data.data(), len);

        recv_buffer_.insert(recv_buffer_.end(), data.begin(), data.end());

        if (!handshake_done_) {
            tryHandshake();
        } else {
            parseFrames();
        }
    }

    void tryHandshake() {
        std::string data_str(recv_buffer_.begin(), recv_buffer_.end());
        size_t end = data_str.find("\r\n\r\n");
        if (end == std::string::npos) return;

        std::string request = data_str.substr(0, end);
        recv_buffer_.erase(recv_buffer_.begin(), recv_buffer_.begin() + end + 4);

        size_t key_pos = request.find("Sec-WebSocket-Key:");
        std::string client_key;
        if (key_pos != std::string::npos) {
            size_t start = key_pos + 19;
            while (start < request.size() && request[start] == ' ') start++;
            size_t end_line = request.find("\r\n", start);
            client_key = request.substr(start, end_line - start);
        }

        std::string accept_key = WebSocketHandshake::computeAccept(client_key);

        std::string response = 
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: " + accept_key + "\r\n"
            "\r\n";

        bufferevent_write(client_bev_, response.c_str(), response.size());
        handshake_done_ = true;
        connected_ = true;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            cv_.notify_all();
        }
        if (on_connect_) on_connect_();
    }

    void parseFrames() {
        while (!recv_buffer_.empty()) {
            WebSocketFrame frame;
            size_t consumed = 0;
            auto result = WebSocketFrameCoder::parse(recv_buffer_.data(), recv_buffer_.size(), frame, consumed);
            
            if (result == FrameParseResult::Incomplete) break;
            if (result == FrameParseResult::Error) {
                recv_buffer_.clear();
                break;
            }

            recv_buffer_.erase(recv_buffer_.begin(), recv_buffer_.begin() + consumed);

            if (frame.opcode == OpCode::Text) {
                std::string text(frame.payload.begin(), frame.payload.end());
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    last_message_ = text;
                    cv_.notify_all();
                }
                if (on_message_) on_message_(text);
            } else if (frame.opcode == OpCode::Binary) {
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    last_binary_ = frame.payload;
                    cv_.notify_all();
                }
                if (on_binary_) on_binary_(frame.payload);
            } else if (frame.opcode == OpCode::Ping) {
                auto pong = WebSocketFrameCoder::encodePong(frame.payload, false);
                bufferevent_write(client_bev_, pong.data(), pong.size());
            } else if (frame.opcode == OpCode::Close) {
                auto close = WebSocketFrameCoder::encodeClose(1000, "", false);
                bufferevent_write(client_bev_, close.data(), close.size());
            }
        }
    }

    void handleEvent(struct bufferevent* bev, short events) {
        if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
            connected_ = false;
            if (on_disconnect_) on_disconnect_();
        }
    }

    int port_;
    struct event_base* base_;
    struct evconnlistener* listener_;
    struct bufferevent* client_bev_;
    std::thread server_thread_;

    int notify_fd_[2];
    struct event* notify_event_;
    std::mutex send_mtx_;
    std::deque<std::vector<uint8_t>> send_queue_;

    std::atomic<bool> connected_;
    bool handshake_done_ = false;
    std::vector<uint8_t> recv_buffer_;

    std::mutex mtx_;
    std::condition_variable cv_;
    std::string last_message_;
    std::vector<uint8_t> last_binary_;

    ConnectHandler on_connect_;
    MessageHandler on_message_;
    BinaryHandler on_binary_;
    DisconnectHandler on_disconnect_;
};

// ─── Simple WebSocket Client for Testing ──────────────────────────

class TestWebSocketClient {
public:
    TestWebSocketClient() : base_(nullptr), bev_(nullptr), connected_(false), notify_event_(nullptr) {
        notify_fd_[0] = -1;
        notify_fd_[1] = -1;
    }

    ~TestWebSocketClient() {
        disconnect();
        if (notify_event_) {
            event_free(notify_event_);
            notify_event_ = nullptr;
        }
        if (notify_fd_[0] >= 0) {
            close(notify_fd_[0]);
            notify_fd_[0] = -1;
        }
        if (notify_fd_[1] >= 0) {
            close(notify_fd_[1]);
            notify_fd_[1] = -1;
        }
        if (base_) {
            event_base_free(base_);
            base_ = nullptr;
        }
    }

    bool connect(const std::string& host, int port) {
        base_ = event_base_new();
        if (!base_) return false;

        if (pipe(notify_fd_) < 0) {
            return false;
        }

        notify_event_ = event_new(base_, notify_fd_[0], EV_READ | EV_PERSIST, &on_notify, this);
        event_add(notify_event_, nullptr);

        bev_ = bufferevent_socket_new(base_, -1, BEV_OPT_CLOSE_ON_FREE);
        if (!bev_) return false;

        bufferevent_setcb(bev_, &on_read, nullptr, &on_event, this);
        bufferevent_enable(bev_, EV_READ | EV_WRITE);

        struct sockaddr_in sin;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &sin.sin_addr);

        if (bufferevent_socket_connect(bev_, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
            return false;
        }

        client_key_ = WebSocketHandshake::generateKey();
        std::string request = 
            "GET /socket.io/?EIO=3&transport=websocket HTTP/1.1\r\n"
            "Host: " + host + ":" + std::to_string(port) + "\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: " + client_key_ + "\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n";

        bufferevent_write(bev_, request.c_str(), request.size());

        std::thread t([this]() {
            event_base_dispatch(base_);
        });
        event_thread_ = std::move(t);

        std::unique_lock<std::mutex> lock(mtx_);
        return cv_.wait_for(lock, std::chrono::seconds(2), [this]() { return connected_.load(); });
    }

    void disconnect() {
        if (base_ && event_thread_.joinable()) {
            event_base_loopbreak(base_);
            if (notify_fd_[1] >= 0) {
                char c = 1;
                write(notify_fd_[1], &c, 1);
            }
            event_thread_.join();
        }
        if (bev_) {
            bufferevent_free(bev_);
            bev_ = nullptr;
        }
        connected_ = false;
    }

    bool isConnected() const { return connected_.load(); }

    void sendText(const std::string& text) {
        if (!bev_ || !connected_) return;
        auto frame = WebSocketFrameCoder::encodeText(text, true);
        queueSend(std::move(frame));
    }

    void sendBinary(const std::vector<uint8_t>& data) {
        if (!bev_ || !connected_) return;
        auto frame = WebSocketFrameCoder::encodeBinary(data, true);
        queueSend(std::move(frame));
    }

    std::string waitForMessage(int timeout_ms = 2000) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this]() { return !message_queue_.empty(); });
        if (message_queue_.empty()) return "";
        std::string msg = message_queue_.front();
        message_queue_.pop_front();
        return msg;
    }
    
    int messageCount() {
        std::lock_guard<std::mutex> lock(mtx_);
        return (int)message_queue_.size();
    }
    
    std::vector<uint8_t> waitForBinary(int timeout_ms = 2000) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this]() { return !last_binary_.empty(); });
        std::vector<uint8_t> bin = last_binary_;
        last_binary_.clear();
        return bin;
    }

private:
    void queueSend(std::vector<uint8_t>&& data) {
        {
            std::lock_guard<std::mutex> lock(send_mtx_);
            send_queue_.push_back(std::move(data));
        }
        char c = 1;
        write(notify_fd_[1], &c, 1);
    }

    static void on_notify(evutil_socket_t fd, short events, void* ctx) {
        (void)events;
        auto* client = static_cast<TestWebSocketClient*>(ctx);
        char buf[64];
        read(fd, buf, sizeof(buf));
        
        std::deque<std::vector<uint8_t>> tmp;
        {
            std::lock_guard<std::mutex> lock(client->send_mtx_);
            tmp.swap(client->send_queue_);
        }
        for (auto& frame : tmp) {
            bufferevent_write(client->bev_, frame.data(), frame.size());
        }
    }

    struct SendData {
        struct bufferevent* bev;
        std::vector<uint8_t>* data;
    };

    static void send_callback(evutil_socket_t fd, short what, void* arg) {
        (void)fd;
        (void)what;
        auto* sd = static_cast<SendData*>(arg);
        bufferevent_write(sd->bev, sd->data->data(), sd->data->size());
        delete sd->data;
        delete sd;
    }

    static void on_read(struct bufferevent* bev, void* ctx) {
        auto* client = static_cast<TestWebSocketClient*>(ctx);
        client->handleRead(bev);
    }

    static void on_event(struct bufferevent* bev, short events, void* ctx) {
        auto* client = static_cast<TestWebSocketClient*>(ctx);
        client->handleEvent(bev, events);
    }

    void handleRead(struct bufferevent* bev) {
        struct evbuffer* input = bufferevent_get_input(bev);
        size_t len = evbuffer_get_length(input);
        if (len == 0) return;

        std::vector<uint8_t> data(len);
        evbuffer_remove(input, data.data(), len);

        recv_buffer_.insert(recv_buffer_.end(), data.begin(), data.end());

        if (!handshake_done_) {
            tryHandshake();
        } else {
            parseFrames();
        }
    }

    void tryHandshake() {
        std::string data_str(recv_buffer_.begin(), recv_buffer_.end());
        size_t end = data_str.find("\r\n\r\n");
        if (end == std::string::npos) return;

        std::string response = data_str.substr(0, end);
        recv_buffer_.erase(recv_buffer_.begin(), recv_buffer_.begin() + end + 4);

        if (response.find("101") != std::string::npos) {
            handshake_done_ = true;
            connected_ = true;
            {
                std::lock_guard<std::mutex> lock(mtx_);
                cv_.notify_all();
            }
        }
    }

    void parseFrames() {
        while (!recv_buffer_.empty()) {
            WebSocketFrame frame;
            size_t consumed = 0;
            auto result = WebSocketFrameCoder::parse(recv_buffer_.data(), recv_buffer_.size(), frame, consumed);
            
            if (result == FrameParseResult::Incomplete) break;
            if (result == FrameParseResult::Error) {
                recv_buffer_.clear();
                break;
            }

            recv_buffer_.erase(recv_buffer_.begin(), recv_buffer_.begin() + consumed);

            if (frame.opcode == OpCode::Text) {
                std::string text(frame.payload.begin(), frame.payload.end());
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    message_queue_.push_back(text);
                    cv_.notify_all();
                }
            } else if (frame.opcode == OpCode::Binary) {
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    last_binary_ = frame.payload;
                    cv_.notify_all();
                }
            }
        }
    }

    void handleEvent(struct bufferevent* bev, short events) {
        if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
            connected_ = false;
        }
    }

    struct event_base* base_;
    struct bufferevent* bev_;
    std::thread event_thread_;

    int notify_fd_[2];
    struct event* notify_event_;
    std::mutex send_mtx_;
    std::deque<std::vector<uint8_t>> send_queue_;

    std::atomic<bool> connected_;
    bool handshake_done_ = false;
    std::string client_key_;
    std::vector<uint8_t> recv_buffer_;

    std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<std::string> message_queue_;
    std::vector<uint8_t> last_binary_;
};

// ─── Basic WebSocket Tests ────────────────────────────────────────

static void test_basic_text_message() {
    TEST("Basic text message via WebSocket");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    bool client_connected = client.connect("127.0.0.1", port);
    ASSERT_TRUE(client_connected);
    
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client.sendText("hello world");
    
    server.waitForMessage(3000);
    std::string msg = server.getLastMessage();
    ASSERT_STR_EQ(msg, "hello world");

    client.disconnect();
    server.stop();
    PASS();
}

// ─── V2 Protocol Integration Tests ────────────────────────────────

static void test_v2_connect_packet() {
    TEST("V2 CONNECT packet via WebSocket");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V2);
    SioPacket pkt;
    pkt.type = PacketType::CONNECT;
    pkt.version = SocketIOVersion::V2;
    auto encoded = builder.encode_packet(pkt);

    client.sendText(encoded.text_packet);
    server.waitForMessage();

    std::string msg = server.getLastMessage();
    ASSERT_FALSE(msg.empty());

    SioPacket decoded = builder.decode_packet(msg);
    ASSERT_EQ(decoded.type, PacketType::CONNECT);
    ASSERT_EQ(decoded.version, SocketIOVersion::V2);

    client.disconnect();
    server.stop();
    PASS();
}

static void test_v2_event_packet_roundtrip() {
    TEST("V2 EVENT packet roundtrip via WebSocket");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V2);
    Json::Value arg("hello world");
    std::vector<Json::Value> args = {arg};
    auto pkt = builder.build_event_packet("test_event", args);
    auto encoded = builder.encode_packet(pkt);

    client.sendText(encoded.text_packet);
    server.waitForMessage();

    std::string msg = server.getLastMessage();
    ASSERT_FALSE(msg.empty());

    SioPacket decoded = builder.decode_packet(msg);
    ASSERT_EQ(decoded.type, PacketType::EVENT);
    ASSERT_STR_EQ(decoded.event_name, "test_event");
    ASSERT_EQ(decoded.args.size(), (size_t)1);
    ASSERT_STR_EQ(decoded.args[0].asString(), "hello world");

    auto encoded2 = builder.encode_packet(decoded);
    server.sendText(encoded2.text_packet);

    std::string reply = client.waitForMessage();
    ASSERT_FALSE(reply.empty());

    SioPacket decoded2 = builder.decode_packet(reply);
    ASSERT_EQ(decoded2.type, PacketType::EVENT);
    ASSERT_STR_EQ(decoded2.event_name, "test_event");

    client.disconnect();
    server.stop();
    PASS();
}

static void test_v2_namespace_event() {
    TEST("V2 namespace event via WebSocket");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V2);
    Json::Value arg(123);
    std::vector<Json::Value> args = {arg};
    auto pkt = builder.build_event_packet("ns_event", args, "/chat");
    auto encoded = builder.encode_packet(pkt);

    client.sendText(encoded.text_packet);
    server.waitForMessage();

    std::string msg = server.getLastMessage();
    ASSERT_FALSE(msg.empty());

    SioPacket decoded = builder.decode_packet(msg);
    ASSERT_EQ(decoded.type, PacketType::EVENT);
    ASSERT_STR_EQ(decoded.event_name, "ns_event");
    ASSERT_STR_EQ(decoded.namespace_s, "/chat");

    client.disconnect();
    server.stop();
    PASS();
}

static void test_v2_ack_packet() {
    TEST("V2 ACK packet via WebSocket");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V2);
    Json::Value result("ack result");
    std::vector<Json::Value> args = {result};
    auto pkt = builder.build_ack_packet(args, "/", 42);
    auto encoded = builder.encode_packet(pkt);

    client.sendText(encoded.text_packet);
    server.waitForMessage();

    std::string msg = server.getLastMessage();
    ASSERT_FALSE(msg.empty());

    SioPacket decoded = builder.decode_packet(msg);
    ASSERT_EQ(decoded.type, PacketType::ACK);
    ASSERT_EQ(decoded.ack_id, 42);
    ASSERT_EQ(decoded.args.size(), (size_t)1);
    ASSERT_STR_EQ(decoded.args[0].asString(), "ack result");

    client.disconnect();
    server.stop();
    PASS();
}

static void test_v2_binary_event() {
    TEST("V2 binary event via WebSocket");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V2);
    std::vector<uint8_t> bin_data = {0xDE, 0xAD, 0xBE, 0xEF};
    SmartBuffer buf(bin_data.data(), bin_data.size());
    
    Json::Value arg;
    arg["data"] = "test";
    std::vector<Json::Value> args = {arg};
    
    auto pkt = builder.build_event_packet("binary_event", args);
    pkt.binary_parts.push_back(buf);
    auto encoded = builder.encode_packet(pkt);

    ASSERT_TRUE(encoded.is_binary);
    ASSERT_EQ(encoded.binary_parts.size(), (size_t)1);

    client.sendText(encoded.text_packet);
    server.waitForMessage();
    std::string msg = server.getLastMessage();
    ASSERT_FALSE(msg.empty());

    std::vector<uint8_t> bin_vec(encoded.binary_parts[0].data(), encoded.binary_parts[0].data() + encoded.binary_parts[0].size());
    client.sendBinary(bin_vec);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::vector<SmartBuffer> server_binaries;
    std::vector<uint8_t> server_bin = server.getLastBinary();
    if (!server_bin.empty()) {
        server_binaries.emplace_back(server_bin.data(), server_bin.size());
    }

    SioPacket decoded = builder.decode_packet(msg, server_binaries);
    ASSERT_EQ(decoded.type, PacketType::BINARY_EVENT);
    ASSERT_STR_EQ(decoded.event_name, "binary_event");
    ASSERT_EQ(decoded.binary_parts.size(), (size_t)1);

    client.disconnect();
    server.stop();
    PASS();
}

// ─── V3 Protocol Integration Tests ────────────────────────────────

static void test_v3_connect_packet() {
    TEST("V3 CONNECT packet via WebSocket");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V3);
    SioPacket pkt;
    pkt.type = PacketType::CONNECT;
    pkt.version = SocketIOVersion::V3;
    auto encoded = builder.encode_packet(pkt);

    client.sendText(encoded.text_packet);
    server.waitForMessage();

    std::string msg = server.getLastMessage();
    ASSERT_FALSE(msg.empty());

    SioPacket decoded = builder.decode_packet(msg);
    ASSERT_EQ(decoded.type, PacketType::CONNECT);
    ASSERT_EQ(decoded.version, SocketIOVersion::V3);

    client.disconnect();
    server.stop();
    PASS();
}

static void test_v3_event_packet_roundtrip() {
    TEST("V3 EVENT packet roundtrip via WebSocket");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V3);
    Json::Value arg("v3 hello");
    std::vector<Json::Value> args = {arg};
    auto pkt = builder.build_event_packet("v3_event", args);
    auto encoded = builder.encode_packet(pkt);

    client.sendText(encoded.text_packet);
    server.waitForMessage();

    std::string msg = server.getLastMessage();
    ASSERT_FALSE(msg.empty());

    SioPacket decoded = builder.decode_packet(msg);
    ASSERT_EQ(decoded.type, PacketType::EVENT);
    ASSERT_STR_EQ(decoded.event_name, "v3_event");
    ASSERT_EQ(decoded.args.size(), (size_t)1);
    ASSERT_STR_EQ(decoded.args[0].asString(), "v3 hello");

    auto encoded2 = builder.encode_packet(decoded);
    server.sendText(encoded2.text_packet);

    std::string reply = client.waitForMessage();
    ASSERT_FALSE(reply.empty());

    SioPacket decoded2 = builder.decode_packet(reply);
    ASSERT_EQ(decoded2.type, PacketType::EVENT);
    ASSERT_STR_EQ(decoded2.event_name, "v3_event");

    client.disconnect();
    server.stop();
    PASS();
}

static void test_v3_namespace_event() {
    TEST("V3 namespace event via WebSocket");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V3);
    Json::Value arg("ns data");
    std::vector<Json::Value> args = {arg};
    auto pkt = builder.build_event_packet("ns_event_v3", args, "/admin");
    auto encoded = builder.encode_packet(pkt);

    client.sendText(encoded.text_packet);
    server.waitForMessage();

    std::string msg = server.getLastMessage();
    ASSERT_FALSE(msg.empty());

    SioPacket decoded = builder.decode_packet(msg);
    ASSERT_EQ(decoded.type, PacketType::EVENT);
    ASSERT_STR_EQ(decoded.event_name, "ns_event_v3");
    ASSERT_STR_EQ(decoded.namespace_s, "/admin");

    client.disconnect();
    server.stop();
    PASS();
}

static void test_v3_ack_packet() {
    TEST("V3 ACK packet via WebSocket");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V3);
    Json::Value result;
    result["status"] = "ok";
    std::vector<Json::Value> args = {result};
    auto pkt = builder.build_ack_packet(args, "/", 99);
    auto encoded = builder.encode_packet(pkt);

    client.sendText(encoded.text_packet);
    server.waitForMessage();

    std::string msg = server.getLastMessage();
    ASSERT_FALSE(msg.empty());

    SioPacket decoded = builder.decode_packet(msg);
    ASSERT_EQ(decoded.type, PacketType::ACK);
    ASSERT_EQ(decoded.ack_id, 99);
    ASSERT_EQ(decoded.args.size(), (size_t)1);
    ASSERT_STR_EQ(decoded.args[0]["status"].asString(), "ok");

    client.disconnect();
    server.stop();
    PASS();
}

static void test_v3_binary_event() {
    TEST("V3 binary event via WebSocket");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V3);
    std::vector<uint8_t> bin_data = {0x01, 0x02, 0x03, 0x04, 0x05};
    SmartBuffer buf(bin_data.data(), bin_data.size());
    
    Json::Value arg;
    arg["file"] = "test.bin";
    arg["_placeholder"] = true;
    arg["num"] = 0;
    std::vector<Json::Value> args = {arg};
    
    auto pkt = builder.build_event_packet("upload", args);
    pkt.binary_parts.push_back(buf);
    auto encoded = builder.encode_packet(pkt);

    ASSERT_TRUE(encoded.is_binary);
    ASSERT_EQ(encoded.binary_parts.size(), (size_t)1);

    client.sendText(encoded.text_packet);
    server.waitForMessage();
    std::string msg = server.getLastMessage();
    ASSERT_FALSE(msg.empty());

    std::vector<uint8_t> bin_vec_v3(encoded.binary_parts[0].data(), encoded.binary_parts[0].data() + encoded.binary_parts[0].size());
    client.sendBinary(bin_vec_v3);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::vector<SmartBuffer> server_binaries;
    std::vector<uint8_t> server_bin = server.getLastBinary();
    if (!server_bin.empty()) {
        server_binaries.emplace_back(server_bin.data(), server_bin.size());
    }

    SioPacket decoded = builder.decode_packet(msg, server_binaries);
    ASSERT_EQ(decoded.type, PacketType::BINARY_EVENT);
    ASSERT_STR_EQ(decoded.event_name, "upload");
    ASSERT_EQ(decoded.binary_parts.size(), (size_t)1);

    client.disconnect();
    server.stop();
    PASS();
}

// ─── Cross-version Compatibility Tests ────────────────────────────

static void test_v2_v3_interop() {
    TEST("V2/V3 interop via WebSocket");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder v2_builder(SocketIOVersion::V2);
    SioPacketBuilder v3_builder(SocketIOVersion::V3);

    Json::Value arg("interop test");
    std::vector<Json::Value> args = {arg};
    
    auto v2_pkt = v2_builder.build_event_packet("interop", args);
    auto v2_encoded = v2_builder.encode_packet(v2_pkt);

    client.sendText(v2_encoded.text_packet);
    server.waitForMessage();
    std::string v2_msg = server.getLastMessage();
    ASSERT_FALSE(v2_msg.empty());

    SioPacket decoded_v2 = v3_builder.decode_packet(v2_msg);
    ASSERT_EQ(decoded_v2.type, PacketType::EVENT);
    ASSERT_STR_EQ(decoded_v2.event_name, "interop");

    auto v3_pkt = v3_builder.build_event_packet("interop_v3", args);
    auto v3_encoded = v3_builder.encode_packet(v3_pkt);

    server.sendText(v3_encoded.text_packet);
    std::string v3_msg = client.waitForMessage();
    ASSERT_FALSE(v3_msg.empty());

    SioPacket decoded_v3 = v2_builder.decode_packet(v3_msg);
    ASSERT_EQ(decoded_v3.type, PacketType::EVENT);
    ASSERT_STR_EQ(decoded_v3.event_name, "interop_v3");

    client.disconnect();
    server.stop();
    PASS();
}

// ─── SIOPacket Integration Tests ──────────────────────────────────

static void test_siopacket_v2_via_websocket() {
    TEST("SIOPacket V2 via WebSocket");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SIOPacket pkt(SocketIOVersion::V2);
    pkt.set_type(PacketType::EVENT);
    pkt.set_event_name("siopacket_v2");
    Json::Value arg("siopacket test");
    pkt.add_arg(arg);

    std::string built = pkt.build();
    ASSERT_FALSE(built.empty());

    client.sendText(built);
    server.waitForMessage();

    std::string msg = server.getLastMessage();
    ASSERT_FALSE(msg.empty());

    SIOPacket decoded(SocketIOVersion::V2);
    ASSERT_TRUE(decoded.parse(msg));
    ASSERT_EQ(decoded.type(), PacketType::EVENT);
    ASSERT_STR_EQ(decoded.event_name(), "siopacket_v2");
    ASSERT_EQ(decoded.args().size(), (size_t)1);
    ASSERT_STR_EQ(decoded.args()[0].asString(), "siopacket test");

    client.disconnect();
    server.stop();
    PASS();
}

static void test_siopacket_v3_via_websocket() {
    TEST("SIOPacket V3 via WebSocket");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SIOPacket pkt(SocketIOVersion::V3);
    pkt.set_type(PacketType::EVENT);
    pkt.set_event_name("siopacket_v3");
    pkt.set_namespace("/test");
    Json::Value arg;
    arg["key"] = "value";
    pkt.add_arg(arg);

    std::string built = pkt.build();
    ASSERT_FALSE(built.empty());

    client.sendText(built);
    server.waitForMessage();

    std::string msg = server.getLastMessage();
    ASSERT_FALSE(msg.empty());

    SIOPacket decoded(SocketIOVersion::V3);
    ASSERT_TRUE(decoded.parse(msg));
    ASSERT_EQ(decoded.type(), PacketType::EVENT);
    ASSERT_STR_EQ(decoded.event_name(), "siopacket_v3");
    ASSERT_STR_EQ(decoded.namespace_str(), "/test");
    ASSERT_EQ(decoded.args().size(), (size_t)1);
    ASSERT_STR_EQ(decoded.args()[0]["key"].asString(), "value");

    client.disconnect();
    server.stop();
    PASS();
}

// ─── Multiple Packet Tests ────────────────────────────────────────

static void test_multiple_v2_events() {
    TEST("Multiple V2 events via WebSocket");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V2);
    
    for (int i = 0; i < 5; i++) {
        Json::Value arg(i);
        std::vector<Json::Value> args = {arg};
        auto pkt = builder.build_event_packet("event_" + std::to_string(i), args);
        auto encoded = builder.encode_packet(pkt);
        client.sendText(encoded.text_packet);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    SioPacketBuilder server_builder(SocketIOVersion::V2);
    for (int i = 0; i < 5; i++) {
        Json::Value arg(i * 2);
        std::vector<Json::Value> args = {arg};
        auto pkt = server_builder.build_event_packet("reply_" + std::to_string(i), args);
        auto encoded = server_builder.encode_packet(pkt);
        server.sendText(encoded.text_packet);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    client.disconnect();
    server.stop();
    PASS();
}

static void test_text_binary_mixed() {
    TEST("Text and binary mixed via WebSocket");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    client.sendText("plain text message");
    server.waitForMessage();
    std::string text_msg = server.getLastMessage();
    ASSERT_STR_EQ(text_msg, "plain text message");

    std::vector<uint8_t> bin_data = {0x10, 0x20, 0x30, 0x40};
    client.sendBinary(bin_data);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::vector<uint8_t> bin_msg = server.getLastBinary();
    ASSERT_EQ(bin_msg.size(), bin_data.size());
    ASSERT_TRUE(bin_msg == bin_data);

    server.sendText("server text");
    std::string server_text = client.waitForMessage();
    ASSERT_STR_EQ(server_text, "server text");

    client.disconnect();
    server.stop();
    PASS();
}

// ─── Full-Duplex ACK Tests ────────────────────────────────────────

static void test_v2_client_to_server_ack() {
    TEST("V2 client-to-server ACK (full-duplex)");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V2);

    // 客户端发送带 ACK 的 EVENT
    Json::Value arg("client request data");
    std::vector<Json::Value> args = {arg};
    int client_ack_id = 42;
    auto pkt = builder.build_event_packet("client_call", args, "/", client_ack_id);
    auto encoded = builder.encode_packet(pkt);

    // 服务端接收消息
    server.setOnMessage([&](const std::string& msg) {
        SioPacketBuilder server_builder(SocketIOVersion::V2);
        SioPacket decoded = server_builder.decode_packet(msg);
        if (decoded.type == PacketType::EVENT && decoded.ack_id == client_ack_id) {
            // 服务端返回 ACK
            Json::Value result;
            result["status"] = "ok";
            result["message"] = "server received";
            std::vector<Json::Value> ack_args = {result};
            auto ack_pkt = server_builder.build_ack_packet(ack_args, "/", client_ack_id);
            auto ack_encoded = server_builder.encode_packet(ack_pkt);
            server.sendText(ack_encoded.text_packet);
        }
    });

    client.sendText(encoded.text_packet);

    // 客户端等待 ACK 响应
    std::string ack_msg = client.waitForMessage(3000);
    ASSERT_FALSE(ack_msg.empty());

    SioPacket ack_decoded = builder.decode_packet(ack_msg);
    ASSERT_EQ(ack_decoded.type, PacketType::ACK);
    ASSERT_EQ(ack_decoded.ack_id, client_ack_id);
    ASSERT_EQ(ack_decoded.args.size(), (size_t)1);
    ASSERT_STR_EQ(ack_decoded.args[0]["status"].asString(), "ok");
    ASSERT_STR_EQ(ack_decoded.args[0]["message"].asString(), "server received");

    client.disconnect();
    server.stop();
    PASS();
}

static void test_v2_server_to_client_ack() {
    TEST("V2 server-to-client ACK (full-duplex)");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V2);
    int server_ack_id = 100;
    std::atomic<bool> ack_received{false};

    // 服务端发送带 ACK 的 EVENT
    Json::Value arg("server push data");
    std::vector<Json::Value> args = {arg};
    auto pkt = builder.build_event_packet("server_push", args, "/", server_ack_id);
    auto encoded = builder.encode_packet(pkt);

    // 服务端等待 ACK 响应
    server.setOnMessage([&](const std::string& msg) {
        SioPacketBuilder server_builder(SocketIOVersion::V2);
        SioPacket decoded = server_builder.decode_packet(msg);
        if (decoded.type == PacketType::ACK && decoded.ack_id == server_ack_id) {
            ack_received = true;
        }
    });

    // 在另一个线程中模拟服务端主动推送
    std::thread push_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        server.sendText(encoded.text_packet);
    });

    // 客户端接收 EVENT 并返回 ACK
    std::string event_msg = client.waitForMessage(3000);
    ASSERT_FALSE(event_msg.empty());

    SioPacket event_decoded = builder.decode_packet(event_msg);
    ASSERT_EQ(event_decoded.type, PacketType::EVENT);
    ASSERT_EQ(event_decoded.ack_id, server_ack_id);
    ASSERT_STR_EQ(event_decoded.event_name, "server_push");

    // 客户端返回 ACK
    Json::Value ack_result;
    ack_result["received"] = true;
    ack_result["client_time"] = "now";
    std::vector<Json::Value> ack_args = {ack_result};
    auto ack_pkt = builder.build_ack_packet(ack_args, "/", server_ack_id);
    auto ack_encoded = builder.encode_packet(ack_pkt);
    client.sendText(ack_encoded.text_packet);

    // 等待服务端确认 ACK（轮询）
    int wait_ms = 0;
    while (!ack_received.load() && wait_ms < 2000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        wait_ms += 50;
    }

    ASSERT_TRUE(ack_received.load());

    push_thread.join();
    client.disconnect();
    server.stop();
    PASS();
}

static void test_v3_client_to_server_ack() {
    TEST("V3 client-to-server ACK (full-duplex)");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V3);

    // 客户端发送带 ACK 的 EVENT
    Json::Value arg("v3 client request");
    std::vector<Json::Value> args = {arg};
    int client_ack_id = 55;
    auto pkt = builder.build_event_packet("v3_client_call", args, "/", client_ack_id);
    auto encoded = builder.encode_packet(pkt);

    // 服务端接收消息并返回 ACK
    server.setOnMessage([&](const std::string& msg) {
        SioPacketBuilder server_builder(SocketIOVersion::V3);
        SioPacket decoded = server_builder.decode_packet(msg);
        if (decoded.type == PacketType::EVENT && decoded.ack_id == client_ack_id) {
            Json::Value result;
            result["code"] = 200;
            result["data"] = "v3 ack response";
            std::vector<Json::Value> ack_args = {result};
            auto ack_pkt = server_builder.build_ack_packet(ack_args, "/", client_ack_id);
            auto ack_encoded = server_builder.encode_packet(ack_pkt);
            server.sendText(ack_encoded.text_packet);
        }
    });

    client.sendText(encoded.text_packet);

    std::string ack_msg = client.waitForMessage(3000);
    ASSERT_FALSE(ack_msg.empty());

    SioPacket ack_decoded = builder.decode_packet(ack_msg);
    ASSERT_EQ(ack_decoded.type, PacketType::ACK);
    ASSERT_EQ(ack_decoded.ack_id, client_ack_id);
    ASSERT_EQ(ack_decoded.args.size(), (size_t)1);
    ASSERT_EQ(ack_decoded.args[0]["code"].asInt(), 200);

    client.disconnect();
    server.stop();
    PASS();
}

static void test_v3_server_to_client_ack() {
    TEST("V3 server-to-client ACK (full-duplex)");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V3);
    int server_ack_id = 200;
    std::atomic<bool> ack_received{false};

    Json::Value arg("v3 server push");
    std::vector<Json::Value> args = {arg};
    auto pkt = builder.build_event_packet("v3_server_push", args, "/", server_ack_id);
    auto encoded = builder.encode_packet(pkt);

    server.setOnMessage([&](const std::string& msg) {
        SioPacketBuilder server_builder(SocketIOVersion::V3);
        SioPacket decoded = server_builder.decode_packet(msg);
        if (decoded.type == PacketType::ACK && decoded.ack_id == server_ack_id) {
            ack_received = true;
        }
    });

    std::thread push_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        server.sendText(encoded.text_packet);
    });

    std::string event_msg = client.waitForMessage(3000);
    ASSERT_FALSE(event_msg.empty());

    SioPacket event_decoded = builder.decode_packet(event_msg);
    ASSERT_EQ(event_decoded.type, PacketType::EVENT);
    ASSERT_EQ(event_decoded.ack_id, server_ack_id);
    ASSERT_STR_EQ(event_decoded.event_name, "v3_server_push");

    Json::Value ack_result;
    ack_result["status"] = "received";
    std::vector<Json::Value> ack_args = {ack_result};
    auto ack_pkt = builder.build_ack_packet(ack_args, "/", server_ack_id);
    auto ack_encoded = builder.encode_packet(ack_pkt);
    client.sendText(ack_encoded.text_packet);

    int wait_ms = 0;
    while (!ack_received.load() && wait_ms < 2000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        wait_ms += 50;
    }

    ASSERT_TRUE(ack_received.load());

    push_thread.join();
    client.disconnect();
    server.stop();
    PASS();
}

// ─── Batch / Stress Tests ─────────────────────────────────────────

static void test_v2_batch_messages_stress() {
    TEST("V2 batch messages stress test (100 messages)");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V2);
    const int NUM_MESSAGES = 100;
    std::atomic<int> server_received{0};
    std::atomic<int> client_received{0};

    // 服务端计数并回显
    server.setOnMessage([&](const std::string& msg) {
        server_received++;
        // 回显给客户端
        server.sendText(msg);
    });

    auto start = std::chrono::high_resolution_clock::now();

    // 客户端批量发送
    for (int i = 0; i < NUM_MESSAGES; i++) {
        Json::Value arg(i);
        std::vector<Json::Value> args = {arg};
        auto pkt = builder.build_event_packet("stress_msg_" + std::to_string(i), args);
        auto encoded = builder.encode_packet(pkt);
        client.sendText(encoded.text_packet);
    }

    // 等待所有消息完成
    int wait_ms = 0;
    while (client_received.load() < NUM_MESSAGES && wait_ms < 5000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        wait_ms += 50;
        // 手动轮询 - 因为 waitForMessage 是一次性的
        std::string msg = client.waitForMessage(50);
        if (!msg.empty()) {
            client_received++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    ASSERT_TRUE(server_received.load() >= NUM_MESSAGES * 0.9);
    ASSERT_TRUE(client_received.load() >= NUM_MESSAGES * 0.9);

    std::cout << "  (sent: " << NUM_MESSAGES 
              << ", server received: " << server_received.load()
              << ", client received: " << client_received.load()
              << ", time: " << duration << "ms)";

    client.disconnect();
    server.stop();
    PASS();
}

static void test_v2_full_duplex_stress() {
    TEST("V2 full-duplex stress test (bidirectional 50+50)");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V2);
    const int NUM_MESSAGES = 50;
    std::atomic<int> server_received{0};
    std::atomic<int> client_received{0};

    server.setOnMessage([&](const std::string& msg) {
        server_received++;
    });

    auto start = std::chrono::high_resolution_clock::now();

    // 服务端批量发送线程
    std::thread server_send_thread([&]() {
        SioPacketBuilder s_builder(SocketIOVersion::V2);
        for (int i = 0; i < NUM_MESSAGES; i++) {
            Json::Value arg(i);
            std::vector<Json::Value> args = {arg};
            auto pkt = s_builder.build_event_packet("server_msg_" + std::to_string(i), args);
            auto encoded = s_builder.encode_packet(pkt);
            server.sendText(encoded.text_packet);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // 客户端批量发送
    for (int i = 0; i < NUM_MESSAGES; i++) {
        Json::Value arg(i);
        std::vector<Json::Value> args = {arg};
        auto pkt = builder.build_event_packet("client_msg_" + std::to_string(i), args);
        auto encoded = builder.encode_packet(pkt);
        client.sendText(encoded.text_packet);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    server_send_thread.join();

    // 等待接收完成
    int wait_ms = 0;
    while (client_received.load() < NUM_MESSAGES && wait_ms < 5000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        wait_ms += 50;
        std::string msg = client.waitForMessage(50);
        if (!msg.empty()) {
            client_received++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    ASSERT_TRUE(server_received.load() >= NUM_MESSAGES * 0.9);
    ASSERT_TRUE(client_received.load() >= NUM_MESSAGES * 0.9);

    std::cout << "  (client→server: " << server_received.load() << "/" << NUM_MESSAGES
              << ", server→client: " << client_received.load() << "/" << NUM_MESSAGES
              << ", time: " << duration << "ms)";

    client.disconnect();
    server.stop();
    PASS();
}

static void test_v3_batch_messages_stress() {
    TEST("V3 batch messages stress test (100 messages)");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V3);
    const int NUM_MESSAGES = 100;
    std::atomic<int> server_received{0};
    std::atomic<int> client_received{0};

    server.setOnMessage([&](const std::string& msg) {
        server_received++;
        server.sendText(msg);
    });

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_MESSAGES; i++) {
        Json::Value arg(i);
        std::vector<Json::Value> args = {arg};
        auto pkt = builder.build_event_packet("v3_stress_" + std::to_string(i), args);
        auto encoded = builder.encode_packet(pkt);
        client.sendText(encoded.text_packet);
    }

    int wait_ms = 0;
    while (client_received.load() < NUM_MESSAGES && wait_ms < 5000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        wait_ms += 50;
        std::string msg = client.waitForMessage(50);
        if (!msg.empty()) {
            client_received++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    ASSERT_TRUE(server_received.load() >= NUM_MESSAGES * 0.9);
    ASSERT_TRUE(client_received.load() >= NUM_MESSAGES * 0.9);

    std::cout << "  (sent: " << NUM_MESSAGES
              << ", server received: " << server_received.load()
              << ", client received: " << client_received.load()
              << ", time: " << duration << "ms)";

    client.disconnect();
    server.stop();
    PASS();
}

static void test_v3_full_duplex_stress() {
    TEST("V3 full-duplex stress test (bidirectional 50+50)");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V3);
    const int NUM_MESSAGES = 50;
    std::atomic<int> server_received{0};
    std::atomic<int> client_received{0};

    server.setOnMessage([&](const std::string& msg) {
        server_received++;
    });

    auto start = std::chrono::high_resolution_clock::now();

    std::thread server_send_thread([&]() {
        SioPacketBuilder s_builder(SocketIOVersion::V3);
        for (int i = 0; i < NUM_MESSAGES; i++) {
            Json::Value arg(i);
            std::vector<Json::Value> args = {arg};
            auto pkt = s_builder.build_event_packet("srv_v3_" + std::to_string(i), args);
            auto encoded = s_builder.encode_packet(pkt);
            server.sendText(encoded.text_packet);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    for (int i = 0; i < NUM_MESSAGES; i++) {
        Json::Value arg(i);
        std::vector<Json::Value> args = {arg};
        auto pkt = builder.build_event_packet("cli_v3_" + std::to_string(i), args);
        auto encoded = builder.encode_packet(pkt);
        client.sendText(encoded.text_packet);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    server_send_thread.join();

    int wait_ms = 0;
    while (client_received.load() < NUM_MESSAGES && wait_ms < 5000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        wait_ms += 50;
        std::string msg = client.waitForMessage(50);
        if (!msg.empty()) {
            client_received++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    ASSERT_TRUE(server_received.load() >= NUM_MESSAGES * 0.9);
    ASSERT_TRUE(client_received.load() >= NUM_MESSAGES * 0.9);

    std::cout << "  (client→server: " << server_received.load() << "/" << NUM_MESSAGES
              << ", server→client: " << client_received.load() << "/" << NUM_MESSAGES
              << ", time: " << duration << "ms)";

    client.disconnect();
    server.stop();
    PASS();
}

static void test_concurrent_ack_stress() {
    TEST("Concurrent ACK stress test (20 parallel ACKs)");
    MockSocketIOServer server;
    int port = server.start();
    ASSERT_TRUE(port > 0);

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    server.waitForConnection();
    ASSERT_TRUE(server.isConnected());

    SioPacketBuilder builder(SocketIOVersion::V2);
    const int NUM_ACKS = 20;
    std::atomic<int> acks_completed{0};

    // 服务端收到 EVENT 后返回 ACK
    server.setOnMessage([&](const std::string& msg) {
        SioPacketBuilder s_builder(SocketIOVersion::V2);
        SioPacket decoded = s_builder.decode_packet(msg);
        if (decoded.type == PacketType::EVENT && decoded.ack_id > 0) {
            Json::Value result;
            result["ack_id"] = decoded.ack_id;
            result["processed"] = true;
            std::vector<Json::Value> ack_args = {result};
            auto ack_pkt = s_builder.build_ack_packet(ack_args, "/", decoded.ack_id);
            auto ack_encoded = s_builder.encode_packet(ack_pkt);
            server.sendText(ack_encoded.text_packet);
        }
    });

    auto start = std::chrono::high_resolution_clock::now();

    // 并发发送多个带 ACK 的请求
    for (int i = 1; i <= NUM_ACKS; i++) {
        Json::Value arg("request_" + std::to_string(i));
        std::vector<Json::Value> args = {arg};
        auto pkt = builder.build_event_packet("concurrent_call", args, "/", i);
        auto encoded = builder.encode_packet(pkt);
        client.sendText(encoded.text_packet);
    }

    // 等待所有 ACK 返回
    int wait_ms = 0;
    while (acks_completed.load() < NUM_ACKS && wait_ms < 5000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        wait_ms += 50;
        std::string msg = client.waitForMessage(50);
        if (!msg.empty()) {
            SioPacket ack_decoded = builder.decode_packet(msg);
            if (ack_decoded.type == PacketType::ACK) {
                acks_completed++;
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    ASSERT_TRUE(acks_completed.load() >= NUM_ACKS * 0.9);

    std::cout << "  (acks completed: " << acks_completed.load() << "/" << NUM_ACKS
              << ", time: " << duration << "ms)";

    client.disconnect();
    server.stop();
    PASS();
}

// ─── Main ─────────────────────────────────────────────────────────

int main() {
    std::cout << std::endl;
    std::cout << "######################################################################" << std::endl;
    std::cout << "#   WebSocket + Socket.IO 协议集成测试" << std::endl;
    std::cout << "#   测试范围: V2/V3/二进制/命名空间/全双工ACK/批量压力/跨版本" << std::endl;
    std::cout << "######################################################################" << std::endl;
    std::cout << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "  基础 WebSocket 连接测试" << std::endl;
    std::cout << "============================================================" << std::endl;
    test_basic_text_message();

    std::cout << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "  V2 协议 WebSocket 集成测试" << std::endl;
    std::cout << "============================================================" << std::endl;
    test_v2_connect_packet();
    test_v2_event_packet_roundtrip();
    test_v2_namespace_event();
    test_v2_ack_packet();
    test_v2_binary_event();

    std::cout << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "  V3 协议 WebSocket 集成测试" << std::endl;
    std::cout << "============================================================" << std::endl;
    test_v3_connect_packet();
    test_v3_event_packet_roundtrip();
    test_v3_namespace_event();
    test_v3_ack_packet();
    test_v3_binary_event();

    std::cout << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "  跨版本兼容性测试" << std::endl;
    std::cout << "============================================================" << std::endl;
    test_v2_v3_interop();

    std::cout << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "  SIOPacket 集成测试" << std::endl;
    std::cout << "============================================================" << std::endl;
    test_siopacket_v2_via_websocket();
    test_siopacket_v3_via_websocket();

    std::cout << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "  多包/混合传输测试" << std::endl;
    std::cout << "============================================================" << std::endl;
    test_multiple_v2_events();
    test_text_binary_mixed();

    std::cout << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "  全双工 ACK 测试" << std::endl;
    std::cout << "============================================================" << std::endl;
    test_v2_client_to_server_ack();
    test_v2_server_to_client_ack();
    test_v3_client_to_server_ack();
    test_v3_server_to_client_ack();

    std::cout << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "  批量消息 & 压力测试" << std::endl;
    std::cout << "============================================================" << std::endl;
    test_v2_batch_messages_stress();
    test_v2_full_duplex_stress();
    test_v3_batch_messages_stress();
    test_v3_full_duplex_stress();
    test_concurrent_ack_stress();

    std::cout << std::endl;
    std::cout << "======================================================================" << std::endl;
    std::cout << "  测试总结" << std::endl;
    std::cout << "======================================================================" << std::endl;
    std::cout << "  总测试数: " << (g_tests_passed + g_tests_failed) << std::endl;
    std::cout << "  通过: " << g_tests_passed << std::endl;
    std::cout << "  失败: " << g_tests_failed << std::endl;
    std::cout << std::endl;

    if (g_tests_failed > 0) {
        std::cout << "  ❌ 部分测试失败！" << std::endl;
        return 1;
    }

    std::cout << "  🎉 所有测试通过！" << std::endl;
    return 0;
}
