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
#include "sio_smart_buffer.hpp"
#include "sio_jsoncpp_binary_helper.hpp"
#include "sio_packet_types.h"  // 添加类型定义头文件
#include "sio_packet_parser.h"  // 添加解析器头文件

namespace sio {

// 分包器：将包含二进制的包拆分为文本部分和二进制部分
template <typename T>
class PacketSplitter {
public:
    struct SplitResult {
        std::string text_part;  // 文本部分（包含占位符的JSON字符串）
        std::vector<SmartBuffer> binary_parts;  // 二进制部分（智能指针管理）
    };
    
    // 异步拆分接口1: 使用lambda回调处理拆分结果
    static void split_data_array_async(
        const std::vector<T>& data_array,
        std::function<void(const std::string& text_part)> text_callback,
        std::function<void(const SmartBuffer& binary_part, size_t index)> binary_callback = nullptr);
    
    // 异步拆分接口2: 单个回调接收完整拆分结果
    static void split_data_array_async(
        const std::vector<T>& data_array,
        std::function<void(const SplitResult& result)> callback);
    
    // 异步合并接口1: 使用lambda回调处理合并结果
    static void combine_to_data_array_async(
        const std::string& text_part,
        const std::vector<SmartBuffer>& binary_parts,
        std::function<void(const std::vector<T>& data_array)> callback);
    
    // 异步合并接口2: 流式合并，逐个添加二进制数据
    static void combine_streaming_async(
        const std::string& text_part,
        std::function<void(const SmartBuffer& binary_part, size_t index)> request_binary_callback,
        std::function<void(const std::vector<T>& data_array)> complete_callback);
    
    // 同步接口（向后兼容）
    static SplitResult split_data_array(const std::vector<T>& data_array);
    static std::vector<T> combine_to_data_array(
        const std::string& text_part,
        const std::vector<SmartBuffer>& binary_parts);
    
    // 从文本中解析二进制占位符数量 - 使用 PacketParser
    static int parse_binary_count(const std::string& text);
    
    // 将 JSON 转换为数据，将占位符替换为二进制数据
    static T json_to_data(const Json::Value& json,
                             const std::vector<SmartBuffer>& binaries);
    
private:
    // 将数据转换为 JSON，提取二进制数据并替换为占位符
    static Json::Value data_to_json(const T& value,
                                  std::function<void(const SmartBuffer& binary_part, size_t index)> binary_callback,
                                  int& placeholder_counter);
    
    // 将数据数组转换为JSON数组，并提取二进制数据
    static Json::Value convert_to_json_with_placeholders(
        const std::vector<T>& data_array,
        std::function<void(const SmartBuffer& binary_part, size_t index)> binary_callback,
        int& placeholder_counter);
    
    // 将 JSON 数组转换为数据数组
    static std::vector<T> json_array_to_data_array(const Json::Value& json_array,
                               const std::vector<SmartBuffer>& binaries);
    
    // 创建二进制占位符
    static Json::Value create_placeholder(int num);
    
    // 判断是否为二进制占位符
    static bool is_placeholder(const Json::Value& value);
    
    // 从占位符获取索引
    static int get_placeholder_index(const Json::Value& value);
};

// 包处理工具类
class PacketUtils {
public:
    // 检测包类型
    static PacketType detect_packet_type(const std::string& packet_str) {
        return PacketParser::getInstance().getPacketType(packet_str);
    }
    
    // 获取包ID
    static int get_packet_id(const std::string& packet_str) {
        return PacketParser::getInstance().getPacketId(packet_str);
    }
    
    // 获取命名空间
    static std::string get_namespace(const std::string& packet_str) {
        return PacketParser::getInstance().getNamespace(packet_str);
    }
    
    // 验证包格式
    static bool validate_packet(const std::string& packet_str) {
        return PacketParser::getInstance().validatePacket(packet_str);
    }
    
    // 构建连接包
    static std::string build_connect_packet(const Json::Value& auth_data = Json::Value(),
                                          const std::string& nsp = "/",
                                          const Json::Value& query_params = Json::Value()) {
        return PacketParser::getInstance().buildConnectString(auth_data, nsp, query_params);
    }
    
    // 构建事件包
    static std::string build_event_packet(const std::string& event_name,
                                        const Json::Value& data = Json::Value(),
                                        int packet_id = -1,
                                        const std::string& nsp = "/",
                                        bool is_binary = false) {
        return PacketParser::getInstance().buildEventString(event_name, data, packet_id, nsp, is_binary);
    }
    
    // 构建ACK包
    static std::string build_ack_packet(int ack_id,
                                      const Json::Value& data = Json::Value(),
                                      const std::string& nsp = "/",
                                      bool is_binary = false) {
        return PacketParser::getInstance().buildAckString(ack_id, data, nsp, is_binary);
    }
    
    // 构建断开连接包
    static std::string build_disconnect_packet(const std::string& nsp = "/") {
        return PacketParser::getInstance().buildDisconnectString(nsp);
    }
    
    // 构建错误包
    static std::string build_error_packet(const std::string& error_message,
                                        const Json::Value& error_data = Json::Value(),
                                        const std::string& nsp = "/") {
        return PacketParser::getInstance().buildErrorString(error_message, error_data, nsp);
    }
    
    // 解析包
    static ParseResult parse_packet(const std::string& packet_str) {
        return PacketParser::getInstance().parsePacket(packet_str);
    }
};

} // namespace sio

#endif // SIO_PACKET_H
