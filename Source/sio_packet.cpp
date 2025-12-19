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
            const rtc::Buffer& buffer = packet.attachments[i];
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

// 从文本中解析二进制占位符数量
template <typename T>
int PacketSplitter<T>::parse_binary_count(const std::string& text) {
    return count_placeholders(text);
}

// 将数据转换为 JSON，提取二进制数据并替换为占位符
// 默认实现 - 用户需要为特定类型提供特化
template <typename T>
Json::Value PacketSplitter<T>::data_to_json(
    const T& value,
    std::function<void(const rtc::Buffer& binary_part, size_t index)> binary_callback,
    int& placeholder_counter) {
    
    // 默认实现，对于不支持的类型返回null
    return Json::Value(Json::nullValue);
}

// 将数据数组转换为JSON数组，并提取二进制数据
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

// 将 JSON 转换为数据，将占位符替换为二进制数据
template <typename T>
T PacketSplitter<T>::json_to_data(const Json::Value& json,
                               const std::vector<rtc::Buffer>& binaries) {
    // 默认实现 - 用户需要为特定类型提供特化
    return T();
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
            // 创建数据的拷贝
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
    
    // 请求所有二进制数据
    for (int i = 0; i < binary_count; i++) {
        if (request_binary_callback) {
            request_binary_callback(rtc::Buffer(), i);
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
        [&result](const rtc::Buffer& binary_part, size_t index) {
            // 确保有足够的空间
            if (result.binary_parts.size() <= index) {
                result.binary_parts.resize(index + 1);
            }
            // 创建数据的拷贝
            rtc::Buffer buffer_copy;
            buffer_copy.SetData(binary_part.data(), binary_part.size());
            result.binary_parts[index] = std::move(buffer_copy);
        }
    );
    
    return result;
}

template <typename T>
std::vector<T> PacketSplitter<T>::combine_to_data_array(
    const std::string& text_part,
    const std::vector<rtc::Buffer>& binary_parts) {
    
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

// ============================================================================
// PacketSender 模板类实现
// ============================================================================

template <typename T>
PacketSender<T>::PacketSender() : state_(new typename PacketSender<T>::SendState()) {
    reset();
}

template <typename T>
PacketSender<T>::~PacketSender() {
    // unique_ptr会自动管理内存
}

template <typename T>
void PacketSender<T>::prepare_data_array_async(
    const std::vector<T>& data_array,
    PacketType type,
    int nsp,
    int id,
    std::function<void()> on_complete) {
    
    reset();
    
    state_->on_complete = on_complete;
    
    // 拆分数据数组
    PacketSplitter<T>::split_data_array_async(
        data_array,
        [this, type, nsp, id](const std::string& text_part) {
            // 创建Packet
            Packet packet;
            packet.type = type;
            packet.nsp = nsp;
            packet.id = id;
            
            // 检查是否包含二进制数据
            bool has_binary = PacketSplitter<T>::parse_binary_count(text_part) > 0;
            
            // 如果有二进制数据，需要更新包类型
            if (has_binary) {
                if (packet.type == PacketType::EVENT) {
                    packet.type = PacketType::BINARY_EVENT;
                } else if (packet.type == PacketType::ACK) {
                    packet.type = PacketType::BINARY_ACK;
                }
            }
            
            packet.data = text_part;
            
            // 将文本部分加入队列
            std::string encoded = encode_packet(packet);
            state_->text_queue.push(encoded);
            
            // 开始处理队列
            process_next_item();
        },
        [this](const rtc::Buffer& binary_part, size_t index) {
            // 创建数据的拷贝
            rtc::Buffer buffer_copy;
            buffer_copy.SetData(binary_part.data(), binary_part.size());
            state_->binary_queue.push(std::move(buffer_copy));
        }
    );
}

template <typename T>
void PacketSender<T>::set_text_callback(std::function<void(const std::string& text)> callback) {
    state_->text_callback = callback;
}

template <typename T>
void PacketSender<T>::set_binary_callback(std::function<void(const rtc::Buffer& binary)> callback) {
    state_->binary_callback = callback;
}

template <typename T>
void PacketSender<T>::process_next_item() {
    // 先发送文本
    if (!state_->text_queue.empty()) {
        std::string text = state_->text_queue.front();
        state_->text_queue.pop();
        
        if (state_->text_callback) {
            state_->text_callback(text);
        }
        
        // 如果有二进制数据需要发送，设置标志
        state_->expecting_binary = !state_->binary_queue.empty();
    }
    
    // 然后发送二进制
    if (!state_->binary_queue.empty()) {
        // 获取二进制数据
        rtc::Buffer binary = std::move(state_->binary_queue.front());
        state_->binary_queue.pop();
        
        if (state_->binary_callback) {
            state_->binary_callback(binary);
        }
    }
    
    // 检查是否完成
    if (state_->text_queue.empty() && state_->binary_queue.empty()) {
        if (state_->on_complete) {
            state_->on_complete();
        }
    }
}

template <typename T>
void PacketSender<T>::reset() {
    state_->text_queue = std::queue<std::string>();
    state_->binary_queue = std::queue<rtc::Buffer>();
    state_->expecting_binary = false;
    state_->text_callback = nullptr;
    state_->binary_callback = nullptr;
    state_->on_complete = nullptr;
}

// ============================================================================
// PacketReceiver 模板类实现
// ============================================================================

template <typename T>
PacketReceiver<T>::PacketReceiver() : state_(new typename PacketReceiver<T>::ReceiveState()) {
    reset();
}

template <typename T>
PacketReceiver<T>::~PacketReceiver() {
    // unique_ptr会自动管理内存
}

template <typename T>
void PacketReceiver<T>::set_complete_callback(std::function<void(const std::vector<T>& data_array)> callback) {
    state_->complete_callback = callback;
}

template <typename T>
bool PacketReceiver<T>::receive_text(const std::string& text) {
    reset();
    
    state_->current_text = text;
    
    // 检查是否是二进制包
    if (text.empty() || !isdigit(text[0])) {
        return false;
    }
    
    int packet_type = text[0] - '0';
    
    // 检查是否是二进制包
    if (packet_type == static_cast<int>(PacketType::BINARY_EVENT) ||
        packet_type == static_cast<int>(PacketType::BINARY_ACK)) {
        // 需要二进制数据
        int binary_count = PacketSplitter<T>::parse_binary_count(text);
        state_->expected_binaries.resize(binary_count);
        state_->received_binaries.clear();
        state_->received_binaries.reserve(binary_count);
        state_->expecting_binary = (binary_count > 0);
        
        if (binary_count == 0) {
            // 虽然是二进制包类型，但没有实际的二进制数据
            check_and_trigger_complete();
        }
    } else {
        // 普通包，已经完整
        check_and_trigger_complete();
    }
    
    return true;
}

template <typename T>
bool PacketReceiver<T>::receive_binary(const rtc::Buffer& binary) {
    if (!state_->expecting_binary) {
        return false;
    }
    
    // 创建数据的拷贝
    rtc::Buffer buffer_copy;
    buffer_copy.SetData(binary.data(), binary.size());
    state_->received_binaries.push_back(std::move(buffer_copy));
    
    // 检查是否已接收完所有预期的二进制数据
    if (state_->received_binaries.size() >= state_->expected_binaries.size()) {
        state_->expecting_binary = false;
        check_and_trigger_complete();
    }
    
    return true;
}

template <typename T>
void PacketReceiver<T>::check_and_trigger_complete() {
    if (state_->complete_callback &&
        (!state_->expecting_binary || state_->received_binaries.size() >= state_->expected_binaries.size())) {
        
        // 合并数据数组
        PacketSplitter<T>::combine_to_data_array_async(
            state_->current_text,
            state_->received_binaries,
            state_->complete_callback
        );
    }
}

template <typename T>
void PacketReceiver<T>::reset() {
    state_->current_text.clear();
    state_->received_binaries.clear();
    state_->expected_binaries.clear();
    state_->expecting_binary = false;
}

// ============================================================================
// 显式特化常用类型
// ============================================================================

// Json::Value 类型的特化实现

template <>
Json::Value PacketSplitter<Json::Value>::data_to_json(
    const Json::Value& value,
    std::function<void(const rtc::Buffer& binary_part, size_t index)> binary_callback,
    int& placeholder_counter) {
    
    // 基本类型直接返回
    if (value.isNull() || value.isBool() || value.isInt() ||
        value.isUInt() || value.isDouble() || value.isString()) {
        return value;
    }
    
    // 处理数组
    if (value.isArray()) {
        Json::Value json_array(Json::arrayValue);
        for (Json::ArrayIndex i = 0; i < value.size(); i++) {
            json_array.append(data_to_json(value[i], binary_callback, placeholder_counter));
        }
        return json_array;
    }
    
    // 处理对象
    if (value.isObject()) {
        // 检查是否是二进制数据对象 - 格式1：_is_binary + data
        if (value.isMember("_is_binary") && value["_is_binary"].isBool() &&
            value["_is_binary"].asBool() && value.isMember("data") && value["data"].isString()) {
            
            // 创建二进制数据
            std::string binary_str = value["data"].asString();
            rtc::Buffer buffer(binary_str.data(), binary_str.size());
            
            // 调用二进制回调
            if (binary_callback) {
                binary_callback(buffer, placeholder_counter);
            }
            
            // 创建占位符
            Json::Value placeholder = create_placeholder(placeholder_counter);
            placeholder_counter++;
            return placeholder;
        }
        
        // 检查是否是二进制数据对象 - 使用binary_helper
        if (sio::binary_helper::is_binary(value)) {
            // 获取二进制数据指针
            rtc::Buffer* buffer_ptr = sio::binary_helper::get_binary_ptr(value);
            
            if (buffer_ptr) {
                // 调用二进制回调
                if (binary_callback) {
                    binary_callback(*buffer_ptr, placeholder_counter);
                }
                
                // 创建占位符
                Json::Value placeholder = create_placeholder(placeholder_counter);
                placeholder_counter++;
                return placeholder;
            }
        }
        
        // 普通对象
        Json::Value json_obj(Json::objectValue);
        Json::Value::Members members = value.getMemberNames();
        for (const auto& key : members) {
            json_obj[key] = data_to_json(value[key], binary_callback, placeholder_counter);
        }
        return json_obj;
    }
    
    return Json::Value(Json::nullValue);
}

template <>
Json::Value PacketSplitter<Json::Value>::json_to_data(
    const Json::Value& json,
    const std::vector<rtc::Buffer>& binaries) {
    
    // 基本类型直接返回
    if (json.isNull() || json.isBool() || json.isInt() ||
        json.isUInt() || json.isDouble() || json.isString()) {
        return json;
    }
    
    // 处理数组
    if (json.isArray()) {
        Json::Value array(Json::arrayValue);
        for (Json::ArrayIndex i = 0; i < json.size(); i++) {
            array.append(json_to_data(json[i], binaries));
        }
        return array;
    }
    
    // 处理对象
    if (json.isObject()) {
        // 检查是否是占位符
        if (is_placeholder(json)) {
            int index = get_placeholder_index(json);
            if (index >= 0 && index < static_cast<int>(binaries.size())) {
                // 获取二进制数据
                const rtc::Buffer& buffer = binaries[index];
                
                // 使用binary_helper创建包含二进制数据的对象
                return sio::binary_helper::create_binary_value(buffer);
            }
            return Json::Value(Json::nullValue);
        }
        
        // 普通对象
        Json::Value obj(Json::objectValue);
        Json::Value::Members members = json.getMemberNames();
        for (const auto& key : members) {
            obj[key] = json_to_data(json[key], binaries);
        }
        return obj;
    }
    
    return Json::Value(Json::nullValue);
}

// 实现Packet::to_string()方法
std::string Packet::to_string() const {
    return Packet_to_string(*this);
}

// 显式实例化常用类型
template class PacketSplitter<Json::Value>;
template class PacketSender<Json::Value>;
template class PacketReceiver<Json::Value>;

} // namespace sio
