#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "core/socketio/sio_client.h"
#include "core/websocket/websocket_client.h"
#include "json/json.h"

enum class EnginePacketType {
    OPEN = 0,
    CLOSE = 1,
    PING = 2,
    PONG = 3,
    MESSAGE = 4,
    UPGRADE = 5,
    NOOP = 6
};

class IntegrationClient {
public:
    IntegrationClient(sio::SocketIOVersion version) : version_(version) {
        sio::SioClient::Config config;
        config.version = version;
        config.enable_logging = false;
        config.max_retries = 0;
        client_ = sio::SioClient::Create(config);
        
        client_->set_send_callback([this](const std::string& text_packet, 
                                           const std::vector<sio::SmartBuffer>& bins) -> bool {
            if (ws_ && ws_->isConnected()) {
                std::string engine_packet = "4" + text_packet;
                ws_->sendText(engine_packet);
                for (const auto& bin : bins) {
                    std::vector<uint8_t> data(bin.data(), bin.data() + bin.size());
                    ws_->sendBinary(data);
                }
                return true;
            }
            return false;
        });
    }
    
    ~IntegrationClient() {
        disconnect();
    }
    
    void connect(const std::string& url) {
        ws_ = std::make_shared<ws::WebSocketClient>();
        
        std::string ws_url = buildWebSocketURL(url);
        
        if (ws_url.substr(0, 6) == "wss://") {
            ws_->setSelfSignedSSL(true);
        }
        
        ws_->setOnConnect([this]() {
            ws_connected_ = true;
        });
        
        ws_->setOnDisconnect([this](const std::string& reason, ws::CloseCode code) {
            std::unique_lock<std::mutex> lock(mtx_);
            ws_connected_ = false;
            sio_connected_ = false;
            cv_.notify_all();
        });
        
        ws_->setOnText([this](const std::string& text) {
            handleEnginePacket(text);
        });
        
        ws_->setOnBinary([this](const std::vector<uint8_t>& data) {
            if (client_) {
                sio::SmartBuffer buf(data.data(), data.size());
                client_->process_binary_data(buf);
            }
        });
        
        ws_->setOnError([this](const std::string& error) {
            std::unique_lock<std::mutex> lock(mtx_);
            ws_error_ = error;
            cv_.notify_all();
        });
        
        ws_->setURL(ws_url);
        ws_->connect();
    }
    
    bool waitForSioConnect(int timeout_sec = 10) {
        std::unique_lock<std::mutex> lock(mtx_);
        return cv_.wait_for(lock, std::chrono::seconds(timeout_sec), 
                           [this] { return sio_connected_; });
    }
    
    bool waitForDisconnect(int timeout_sec = 10) {
        std::unique_lock<std::mutex> lock(mtx_);
        return cv_.wait_for(lock, std::chrono::seconds(timeout_sec), 
                           [this] { return !ws_connected_; });
    }
    
    void disconnect() {
        if (ws_) {
            ws_->disconnect();
        }
    }
    
    void on(const std::string& event, sio::EventHandler handler) {
        client_->on(event, [handler, event](const std::vector<Json::Value>& args, sio::AckResponder ack) {
            if (handler) {
                handler(args, ack);
            }
        });
    }
    
    void onSioConnect(std::function<void()> cb) {
        connect_callback_ = cb;
    }
    
    void onSioDisconnect(std::function<void()> cb) {
        client_->on("disconnect", [cb](const std::vector<Json::Value>& args, sio::AckResponder ack) {
            if (cb) cb();
        });
    }
    
    void emit(const std::string& event, std::initializer_list<Json::Value> args) {
        client_->emit(event, args);
    }
    
    void emitWithAck(const std::string& event, 
                     std::initializer_list<Json::Value> args,
                     sio::AckCallback ack_cb,
                     sio::AckTimeoutCallback timeout_cb = nullptr,
                     std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
        client_->emit(event, args, ack_cb, timeout_cb, timeout);
    }
    
    std::shared_ptr<sio::SioClient> sioClient() { return client_; }
    std::shared_ptr<ws::WebSocketClient> wsClient() { return ws_; }
    
private:
    void handleEnginePacket(const std::string& text) {
        if (text.empty()) return;
        
        char type_char = text[0];
        if (type_char < '0' || type_char > '9') return;
        
        EnginePacketType type = static_cast<EnginePacketType>(type_char - '0');
        std::string content = text.substr(1);
        
        switch (type) {
            case EnginePacketType::OPEN:
                handleEngineOpen(content);
                break;
            case EnginePacketType::CLOSE:
                break;
            case EnginePacketType::PING:
                if (ws_ && ws_->isConnected()) {
                    ws_->sendText("3");
                }
                break;
            case EnginePacketType::PONG:
                break;
            case EnginePacketType::MESSAGE:
                handleSioMessage(content);
                break;
            case EnginePacketType::UPGRADE:
                break;
            case EnginePacketType::NOOP:
                break;
        }
    }
    
    void handleEngineOpen(const std::string& content) {
        sendSioConnect();
    }
    
    void sendSioConnect() {
        if (ws_ && ws_->isConnected()) {
            std::string connect_packet = "40";
            ws_->sendText(connect_packet);
        }
    }
    
    void handleSioMessage(const std::string& content) {
        if (content.empty()) return;
        
        char sio_type = content[0];
        
        if (sio_type == '0') {
            std::unique_lock<std::mutex> lock(mtx_);
            sio_connected_ = true;
            cv_.notify_all();
            lock.unlock();
            if (connect_callback_) {
                connect_callback_();
            }
            return;
        }
        
        if (sio_type == '1') {
            std::unique_lock<std::mutex> lock(mtx_);
            sio_connected_ = false;
            cv_.notify_all();
            return;
        }
        
        if (sio_type == '2' || sio_type == '5') {
            size_t bracket = content.find('[');
            if (bracket != std::string::npos) {
                size_t quote1 = content.find('"', bracket);
                if (quote1 != std::string::npos) {
                    size_t quote2 = content.find('"', quote1 + 1);
                    if (quote2 != std::string::npos) {
                        std::string event_name = content.substr(quote1 + 1, quote2 - quote1 - 1);
                        std::cout << "[DEBUG] Event received: " << event_name << std::endl;
                    }
                }
            }
        }
        
        if (client_) {
            client_->process_text_packet(content);
        }
    }
    
    std::string buildWebSocketURL(const std::string& base_url) {
        std::string url = base_url;
        
        if (url.substr(0, 7) == "http://") {
            url = "ws://" + url.substr(7);
        } else if (url.substr(0, 8) == "https://") {
            url = "wss://" + url.substr(8);
        }
        
        if (url.back() != '/') {
            url += "/";
        }
        url += "socket.io/?EIO=";
        
        switch (version_) {
            case sio::SocketIOVersion::V2:
                url += "3";
                break;
            case sio::SocketIOVersion::V3:
            case sio::SocketIOVersion::V4:
                url += "4";
                break;
        }
        url += "&transport=websocket";
        
        return url;
    }
    
    sio::SocketIOVersion version_;
    std::shared_ptr<sio::SioClient> client_;
    std::shared_ptr<ws::WebSocketClient> ws_;
    std::function<void()> connect_callback_;
    
    std::mutex mtx_;
    std::condition_variable cv_;
    bool ws_connected_ = false;
    bool sio_connected_ = false;
    std::string ws_error_;
};

struct TestContext {
    int passed = 0;
    int failed = 0;
    std::string current_test;
};

static void report_title(const std::string& title) {
    std::cout << "\n=== " << title << " ===" << std::endl;
}

static void report_result(const std::string& name, bool passed) {
    if (passed) {
        std::cout << "  [PASS] " << name << std::endl;
    } else {
        std::cout << "  [FAIL] " << name << std::endl;
    }
}

static void test_connect_v2(TestContext& ctx, const std::string& base_url) {
    report_title("V2 Connect Test");
    
    IntegrationClient client(sio::SocketIOVersion::V2);
    
    bool connected = false;
    client.onSioConnect([&]() { connected = true; });
    
    client.connect(base_url);
    
    if (client.waitForSioConnect(10)) {
        report_result("V2 connect", true);
        ctx.passed++;
    } else {
        report_result("V2 connect", false);
        ctx.failed++;
    }
    
    client.disconnect();
    client.waitForDisconnect(5);
}

static void test_connect_v3(TestContext& ctx, const std::string& base_url) {
    report_title("V3 Connect Test");
    
    IntegrationClient client(sio::SocketIOVersion::V3);
    
    bool connected = false;
    client.onSioConnect([&]() { connected = true; });
    
    client.connect(base_url);
    
    if (client.waitForSioConnect(10)) {
        report_result("V3 connect", true);
        ctx.passed++;
    } else {
        report_result("V3 connect", false);
        ctx.failed++;
    }
    
    client.disconnect();
    client.waitForDisconnect(5);
}

static void test_ack_v3(TestContext& ctx, const std::string& base_url) {
    report_title("V3 ACK Test");
    
    IntegrationClient client(sio::SocketIOVersion::V3);
    client.connect(base_url);
    
    if (!client.waitForSioConnect(10)) {
        report_result("V3 ACK - connect", false);
        ctx.failed++;
        return;
    }
    report_result("V3 ACK - connect", true);
    ctx.passed++;
    
    std::mutex mtx;
    std::condition_variable cv;
    bool ack_received = false;
    bool ack_ok = false;
    
    client.emitWithAck("echo", {Json::Value("hello_ack")}, 
        [&](const std::vector<Json::Value>& data) {
            std::unique_lock<std::mutex> lock(mtx);
            ack_received = true;
            if (data.size() > 0 && data[0].isObject()) {
                ack_ok = data[0]["echoed"].asString() == "hello_ack";
            }
            cv.notify_all();
        },
        [&](int ack_id) {
            std::unique_lock<std::mutex> lock(mtx);
            ack_received = true;
            ack_ok = false;
            cv.notify_all();
        },
        std::chrono::milliseconds(5000)
    );
    
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(8), [&] { return ack_received; });
    }
    
    report_result("V3 ACK - echo response", ack_ok);
    if (ack_ok) ctx.passed++; else ctx.failed++;
    
    client.disconnect();
    client.waitForDisconnect(5);
}

static void test_event_send_receive_v3(TestContext& ctx, const std::string& base_url) {
    report_title("V3 Event Send/Receive Test");
    
    IntegrationClient client(sio::SocketIOVersion::V3);
    client.connect(base_url);
    
    if (!client.waitForSioConnect(10)) {
        report_result("V3 event - connect", false);
        ctx.failed++;
        return;
    }
    report_result("V3 event - connect", true);
    ctx.passed++;
    
    std::mutex mtx;
    std::condition_variable cv;
    bool received = false;
    bool ok = false;
    
    client.on("pong", [&](const std::vector<Json::Value>& args, sio::AckResponder ack) {
        std::unique_lock<std::mutex> lock(mtx);
        if (received) return;
        if (args.size() > 0 && args[0].isObject() && 
            args[0]["received"].isObject() && 
            args[0]["received"].isMember("test")) {
            received = true;
            ok = args[0]["received"]["test"].asString() == "hello_v3";
            cv.notify_all();
        }
    });
    
    Json::Value payload;
    payload["test"] = "hello_v3";
    client.emit("ping", {payload});
    
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(8), [&] { return received; });
    }
    
    report_result("V3 event - ping/pong", ok);
    if (ok) ctx.passed++; else ctx.failed++;
    
    client.disconnect();
    client.waitForDisconnect(5);
}

static void test_ack_timeout_v3(TestContext& ctx, const std::string& base_url) {
    report_title("V3 ACK Timeout Test");
    
    IntegrationClient client(sio::SocketIOVersion::V3);
    client.connect(base_url);
    
    if (!client.waitForSioConnect(10)) {
        report_result("V3 ACK timeout - connect", false);
        ctx.failed++;
        return;
    }
    report_result("V3 ACK timeout - connect", true);
    ctx.passed++;
    
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;
    bool timed_out = false;
    
    client.emitWithAck("nonexistent_event_xyz123", {Json::Value("test")}, 
        [&](const std::vector<Json::Value>& data) {
            std::unique_lock<std::mutex> lock(mtx);
            done = true;
            timed_out = false;
            cv.notify_all();
        },
        [&](int ack_id) {
            std::unique_lock<std::mutex> lock(mtx);
            done = true;
            timed_out = true;
            cv.notify_all();
        },
        std::chrono::milliseconds(2000)
    );
    
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(5), [&] { return done; });
    }
    
    report_result("V3 ACK timeout", timed_out);
    if (timed_out) ctx.passed++; else ctx.failed++;
    
    client.disconnect();
    client.waitForDisconnect(5);
}

static void test_binary_v3(TestContext& ctx, const std::string& base_url) {
    report_title("V3 Binary Test");
    
    IntegrationClient client(sio::SocketIOVersion::V3);
    client.connect(base_url);
    
    if (!client.waitForSioConnect(10)) {
        report_result("V3 binary - connect", false);
        ctx.failed++;
        return;
    }
    report_result("V3 binary - connect", true);
    ctx.passed++;
    
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;
    bool ok = false;
    
    client.emitWithAck("echo", {Json::Value("binary_test_check")}, 
        [&](const std::vector<Json::Value>& data) {
            std::unique_lock<std::mutex> lock(mtx);
            done = true;
            if (data.size() > 0 && data[0].isObject()) {
                ok = data[0]["echoed"].asString() == "binary_test_check";
            }
            cv.notify_all();
        },
        [&](int ack_id) {
            std::unique_lock<std::mutex> lock(mtx);
            done = true;
            cv.notify_all();
        },
        std::chrono::milliseconds(5000)
    );
    
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(8), [&] { return done; });
    }
    
    report_result("V3 binary ACK", ok);
    if (ok) ctx.passed++; else ctx.failed++;
    
    client.disconnect();
    client.waitForDisconnect(5);
}

static void test_https_connect_v3(TestContext& ctx, const std::string& https_url) {
    report_title("V3 HTTPS Connect Test");
    
    IntegrationClient client(sio::SocketIOVersion::V3);
    
    bool connected = false;
    client.onSioConnect([&]() { connected = true; });
    
    client.connect(https_url);
    
    if (client.waitForSioConnect(10)) {
        report_result("V3 HTTPS connect", true);
        ctx.passed++;
    } else {
        report_result("V3 HTTPS connect", false);
        ctx.failed++;
    }
    
    client.disconnect();
    client.waitForDisconnect(5);
}

int main(int argc, char* argv[]) {
    std::string v2_url = "http://localhost:3002";
    std::string v3_url = "http://localhost:3003";
    std::string v3_https_url = "https://localhost:3004";
    
    if (argc > 1) v2_url = argv[1];
    if (argc > 2) v3_url = argv[2];
    if (argc > 3) v3_https_url = argv[3];
    
    TestContext ctx;
    
    std::cout << "========================================" << std::endl;
    std::cout << "Socket.IO C++ Integration Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "V2 URL:  " << v2_url << std::endl;
    std::cout << "V3 URL:  " << v3_url << std::endl;
    std::cout << "V3 HTTPS: " << v3_https_url << std::endl;
    
    test_connect_v2(ctx, v2_url);
    test_connect_v3(ctx, v3_url);
    test_event_send_receive_v3(ctx, v3_url);
    test_ack_v3(ctx, v3_url);
    test_ack_timeout_v3(ctx, v3_url);
    test_binary_v3(ctx, v3_url);
    test_https_connect_v3(ctx, v3_https_url);
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "SUMMARY" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Passed: " << ctx.passed << std::endl;
    std::cout << "Failed: " << ctx.failed << std::endl;
    std::cout << "Total:  " << (ctx.passed + ctx.failed) << std::endl;
    std::cout << "========================================" << std::endl;
    
    return ctx.failed > 0 ? 1 : 0;
}
