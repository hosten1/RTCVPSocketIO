#ifndef SIO_CLIENT_H
#define SIO_CLIENT_H

#include <memory>
#include <string>
#include <vector>
#include <initializer_list>
#include <functional>
#include <chrono>
#include "json/json.h"
#include "lib/sio_packet_types.h"
#include "lib/sio_smart_buffer.hpp"
#include "api/task_queue/task_queue_factory.h"

namespace sio {

class PacketSender;
class PacketReceiver;
class SioAckManager;

using AckCallback = std::function<void(const std::vector<Json::Value>&)>;
using AckTimeoutCallback = std::function<void(int ack_id)>;
using AckResponder = std::function<void(const std::vector<Json::Value>& args)>;
using EventHandler = std::function<void(const std::vector<Json::Value>& args, AckResponder ack)>;
using TextSendCallback = std::function<bool(const std::string& text_packet, const std::vector<SmartBuffer> binary_data)>;

class SioClient {
public:
    struct Config {
        SocketIOVersion version;
        std::chrono::milliseconds default_ack_timeout;
        int max_retries;
        bool enable_logging;
        
        Config() : version(SocketIOVersion::V4),
                   default_ack_timeout(5000),
                   max_retries(3),
                   enable_logging(false) {}
    };
    
    static std::shared_ptr<SioClient> Create(const Config& config = Config());
    
    ~SioClient();
    
    void set_version(SocketIOVersion version);
    SocketIOVersion get_version() const;
    
    void set_send_callback(TextSendCallback callback);
    
    void on(const std::string& event_name, EventHandler handler);
    void off(const std::string& event_name);
    void remove_all_listeners();
    
    void emit(const std::string& event_name,
              std::initializer_list<Json::Value> args,
              const std::string& namespace_s = "/");
    
    void emit(const std::string& event_name,
              std::initializer_list<Json::Value> args,
              AckCallback ack_callback,
              const std::string& namespace_s = "/");
    
    void emit(const std::string& event_name,
              std::initializer_list<Json::Value> args,
              AckCallback ack_callback,
              AckTimeoutCallback timeout_callback,
              std::chrono::milliseconds timeout,
              const std::string& namespace_s = "/");
    
    void emit(const std::string& event_name,
              const std::vector<Json::Value>& args,
              const std::string& namespace_s = "/");
    
    void emit(const std::string& event_name,
              const std::vector<Json::Value>& args,
              AckCallback ack_callback,
              const std::string& namespace_s = "/");
    
    void emit(const std::string& event_name,
              const std::vector<Json::Value>& args,
              AckCallback ack_callback,
              AckTimeoutCallback timeout_callback,
              std::chrono::milliseconds timeout,
              const std::string& namespace_s = "/");
    
    bool process_text_packet(const std::string& text_packet);
    bool process_binary_data(const SmartBuffer& binary_data);
    
    void reset();
    
    struct Stats {
        int total_sent;
        int total_received;
        int total_acked;
        int pending_acks;
        
        Stats() : total_sent(0), total_received(0),
                  total_acked(0), pending_acks(0) {}
    };
    Stats get_stats() const;
    
    std::shared_ptr<PacketSender> sender() { return sender_; }
    std::shared_ptr<PacketReceiver> receiver() { return receiver_; }
    
private:
    SioClient(const Config& config);
    void initialize();
    
    std::shared_ptr<SioAckManager> ack_manager_;
    std::shared_ptr<PacketSender> sender_;
    std::shared_ptr<PacketReceiver> receiver_;
    std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory_;
    
    Config config_;
    
    SioClient(const SioClient&) = delete;
    SioClient& operator=(const SioClient&) = delete;
};

} // namespace sio

#endif // SIO_CLIENT_H
