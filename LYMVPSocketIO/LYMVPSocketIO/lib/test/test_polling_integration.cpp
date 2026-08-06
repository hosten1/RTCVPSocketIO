#include <iostream>
#include <string>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "lib/engineio/engineio_client.h"
#include "lib/sio_client.h"
#include "json/json.h"

struct TestContext {
    int passed = 0;
    int failed = 0;
};

static void report_title(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

static void report_result(const std::string& name, bool ok) {
    if (ok) {
        std::cout << "  ✓ " << name << " - PASS" << std::endl;
    } else {
        std::cout << "  ✗ " << name << " - FAIL" << std::endl;
    }
}

class PollingIntegrationTest {
public:
    PollingIntegrationTest(sio::SocketIOVersion version,
                           engineio::TransportType transport)
        : sio_version_(version), transport_(transport) {
        
        sio::SioClient::Config sio_config;
        sio_config.version = version;
        sio_config.enable_logging = false;
        sio_client_ = sio::SioClient::Create(sio_config);
        
        engineio::EngineIOClient::Config eio_config;
        eio_config.version = (version == sio::SocketIOVersion::V2)
            ? engineio::EngineIOVersion::V2
            : engineio::EngineIOVersion::V3;
        eio_config.transport = transport;
        eio_config.self_signed_ssl = true;
        
        engineio_client_ = engineio::EngineIOClient::Create(eio_config);
    }
    
    void connect(const std::string& url) {
        setup_callbacks();
        engineio_client_->connect(url);
    }
    
    void disconnect() {
        engineio_client_->disconnect();
    }
    
    bool wait_for_connect(int timeout_sec) {
        std::unique_lock<std::mutex> lock(mtx_);
        return cv_.wait_for(lock, std::chrono::seconds(timeout_sec),
                           [this] { return sio_connected_; });
    }
    
    bool wait_for_disconnect(int timeout_sec) {
        std::unique_lock<std::mutex> lock(mtx_);
        return cv_.wait_for(lock, std::chrono::seconds(timeout_sec),
                           [this] { return !sio_connected_; });
    }
    
    void on(const std::string& event, std::function<void(const std::vector<Json::Value>&)> handler) {
        sio_client_->on(event,
            [handler](const std::vector<Json::Value>& args, sio::AckResponder ack) {
                (void)ack;
                if (handler) handler(args);
            });
    }
    
    void emit(const std::string& event, std::initializer_list<Json::Value> args) {
        sio_client_->emit(event, args);
    }
    
    void emit_with_ack(const std::string& event,
                       std::initializer_list<Json::Value> args,
                       sio::AckCallback ack_cb,
                       sio::AckTimeoutCallback timeout_cb,
                       std::chrono::milliseconds timeout) {
        sio_client_->emit(event, args, ack_cb, timeout_cb, timeout);
    }
    
    bool sio_connected() const { return sio_connected_; }
    
    void notify() { cv_.notify_all(); }
    std::mutex& mutex() { return mtx_; }
    
private:
    void setup_callbacks() {
        auto self = this;
        
        engineio_client_->set_open_callback([self]() {
            self->send_sio_connect();
        });
        
        engineio_client_->set_message_callback([self](const std::string& message) {
            if (!message.empty() && message[0] == '0') {
                std::unique_lock<std::mutex> lock(self->mtx_);
                self->sio_connected_ = true;
                self->cv_.notify_all();
            }
            
            self->sio_client_->process_text_packet(message);
        });
        
        engineio_client_->set_close_callback([self](const std::string& reason) {
            (void)reason;
            std::unique_lock<std::mutex> lock(self->mtx_);
            self->sio_connected_ = false;
            self->cv_.notify_all();
        });
        
        sio_client_->set_send_callback(
            [self](const std::string& text, const std::vector<sio::SmartBuffer>& bins) {
                (void)bins;
                if (self->engineio_client_->is_connected()) {
                    self->engineio_client_->send(text);
                    return true;
                }
                return false;
            }
        );
    }
    
    void send_sio_connect() {
        if (sio_version_ >= sio::SocketIOVersion::V3) {
            std::string connect_packet = "0";
            engineio_client_->send(connect_packet);
        }
    }
    
    sio::SocketIOVersion sio_version_;
    engineio::TransportType transport_;
    std::shared_ptr<sio::SioClient> sio_client_;
    std::shared_ptr<engineio::EngineIOClient> engineio_client_;
    
    bool sio_connected_ = false;
    std::mutex mtx_;
    std::condition_variable cv_;
};

// --- 测试用例 ---

static void test_v2_polling_connect(TestContext& ctx, const std::string& url) {
    report_title("V2 Polling - Connect Test");
    
    PollingIntegrationTest client(sio::SocketIOVersion::V2,
                                   engineio::TransportType::POLLING);
    client.connect(url);
    
    bool ok = client.wait_for_connect(10);
    report_result("V2 Polling connect", ok);
    if (ok) ctx.passed++; else ctx.failed++;
    
    client.disconnect();
}

static void test_v3_polling_connect(TestContext& ctx, const std::string& url) {
    report_title("V3 Polling - Connect Test");
    
    PollingIntegrationTest client(sio::SocketIOVersion::V3,
                                   engineio::TransportType::POLLING);
    client.connect(url);
    
    bool ok = client.wait_for_connect(10);
    report_result("V3 Polling connect", ok);
    if (ok) ctx.passed++; else ctx.failed++;
    
    client.disconnect();
}

static void test_v3_polling_event(TestContext& ctx, const std::string& url) {
    report_title("V3 Polling - Event Test");
    
    PollingIntegrationTest client(sio::SocketIOVersion::V3,
                                   engineio::TransportType::POLLING);
    
    bool received = false;
    std::mutex mtx;
    std::condition_variable cv;
    
    client.on("pong", [&](const std::vector<Json::Value>& args) {
        if (args.size() > 0 && args[0]["received"]["test"].asString() == "hello_polling") {
            std::lock_guard<std::mutex> lock(mtx);
            received = true;
            cv.notify_all();
        }
    });
    
    client.connect(url);
    
    bool connected = client.wait_for_connect(10);
    report_result("V3 Polling event - connect", connected);
    if (connected) ctx.passed++; else ctx.failed++;
    
    if (connected) {
        Json::Value payload;
        payload["test"] = "hello_polling";
        client.emit("ping", {payload});
        
        std::unique_lock<std::mutex> lock(mtx);
        bool ok = cv.wait_for(lock, std::chrono::seconds(10), [&] { return received; });
        report_result("V3 Polling event - ping/pong", ok);
        if (ok) ctx.passed++; else ctx.failed++;
    }
    
    client.disconnect();
}

static void test_v3_polling_ack(TestContext& ctx, const std::string& url) {
    report_title("V3 Polling - ACK Test");
    
    PollingIntegrationTest client(sio::SocketIOVersion::V3,
                                   engineio::TransportType::POLLING);
    
    bool ack_received = false;
    std::mutex mtx;
    std::condition_variable cv;
    
    client.connect(url);
    
    bool connected = client.wait_for_connect(10);
    report_result("V3 Polling ACK - connect", connected);
    if (connected) ctx.passed++; else ctx.failed++;
    
    if (connected) {
        client.emit_with_ack("echo",
            {Json::Value("polling_ack_test")},
            [&](const std::vector<Json::Value>& args) {
                if (args.size() > 0 && args[0]["echoed"].asString() == "polling_ack_test") {
                    std::lock_guard<std::mutex> lock(mtx);
                    ack_received = true;
                    cv.notify_all();
                }
            },
            [&](int timeout) {
                (void)timeout;
                cv.notify_all();
            },
            std::chrono::seconds(10)
        );
        
        std::unique_lock<std::mutex> lock(mtx);
        bool ok = cv.wait_for(lock, std::chrono::seconds(10), [&] { return ack_received; });
        report_result("V3 Polling ACK - echo response", ok);
        if (ok) ctx.passed++; else ctx.failed++;
    }
    
    client.disconnect();
}

static void test_v3_polling_https_connect(TestContext& ctx, const std::string& url) {
    report_title("V3 Polling - HTTPS Test");
    
    PollingIntegrationTest client(sio::SocketIOVersion::V3,
                                   engineio::TransportType::POLLING);
    client.connect(url);
    
    bool ok = client.wait_for_connect(10);
    report_result("V3 Polling HTTPS connect", ok);
    if (ok) ctx.passed++; else ctx.failed++;
    
    client.disconnect();
}

int main() {
    TestContext ctx;
    
    std::cout << "========================================" << std::endl;
    std::cout << "  HTTP Polling Integration Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    
    test_v2_polling_connect(ctx, "http://localhost:3002/socket.io/");
    test_v3_polling_connect(ctx, "http://localhost:3003/socket.io/");
    test_v3_polling_event(ctx, "http://localhost:3003/socket.io/");
    test_v3_polling_ack(ctx, "http://localhost:3003/socket.io/");
    test_v3_polling_https_connect(ctx, "https://localhost:3004/socket.io/");
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Test Summary" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Total: " << ctx.passed + ctx.failed << std::endl;
    std::cout << "  Passed: " << ctx.passed << std::endl;
    std::cout << "  Failed: " << ctx.failed << std::endl;
    std::cout << "========================================" << std::endl;
    
    return ctx.failed > 0 ? 1 : 0;
}
