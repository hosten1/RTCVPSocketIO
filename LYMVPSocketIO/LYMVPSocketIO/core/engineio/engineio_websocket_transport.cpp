#include "core/engineio/engineio_websocket_transport.h"
#include "core/websocket/websocket_client.h"

#include "json/json.h"

#include <sstream>

namespace engineio {

struct WebSocketTransport::Impl {
    std::shared_ptr<ws::WebSocketClient> ws_client;
    int protocol_version;
    
    std::string sid;
    int ping_interval = 25000;
    int ping_timeout = 20000;
    
    bool connected = false;
    bool connecting = false;
    bool disconnecting = false;
};

std::shared_ptr<WebSocketTransport> WebSocketTransport::Create(
    std::shared_ptr<ws::WebSocketClient> ws_client,
    int protocol_version) {
    return std::shared_ptr<WebSocketTransport>(
        new WebSocketTransport(std::move(ws_client), protocol_version));
}

WebSocketTransport::WebSocketTransport(
    std::shared_ptr<ws::WebSocketClient> ws_client,
    int protocol_version)
    : impl_(new Impl()) {
    impl_->ws_client = std::move(ws_client);
    impl_->protocol_version = protocol_version;
}

WebSocketTransport::~WebSocketTransport() = default;

void WebSocketTransport::set_self_signed_ssl(bool enabled) {
    if (impl_->ws_client) {
        impl_->ws_client->setSelfSignedSSL(enabled);
    }
}

bool WebSocketTransport::is_connected() const {
    return impl_->connected;
}

void WebSocketTransport::connect(const std::string& url) {
    if (impl_->connecting || impl_->connected) return;
    
    impl_->connecting = true;
    
    auto self = shared_from_this();
    
    impl_->ws_client->setOnConnect([self]() {
        self->handle_ws_connect();
    });
    
    impl_->ws_client->setOnText([self](const std::string& text) {
        self->handle_ws_message(text);
    });
    
    impl_->ws_client->setOnDisconnect([self](const std::string& reason, ws::CloseCode code) {
        (void)code;
        self->handle_ws_disconnect();
    });
    
    impl_->ws_client->setOnError([self](const std::string& error) {
        self->handle_ws_error(error);
    });
    
    std::string ws_url = url;
    if (ws_url.find("http://") == 0) {
        ws_url.replace(0, 7, "ws://");
    } else if (ws_url.find("https://") == 0) {
        ws_url.replace(0, 8, "wss://");
    }
    
    std::string eio_version = (impl_->protocol_version >= 3) ? "4" : "3";
    std::string transport = "websocket";
    
    if (ws_url.find('?') == std::string::npos) {
        ws_url += "?EIO=" + eio_version + "&transport=" + transport;
    } else {
        ws_url += "&EIO=" + eio_version + "&transport=" + transport;
    }
    
    impl_->ws_client->setURL(ws_url);
    impl_->ws_client->connect();
}

void WebSocketTransport::disconnect() {
    if (impl_->disconnecting) return;
    
    impl_->disconnecting = true;
    
    if (impl_->connected) {
        std::string close_packet;
        close_packet += static_cast<char>(EnginePacketType::CLOSE);
        impl_->ws_client->sendText(close_packet);
    }
    
    impl_->ws_client->disconnect();
}

void WebSocketTransport::send(const std::string& message) {
    if (!impl_->connected) return;
    
    std::string packet;
    packet += static_cast<char>(EnginePacketType::MESSAGE);
    packet += message;
    
    impl_->ws_client->sendText(packet);
}

void WebSocketTransport::send_ping() {
    if (!impl_->connected) return;
    
    std::string packet;
    packet += static_cast<char>(EnginePacketType::PING);
    packet += "probe";
    
    impl_->ws_client->sendText(packet);
}

void WebSocketTransport::handle_ws_connect() {
}

void WebSocketTransport::handle_ws_message(const std::string& text) {
    handle_engine_packet(text);
}

void WebSocketTransport::handle_ws_disconnect() {
    impl_->connected = false;
    impl_->connecting = false;
    
    if (on_close_ && !impl_->disconnecting) {
        on_close_("websocket disconnect");
    } else if (on_close_ && impl_->disconnecting) {
        on_close_("client disconnect");
    }
    
    impl_->disconnecting = false;
}

void WebSocketTransport::handle_ws_error(const std::string& error) {
    if (on_error_) {
        on_error_(error);
    }
}

void WebSocketTransport::handle_engine_packet(const std::string& packet) {
    if (packet.empty()) return;
    
    char type_char = packet[0];
    std::string content = packet.substr(1);
    
    switch (type_char) {
        case static_cast<char>(EnginePacketType::OPEN):
            handle_open(content);
            break;
            
        case static_cast<char>(EnginePacketType::CLOSE):
            impl_->connected = false;
            impl_->connecting = false;
            if (on_close_) {
                on_close_("server close");
            }
            break;
            
        case static_cast<char>(EnginePacketType::PING): {
            std::string pong_packet;
            pong_packet += static_cast<char>(EnginePacketType::PONG);
            pong_packet += content;
            impl_->ws_client->sendText(pong_packet);
            break;
        }
            
        case static_cast<char>(EnginePacketType::PONG):
            if (on_pong_) {
                on_pong_();
            }
            break;
            
        case static_cast<char>(EnginePacketType::MESSAGE):
            handle_message(content);
            break;
            
        default:
            break;
    }
}

void WebSocketTransport::handle_open(const std::string& data) {
    Json::Value json;
    Json::CharReaderBuilder builder;
    std::string errors;
    std::istringstream iss(data);
    
    if (!Json::parseFromStream(builder, iss, &json, &errors)) {
        if (on_error_) {
            on_error_("failed to parse open packet");
        }
        return;
    }
    
    if (json.isMember("sid")) {
        impl_->sid = json["sid"].asString();
    }
    if (json.isMember("pingInterval")) {
        impl_->ping_interval = json["pingInterval"].asInt();
    }
    if (json.isMember("pingTimeout")) {
        impl_->ping_timeout = json["pingTimeout"].asInt();
    }
    
    impl_->connecting = false;
    impl_->connected = true;
    
    if (on_open_) {
        on_open_(impl_->sid, impl_->ping_interval, impl_->ping_timeout);
    }
}

void WebSocketTransport::handle_message(const std::string& data) {
    if (on_message_) {
        on_message_(data);
    }
}

} // namespace engineio
