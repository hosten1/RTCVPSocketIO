#ifndef sio_packet_impl_hpp
#define sio_packet_impl_hpp

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <queue>
#include <map>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <sstream>
#include "json/json.h"
#include "rtc_base/buffer.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread.h"
#include "lib/sio_smart_buffer.hpp"
#include "rtc_base/task_queue.h"
#include "api/task_queue/task_queue_factory.h"
#include "absl/memory/memory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "lib/sio_ack_manager_interface.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "lib/sio_packet_builder.h"


// 前向声明
namespace sio {
class SioAckManager;
}

namespace sio {

// 回调函数类型定义
using EventCallback = std::function<void(const SioPacket &packet)>;
using SendResultCallback = std::function<void(bool success, const std::string& error)>;
using TextSendCallback = std::function<bool(const std::string& text_packet, const std::vector<SmartBuffer> binary_data)>;


// 包发送器
class PacketSender : public std::enable_shared_from_this<PacketSender> {
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
    
    PacketSender(std::shared_ptr<IAckManager> ack_manager,
                webrtc::TaskQueueFactory* task_queue_factory = nullptr,
                const Config& config = Config());
    
    ~PacketSender();
    
    // 配置方法
    void set_config(const Config& config);
    Config get_config() const { return config_; }
    
    // 设置ACK管理器
    // void set_ack_manager(std::shared_ptr<IAckManager> ack_manager);
    std::shared_ptr<IAckManager> get_ack_manager() { return ack_manager_; }
    
    // 发送事件（同步）
    bool send_event(const std::string& event_name,
                   const std::vector<Json::Value>& args,
                   TextSendCallback text_callback,
                   SendResultCallback complete_callback = nullptr,
                   const std::string& namespace_s = "/");
    
    // 发送事件（异步，支持ACK）
    int send_event_with_ack(
        const std::string& event_name,
        const std::vector<Json::Value>& args,
        TextSendCallback text_callback,
        AckCallback ack_callback = nullptr,
        AckTimeoutCallback timeout_callback = nullptr,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0),
        const std::string& namespace_s = "/");
    
    // 发送ACK响应
    bool send_ack_response(
        int ack_id,
        const std::vector<Json::Value>& args,
        TextSendCallback text_callback,
        const std::string& namespace_s = "/");
    
    // 重置发送器
    void reset();
    
    // 获取统计信息
    struct Stats {
        int total_sent;
        int total_acked;
        int total_failed;
        int total_timeout;
        
        Stats() : total_sent(0), total_acked(0),
                  total_failed(0), total_timeout(0) {}
    };
    Stats get_stats() const;
    
private:
    struct PendingRequest {
        int ack_id;
        std::chrono::steady_clock::time_point send_time;
        std::chrono::milliseconds timeout;
        bool waiting_for_ack;
        std::string event_name;
        
        PendingRequest() : ack_id(-1), timeout(0),
                          waiting_for_ack(false) {}
        
        bool is_expired() const {
            auto now = std::chrono::steady_clock::now();
            return (now - send_time) > timeout;
        }
    };
    
    void initialize_task_queue();
    void cleanup_expired_requests();
    void start_cleanup_timer();
    void stop_cleanup_timer();
    
    std::shared_ptr<IAckManager> ack_manager_;
    std::unique_ptr<SioPacketBuilder> packet_builder_;
    
    Config config_;
    std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory_;
    std::shared_ptr<rtc::TaskQueue> task_queue_;
    webrtc::RepeatingTaskHandle cleanup_handle_;
    
    mutable webrtc::Mutex stats_mutex_;
    Stats stats_;
    
    mutable webrtc::Mutex pending_mutex_;
    std::unordered_map<int, PendingRequest> pending_requests_;
    
    std::atomic<bool> initialized_;
    std::atomic<bool> running_;
    
    // 禁止拷贝
    PacketSender(const PacketSender&) = delete;
    PacketSender& operator=(const PacketSender&) = delete;
};

// 包接收器
class PacketReceiver {
public:
    struct Config {
        SocketIOVersion default_version;
        bool auto_detect_version;
        bool enable_logging;
        int max_binary_size;
        
        Config() : default_version(SocketIOVersion::V4),
                   auto_detect_version(true),
                   enable_logging(false),
                   max_binary_size(10 * 1024 * 1024) {} // 10MB
    };
    
    PacketReceiver(std::shared_ptr<IAckManager> ack_manager,
                  webrtc::TaskQueueFactory* task_queue_factory = nullptr,
                  const Config& config = Config());
    
    ~PacketReceiver();
    
    // 配置方法
    void set_config(const Config& config);
    Config get_config() const { return config_; }
    
    // 设置ACK管理器
    // void set_ack_manager(std::shared_ptr<IAckManager> ack_manager);
    std::shared_ptr<IAckManager> get_ack_manager() { return ack_manager_; }
    
    // 设置事件回调
    void set_event_callback(EventCallback callback);
    
    // 处理文本包
    bool process_text_packet(const std::string& text_packet);
    
    // 处理二进制数据
    bool process_binary_data(const SmartBuffer& binary_data);
    
    // 重置接收器状态
    void reset();
    
    // 获取接收状态
    bool is_waiting_for_binary() const;
    int get_expected_binary_count() const;
    int get_received_binary_count() const;
    
    // 获取统计信息
    struct Stats {
        int total_received;
        int text_packets;
        int binary_packets;
        int parse_errors;
        int ack_processed;
        
        Stats() : total_received(0), text_packets(0),
                  binary_packets(0), parse_errors(0),
                  ack_processed(0) {}
    };
    Stats get_stats() const;
    
private:
    struct ReceiveState {
        enum State {
            IDLE,
            WAITING_FOR_BINARY,
            COMPLETE
        };
        
        State state;
        SioPacket current_packet;
        std::string original_text_packet;  // 保存原始文本包
        std::vector<SmartBuffer> received_binaries;
        int expected_binary_count;
        SocketIOVersion packet_version;
        
        ReceiveState() : state(IDLE), expected_binary_count(0),
                         packet_version(SocketIOVersion::V4) {}
        
        void reset() {
            state = IDLE;
            current_packet = SioPacket();
            original_text_packet.clear();
            received_binaries.clear();
            expected_binary_count = 0;
            packet_version = SocketIOVersion::V4;
        }
        
        bool is_complete() const {
            if (state == COMPLETE) return true;
            if (state == WAITING_FOR_BINARY) {
                return static_cast<int>(received_binaries.size()) >= expected_binary_count;
            }
            return false;
        }
    };
    
    void initialize_task_queue();
    void process_complete_packet(const SioPacket& packet);
    void handle_ack_packet(const SioPacket& packet);
    
    std::shared_ptr<IAckManager> ack_manager_;
    std::unique_ptr<SioPacketBuilder> packet_builder_;
    EventCallback event_callback_;
    
    Config config_;
    std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory_;
    std::shared_ptr<rtc::TaskQueue> task_queue_;
    
    ReceiveState state_;
    
    mutable webrtc::Mutex stats_mutex_;
    Stats stats_;
    
    mutable webrtc::Mutex state_mutex_;
    
    // 禁止拷贝
    PacketReceiver(const PacketReceiver&) = delete;
    PacketReceiver& operator=(const PacketReceiver&) = delete;
};

} // namespace sio

#endif /* sio_packet_impl_hpp */
