#include "core/engineio/engineio_client.h"
#include "core/engineio/engineio_polling_transport.h"
#include "core/engineio/engineio_websocket_transport.h"
#include "core/engineio/http_client.h"
#include "core/websocket/websocket_client.h"
#include "api/task_queue/default_task_queue_factory.h"

#include <memory>

namespace engineio {

struct EngineIOClient::Impl {
    Config config;
    std::shared_ptr<EngineTransport> transport;
    std::shared_ptr<HttpClient> http_client;
    std::shared_ptr<ws::WebSocketClient> ws_client;
    
    std::unique_ptr<webrtc::TaskQueueFactory> owned_task_queue_factory;
    webrtc::TaskQueueFactory* task_queue_factory = nullptr;
    
    std::string sid;
    bool connected = false;
    
    OpenCallback on_open;
    MessageCallback on_message;
    CloseCallback on_close;
    ErrorCallback on_error;
};

std::shared_ptr<EngineIOClient> EngineIOClient::Create(const Config& config,
                                                        webrtc::TaskQueueFactory* task_queue_factory) {
    return std::shared_ptr<EngineIOClient>(new EngineIOClient(config, task_queue_factory));
}

EngineIOClient::EngineIOClient(const Config& config,
                               webrtc::TaskQueueFactory* task_queue_factory)
    : impl_(new Impl()) {
    impl_->config = config;
    if (!task_queue_factory) {
        impl_->owned_task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();
        impl_->task_queue_factory = impl_->owned_task_queue_factory.get();
    } else {
        impl_->task_queue_factory = task_queue_factory;
    }
}

EngineIOClient::~EngineIOClient() = default;

void EngineIOClient::set_open_callback(OpenCallback cb) {
    impl_->on_open = std::move(cb);
}

void EngineIOClient::set_message_callback(MessageCallback cb) {
    impl_->on_message = std::move(cb);
}

void EngineIOClient::set_close_callback(CloseCallback cb) {
    impl_->on_close = std::move(cb);
}

void EngineIOClient::set_error_callback(ErrorCallback cb) {
    impl_->on_error = std::move(cb);
}

bool EngineIOClient::is_connected() const {
    return impl_->connected;
}

std::string EngineIOClient::get_sid() const {
    return impl_->sid;
}

void EngineIOClient::set_transport(TransportType type) {
    impl_->config.transport = type;
}

void EngineIOClient::create_transport() {
    int version = static_cast<int>(impl_->config.version);
    
    if (impl_->config.transport == TransportType::POLLING) {
        if (!impl_->http_client) {
            impl_->http_client = HttpClient::Create();
        }
        auto polling = PollingTransport::Create(
            impl_->http_client, version, impl_->task_queue_factory);
        polling->set_self_signed_ssl(impl_->config.self_signed_ssl);
        impl_->transport = polling;
    } else {
        if (!impl_->ws_client) {
            impl_->ws_client = std::make_shared<ws::WebSocketClient>();
        }
        auto ws = WebSocketTransport::Create(impl_->ws_client, version);
        ws->set_self_signed_ssl(impl_->config.self_signed_ssl);
        impl_->transport = ws;
    }
    
    setup_transport_callbacks();
}

void EngineIOClient::setup_transport_callbacks() {
    if (!impl_->transport) return;
    
    auto self = shared_from_this();
    
    impl_->transport->set_open_callback(
        [self](const std::string& sid, int ping_interval, int ping_timeout) {
            self->on_transport_open(sid, ping_interval, ping_timeout);
        }
    );
    
    impl_->transport->set_message_callback(
        [self](const std::string& message) {
            self->on_transport_message(message);
        }
    );
    
    impl_->transport->set_close_callback(
        [self](const std::string& reason) {
            self->on_transport_close(reason);
        }
    );
    
    impl_->transport->set_error_callback(
        [self](const std::string& error) {
            self->on_transport_error(error);
        }
    );
}

void EngineIOClient::connect(const std::string& url) {
    if (impl_->connected) return;
    
    create_transport();
    
    if (!impl_->transport) {
        if (impl_->on_error) {
            impl_->on_error("failed to create transport");
        }
        return;
    }
    
    impl_->transport->connect(url);
}

void EngineIOClient::disconnect() {
    if (impl_->transport) {
        impl_->transport->disconnect();
    }
    impl_->connected = false;
}

void EngineIOClient::send(const std::string& message) {
    if (impl_->transport && impl_->connected) {
        impl_->transport->send(message);
    }
}

void EngineIOClient::on_transport_open(const std::string& sid,
                                        int /*ping_interval*/,
                                        int /*ping_timeout*/) {
    impl_->sid = sid;
    impl_->connected = true;
    
    if (impl_->on_open) {
        impl_->on_open();
    }
}

void EngineIOClient::on_transport_message(const std::string& message) {
    if (impl_->on_message) {
        impl_->on_message(message);
    }
}

void EngineIOClient::on_transport_close(const std::string& reason) {
    impl_->connected = false;
    
    if (impl_->on_close) {
        impl_->on_close(reason);
    }
}

void EngineIOClient::on_transport_error(const std::string& error) {
    if (impl_->on_error) {
        impl_->on_error(error);
    }
}

} // namespace engineio
