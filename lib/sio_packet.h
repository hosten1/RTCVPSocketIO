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
#include <map>
#include <functional>
#include "json/json.h"
#include "rtc_base/buffer.h"
#include "sio_jsoncpp_binary_helper.hpp"

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
    
    // 生成调试信息字符串
    std::string to_string() const;
};

// 分包器：将包含二进制的包拆分为文本部分和二进制部分
template <typename T>
class PacketSplitter {
public:
    struct SplitResult {
        std::string text_part;  // 文本部分（包含占位符的JSON字符串）
        std::vector<rtc::Buffer> binary_parts;  // 二进制部分
    };
    
    // 异步拆分接口1: 使用lambda回调处理拆分结果
    static void split_data_array_async(
        const std::vector<T>& data_array,
        std::function<void(const std::string& text_part)> text_callback,
        std::function<void(const rtc::Buffer& binary_part, size_t index)> binary_callback = nullptr);
    
    // 异步拆分接口2: 单个回调接收完整拆分结果
    static void split_data_array_async(
        const std::vector<T>& data_array,
        std::function<void(const SplitResult& result)> callback);
    
    // 异步合并接口1: 使用lambda回调处理合并结果
    static void combine_to_data_array_async(
        const std::string& text_part,
        const std::vector<rtc::Buffer>& binary_parts,
        std::function<void(const std::vector<T>& data_array)> callback);
    
    // 异步合并接口2: 流式合并，逐个添加二进制数据
    static void combine_streaming_async(
        const std::string& text_part,
        std::function<void(const rtc::Buffer& binary_part, size_t index)> request_binary_callback,
        std::function<void(const std::vector<T>& data_array)> complete_callback);
    
    // 同步接口（向后兼容）
    static SplitResult split_data_array(const std::vector<T>& data_array);
    static std::vector<T> combine_to_data_array(
        const std::string& text_part,
        const std::vector<rtc::Buffer>& binary_parts);
    
    // 从文本中解析二进制占位符数量
    static int parse_binary_count(const std::string& text);
    
private:
    // 将数据转换为 JSON，提取二进制数据并替换为占位符
    static Json::Value data_to_json(const T& value,
                                  std::function<void(const rtc::Buffer& binary_part, size_t index)> binary_callback,
                                  int& placeholder_counter);
    
    // 将数据数组转换为JSON数组，并提取二进制数据
    static Json::Value convert_to_json_with_placeholders(
        const std::vector<T>& data_array,
        std::function<void(const rtc::Buffer& binary_part, size_t index)> binary_callback,
        int& placeholder_counter);
    
    // 将 JSON 转换为数据，将占位符替换为二进制数据
    static T json_to_data(const Json::Value& json,
                               const std::vector<rtc::Buffer>& binaries);
    
    // 将 JSON 数组转换为数据数组
    static std::vector<T> json_array_to_data_array(const Json::Value& json_array,
                               const std::vector<rtc::Buffer>& binaries);
    
    // 创建二进制占位符
    static Json::Value create_placeholder(int num);
    
    // 判断是否为二进制占位符
    static bool is_placeholder(const Json::Value& value);
    
    // 从占位符获取索引
    static int get_placeholder_index(const Json::Value& value);
};

} // namespace sio

#endif // SIO_PACKET_H
