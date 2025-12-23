//
//  SioAckManager.hpp
//  LYMVPSocketIO
//
//  Created by luoyongmeng on 2025/12/23.
//

#ifndef LIB_SIO_ACK_MANAGER_H
#define LIB_SIO_ACK_MANAGER_H

#include <functional>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <vector>
#include <condition_variable>
#include "json/json.h"
#include "rtc_base/task_queue.h"
#include "api/task_queue/task_queue_factory.h"
#include "absl/memory/memory.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "sio_ack_manager_interface.h"

namespace sio {

// ACK 管理器类（继承自IAckManager）
class SioAckManager : public IAckManager, public std::enable_shared_from_this<SioAckManager> {
public:
    static std::shared_ptr<SioAckManager> Create(webrtc::TaskQueueFactory* task_queue_factory = nullptr);
    
    ~SioAckManager();
    
    // IAckManager 接口实现
    int generate_ack_id() override;
    bool register_ack_callback(int ack_id,
                              AckCallback callback,
                              std::chrono::milliseconds timeout = std::chrono::milliseconds(5000),
                              AckTimeoutCallback timeout_callback = nullptr) override;
    bool handle_ack_response(int ack_id, const std::vector<Json::Value>& data_array) override;
    bool cancel_ack(int ack_id) override;
    void clear_all_acks() override;
    void set_default_timeout(std::chrono::milliseconds timeout) override;
    Stats get_stats() const override;
    
    // 获取任务队列
    std::shared_ptr<rtc::TaskQueue> get_task_queue() { return task_queue_; }
    
    // 停止管理器
    void stop();
    
    // 检查是否运行中
    bool is_running() const { return running_; }
    
private:
    struct AckInfo {
        AckCallback callback;
        AckTimeoutCallback timeout_callback;
        std::chrono::milliseconds timeout;
        std::chrono::steady_clock::time_point create_time;
        std::chrono::steady_clock::time_point expiry_time;
        bool processed;
        
        AckInfo() : callback(nullptr), timeout_callback(nullptr),
                   timeout(5000), processed(false) {}
        
        bool is_expired() const {
            return std::chrono::steady_clock::now() > expiry_time;
        }
    };
    
    SioAckManager(webrtc::TaskQueueFactory* task_queue_factory);
    
    void initialize();
    void start_timeout_checker();
    void stop_timeout_checker();
    void check_timeouts();
    
    std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory_;
    std::shared_ptr<rtc::TaskQueue> task_queue_;
    webrtc::RepeatingTaskHandle timeout_checker_handle_;
    
    mutable std::mutex mutex_;
    std::atomic<int> next_ack_id_;
    std::unordered_map<int, AckInfo> pending_acks_;
    std::chrono::milliseconds default_timeout_;
    
    // 统计信息
    mutable std::mutex stats_mutex_;
    int total_requests_;
    int timeout_requests_;
    int success_requests_;
    std::chrono::milliseconds total_response_time_;
    
    std::atomic<bool> running_;
    
    // 禁止拷贝
    SioAckManager(const SioAckManager&) = delete;
    SioAckManager& operator=(const SioAckManager&) = delete;
};

} // namespace sio

#endif // LIB_SIO_ACK_MANAGER_H
