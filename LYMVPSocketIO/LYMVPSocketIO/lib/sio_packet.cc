//
//  sio_packet.cpp
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/18.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#include "sio_packet.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <atomic>
#include "rtc_base/string_encode.h"

namespace sio {

namespace {
    // 获取PacketType的字符串表示
    std::string packet_type_to_string(PacketType type) {
        switch (type) {
            case PacketType::CONNECT: return "CONNECT";
            case PacketType::DISCONNECT: return "DISCONNECT";
            case PacketType::EVENT: return "EVENT";
            case PacketType::ACK: return "ACK";
            case PacketType::ERROR: return "ERROR";
            case PacketType::BINARY_EVENT: return "BINARY_EVENT";
            case PacketType::BINARY_ACK: return "BINARY_ACK";
            default: return "UNKNOWN";
        }
    }
    
    // Socket.IO包编码格式
    std::string encode_packet(const Packet& packet) {
        std::stringstream ss;
        
        // 包类型
        ss << static_cast<int>(packet.type);
        
        // 命名空间（如果有且不是默认命名空间）
        if (packet.nsp != 0) {
            ss << packet.nsp << ",";
        }
        
        // 包ID（如果有）
        if (packet.id >= 0) {
            ss << packet.id;
        }
        
        // 数据部分
        if (!packet.data.empty()) {
            if (packet.id >= 0) {
                ss << ",";
            }
            ss << packet.data;
        }
        
        return ss.str();
    }
    
    // 生成Packet的调试信息字符串
    std::string Packet_to_string(const Packet& packet) {
        std::stringstream ss;
        ss << "Packet {" << std::endl;
        ss << "  type: " << packet_type_to_string(packet.type) << " (" << static_cast<int>(packet.type) << ")" << std::endl;
        ss << "  nsp: " << packet.nsp << std::endl;
        ss << "  id: " << packet.id << std::endl;
        ss << "  data: " << packet.data << std::endl;
        ss << "  attachments: " << packet.attachments.size() << " 个二进制附件" << std::endl;
        for (size_t i = 0; i < packet.attachments.size(); ++i) {
            const SmartBuffer& buffer = packet.attachments[i];
            ss << "    [" << i << "]: 大小=" << buffer.size() << " 字节, 前16字节=";
            // 只打印前16字节的十六进制
            size_t print_size = std::min(buffer.size(), size_t(16));
            ss << rtc::hex_encode_with_delimiter(reinterpret_cast<const char*>(buffer.data()), print_size, ' ');
            if (buffer.size() > 16) {
                ss << "...";
            }
            ss << std::endl;
        }
        ss << "}";
        return ss.str();
    }
    
    // 在JSON字符串中查找二进制占位符数量（简单实现）
int count_placeholders(const std::string& json) {
    int count = 0;
    size_t pos = 0;
    
    while ((pos = json.find("\"_placeholder\":true", pos)) != std::string::npos) {
        count++;
        pos += 18; // 跳过已找到的字符串
    }
    
    std::cout << "DEBUG: count_placeholders 返回: " << count << std::endl;
    return count;
}
}

// PacketSplitter 模板类实现

// 创建二进制占位符
template <typename T>
Json::Value PacketSplitter<T>::create_placeholder(int num) {
    Json::Value placeholder(Json::objectValue);
    placeholder["_placeholder"] = true;
    placeholder["num"] = num;
    return placeholder;
}

// 判断是否为二进制占位符
template <typename T>
bool PacketSplitter<T>::is_placeholder(const Json::Value& value) {
    return value.isObject() &&
           value.isMember("_placeholder") &&
           value["_placeholder"].isBool() &&
           value["_placeholder"].asBool() &&
           value.isMember("num") &&
           value["num"].isInt();
}

// 从占位符获取索引
template <typename T>
int PacketSplitter<T>::get_placeholder_index(const Json::Value& value) {
    if (is_placeholder(value)) {
        return value["num"].asInt();
    }
    return -1;
}

// 解析文本中的二进制计数
template <typename T>
int PacketSplitter<T>::parse_binary_count(const std::string& text) {
    // 跳过包类型、命名空间和ID部分，只解析JSON数据部分
    size_t json_start = text.find_first_of("[{]");
    if (json_start == std::string::npos) {
        // 没有找到JSON开始标记，说明没有二进制数据
        std::cout << "DEBUG: parse_binary_count - 未找到JSON开始标记，返回0" << std::endl;
        return 0;
    }
    
    // 提取JSON数据部分
    std::string json_data = text.substr(json_start);
    int count = count_placeholders(json_data);
    
    std::cout << "DEBUG: parse_binary_count - 原始文本: " << text << std::endl;
    std::cout << "DEBUG: parse_binary_count - JSON部分: " << json_data << std::endl;
    std::cout << "DEBUG: parse_binary_count - 二进制计数: " << count << std::endl;
    
    return count;
}

// 将数据转换为 JSON，提取二进制数据并替换为占位符
// 默认实现 - 用户需要为特定类型提供特化
template <typename T>
Json::Value PacketSplitter<T>::data_to_json(
    const T& value,
    std::function<void(const SmartBuffer& binary_part, size_t index)> binary_callback,
    int& placeholder_counter) {
    
    // 默认实现，对于不支持的类型返回null
    return Json::Value(Json::nullValue);
}

// 将数据数组转换为JSON数组，并提取二进制数据
template <typename T>
Json::Value PacketSplitter<T>::convert_to_json_with_placeholders(
    const std::vector<T>& data_array,
    std::function<void(const SmartBuffer& binary_part, size_t index)> binary_callback,
    int& placeholder_counter) {
    
    Json::Value json_array(Json::arrayValue);
    for (const auto& item : data_array) {
        json_array.append(data_to_json(item, binary_callback, placeholder_counter));
    }
    return json_array;
}

// 将 JSON 转换为数据，将占位符替换为二进制数据
template <typename T>
T PacketSplitter<T>::json_to_data(const Json::Value& json,
                               const std::vector<SmartBuffer>& binaries) {
    // 默认实现 - 用户需要为特定类型提供特化
    return T();
}

// 将 JSON 数组转换为数据数组
template <typename T>
std::vector<T> PacketSplitter<T>::json_array_to_data_array(const Json::Value& json_array,
                               const std::vector<SmartBuffer>& binaries) {
    std::vector<T> data_array;
    if (json_array.isArray()) {
        for (Json::ArrayIndex i = 0; i < json_array.size(); i++) {
            data_array.push_back(json_to_data(json_array[i], binaries));
        }
    }
    return data_array;
}

// 异步拆分接口1: 使用lambda回调处理拆分结果
template <typename T>
void PacketSplitter<T>::split_data_array_async(
    const std::vector<T>& data_array,
    std::function<void(const std::string& text_part)> text_callback,
    std::function<void(const SmartBuffer& binary_part, size_t index)> binary_callback) {
    
    if (data_array.empty()) {
        Json::FastWriter writer;
        std::string empty_json = writer.write(Json::Value(Json::arrayValue));
        if (text_callback) {
            text_callback(empty_json);
        }
        return;
    }
    
    // 提取二进制数据并创建占位符
    int placeholder_counter = 0;
    Json::Value json_array = convert_to_json_with_placeholders(
        data_array, binary_callback, placeholder_counter);
    
    // 序列化JSON数组并回调
    Json::FastWriter writer;
    std::string text_part = writer.write(json_array);
    if (text_callback) {
        text_callback(text_part);
    }
}

// 异步拆分接口2: 单个回调接收完整拆分结果
template <typename T>
void PacketSplitter<T>::split_data_array_async(
    const std::vector<T>& data_array,
    std::function<void(const SplitResult& result)> callback) {
    
    if (!callback) return;
    
    SplitResult result;
    
    // 使用第一个异步接口，收集所有二进制数据
    std::vector<SmartBuffer> collected_binaries;
    
    split_data_array_async(
        data_array,
        [&result](const std::string& text_part) {
            result.text_part = text_part;
        },
        [&collected_binaries](const SmartBuffer& binary_part, size_t index) {
            // 确保有足够的空间
            if (collected_binaries.size() <= index) {
                collected_binaries.resize(index + 1);
            }
            // 使用SmartBuffer的拷贝构造函数
            collected_binaries[index] = binary_part;
        }
    );
    
    result.binary_parts = std::move(collected_binaries);
    callback(result);
}

// 异步合并接口1: 使用lambda回调处理合并结果
template <typename T>
void PacketSplitter<T>::combine_to_data_array_async(
    const std::string& text_part,
    const std::vector<SmartBuffer>& binary_parts,
    std::function<void(const std::vector<T>& data_array)> callback) {
    
    if (!callback) return;
    
    // 解析JSON
    Json::Value json_root;
    Json::Reader reader;
    
    if (!reader.parse(text_part, json_root)) {
        callback(std::vector<T>());
        return;
    }
    
    // 将JSON数组转换为数据数组
    std::vector<T> data_array = json_array_to_data_array(json_root, binary_parts);
    callback(data_array);
}

// 异步合并接口2: 流式合并，逐个添加二进制数据
template <typename T>
void PacketSplitter<T>::combine_streaming_async(
    const std::string& text_part,
    std::function<void(const SmartBuffer& binary_part, size_t index)> request_binary_callback,
    std::function<void(const std::vector<T>& data_array)> complete_callback) {
    
    if (!complete_callback) return;
    
    // 解析二进制占位符数量
    int binary_count = parse_binary_count(text_part);
    
    if (binary_count == 0) {
        // 没有二进制数据，直接合并
        combine_to_data_array_async(text_part, {}, complete_callback);
        return;
    }
    
    // 收集所有二进制数据
    std::vector<SmartBuffer> binary_parts(binary_count);
    
    // 请求所有二进制数据
    for (int i = 0; i < binary_count; i++) {
        if (request_binary_callback) {
            request_binary_callback(SmartBuffer(), i);
        }
    }
    
    // 注意：实际应用中，需要等待二进制数据接收完成
}

// 同步接口（向后兼容）
template <typename T>
typename PacketSplitter<T>::SplitResult PacketSplitter<T>::split_data_array(const std::vector<T>& data_array) {
    SplitResult result;
    
    split_data_array_async(
        data_array,
        [&result](const std::string& text_part) {
            result.text_part = text_part;
        },
        [&result](const SmartBuffer& binary_part, size_t index) {
            // 确保有足够的空间
            if (result.binary_parts.size() <= index) {
                result.binary_parts.resize(index + 1);
            }
            // 使用SmartBuffer的拷贝构造函数
            result.binary_parts[index] = binary_part;
        }
    );
    
    return result;
}

template <typename T>
std::vector<T> PacketSplitter<T>::combine_to_data_array(
    const std::string& text_part,
    const std::vector<SmartBuffer>& binary_parts) {
    
    std::vector<T> result;
    
    combine_to_data_array_async(
        text_part,
        binary_parts,
        [&result](const std::vector<T>& data_array) {
            result = data_array;
        }
    );
    
    return result;
}

// 实现Packet::to_string()方法
std::string Packet::to_string() const {
    return Packet_to_string(*this);
}

// 显式实例化常用类型
template class PacketSplitter<Json::Value>;

} // namespace sio
