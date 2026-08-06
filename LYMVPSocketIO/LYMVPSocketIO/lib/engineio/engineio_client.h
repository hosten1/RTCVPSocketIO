#ifndef ENGINEIO_CLIENT_H
#define ENGINEIO_CLIENT_H

#include "lib/engineio/engineio_transport.h"
#include "api/task_queue/task_queue_factory.h"

#include <memory>
#include <string>
#include <functional>

namespace engineio {

enum class EngineIOVersion {
    V2 = 2,
    V3 = 3,
    V4 = 4
};

class EngineIOClient : public std::enable_shared_from_this<EngineIOClient> {
public:
    using OpenCallback = std::function<void()>;
    using MessageCallback = std::function<void(const std::string& message)>;
    using CloseCallback = std::function<void(const std::string& reason)>;
    using ErrorCallback = std::function<void(const std::string& error)>;
    
    struct Config {
        EngineIOVersion version;
        TransportType transport;
        bool self_signed_ssl;
        
        Config() : version(EngineIOVersion::V4),
                   transport(TransportType::POLLING),
                   self_signed_ssl(false) {}
    };
    
    static std::shared_ptr<EngineIOClient> Create(const Config& config,
                                                   webrtc::TaskQueueFactory* task_queue_factory = nullptr);
    
    ~EngineIOClient();
    
    void connect(const std::string& url);
    void disconnect();
    
    void send(const std::string& message);
    
    bool is_connected() const;
    std::string get_sid() const;
    
    void set_open_callback(OpenCallback cb);
    void set_message_callback(MessageCallback cb);
    void set_close_callback(CloseCallback cb);
    void set_error_callback(ErrorCallback cb);
    
    void set_transport(TransportType type);
    
private:
    EngineIOClient(const Config& config,
                   webrtc::TaskQueueFactory* task_queue_factory);
    
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    void create_transport();
    void setup_transport_callbacks();
    
    void on_transport_open(const std::string& sid,
                           int ping_interval,
                           int ping_timeout);
    void on_transport_message(const std::string& message);
    void on_transport_close(const std::string& reason);
    void on_transport_error(const std::string& error);
};

} // namespace engineio

#endif // ENGINEIO_CLIENT_H
