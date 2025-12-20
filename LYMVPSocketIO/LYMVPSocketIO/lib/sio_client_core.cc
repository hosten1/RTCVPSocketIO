//
//  sio_client_core.cc
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/20.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#include "sio_client_core.h"
#include <chrono>
#include "rtc_base/logging.h"

namespace sio {

ClientCore::ClientCore(Version version)
    : status_(Status::kNotConnected),
      nsp_("/"),
      reconnects_(true),
      reconnect_attempts_(-1),
      reconnect_wait_(10),
      current_reconnect_attempt_(0),
      reconnecting_(false),
      current_ack_id_(-1),
      timeout_interval_ms_(1000),
      version_(version) {
    
    InitializeTaskQueue();
}

ClientCore::~ClientCore() {
    Disconnect();
    StopRepeatingTask();
    task_queue_.reset();
    task_queue_factory_.reset();
}

void ClientCore::InitializeTaskQueue() {
    task_queue_factory_ = webrtc::CreateDefaultTaskQueueFactory();
    task_queue_ = absl::make_unique<rtc::TaskQueue>(
        task_queue_factory_->CreateTaskQueue(
            "SocketIOClientQueue", webrtc::TaskQueueFactory::Priority::NORMAL));
}

void ClientCore::StartRepeatingTask(uint64_t interval_ms) {
    timeout_interval_ms_ = interval_ms;
    
    // 使用 libwebrtc 的 RepeatingTaskHandle 启动重复任务
    repeating_task_handle_ = webrtc::RepeatingTaskHandle::Start(
        task_queue_->Get(),
        [this]() {
            auto startTime = std::chrono::steady_clock::now();
            
            // 执行超时检查
            HandleTimeoutCheck();
            
            auto endTime = std::chrono::steady_clock::now();
            auto diffTime = std::chrono::duration_cast<std::chrono::microseconds>(
                endTime - startTime).count();
            
            uint64_t diffTimer = static_cast<uint64_t>(diffTime);
            
            // 计算下一次执行的延迟时间，确保总间隔为 interval_ms
            return webrtc::TimeDelta::ms(
                diffTimer < timeout_interval_ms_ ? 
                (timeout_interval_ms_ - diffTimer) : timeout_interval_ms_);
        });
}

void ClientCore::StopRepeatingTask() {
    if (repeating_task_handle_.Running()) {
        // 必须在任务队列中停止重复任务
        task_queue_->PostTask([this]() {
            if (repeating_task_handle_.Running()) {
                repeating_task_handle_.Stop();
            }
        });
    }
}

void ClientCore::HandleTimeoutCheck() {
    // 这里实现超时检查逻辑
    // 遍历所有未处理的 ACK 请求，检查是否超时
    auto now = std::chrono::steady_clock::now();
    int64_t current_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    // 检查 ACK 超时
    std::vector<int64_t> timed_out_acks;
    for (const auto& pair : ack_timeouts_) {
        int64_t ack_id = pair.first;
        int64_t timeout_time = pair.second;
        
        if (current_time_ms >= timeout_time) {
            timed_out_acks.push_back(ack_id);
        }
    }
    
    // 处理超时的 ACK
    for (int64_t ack_id : timed_out_acks) {
        auto it = ack_handlers_.find(ack_id);
        if (it != ack_handlers_.end()) {
            // 调用回调函数，标记为超时
            it->second({}, true);
            ack_handlers_.erase(it);
        }
        ack_timeouts_.erase(ack_id);
    }
}

void ClientCore::StartTimeoutTimer(uint64_t interval_ms) {
    StartRepeatingTask(interval_ms);
}

void ClientCore::StopTimeoutTimer() {
    StopRepeatingTask();
}

void ClientCore::Connect(const std::string& url, const std::map<std::string, std::string>& options) {
    if (status_ != Status::kConnected) {
        url_ = url;
        SetStatus(Status::kConnecting);
        
        // 这里简化实现，实际应该连接到服务器
        RTC_LOG(LS_INFO) << "Connecting to " << url;
        
        // 启动超时计时器
        StartTimeoutTimer(1000);
    }
}

void ClientCore::Disconnect() {
    RTC_LOG(LS_INFO) << "Disconnecting...";
    SetStatus(Status::kDisconnected);
    
    // 停止超时计时器
    StopTimeoutTimer();
    
    // 清理所有 ACK 处理器
    ack_handlers_.clear();
    ack_timeouts_.clear();
}

void ClientCore::Reconnect() {
    if (!reconnecting_) {
        RTC_LOG(LS_INFO) << "Reconnecting...";
        Connect(url_);
    }
}

void ClientCore::SetStatus(Status status) {
    if (status_ != status) {
        status_ = status;
        
        // 触发状态变化信号
        StatusChanged(status);
        
        // 触发事件
        std::string status_str;
        switch (status) {
            case Status::kNotConnected: status_str = "notconnected"; break;
            case Status::kDisconnected: status_str = "disconnected"; break;
            case Status::kConnecting: status_str = "connecting"; break;
            case Status::kOpened: status_str = "opened"; break;
            case Status::kConnected: status_str = "connected"; break;
        }
        
        EventReceived("statusChange", {Json::Value(status_str)});
    }
}

void ClientCore::Emit(const std::string& event, const std::vector<Json::Value>& items) {
    EmitWithAck(event, items, nullptr);
}

void ClientCore::EmitWithAck(const std::string& event, 
                            const std::vector<Json::Value>& items,
                            std::function<void(const std::vector<Json::Value>&, bool)> ack_callback,
                            double timeout) {
    if (status_ != Status::kConnected) {
        RTC_LOG(LS_WARNING) << "Cannot emit event, client not connected";
        return;
    }
    
    int64_t ack_id = -1;
    if (ack_callback) {
        ack_id = GenerateNextAckId();
        ack_handlers_[ack_id] = ack_callback;
        
        // 设置超时时间
        auto now = std::chrono::steady_clock::now();
        int64_t timeout_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count() + static_cast<int64_t>(timeout * 1000);
        ack_timeouts_[ack_id] = timeout_time;
    }
    
    // 构建并发送事件包
    // 将 std::vector<Json::Value> 转换为 Json::Value
    Json::Value data;
    if (items.size() == 1) {
        // 如果只有一个元素，直接使用该元素
        data = items[0];
    } else if (items.size() > 1) {
        // 如果有多个元素，创建一个数组
        data = Json::Value(Json::arrayValue);
        for (const auto& item : items) {
            data.append(item);
        }
    }
    // 否则，使用空的 Json::Value
    
    std::string packet = PacketUtils::build_event_packet(
        event, data, ack_id, nsp_, false);
    
    RTC_LOG(LS_INFO) << "Emitting event: " << event << ", packet: " << packet;
    
    // 这里简化实现，实际应该发送到服务器
}

void ClientCore::HandleAck(int64_t ack_id, const std::vector<Json::Value>& data) {
    auto it = ack_handlers_.find(ack_id);
    if (it != ack_handlers_.end()) {
        // 调用回调函数，标记为成功
        it->second(data, false);
        ack_handlers_.erase(it);
        ack_timeouts_.erase(ack_id);
    }
}

int64_t ClientCore::GenerateNextAckId() {
    current_ack_id_ += 1;
    if (current_ack_id_ >= 1000) {
        current_ack_id_ = 0;
    }
    return current_ack_id_;
}

void ClientCore::OnAny(std::function<void(const std::string&, const std::vector<Json::Value>&)> callback) {
    any_handler_ = callback;
}

void ClientCore::Off(const std::string& event) {
    event_handlers_.erase(event);
}

void ClientCore::RemoveAllHandlers() {
    event_handlers_.clear();
    any_handler_ = nullptr;
}

void ClientCore::LeaveNamespace() {
    if (nsp_ != "/") {
        // 发送离开命名空间的消息
        std::string packet = PacketUtils::build_disconnect_packet(nsp_);
        RTC_LOG(LS_INFO) << "Leaving namespace: " << nsp_ << ", packet: " << packet;
        
        nsp_ = "/";
    }
}

void ClientCore::JoinNamespace(const std::string& nsp) {
    if (!nsp.empty() && nsp != nsp_) {
        nsp_ = nsp;
        
        // 发送加入命名空间的消息
        std::string packet = PacketUtils::build_connect_packet(Json::Value(), nsp_);
        RTC_LOG(LS_INFO) << "Joining namespace: " << nsp_ << ", packet: " << packet;
    }
}

} // namespace sio
