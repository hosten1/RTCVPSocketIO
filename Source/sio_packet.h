//
//  sio_packet.hpp
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/18.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#ifndef SIO_PACKET_H
#define SIO_PACKET_H

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <queue>
#include <any>
#include <map>
#include <functional>
#include "json/json.h"
#include "rtc_base/buffer.h"

namespace sio {

// 数据包类型
enum class PacketType {
    CONNECT = 0,
    DISCONNECT = 1,
    EVENT = 2,
    ACK = 3,
    ERROR = 4,
    BINARY_EVENT = 5,
    BINARY_ACK = 6
};

// Socket.IO数据包结构
struct Packet {
    PacketType type;
    int nsp;  // 命名空间索引
    int id;   // 包ID（用于ACK）
    std::string data;  // JSON数据
    std::vector<rtc::Buffer> attachments;  // 二进制附件（使用 WebRTC Buffer）
    
    Packet() : type(PacketType::EVENT), nsp(0), id(-1) {}
    
    // 检查是否包含二进制数据
    bool has_binary() const { return !attachments.empty(); }
};

// 分包器：将包含二进制的包拆分为文本部分和二进制部分
class PacketSplitter {
public:
    struct SplitResult {
        std::string text_part;  // 文本部分（包含占位符的JSON字符串）
        std::vector<rtc::Buffer> binary_parts;  // 二进制部分
    };
    
    // 异步拆分接口1: 使用lambda回调处理拆分结果
    // text_callback: 处理文本部分的回调
    // binary_callback: 处理每个二进制部分的回调
    static void split_data_array_async(
        const std::vector<std::any>& data_array,
        std::function<void(const std::string& text_part)> text_callback,
        std::function<void(const rtc::Buffer& binary_part, size_t index)> binary_callback = nullptr);
    
    // 异步拆分接口2: 单个回调接收完整拆分结果
    static void split_data_array_async(
        const std::vector<std::any>& data_array,
        std::function<void(const SplitResult& result)> callback);
    
    // 异步合并接口1: 使用lambda回调处理合并结果
    static void combine_to_data_array_async(
        const std::string& text_part,
        const std::vector<rtc::Buffer>& binary_parts,
        std::function<void(const std::vector<std::any>& data_array)> callback);
    
    // 异步合并接口2: 流式合并，逐个添加二进制数据
    static void combine_streaming_async(
        const std::string& text_part,
        std::function<void(const rtc::Buffer& binary_part, size_t index)> request_binary_callback,
        std::function<void(const std::vector<std::any>& data_array)> complete_callback);
    
    // 同步接口（向后兼容）
    static SplitResult split_data_array(const std::vector<std::any>& data_array);
    static std::vector<std::any> combine_to_data_array(
        const std::string& text_part,
        const std::vector<rtc::Buffer>& binary_parts);
    
private:
    // 将数据数组转换为JSON数组，并提取二进制数据
    static Json::Value convert_to_json_with_placeholders(
        const std::vector<std::any>& data_array,
        std::function<void(const rtc::Buffer& binary_part, size_t index)> binary_callback,
        int& placeholder_counter);
    
    // 将 JSON 转换为 std::any，将占位符替换为二进制数据
    static std::any json_to_any(const Json::Value& json,
                               const std::vector<rtc::Buffer>& binaries);
    
    // 将 std::any 转换为 JSON，提取二进制数据并替换为占位符
    static Json::Value any_to_json(const std::any& value,
                                  std::function<void(const rtc::Buffer& binary_part, size_t index)> binary_callback,
                                  int& placeholder_counter);
    
    // 创建二进制占位符
    static Json::Value create_placeholder(int num);
    
    // 判断是否为二进制占位符
    static bool is_placeholder(const Json::Value& value);
    
    // 从占位符获取索引
    static int get_placeholder_index(const Json::Value& value);
    
    // 从文本中解析二进制占位符数量
    static int parse_binary_count(const std::string& text);
};

// 发送队列管理类（使用lambda回调）
class PacketSender {
public:
    PacketSender();
    ~PacketSender();
    
    // 准备要发送的数据数组（异步处理）
    void prepare_data_array_async(
        const std::vector<std::any>& data_array,
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
class PacketReceiver {
public:
    PacketReceiver();
    ~PacketReceiver();
    
    // 设置接收完成回调
    void set_complete_callback(std::function<void(const std::vector<std::any>& data_array)> callback);
    
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
        std::function<void(const std::vector<std::any>& data_array)> complete_callback;
    };
    
    std::unique_ptr<ReceiveState> state_;
    
    // 检查并触发完成回调
    void check_and_trigger_complete();
};

} // namespace sio

#endif // SIO_PACKET_H
