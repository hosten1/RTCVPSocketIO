//
//  sio_packet_impl.hpp
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/19.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#ifndef sio_packet_impl_hpp
#define sio_packet_impl_hpp

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <queue>
#include <map>
#include <functional>
#include "json/json.h"
#include "rtc_base/buffer.h"
#include "sio_packet.h"
#include "sio_jsoncpp_binary_helper.hpp"

namespace sio {

// 发送队列管理类（使用lambda回调）
template <typename T>
class PacketSender {
public:
    PacketSender();
    ~PacketSender();
    
    // 准备要发送的数据数组（异步处理）
    void prepare_data_array_async(
        const std::vector<T>& data_array,
        PacketType type = PacketType::EVENT,
        int nsp = 0,
        int id = -1,
        std::function<void()> on_complete = nullptr);
    
    // 设置文本数据回调
    void set_text_callback(std::function<void(const std::string& text)> callback);
    
    // 设置二进制数据回调
    void set_binary_callback(std::function<void(const rtc::Buffer& binary)> callback);
    
    // 重置发送状态
    void reset();
    
private:
    struct SendState {
        std::queue<std::string> text_queue;
        std::queue<rtc::Buffer> binary_queue;
        bool expecting_binary;
        std::function<void(const std::string& text)> text_callback;
        std::function<void(const rtc::Buffer& binary)> binary_callback;
        std::function<void()> on_complete;
    };
    
    std::unique_ptr<SendState> state_;
    
    // 处理下一个待发送项
    void process_next_item();
};

// 接收组合器（使用lambda回调）
template <typename T>
class PacketReceiver {
public:
    PacketReceiver();
    ~PacketReceiver();
    
    // 设置接收完成回调
    void set_complete_callback(std::function<void(const std::vector<T>& data_array)> callback);
    
    // 接收文本部分
    bool receive_text(const std::string& text);
    
    // 接收二进制部分
    bool receive_binary(const rtc::Buffer& binary);
    
    // 重置接收状态
    void reset();
    
private:
    struct ReceiveState {
        std::string current_text;
        std::vector<rtc::Buffer> received_binaries;
        std::vector<rtc::Buffer> expected_binaries;
        bool expecting_binary;
        std::function<void(const std::vector<T>& data_array)> complete_callback;
    };
    
    std::unique_ptr<ReceiveState> state_;
    
    // 检查并触发完成回调
    void check_and_trigger_complete();
};

} // namespace sio

#endif /* sio_packet_impl_hpp */
