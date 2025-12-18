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

// PacketSplitter实现

// 创建二进制占位符
Json::Value PacketSplitter::create_placeholder(int num) {
    Json::Value placeholder(Json::objectValue);
    placeholder["_placeholder"] = true;
    placeholder["num"] = num;
    return placeholder;
}

// 判断是否为二进制占位符
bool PacketSplitter::is_placeholder(const Json::Value& value) {
    return value.isObject() &&
           value.isMember("_placeholder") &&
           value["_placeholder"].isBool() &&
           value["_placeholder"].asBool() &&
           value.isMember("num") &&
           value["num"].isInt();
}

// 从占位符获取索引
int PacketSplitter::get_placeholder_index(const Json::Value& value) {
    if (is_placeholder(value)) {
        return value["num"].asInt();
    }
    return -1;
}

// 从文本中解析二进制占位符数量
int PacketSplitter::parse_binary_count(const std::string& text) {
    return count_placeholders(text);
}

// 将 std::any 转换为 JSON，提取二进制数据并替换为占位符（使用回调）
Json::Value PacketSplitter::any_to_json(
    const std::any& value,
    std::function<void(const rtc::Buffer& binary_part, size_t index)> binary_callback,
    int& placeholder_counter) {
    
    if (value.type() == typeid(std::string)) {
        return Json::Value(std::any_cast<std::string>(value));
    } else if (value.type() == typeid(int)) {
        return Json::Value(std::any_cast<int>(value));
    } else if (value.type() == typeid(double)) {
        return Json::Value(std::any_cast<double>(value));
    } else if (value.type() == typeid(bool)) {
        return Json::Value(std::any_cast<bool>(value));
    } else if (value.type() == typeid(rtc::Buffer)) {
        // 二进制数据：创建占位符并通过回调传递二进制数据
        const rtc::Buffer& buffer = std::any_cast<const rtc::Buffer&>(value);
        
        if (binary_callback) {
            binary_callback(buffer, placeholder_counter);
        }
        
        Json::Value placeholder = create_placeholder(placeholder_counter);
        placeholder_counter++;
        return placeholder;
    } else if (value.type() == typeid(std::vector<std::any>)) {
        // 数组类型
        const std::vector<std::any>& array = std::any_cast<const std::vector<std::any>&>(value);
        Json::Value json_array(Json::arrayValue);
        for (const auto& item : array) {
            json_array.append(any_to_json(item, binary_callback, placeholder_counter));
        }
        return json_array;
    } else if (value.type() == typeid(std::map<std::string, std::any>)) {
        // 对象类型
        const std::map<std::string, std::any>& obj = std::any_cast<const std::map<std::string, std::any>&>(value);
        Json::Value json_obj(Json::objectValue);
        for (const auto& [key, val] : obj) {
            json_obj[key] = any_to_json(val, binary_callback, placeholder_counter);
        }
        return json_obj;
    } else if (value.type() == typeid(nullptr)) {
        return Json::Value(Json::nullValue);
    }
    
    // 未知类型，返回空值
    return Json::Value(Json::nullValue);
}

// 将数据数组转换为JSON数组，并提取二进制数据（使用回调）
Json::Value PacketSplitter::convert_to_json_with_placeholders(
    const std::vector<std::any>& data_array,
    std::function<void(const rtc::Buffer& binary_part, size_t index)> binary_callback,
    int& placeholder_counter) {
    
    Json::Value json_array(Json::arrayValue);
    for (const auto& item : data_array) {
        json_array.append(any_to_json(item, binary_callback, placeholder_counter));
    }
    return json_array;
}

// 将 JSON 转换为 std::any，将占位符替换为二进制数据
std::any PacketSplitter::json_to_any(const Json::Value& json,
                                    const std::vector<rtc::Buffer>& binaries) {
    if (json.isString()) {
        return std::any(json.asString());
    } else if (json.isInt()) {
        return std::any(json.asInt());
    } else if (json.isDouble()) {
        return std::any(json.asDouble());
    } else if (json.isBool()) {
        return std::any(json.asBool());
    } else if (json.isNull()) {
        return std::any(nullptr);
    } else if (json.isArray()) {
        std::vector<std::any> array;
        for (Json::ArrayIndex i = 0; i < json.size(); i++) {
            array.push_back(json_to_any(json[i], binaries));
        }
        return std::any(array);
    } else if (json.isObject()) {
        // 检查是否是占位符
        if (is_placeholder(json)) {
            int index = get_placeholder_index(json);
            if (index >= 0 && index < static_cast<int>(binaries.size())) {
                return std::any(binaries[index]);
            }
            return std::any(nullptr);
        }
        
        // 普通对象
        std::map<std::string, std::any> obj;
        auto members = json.getMemberNames();
        for (const auto& member : members) {
            obj[member] = json_to_any(json[member], binaries);
        }
        return std::any(obj);
    }
    
    return std::any(nullptr);
}

// 异步拆分接口1: 使用lambda回调处理拆分结果
void PacketSplitter::split_data_array_async(
    const std::vector<std::any>& data_array,
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
void PacketSplitter::split_data_array_async(
    const std::vector<std::any>& data_array,
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
            collected_binaries[index] = binary_part;
        }
    );
    
    result.binary_parts = std::move(collected_binaries);
    callback(result);
}

// 异步合并接口1: 使用lambda回调处理合并结果
void PacketSplitter::combine_to_data_array_async(
    const std::string& text_part,
    const std::vector<rtc::Buffer>& binary_parts,
    std::function<void(const std::vector<std::any>& data_array)> callback) {
    
    if (!callback) return;
    
    // 解析JSON
    Json::Value json_root;
    Json::Reader reader;
    
    if (!reader.parse(text_part, json_root)) {
        callback(std::vector<std::any>());
        return;
    }
    
    // 将JSON数组转换为数据数组
    std::vector<std::any> data_array;
    
    if (!json_root.isArray()) {
        // 如果不是数组，返回包含单个元素的数组
        data_array.push_back(json_to_any(json_root, binary_parts));
    } else {
        for (Json::ArrayIndex i = 0; i < json_root.size(); i++) {
            data_array.push_back(json_to_any(json_root[i], binary_parts));
        }
    }
    
    callback(data_array);
}

// 异步合并接口2: 流式合并，逐个添加二进制数据
void PacketSplitter::combine_streaming_async(
    const std::string& text_part,
    std::function<void(const rtc::Buffer& binary_part, size_t index)> request_binary_callback,
    std::function<void(const std::vector<std::any>& data_array)> complete_callback) {
    
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

// 同步接口（向后兼容）
PacketSplitter::SplitResult PacketSplitter::split_data_array(const std::vector<std::any>& data_array) {
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
            result.binary_parts[index] = binary_part;
        }
    );
    
    return result;
}

std::vector<std::any> PacketSplitter::combine_to_data_array(
    const std::string& text_part,
    const std::vector<rtc::Buffer>& binary_parts) {
    
    std::vector<std::any> result;
    
    combine_to_data_array_async(
        text_part,
        binary_parts,
        [&result](const std::vector<std::any>& data_array) {
            result = data_array;
        }
    );
    
    return result;
}

// PacketSender实现
PacketSender::PacketSender() : state_(new PacketSender::SendState()) {
    reset();
}

PacketSender::~PacketSender() {
    // unique_ptr会自动管理内存
}

void PacketSender::prepare_data_array_async(
    const std::vector<std::any>& data_array,
    PacketType type,
    int nsp,
    int id,
    std::function<void()> on_complete) {
    
    reset();
    
    state_->on_complete = on_complete;
    
    // 拆分数据数组
    split_data_array_async(
        data_array,
        [this, type, nsp, id](const std::string& text_part) {
            // 创建Packet
            Packet packet;
            packet.type = type;
            packet.nsp = nsp;
            packet.id = id;
            
            // 检查是否包含二进制数据
            bool has_binary = parse_binary_count(text_part) > 0;
            
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
            // 将二进制部分加入队列
            state_->binary_queue.push(binary_part);
        }
    );
}

void PacketSender::set_text_callback(std::function<void(const std::string& text)> callback) {
    state_->text_callback = callback;
}

void PacketSender::set_binary_callback(std::function<void(const rtc::Buffer& binary)> callback) {
    state_->binary_callback = callback;
}

void PacketSender::process_next_item() {
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
        rtc::Buffer binary = state_->binary_queue.front();
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

void PacketSender::reset() {
    state_->text_queue = std::queue<std::string>();
    state_->binary_queue = std::queue<rtc::Buffer>();
    state_->expecting_binary = false;
    state_->text_callback = nullptr;
    state_->binary_callback = nullptr;
    state_->on_complete = nullptr;
}

// PacketReceiver实现
PacketReceiver::PacketReceiver() : state_(new PacketReceiver::ReceiveState()) {
    reset();
}

PacketReceiver::~PacketReceiver() {
    // unique_ptr会自动管理内存
}

void PacketReceiver::set_complete_callback(std::function<void(const std::vector<std::any>& data_array)> callback) {
    state_->complete_callback = callback;
}

bool PacketReceiver::receive_text(const std::string& text) {
    reset();
    
    state_->current_text = text;
    
    // 检查是否是二进制包
    size_t pos = 0;
    if (text.empty() || !isdigit(text[0])) {
        return false;
    }
    
    int packet_type = text[0] - '0';
    
    // 检查是否是二进制包
    if (packet_type == static_cast<int>(PacketType::BINARY_EVENT) ||
        packet_type == static_cast<int>(PacketType::BINARY_ACK)) {
        // 需要二进制数据
        int binary_count = parse_binary_count(text);
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

bool PacketReceiver::receive_binary(const rtc::Buffer& binary) {
    if (!state_->expecting_binary) {
        return false;
    }
    
    state_->received_binaries.push_back(binary);
    
    // 检查是否已接收完所有预期的二进制数据
    if (state_->received_binaries.size() >= state_->expected_binaries.size()) {
        state_->expecting_binary = false;
        check_and_trigger_complete();
    }
    
    return true;
}

void PacketReceiver::check_and_trigger_complete() {
    if (state_->complete_callback &&
        (!state_->expecting_binary || state_->received_binaries.size() >= state_->expected_binaries.size())) {
        
        // 合并数据数组
        combine_to_data_array_async(
            state_->current_text,
            state_->received_binaries,
            state_->complete_callback
        );
    }
}

void PacketReceiver::reset() {
    state_->current_text.clear();
    state_->received_binaries.clear();
    state_->expected_binaries.clear();
    state_->expecting_binary = false;
}

} // namespace sio
