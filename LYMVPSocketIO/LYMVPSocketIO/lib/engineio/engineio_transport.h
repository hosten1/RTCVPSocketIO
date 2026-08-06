#ifndef ENGINEIO_TRANSPORT_H
#define ENGINEIO_TRANSPORT_H

#include <string>
#include <functional>
#include <vector>
#include <memory>

namespace engineio {

enum class EnginePacketType {
    OPEN = '0',
    CLOSE = '1',
    PING = '2',
    PONG = '3',
    MESSAGE = '4',
    UPGRADE = '5',
    NOOP = '6'
};

enum class TransportType {
    POLLING,
    WEBSOCKET
};

class EngineTransport {
public:
    using OpenCallback = std::function<void(const std::string& sid,
                                             int ping_interval,
                                             int ping_timeout)>;
    using MessageCallback = std::function<void(const std::string& message)>;
    using CloseCallback = std::function<void(const std::string& reason)>;
    using ErrorCallback = std::function<void(const std::string& error)>;
    using PongCallback = std::function<void()>;
    
    virtual ~EngineTransport() = default;
    
    virtual TransportType type() const = 0;
    
    virtual void connect(const std::string& url) = 0;
    virtual void disconnect() = 0;
    
    virtual void send(const std::string& message) = 0;
    virtual void send_ping() = 0;
    
    virtual bool is_connected() const = 0;
    
    void set_open_callback(OpenCallback cb) { on_open_ = std::move(cb); }
    void set_message_callback(MessageCallback cb) { on_message_ = std::move(cb); }
    void set_close_callback(CloseCallback cb) { on_close_ = std::move(cb); }
    void set_error_callback(ErrorCallback cb) { on_error_ = std::move(cb); }
    void set_pong_callback(PongCallback cb) { on_pong_ = std::move(cb); }
    
protected:
    OpenCallback on_open_;
    MessageCallback on_message_;
    CloseCallback on_close_;
    ErrorCallback on_error_;
    PongCallback on_pong_;
};

} // namespace engineio

#endif // ENGINEIO_TRANSPORT_H
