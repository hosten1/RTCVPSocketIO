//
//  sio_packet.cpp
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/18.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#include "sio_packet.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

namespace sio {

namespace {
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
    
    // 在JSON字符串中查找二进制占位符数量（简单实现）
    int count_placeholders(const std::string& json) {
        int count = 0;
        size_t pos = 0;
        
        while ((pos = json.find("\"_placeholder\":true", pos)) != std::string::npos) {
            count++;
            pos += 18; // 跳过已找到的字符串
        }
        
        return count;
    }
}

// PacketSplitter 模板类实现

// 显式特化 Json::Value 类型的 data_to_json 函数
template <>
Json::Value PacketSplitter<Json::Value>::data_to_json(
    const Json::Value& value,
    std::function<void(const rtc::Buffer& binary_part, size_t index)> binary_callback,
    int& placeholder_counter) {
    
    try {
        // 检查是否是二进制数据
        if (value.isObject() && value.isMember("_binary_data") && value["_binary_data"].isBool() && value["_binary_data"].asBool()) {
            // 提取二进制数据并通过回调传递
            const rtc::Buffer* buffer = reinterpret_cast<const rtc::Buffer*>(value["_buffer_ptr"].asUInt64());
            if (buffer && binary_callback) {
                binary_callback(*buffer, placeholder_counter);
            }
            
            // 创建占位符
            Json::Value placeholder = create_placeholder(placeholder_counter);
            placeholder_counter++;
            return placeholder;
        }
        
        // 检查是否是数组
        if (value.isArray()) {
            Json::Value json_array(Json::arrayValue);
            for (Json::ArrayIndex i = 0; i < value.size(); i++) {
                json_array.append(data_to_json(value[i], binary_callback, placeholder_counter));
            }
            return json_array;
        }
        
        // 检查是否是对象
        if (value.isObject()) {
            Json::Value json_obj(Json::objectValue);
            Json::Value::Members members = value.getMemberNames();
            for (Json::Value::Members::const_iterator it = members.begin(); it != members.end(); ++it) {
                const std::string& key = *it;
                json_obj[key] = data_to_json(value[key], binary_callback, placeholder_counter);
            }
            return json_obj;
        }
        
        // 基本类型直接返回
        return value;
    } catch (const std::exception& e) {
        // 捕获任何异常，避免程序崩溃
        return Json::Value(Json::nullValue);
    } catch (...) {
        // 捕获未知异常
        return Json::Value(Json::nullValue);
    }
}

// 显式特化 Json::Value 类型的 json_to_data 函数
template <>
Json::Value PacketSplitter<Json::Value>::json_to_data(const Json::Value& json,
                                    const std::vector<rtc::Buffer>& binaries) {
    if (json.isArray()) {
        // 处理数组类型
        Json::Value array(Json::arrayValue);
        for (Json::ArrayIndex i = 0; i < json.size(); i++) {
            array.append(json_to_data(json[i], binaries));
        }
        return array;
    } else if (json.isObject()) {
        // 检查是否是占位符
        if (is_placeholder(json)) {
            int index = get_placeholder_index(json);
            if (index >= 0 && index < static_cast<int>(binaries.size())) {
                // 创建一个包含二进制数据的对象
                Json::Value binary_obj(Json::objectValue);
                binary_obj["_binary_data"] = true;
                // 注意：这里我们不能直接存储指针，因为binaries是临时的
                // 我们会在后续处理中提取实际的二进制数据
                return binary_obj;
            }
            return Json::Value(Json::nullValue);
        }
        
        // 处理普通对象
        Json::Value obj(Json::objectValue);
        Json::Value::Members members = json.getMemberNames();
        for (Json::Value::Members::const_iterator it = members.begin(); it != members.end(); ++it) {
            const std::string& member = *it;
            obj[member] = json_to_data(json[member], binaries);
        }
        return obj;
    }
    
    // 基本类型直接返回
    return json;
}

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

// 从文本中解析二进制占位符数量
template <typename T>
int PacketSplitter<T>::parse_binary_count(const std::string& text) {
    return count_placeholders(text);
}

// 将数据数组转换为JSON数组，并提取二进制数据（使用回调）
template <typename T>
Json::Value PacketSplitter<T>::convert_to_json_with_placeholders(
    const std::vector<T>& data_array,
    std::function<void(const rtc::Buffer& binary_part, size_t index)> binary_callback,
    int& placeholder_counter) {
    
    Json::Value json_array(Json::arrayValue);
    for (const auto& item : data_array) {
        json_array.append(data_to_json(item, binary_callback, placeholder_counter));
    }
    return json_array;
}

// 将 JSON 数组转换为数据数组
template <typename T>
std::vector<T> PacketSplitter<T>::json_array_to_data_array(const Json::Value& json_array,
                                    const std::vector<rtc::Buffer>& binaries) {
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
    std::function<void(const rtc::Buffer& binary_part, size_t index)> binary_callback) {
    
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
    std::vector<rtc::Buffer> collected_binaries;
    
    split_data_array_async(
        data_array,
        [&result](const std::string& text_part) {
            result.text_part = text_part;
        },
        [&collected_binaries](const rtc::Buffer& binary_part, size_t index) {
            // 确保有足够的空间
            if (collected_binaries.size() <= index) {
                collected_binaries.resize(index + 1);
            }
            // 使用移动语义处理不可拷贝类型
            rtc::Buffer buffer_copy;
            buffer_copy.SetData(binary_part.data(), binary_part.size());
            collected_binaries[index] = std::move(buffer_copy);
        }
    );
    
    result.binary_parts = std::move(collected_binaries);
    callback(result);
}

// 异步合并接口1: 使用lambda回调处理合并结果
template <typename T>
void PacketSplitter<T>::combine_to_data_array_async(
    const std::string& text_part,
    const std::vector<rtc::Buffer>& binary_parts,
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
    std::function<void(const rtc::Buffer& binary_part, size_t index)> request_binary_callback,
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
    std::vector<rtc::Buffer> binary_parts(binary_count);
    std::atomic<int> received_count{0};
    
    // 请求所有二进制数据
    for (int i = 0; i < binary_count; i++) {
        if (request_binary_callback) {
            request_binary_callback(rtc::Buffer(), i);
        }
    }
    
    // 注意：实际应用中，这里需要等待二进制数据接收完成
    // 这里简化处理，假设调用者会通过其他方式提供二进制数据
    // 实际使用时，需要一个机制来等待所有二进制数据就绪
}

// 显式实例化模板类，支持Json::Value类型
template class PacketSplitter<Json::Value>;

} // namespace sio