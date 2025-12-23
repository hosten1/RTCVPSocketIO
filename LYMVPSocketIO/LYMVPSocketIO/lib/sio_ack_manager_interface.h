//
//  sio_ack_manager_interface.hpp
//  LYMVPSocketIO
//
//  Created by luoyongmeng on 2025/12/23.
//

#ifndef SIO_ACK_MANAGER_INTERFACE_H
#define SIO_ACK_MANAGER_INTERFACE_H

#include <functional>
#include <memory>
#include <chrono>
#include <vector>
#include "json/json.h"

namespace sio {

// 回调函数类型定义
using AckCallback = std::function<void(const std::vector<Json::Value>&)>;
using AckTimeoutCallback = std::function<void(int ack_id)>;

// ACK 管理器接口
class IAckManager {
public:
    virtual ~IAckManager() = default;
    
    // 生成唯一的ACK ID
    virtual int generate_ack_id() = 0;
    
    // 注册ACK回调
    virtual bool register_ack_callback(int ack_id,
                                      AckCallback callback,
                                      std::chrono::milliseconds timeout = std::chrono::milliseconds(5000),
                                      AckTimeoutCallback timeout_callback = nullptr) = 0;
    
    // 处理ACK响应
    virtual bool handle_ack_response(int ack_id, const std::vector<Json::Value>& data_array) = 0;
    
    // 取消ACK
    virtual bool cancel_ack(int ack_id) = 0;
    
    // 清除所有未处理的ACK
    virtual void clear_all_acks() = 0;
    
    // 设置默认超时时间
    virtual void set_default_timeout(std::chrono::milliseconds timeout) = 0;
    
    // 获取统计信息
    struct Stats {
        int total_requests{0};
        int pending_requests{0};
        int timeout_requests{0};
        int success_requests{0};
        std::chrono::milliseconds average_response_time{0};
        
        Stats() : total_requests(0), pending_requests(0),
                  timeout_requests(0), success_requests(0),
                  average_response_time(0) {}
    };
    virtual Stats get_stats() const = 0;
};

} // namespace sio

#endif // SIO_ACK_MANAGER_INTERFACE_H
