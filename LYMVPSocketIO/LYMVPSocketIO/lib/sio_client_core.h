//
//  sio_client_core.h
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/20.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#ifndef SIO_CLIENT_CORE_H
#define SIO_CLIENT_CORE_H

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <functional>
#include <map>
#include <queue>
#include "api/task_queue/default_task_queue_factory.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "sio_packet.h"
#include "sio_packet_types.h"

namespace sio {

// SocketIO 客户端核心类
class ClientCore {
public:
    // 客户端状态枚举
    enum class Status {
        kNotConnected,
        kDisconnected,
        kConnecting,
        kOpened,
        kConnected
    };

    // 事件类型定义
    enum class EventType {
        kConnect,
        kDisconnect,
        kError,
        kReconnect,
        kReconnectAttempt,
        kStatusChange
    };

    // 构造函数和析构函数
    ClientCore();
    ~ClientCore();

    // 连接管理
    void Connect(const std::string& url, const std::map<std::string, std::string>& options = {});
    void Disconnect();
    void Reconnect();
    Status GetStatus() const { return status_; }

    // 事件发送
    void Emit(const std::string& event, const std::vector<Json::Value>& items = {});
    void EmitWithAck(const std::string& event, 
                    const std::vector<Json::Value>& items = {},
                    std::function<void(const std::vector<Json::Value>&, bool)> ack_callback = nullptr,
                    double timeout = 10.0);

    // 事件监听
    template<typename... Args>
    void On(const std::string& event, std::function<void(Args...)> callback);

    void OnAny(std::function<void(const std::string&, const std::vector<Json::Value>&)> callback);
    void Off(const std::string& event);
    void RemoveAllHandlers();

    // 命名空间管理
    void LeaveNamespace();
    void JoinNamespace(const std::string& nsp);

    // ACK 管理
    void HandleAck(int64_t ack_id, const std::vector<Json::Value>& data);

    // 状态设置
    void SetStatus(Status status);

    // 超时计时器管理
    void StartTimeoutTimer(uint64_t interval_ms);
    void StopTimeoutTimer();

    // 事件信号（用于与 Objective-C++ 层通信）
    sigslot::signal<Status> StatusChanged;
    sigslot::signal<const std::string&, const std::vector<Json::Value>&> EventReceived;

private:
    // 私有方法
    void InitializeTaskQueue();
    void StartRepeatingTask(uint64_t interval_ms);
    void StopRepeatingTask();
    void HandleTimeoutCheck();
    int64_t GenerateNextAckId();

    // 私有成员变量
    Status status_;
    std::string url_;
    std::string nsp_;
    bool reconnects_;
    int64_t reconnect_attempts_;
    int64_t reconnect_wait_;
    int64_t current_reconnect_attempt_;
    bool reconnecting_;

    // ACK 管理
    int64_t current_ack_id_;
    std::map<int64_t, std::function<void(const std::vector<Json::Value>&, bool)>> ack_handlers_;
    std::map<int64_t, int64_t> ack_timeouts_;

    // 事件处理
    std::map<std::string, std::vector<std::function<void(const std::vector<Json::Value>&)>>> event_handlers_;
    std::function<void(const std::string&, const std::vector<Json::Value>&)> any_handler_;

    // 任务队列和计时器
    std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory_;
    std::unique_ptr<rtc::TaskQueue> task_queue_;
    webrtc::RepeatingTaskHandle repeating_task_handle_;
    uint64_t timeout_interval_ms_;
};

// 模板方法实现
template<typename... Args>
void ClientCore::On(const std::string& event, std::function<void(Args...)> callback) {
    // 这里简化实现，实际应该根据 Args 类型进行类型转换
    event_handlers_[event].push_back([callback](const std::vector<Json::Value>& data) {
        // 简化处理，实际应该根据 Args 类型从 data 中提取参数
        callback();
    });
}

} // namespace sio

#endif // SIO_CLIENT_CORE_H
