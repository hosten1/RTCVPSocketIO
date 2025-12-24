//
//  SioAckManager.cpp
//  LYMVPSocketIO
//
//  Created by luoyongmeng on 2025/12/23.
//

#include "sio_ack_manager.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/time_utils.h"
#include <chrono>
#include <iostream>

namespace sio {

std::shared_ptr<SioAckManager> SioAckManager::Create(webrtc::TaskQueueFactory* task_queue_factory) {
    std::shared_ptr<SioAckManager> instance(new SioAckManager(task_queue_factory));
    instance->initialize();
    return instance;
}

SioAckManager::SioAckManager(webrtc::TaskQueueFactory* task_queue_factory)
    : task_queue_factory_(task_queue_factory ? nullptr : webrtc::CreateDefaultTaskQueueFactory()),
      next_ack_id_(1),
      default_timeout_(5000),
      total_requests_(0),
      timeout_requests_(0),
      success_requests_(0),
      total_response_time_(0),
      running_(false) {
    
    // 如果传入的factory为空，使用默认的
    if (!task_queue_factory) {
        task_queue_factory = task_queue_factory_.get();
    }
}

SioAckManager::~SioAckManager() {
    stop_timeout_checker();
    clear_all_acks();
}

void SioAckManager::initialize() {
    if (!task_queue_factory_) {
        task_queue_factory_ = webrtc::CreateDefaultTaskQueueFactory();
    }
    
    if (task_queue_factory_) {
        task_queue_ = std::shared_ptr<rtc::TaskQueue>(
            new rtc::TaskQueue(
                task_queue_factory_->CreateTaskQueue(
                    "sio_ack_manager",
                    webrtc::TaskQueueFactory::Priority::NORMAL)));
        
        start_timeout_checker();
    }
}

int SioAckManager::generate_ack_id() {
    return next_ack_id_++;
}

bool SioAckManager::register_ack_callback(int ack_id,
                                         AckCallback callback,
                                         std::chrono::milliseconds timeout,
                                         AckTimeoutCallback timeout_callback) {
    if (!callback || ack_id <= 0) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查是否已存在
    if (pending_acks_.find(ack_id) != pending_acks_.end()) {
        return false;
    }
    
    AckInfo info;
    info.callback = callback;
    info.timeout_callback = timeout_callback;
    info.timeout = timeout.count() > 0 ? timeout : default_timeout_;
    info.create_time = std::chrono::steady_clock::now();
    info.expiry_time = info.create_time + info.timeout;
    info.processed = false;
    
    pending_acks_[ack_id] = info;
    
    // 更新统计
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        total_requests_++;
    }
    
    return true;
}

bool SioAckManager::handle_ack_response(int ack_id, const std::vector<Json::Value>& data_array) {
    AckCallback callback;
    std::chrono::steady_clock::time_point create_time;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = pending_acks_.find(ack_id);
        if (it == pending_acks_.end()) {
            return false;
        }
        
        if (it->second.processed) {
            return false;
        }
        
        callback = it->second.callback;
        create_time = it->second.create_time;
        it->second.processed = true;
        
        // 更新统计
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            success_requests_++;
            auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - create_time);
            total_response_time_ += response_time;
        }
        
        pending_acks_.erase(it);
    }
    
    // 在任务队列中执行回调
    if (callback && task_queue_) {
        task_queue_->PostTask([callback, data_array]() {
            callback(data_array);
        });
    }
    
    return true;
}

bool SioAckManager::cancel_ack(int ack_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_acks_.erase(ack_id) > 0;
}

void SioAckManager::clear_all_acks() {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_acks_.clear();
}

void SioAckManager::set_default_timeout(std::chrono::milliseconds timeout) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_timeout_ = timeout;
}

SioAckManager::Stats SioAckManager::get_stats() const {
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    std::lock_guard<std::mutex> lock(mutex_);
    
    Stats stats;
    stats.total_requests = total_requests_;
    stats.pending_requests = static_cast<int>(pending_acks_.size());
    stats.timeout_requests = timeout_requests_;
    stats.success_requests = success_requests_;
    
    if (success_requests_ > 0) {
        stats.average_response_time = std::chrono::milliseconds(
            total_response_time_.count() / success_requests_);
    }
    
    return stats;
}

void SioAckManager::start_timeout_checker() {
    if (!task_queue_ || running_) {
        return;
    }
    
    running_ = true;
    
    // 使用lambda捕获weak_ptr避免循环引用
    std::weak_ptr<SioAckManager> weak_this = shared_from_this();
    
    timeout_checker_handle_ = webrtc::RepeatingTaskHandle::Start(
        task_queue_->Get(),
        [weak_this]() {
            auto self = weak_this.lock();
            if (!self || !self->running_) {
                return webrtc::TimeDelta::PlusInfinity();
            }
            
            self->check_timeouts();
            
            // 每500ms检查一次超时
            return webrtc::TimeDelta::ms(500);
        });
}

void SioAckManager::stop_timeout_checker() {
    // 检查当前是否已经在任务队列线程上
       if (task_queue_->IsCurrent()) {
           // 直接执行，不需要PostTask
           running_ = false;
           timeout_checker_handle_.Stop();
       } else {
           // 不在任务队列线程上，需要PostTask
           task_queue_->PostTask([this](){
               running_ = false;
               timeout_checker_handle_.Stop();
           });
       }
}

void SioAckManager::stop() {
    // 检查当前是否已经在任务队列线程上
       if (task_queue_->IsCurrent()) {
           // 直接执行，不需要PostTask
           stop_timeout_checker();
           clear_all_acks();
       } else {
           // 不在任务队列线程上，需要PostTask
           task_queue_->PostTask([this](){
               stop_timeout_checker();
               clear_all_acks();
           });
       }
    
}

void SioAckManager::check_timeouts() {
//    auto now = std::chrono::steady_clock::now();
    std::vector<int> expired_acks;
    std::vector<std::pair<int, AckTimeoutCallback>> timeout_callbacks;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& kv : pending_acks_) {
            int ack_id = kv.first;
            AckInfo& info = kv.second;
            
            if (!info.processed && info.is_expired()) {
                expired_acks.push_back(ack_id);
                
                if (info.timeout_callback) {
                    timeout_callbacks.emplace_back(ack_id, info.timeout_callback);
                }
                
                // 标记为已处理
                info.processed = true;
                
                // 更新统计
                {
                    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                    timeout_requests_++;
                }
            }
        }
        
        // 移除超时的ACK
        for (auto ack_id : expired_acks) {
            pending_acks_.erase(ack_id);
        }
    }
    
    // 执行超时回调
    if (task_queue_) {
        for (auto& callback_pair : timeout_callbacks) {
            int ack_id = callback_pair.first;
            AckTimeoutCallback callback = callback_pair.second;
            
            task_queue_->PostTask([callback, ack_id]() {
                if (callback) {
                    callback(ack_id);
                }
            });
        }
    }
}

} // namespace sio
