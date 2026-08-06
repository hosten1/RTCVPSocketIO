#ifndef ENGINEIO_POLLING_TRANSPORT_H
#define ENGINEIO_POLLING_TRANSPORT_H

#include "core/engineio/engineio_transport.h"
#include "core/engineio/http_client.h"
#include "api/task_queue/task_queue_factory.h"
#include "rtc_base/task_queue.h"

#include <string>
#include <memory>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace engineio {

class PollingTransport : public EngineTransport,
                         public std::enable_shared_from_this<PollingTransport> {
public:
    static std::shared_ptr<PollingTransport> Create(
        std::shared_ptr<HttpClient> http_client,
        int protocol_version,
        webrtc::TaskQueueFactory* task_queue_factory = nullptr);
    
    ~PollingTransport() override;
    
    TransportType type() const override { return TransportType::POLLING; }
    
    void connect(const std::string& url) override;
    void disconnect() override;
    
    void send(const std::string& message) override;
    void send_ping() override;
    
    bool is_connected() const override;
    
    void set_self_signed_ssl(bool enabled);
    
private:
    PollingTransport(std::shared_ptr<HttpClient> http_client,
                     int protocol_version,
                     webrtc::TaskQueueFactory* task_queue_factory);
    
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    void poll_thread_func();
    void do_handshake();
    void do_poll();
    void do_post(const std::string& payload);
    
    void handle_engine_packet(const std::string& packet);
    void handle_open(const std::string& data);
    void handle_message(const std::string& data);
    
    std::string build_url(bool with_sid, const std::string& transport);
    std::string generate_t_param();
    
    std::string encode_packets(const std::vector<std::string>& packets);
    std::vector<std::string> decode_packets(const std::string& data);
};

} // namespace engineio

#endif // ENGINEIO_POLLING_TRANSPORT_H
