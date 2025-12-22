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
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread.h"
#include "sio_packet.h"
#include "sio_jsoncpp_binary_helper.hpp"
#include "sio_smart_buffer.hpp"
#include "sio_packet_parser.h"  // 添加解析器头文件
#include "rtc_base/task_queue.h"
#include "api/task_queue/task_queue_factory.h"
#include "absl/memory/memory.h"

namespace sio {

// Socket.IO 协议格式的结果结构（异步使用）
struct SocketIOPacketResult {
    std::string text_packet;               // 符合Socket.IO协议的文本包（包含占位符）
    std::vector<SmartBuffer> binary_parts; // 二进制数据部分，按占位符顺序排列
    bool is_binary_packet;                 // 是否是二进制包
    int binary_count;                      // 二进制数据数量（用于包头的二进制计数）
    PacketType original_packet_type;       // 原始包类型（EVENT 或 ACK）
    PacketType actual_packet_type;         // 实际包类型（可能是 BINARY_EVENT 或 BINARY_ACK）
    int namespace_id;                      // 命名空间ID
    int packet_id;                         // 包ID
    SocketIOVersion version;               // Socket.IO 版本
    
    SocketIOPacketResult()
        : is_binary_packet(false),
          binary_count(0),
          original_packet_type(PacketType::EVENT),
          actual_packet_type(PacketType::EVENT),
          namespace_id(0),
          packet_id(-1),
          version(SocketIOVersion::V3) {}  // 默认使用 V3
    
    // 检查是否有效
    bool is_valid() const {
        return !text_packet.empty();
    }
    
    // 转换为字符串（调试用）
    std::string to_string() const {
        std::stringstream ss;
        ss << "SocketIOPacketResult {" << std::endl;
        ss << "  text_packet: " << text_packet.substr(0, 100);
        if (text_packet.length() > 100) ss << "...";
        ss << std::endl;
        ss << "  binary_parts: " << binary_parts.size() << " 个" << std::endl;
        ss << "  is_binary_packet: " << (is_binary_packet ? "true" : "false") << std::endl;
        ss << "  binary_count: " << binary_count << std::endl;
        ss << "  original_packet_type: " << static_cast<int>(original_packet_type) << std::endl;
        ss << "  actual_packet_type: " << static_cast<int>(actual_packet_type) << std::endl;
        ss << "  namespace_id: " << namespace_id << std::endl;
        ss << "  packet_id: " << packet_id << std::endl;
        ss << "  version: " << static_cast<int>(version) << std::endl;
        ss << "}";
        return ss.str();
    }
};

// ACK 回调函数类型
typedef std::function<void(const std::vector<Json::Value>&)> AckCallback;

// ACK 超时回调函数类型
typedef std::function<void()> AckTimeoutCallback;

// ACK 管理类，用于管理ACK和Callback
class AckManager {
public:
    AckManager(std::shared_ptr<rtc::TaskQueue> task_queue);
    ~AckManager();
    
    // 生成唯一的ACK ID
    int generate_ack_id();
    
    // 注册ACK回调
    void register_ack_callback(int ack_id, AckCallback callback, std::chrono::milliseconds timeout = std::chrono::milliseconds(30000));
    
    // 注册ACK超时回调
    void register_ack_timeout_callback(int ack_id, AckTimeoutCallback callback);
    
    // 处理ACK响应
    bool handle_ack_response(int ack_id, const std::vector<Json::Value>& data_array);
    
    // 取消ACK（用于超时或手动取消）
    bool cancel_ack(int ack_id);
    
    // 设置默认超时时间
    void set_default_timeout(std::chrono::milliseconds timeout);
    
    // 获取默认超时时间
    std::chrono::milliseconds get_default_timeout() const;
    
    // 清除所有未处理的ACK
    void clear_all_acks();
    
private:
    // ACK 信息结构体
    struct AckInfo {
        AckCallback callback;                          // ACK 回调函数
        AckTimeoutCallback timeout_callback;           // 超时回调函数
        std::chrono::milliseconds timeout;             // 超时时间
        std::chrono::steady_clock::time_point start_time; // 开始时间
        
        AckInfo()
            : callback(nullptr),
              timeout_callback(nullptr),
              timeout(std::chrono::milliseconds(30000)),
              start_time(std::chrono::steady_clock::now()) {}
    };
    
    std::shared_ptr<rtc::TaskQueue> task_queue_;
    mutable webrtc::Mutex mutex_;                      // 互斥锁，保护共享数据
    std::atomic<int> next_ack_id_;                     // 下一个可用的ACK ID
    std::map<int, std::unique_ptr<AckInfo>> acks_;     // ACK 信息映射表
    std::chrono::milliseconds default_timeout_;        // 默认超时时间
};

// Socket.IO 异步发送回调接口（支持多版本）
class SocketIOSender {
public:
    virtual ~SocketIOSender() = default;
    
    // 发送文本包
    virtual bool send_text(const std::string& text_packet) = 0;
    
    // 发送二进制数据
    virtual bool send_binary(const SmartBuffer& binary_data) = 0;
    
    // 发送完成回调
    virtual void on_send_complete(bool success, const std::string& error = "") = 0;
    
    // 获取支持的Socket.IO版本
    virtual SocketIOVersion get_supported_version() const { return SocketIOVersion::V3; }
};

// 发送队列管理类（支持多版本）
template <typename T>
class PacketSender {
public:
    PacketSender(webrtc::TaskQueueFactory* task_queue_factory, SocketIOVersion version = SocketIOVersion::V3);
    ~PacketSender();
    
    // 获取当前版本
    SocketIOVersion get_version() const { return version_; }
    
    // 设置版本
    void set_version(SocketIOVersion version) {
        version_ = version;
        update_parser_config();
    }
    
    // 准备要发送的数据数组（异步处理）
    void prepare_data_array_async(
        const std::vector<T>& data_array,
        PacketType type = PacketType::EVENT,
        int nsp = 0,
        int id = -1,
        std::function<void()> on_complete = nullptr);
    
    // 异步发送数据（符合Socket.IO协议：先文本后二进制）
    void send_data_array_async(
        const std::vector<T>& data_array,
        std::function<bool(const std::string& text_packet)> text_callback,
        std::function<bool(const SmartBuffer& binary_data, int index)> binary_callback = nullptr,
        std::function<void(bool success, const std::string& error)> complete_callback = nullptr,
        PacketType type = PacketType::EVENT,
        int nsp = 0,
        int id = -1);
    
    // 异步发送数据并注册ACK回调（主要方法）
    void send_data_array_with_ack_async(
        const std::vector<T>& data_array,
        std::function<bool(const std::string& text_packet)> text_callback,
        std::function<bool(const SmartBuffer& binary_data, int index)> binary_callback = nullptr,
        std::function<void(bool success, const std::string& error)> complete_callback = nullptr,
        std::function<void(const std::vector<T>& data_array)> ack_callback = nullptr,
        std::function<void()> ack_timeout_callback = nullptr,
        std::chrono::milliseconds ack_timeout = std::chrono::milliseconds(30000),
        PacketType type = PacketType::EVENT,
        int nsp = 0);
    
    // 使用 SocketIOSender 接口发送数据（更面向对象的方式）
    void send_data_array_async(
        const std::vector<T>& data_array,
        SocketIOSender* sender,
        PacketType type = PacketType::EVENT,
        int nsp = 0,
        int id = -1);
    
    // 为v2客户端准备发送数据（v2格式不同）
    void send_data_array_async_v2(
        const std::vector<T>& data_array,
        std::function<bool(const std::string& text_packet)> text_callback,
        std::function<bool(const SmartBuffer& binary_data, int index)> binary_callback = nullptr,
        std::function<void(bool success, const std::string& error)> complete_callback = nullptr,
        PacketType type = PacketType::EVENT,
        int nsp = 0,
        int id = -1);
    
    // 为v3+客户端准备发送数据
    void send_data_array_async_v3(
        const std::vector<T>& data_array,
        std::function<bool(const std::string& text_packet)> text_callback,
        std::function<bool(const SmartBuffer& binary_data, int index)> binary_callback = nullptr,
        std::function<void(bool success, const std::string& error)> complete_callback = nullptr,
        PacketType type = PacketType::EVENT,
        int nsp = 0,
        int id = -1);
    
    // 设置文本数据回调
    void set_text_callback(std::function<void(const std::string& text)> callback);
    
    // 设置二进制数据回调
    void set_binary_callback(std::function<void(const SmartBuffer& binary)> callback);
    
    // 重置发送状态
    void reset();
    
    // 获取ACK管理器
    AckManager& get_ack_manager() { return ack_manager_; }
    const AckManager& get_ack_manager() const { return ack_manager_; }
    
private:
    struct SendState {
        std::queue<std::string> text_queue;
        std::queue<SmartBuffer> binary_queue;
        bool expecting_binary;
        std::function<void(const std::string& text)> text_callback;
        std::function<void(const SmartBuffer& binary)> binary_callback;
        std::function<void()> on_complete;
    };
    
    webrtc::TaskQueueFactory* task_queue_factory_;
    std::shared_ptr<rtc::TaskQueue> task_queue_;
    SocketIOVersion version_;
    std::unique_ptr<SendState> state_;
    AckManager ack_manager_;  // ACK 管理器
    
    // 更新解析器配置
    void update_parser_config();
    
    // 处理下一个待发送项
    void process_next_item();
};

// 接收组合器（支持多版本）
template <typename T>
class PacketReceiver {
public:
    PacketReceiver(webrtc::TaskQueueFactory* task_queue_factory, SocketIOVersion version = SocketIOVersion::V3);
    ~PacketReceiver();
    
    // 获取当前版本
    SocketIOVersion get_version() const { return version_; }
    
    // 设置版本
    void set_version(SocketIOVersion version) {
        version_ = version;
        update_parser_config();
    }
    
    // 设置接收完成回调
    void set_complete_callback(std::function<void(const std::vector<T>& data_array)> callback);
    
    // 设置ACK管理器
    void set_ack_manager(AckManager* ack_manager);
    
    // 接收文本部分（自动检测版本）
    bool receive_text(const std::string& text);
    
    // 接收文本部分（指定版本）
    bool receive_text_with_version(const std::string& text, SocketIOVersion version);
    
    // 接收二进制部分
    bool receive_binary(const SmartBuffer& binary);
    
    // 重置接收状态
    void reset();
    
private:
    struct ReceiveState {
        std::string current_text;
        std::vector<SmartBuffer> received_binaries;
        std::vector<SmartBuffer> expected_binaries;
        bool expecting_binary;
        Packet packet_info;              // 存储解析后的包信息
        std::string namespace_str;       // 存储命名空间字符串
        SocketIOVersion packet_version;  // 存储包版本
        std::function<void(const std::vector<T>& data_array)> complete_callback;
        
        ReceiveState() : expecting_binary(false), packet_version(SocketIOVersion::V3) {}
    };
    
    webrtc::TaskQueueFactory* task_queue_factory_;
    std::shared_ptr<rtc::TaskQueue> task_queue_;
    SocketIOVersion version_;
    std::unique_ptr<ReceiveState> state_;
    AckManager* ack_manager_;  // ACK 管理器指针
    
    // 更新解析器配置
    void update_parser_config();
    
    // 检查并触发完成回调
    void check_and_trigger_complete();
};

} // namespace sio

#endif /* sio_packet_impl_hpp */
