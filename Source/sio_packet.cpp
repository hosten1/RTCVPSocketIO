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

// 判断数据数组中是否包含二进制数据
bool contains_binary(const std::vector<std::any>& data_array) {
    for (const auto& item : data_array) {
        if (item.type() == typeid(rtc::Buffer)) {
            return true;
        } else if (item.type() == typeid(std::vector<std::any>)) {
            const auto& nested_array = std::any_cast<const std::vector<std::any>&>(item);
            if (contains_binary(nested_array)) {
                return true;
            }
        } else if (item.type() == typeid(std::map<std::string, std::any>)) {
            // 对于对象，需要递归检查所有值
            const auto& obj = std::any_cast<const std::map<std::string, std::any>&>(item);
            for (const auto& [key, value] : obj) {
                if (value.type() == typeid(rtc::Buffer)) {
                    return true;
                } else if (value.type() == typeid(std::vector<std::any>)) {
                    const auto& nested_array = std::any_cast<const std::vector<std::any>&>(value);
                    if (contains_binary(nested_array)) {
                        return true;
                    }
                } else if (value.type() == typeid(std::map<std::string, std::any>)) {
                    // 递归检查嵌套对象
                    const auto& nested_obj = std::any_cast<const std::map<std::string, std::any>&>(value);
                    std::vector<std::any> nested_values;
                    for (const auto& [k, v] : nested_obj) {
                        nested_values.push_back(v);
                    }
                    if (contains_binary(nested_values)) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
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

// 将 std::any 转换为 JSON，提取二进制数据并替换为占位符
Json::Value PacketSplitter::any_to_json(const std::any& value,
                                       std::vector<rtc::Buffer>& binaries,
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
        // 二进制数据：提取并创建占位符
        const rtc::Buffer& buffer = std::any_cast<const rtc::Buffer&>(value);
        binaries.push_back(buffer);
        
        Json::Value placeholder = create_placeholder(placeholder_counter);
        placeholder_counter++;
        return placeholder;
    } else if (value.type() == typeid(std::vector<std::any>)) {
        // 数组类型
        const std::vector<std::any>& array = std::any_cast<const std::vector<std::any>&>(value);
        Json::Value json_array(Json::arrayValue);
        for (const auto& item : array) {
            json_array.append(any_to_json(item, binaries, placeholder_counter));
        }
        return json_array;
    } else if (value.type() == typeid(std::map<std::string, std::any>)) {
        // 对象类型
        const std::map<std::string, std::any>& obj = std::any_cast<const std::map<std::string, std::any>&>(value);
        Json::Value json_obj(Json::objectValue);
        for (const auto& [key, val] : obj) {
            json_obj[key] = any_to_json(val, binaries, placeholder_counter);
        }
        return json_obj;
    } else if (value.type() == typeid(nullptr)) {
        return Json::Value(Json::nullValue);
    }
    
    // 未知类型，返回空值
    return Json::Value(Json::nullValue);
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

// 将数据数组转换为JSON数组，并提取二进制数据
Json::Value PacketSplitter::convert_to_json_with_placeholders(
    const std::vector<std::any>& data_array,
    std::vector<rtc::Buffer>& binaries,
    int& placeholder_counter) {
    
    Json::Value json_array(Json::arrayValue);
    for (const auto& item : data_array) {
        json_array.append(any_to_json(item, binaries, placeholder_counter));
    }
    return json_array;
}

// 将JSON数组转换为数据数组，替换占位符为二进制数据
std::vector<std::any> PacketSplitter::convert_json_to_data_array(
    const Json::Value& json_array,
    const std::vector<rtc::Buffer>& binaries) {
    
    std::vector<std::any> data_array;
    
    if (!json_array.isArray()) {
        // 如果不是数组，返回包含单个元素的数组
        data_array.push_back(json_to_any(json_array, binaries));
        return data_array;
    }
    
    for (Json::ArrayIndex i = 0; i < json_array.size(); i++) {
        data_array.push_back(json_to_any(json_array[i], binaries));
    }
    
    return data_array;
}

// 核心接口1: 拆分 - 输入一个数据数组，输出拆分结果
PacketSplitter::SplitResult PacketSplitter::split_data_array(const std::vector<std::any>& data_array) {
    SplitResult result;
    
    if (data_array.empty()) {
        Json::FastWriter writer;
        result.text_part = writer.write(Json::Value(Json::arrayValue));
        return result;
    }
    
    // 检查是否包含二进制数据
    if (!contains_binary(data_array)) {
        // 不包含二进制数据，直接序列化
        int placeholder_counter = 0;
        Json::Value json_array = convert_to_json_with_placeholders(data_array, result.binary_parts, placeholder_counter);
        Json::FastWriter writer;
        result.text_part = writer.write(json_array);
        return result;
    }
    
    // 包含二进制数据，提取二进制并创建占位符
    int placeholder_counter = 0;
    Json::Value json_array = convert_to_json_with_placeholders(data_array, result.binary_parts, placeholder_counter);
    
    // 序列化JSON数组
    Json::FastWriter writer;
    result.text_part = writer.write(json_array);
    
    return result;
}

// 核心接口2: 合并 - 输入文本部分和二进制部分，输出数据数组
std::vector<std::any> PacketSplitter::combine_to_data_array(
    const std::string& text_part,
    const std::vector<rtc::Buffer>& binary_parts) {
    
    Json::Value json_root;
    Json::Reader reader;
    
    if (!reader.parse(text_part, json_root)) {
        return std::vector<std::any>();
    }
    
    // 将JSON数组转换为数据数组
    return convert_json_to_data_array(json_root, binary_parts);
}

// PacketSender实现
PacketSender::PacketSender() : state_(new PacketSender::SendState()) {
    reset();
}

PacketSender::~PacketSender() {
    // unique_ptr会自动管理内存
}

void PacketSender::prepare_data_array(const std::vector<std::any>& data_array,
                                     PacketType type,
                                     int nsp,
                                     int id) {
    reset();
    
    // 拆分数据数组
    auto split_result = PacketSplitter::split_data_array(data_array);
    
    // 创建Packet
    Packet packet;
    packet.type = type;
    packet.nsp = nsp;
    packet.id = id;
    
    // 如果有二进制数据，需要更新包类型
    if (!split_result.binary_parts.empty()) {
        if (packet.type == PacketType::EVENT) {
            packet.type = PacketType::BINARY_EVENT;
        } else if (packet.type == PacketType::ACK) {
            packet.type = PacketType::BINARY_ACK;
        }
    }
    
    packet.data = split_result.text_part;
    packet.attachments = split_result.binary_parts;
    
    // 处理发送队列
    if (!packet.has_binary()) {
        // 没有二进制数据，直接发送
        std::string encoded = encode_packet(packet);
        state_->text_queue.push(encoded);
        state_->expecting_binary = false;
    } else {
        // 有二进制数据
        state_->text_queue.push(encode_packet(packet));
        
        // 二进制部分
        for (const auto& binary : split_result.binary_parts) {
            state_->binary_queue.push(binary);
        }
        
        state_->current_attachments = split_result.binary_parts;
        state_->expecting_binary = !split_result.binary_parts.empty();
    }
}

bool PacketSender::has_text_to_send() const {
    return !state_->text_queue.empty();
}

bool PacketSender::get_next_text(std::string& text) {
    if (state_->text_queue.empty()) {
        return false;
    }
    
    text = state_->text_queue.front();
    state_->text_queue.pop();
    return true;
}

bool PacketSender::has_binary_to_send() const {
    return !state_->binary_queue.empty();
}

bool PacketSender::get_next_binary(rtc::Buffer& binary) {
    if (state_->binary_queue.empty()) {
        return false;
    }
    
    binary = state_->binary_queue.front();
    state_->binary_queue.pop();
    return true;
}

void PacketSender::reset() {
    state_->text_queue = std::queue<std::string>();
    state_->binary_queue = std::queue<rtc::Buffer>();
    state_->current_attachments.clear();
    state_->expecting_binary = false;
}

// PacketReceiver实现
PacketReceiver::PacketReceiver() : state_(new PacketReceiver::ReceiveState()) {
    reset();
}

PacketReceiver::~PacketReceiver() {
    // unique_ptr会自动管理内存
}

int PacketReceiver::parse_binary_count(const std::string& text) {
    return count_placeholders(text);
}

bool PacketReceiver::receive_text(const std::string& text) {
    reset();  // 每次接收新文本都重置状态
    
    state_->current_text = text;
    state_->expecting_binary = false;
    state_->has_complete = false;
    
    // 解析包类型
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
            state_->has_complete = true;
        }
    } else {
        // 普通包，已经完整
        state_->has_complete = true;
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
        state_->has_complete = true;
    }
    
    return true;
}

bool PacketReceiver::has_complete_packet() const {
    return state_->has_complete;
}

bool PacketReceiver::get_complete_data_array(std::vector<std::any>& data_array) {
    if (!state_->has_complete) {
        return false;
    }
    
    // 组合数据数组
    data_array = PacketSplitter::combine_to_data_array(state_->current_text, state_->received_binaries);
    reset();
    
    return true;
}

void PacketReceiver::reset() {
    state_->current_text.clear();
    state_->received_binaries.clear();
    state_->expected_binaries.clear();
    state_->expecting_binary = false;
    state_->has_complete = false;
}

} // namespace sio
