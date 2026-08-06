#ifndef ENGINEIO_WEBSOCKET_TRANSPORT_H
#define ENGINEIO_WEBSOCKET_TRANSPORT_H

#include "lib/engineio/engineio_transport.h"

#include <memory>
#include <string>

namespace ws {
class WebSocketClient;
}

namespace engineio {

class WebSocketTransport : public EngineTransport,
                           public std::enable_shared_from_this<WebSocketTransport> {
public:
    static std::shared_ptr<WebSocketTransport> Create(
        std::shared_ptr<ws::WebSocketClient> ws_client,
        int protocol_version);
    
    ~WebSocketTransport() override;
    
    TransportType type() const override { return TransportType::WEBSOCKET; }
    
    void connect(const std::string& url) override;
    void disconnect() override;
    
    void send(const std::string& message) override;
    void send_ping() override;
    
    bool is_connected() const override;
    
    void set_self_signed_ssl(bool enabled);
    
private:
    WebSocketTransport(std::shared_ptr<ws::WebSocketClient> ws_client,
                       int protocol_version);
    
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    void handle_ws_connect();
    void handle_ws_message(const std::string& message);
    void handle_ws_disconnect();
    void handle_ws_error(const std::string& error);
    
    void handle_engine_packet(const std::string& packet);
    void handle_open(const std::string& data);
    void handle_message(const std::string& data);
};

} // namespace engineio

#endif // ENGINEIO_WEBSOCKET_TRANSPORT_H
