//
//  sio_packet_impl.cpp
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/19.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#include "sio_packet_impl.h"
#include "sio_packet_parser.h"
#include <atomic>
#include <future>
#include <iostream>

namespace sio {
// ============================================================================
// PacketSender 模板类实现
// ============================================================================

template <typename T>
PacketSender<T>::PacketSender(SocketIOVersion version)
    : version_(version), state_(new typename PacketSender<T>::SendState()) {
    reset();
    update_parser_config();
}

template <typename T>
PacketSender<T>::~PacketSender() {
    // unique_ptr会自动管理内存
}

template <typename T>
void PacketSender<T>::update_parser_config() {
    // 更新PacketParser的配置
    ParserConfig config = PacketParser::getInstance().getConfig();
    config.version = version_;
    PacketParser::getInstance().setConfig(config);
}

template <typename T>
void PacketSender<T>::prepare_data_array_async(
    const std::vector<T>& data_array,
    PacketType type,
    int nsp,
    int id,
    std::function<void()> on_complete) {
    
    // 保存当前的回调函数
    auto saved_text_callback = state_->text_callback;
    auto saved_binary_callback = state_->binary_callback;
    
    reset();
    
    // 恢复回调函数
    state_->text_callback = saved_text_callback;
    state_->binary_callback = saved_binary_callback;
    state_->on_complete = on_complete;
    
    // 根据版本选择发送方法
    if (static_cast<int>(version_) < 3) {
        // v2版本
        send_data_array_async_v2(
            data_array,
            [this](const std::string& text) -> bool {
                state_->text_queue.push(text);
                return true;
            },
            [this](const SmartBuffer& binary, int index) -> bool {
                state_->binary_queue.push(binary);
                return true;
            },
            nullptr,
            type,
            nsp,
            id
        );
    } else {
        // v3+版本
        send_data_array_async_v3(
            data_array,
            [this](const std::string& text) -> bool {
                state_->text_queue.push(text);
                return true;
            },
            [this](const SmartBuffer& binary, int index) -> bool {
                state_->binary_queue.push(binary);
                return true;
            },
            nullptr,
            type,
            nsp,
            id
        );
    }
    
    // 开始处理队列
    process_next_item();
}

template <typename T>
void PacketSender<T>::send_data_array_async(
    const std::vector<T>& data_array,
    std::function<bool(const std::string& text_packet)> text_callback,
    std::function<bool(const SmartBuffer& binary_data, int index)> binary_callback,
    std::function<void(bool success, const std::string& error)> complete_callback,
    PacketType type,
    int nsp,
    int id) {
    
    // 根据版本选择发送方法
    if (static_cast<int>(version_) < 3) {
        send_data_array_async_v2(
            data_array,
            text_callback,
            binary_callback,
            complete_callback,
            type,
            nsp,
            id
        );
    } else {
        send_data_array_async_v3(
            data_array,
            text_callback,
            binary_callback,
            complete_callback,
            type,
            nsp,
            id
        );
    }
}

template <typename T>
void PacketSender<T>::send_data_array_async_v2(
    const std::vector<T>& data_array,
    std::function<bool(const std::string& text_packet)> text_callback,
    std::function<bool(const SmartBuffer& binary_data, int index)> binary_callback,
    std::function<void(bool success, const std::string& error)> complete_callback,
    PacketType type,
    int nsp,
    int id) {
    
    if (!text_callback) {
        if (complete_callback) {
            complete_callback(false, "text_callback is required");
        }
        return;
    }
    
    // 保存当前配置
    ParserConfig original_config = PacketParser::getInstance().getConfig();
    
    // 临时设置为v2配置
    ParserConfig v2_config = original_config;
    v2_config.version = SocketIOVersion::V2;
    v2_config.allow_numeric_nsp = false;  // v2不支持数字命名空间
    PacketParser::getInstance().setConfig(v2_config);
    
    // 异步拆分数据数组
    PacketSplitter<T>::split_data_array_async(
        data_array,
        [=](const std::string& text_part) {
            // 创建Packet
            Packet packet;
            packet.type = type;
            packet.nsp = nsp;
            packet.id = id;
            
            // 使用 PacketParser 检测二进制数据数量
            int binary_count = PacketParser::getInstance().countBinaryPlaceholders(text_part);
            
            // 如果有二进制数据，需要更新包类型
            if (binary_count > 0) {
                if (packet.type == PacketType::EVENT) {
                    packet.type = PacketType::BINARY_EVENT;
                } else if (packet.type == PacketType::ACK) {
                    packet.type = PacketType::BINARY_ACK;
                }
            }
            
            packet.data = text_part;
            
            // 使用 PacketParser 构建包字符串（v2格式）
            BuildOptions options;
            options.namespace_str = PacketParser::getInstance().indexToNamespace(packet.nsp);
            options.include_binary_count = (binary_count > 0);
            
            std::string text_packet = PacketParser::getInstance().buildPacketString(packet, options);
            
            // 先发送文本包
            bool text_sent = text_callback(text_packet);
            if (!text_sent) {
                if (complete_callback) {
                    complete_callback(false, "Failed to send text packet");
                }
                return;
            }
            
            // 如果没有二进制数据，直接完成
            if (binary_count == 0 || !binary_callback) {
                if (complete_callback) {
                    complete_callback(true, "");
                }
                return;
            }
        },
        [=](const SmartBuffer& binary_part, size_t index) {
            // 异步发送二进制数据
            if (binary_callback) {
                bool binary_sent = binary_callback(binary_part, static_cast<int>(index));
                if (!binary_sent && complete_callback) {
                    complete_callback(false, "Failed to send binary data at index " + std::to_string(index));
                }
            }
        }
    );
    
    // 恢复原始配置
    PacketParser::getInstance().setConfig(original_config);
}

template <typename T>
void PacketSender<T>::send_data_array_async_v3(
    const std::vector<T>& data_array,
    std::function<bool(const std::string& text_packet)> text_callback,
    std::function<bool(const SmartBuffer& binary_data, int index)> binary_callback,
    std::function<void(bool success, const std::string& error)> complete_callback,
    PacketType type,
    int nsp,
    int id) {
    
    if (!text_callback) {
        if (complete_callback) {
            complete_callback(false, "text_callback is required");
        }
        return;
    }
    
    // 保存当前配置
    ParserConfig original_config = PacketParser::getInstance().getConfig();
    
    // 临时设置为v3配置
    ParserConfig v3_config = original_config;
    v3_config.version = SocketIOVersion::V3;
    v3_config.allow_numeric_nsp = true;  // v3支持数字命名空间
    PacketParser::getInstance().setConfig(v3_config);
    
    // 异步拆分数据数组
    PacketSplitter<T>::split_data_array_async(
        data_array,
        [=](const std::string& text_part) {
            // 创建Packet
            Packet packet;
            packet.type = type;
            packet.nsp = nsp;
            packet.id = id;
            
            // 使用 PacketParser 检测二进制数据数量
            int binary_count = PacketParser::getInstance().countBinaryPlaceholders(text_part);
            
            // 如果有二进制数据，需要更新包类型
            if (binary_count > 0) {
                if (packet.type == PacketType::EVENT) {
                    packet.type = PacketType::BINARY_EVENT;
                } else if (packet.type == PacketType::ACK) {
                    packet.type = PacketType::BINARY_ACK;
                }
            }
            
            packet.data = text_part;
            
            // 使用 PacketParser 构建包字符串（v3格式）
            BuildOptions options;
            options.namespace_str = PacketParser::getInstance().indexToNamespace(packet.nsp);
            options.include_binary_count = (binary_count > 0);
            
            std::string text_packet = PacketParser::getInstance().buildPacketString(packet, options);
            
            // 先发送文本包
            bool text_sent = text_callback(text_packet);
            if (!text_sent) {
                if (complete_callback) {
                    complete_callback(false, "Failed to send text packet");
                }
                return;
            }
            
            // 如果没有二进制数据，直接完成
            if (binary_count == 0 || !binary_callback) {
                if (complete_callback) {
                    complete_callback(true, "");
                }
                return;
            }
        },
        [=](const SmartBuffer& binary_part, size_t index) {
            // 异步发送二进制数据
            if (binary_callback) {
                bool binary_sent = binary_callback(binary_part, static_cast<int>(index));
                if (!binary_sent && complete_callback) {
                    complete_callback(false, "Failed to send binary data at index " + std::to_string(index));
                }
            }
        }
    );
    
    // 恢复原始配置
    PacketParser::getInstance().setConfig(original_config);
}

template <typename T>
void PacketSender<T>::send_data_array_async(
    const std::vector<T>& data_array,
    SocketIOSender* sender,
    PacketType type,
    int nsp,
    int id) {
    
    if (!sender) {
        return;
    }
    
    // 使用发送器的版本
    SocketIOVersion sender_version = sender->get_supported_version();
    SocketIOVersion original_version = version_;
    
    // 临时设置版本
    if (sender_version != version_) {
        set_version(sender_version);
    }
    
    // 使用lambda回调的版本
    send_data_array_async(
        data_array,
        [sender](const std::string& text_packet) {
            return sender->send_text(text_packet);
        },
        [sender](const SmartBuffer& binary_data, int index) {
            return sender->send_binary(binary_data);
        },
        [sender](bool success, const std::string& error) {
            sender->on_send_complete(success, error);
        },
        type,
        nsp,
        id
    );
    
    // 恢复原始版本
    if (sender_version != original_version) {
        set_version(original_version);
    }
}

template <typename T>
void PacketSender<T>::set_text_callback(std::function<void(const std::string& text)> callback) {
    state_->text_callback = callback;
}

template <typename T>
void PacketSender<T>::set_binary_callback(std::function<void(const SmartBuffer& binary)> callback) {
    state_->binary_callback = callback;
}

template <typename T>
void PacketSender<T>::process_next_item() {
    // 处理所有待发送的文本和二进制数据
    while (!state_->text_queue.empty() || !state_->binary_queue.empty()) {
        // 先发送所有文本
        while (!state_->text_queue.empty()) {
            std::string text = state_->text_queue.front();
            state_->text_queue.pop();
            
            if (state_->text_callback) {
                state_->text_callback(text);
            }
            
            // 如果有二进制数据需要发送，设置标志
            state_->expecting_binary = !state_->binary_queue.empty();
        }
        
        // 然后发送所有二进制
        while (!state_->binary_queue.empty()) {
            // 获取二进制数据
            SmartBuffer binary = state_->binary_queue.front();
            state_->binary_queue.pop();
            
            if (state_->binary_callback) {
                state_->binary_callback(binary);
            }
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
    state_->binary_queue = std::queue<SmartBuffer>();
    state_->expecting_binary = false;
    state_->text_callback = nullptr;
    state_->binary_callback = nullptr;
    state_->on_complete = nullptr;
}

// ============================================================================
// PacketReceiver 模板类实现
// ============================================================================

template <typename T>
PacketReceiver<T>::PacketReceiver(SocketIOVersion version)
    : version_(version), state_(new typename PacketReceiver<T>::ReceiveState()) {
    reset();
    update_parser_config();
}

template <typename T>
PacketReceiver<T>::~PacketReceiver() {
    // unique_ptr会自动管理内存
}

template <typename T>
void PacketReceiver<T>::update_parser_config() {
    // 更新PacketParser的配置
    ParserConfig config = PacketParser::getInstance().getConfig();
    config.version = version_;
    PacketParser::getInstance().setConfig(config);
}

template <typename T>
void PacketReceiver<T>::set_complete_callback(std::function<void(const std::vector<T>& data_array)> callback) {
    state_->complete_callback = callback;
}

template <typename T>
bool PacketReceiver<T>::receive_text(const std::string& text) {
    reset();
    
    state_->current_text = text;
    
    // 自动检测版本
    SocketIOVersion detected_version = PacketParser::getInstance().detectVersion(text);
    state_->packet_version = detected_version;
    
    // 保存当前配置
    ParserConfig original_config = PacketParser::getInstance().getConfig();
    
    // 临时设置检测到的版本
    ParserConfig temp_config = original_config;
    temp_config.version = detected_version;
    temp_config.allow_numeric_nsp = (static_cast<int>(detected_version) >= 3);
    PacketParser::getInstance().setConfig(temp_config);
    
    // 使用 PacketParser 进行完整解析
    auto parse_result = PacketParser::getInstance().parsePacket(text);
    
    // 恢复原始配置
    PacketParser::getInstance().setConfig(original_config);
    
    if (!parse_result.success) {
        return false;
    }
    
    // 存储解析结果
    state_->packet_info = parse_result.packet;
    state_->namespace_str = parse_result.namespace_str;
    
    // 检查是否是二进制包
    if (parse_result.is_binary_packet) {
        // 需要二进制数据
        int binary_count = parse_result.binary_count;
        
        // 如果binary_count为0，从JSON数据中计算占位符数量
        if (binary_count == 0 && !parse_result.json_data.empty()) {
            // 简单实现：统计JSON数据中的"_placeholder":true出现次数
            size_t pos = 0;
            std::string placeholder = "\"_placeholder\":true";
            while ((pos = parse_result.json_data.find(placeholder, pos)) != std::string::npos) {
                binary_count++;
                pos += placeholder.length();
            }
        }
        
        // 设置预期的二进制数据数量
        state_->expected_binaries.resize(binary_count);
        state_->received_binaries.clear();
        state_->received_binaries.reserve(binary_count);
        state_->expecting_binary = (binary_count > 0);
        
        if (binary_count == 0) {
            // 虽然是二进制包类型，但没有实际的二进制数据
            state_->expecting_binary = false;
            check_and_trigger_complete();
        }
    } else {
        // 普通包，已经完整
        state_->expecting_binary = false;
        check_and_trigger_complete();
    }
    
    return true;
}

template <typename T>
bool PacketReceiver<T>::receive_text_with_version(const std::string& text, SocketIOVersion version) {
    reset();
    
    state_->current_text = text;
    state_->packet_version = version;
    
    // 保存当前配置
    ParserConfig original_config = PacketParser::getInstance().getConfig();
    
    // 临时设置指定版本
    ParserConfig temp_config = original_config;
    temp_config.version = version;
    temp_config.allow_numeric_nsp = (static_cast<int>(version) >= 3);
    PacketParser::getInstance().setConfig(temp_config);
    
    // 使用 PacketParser 进行完整解析
    auto parse_result = PacketParser::getInstance().parsePacket(text);
    
    // 恢复原始配置
    PacketParser::getInstance().setConfig(original_config);
    
    if (!parse_result.success) {
        return false;
    }
    
    // 存储解析结果
    state_->packet_info = parse_result.packet;
    state_->namespace_str = parse_result.namespace_str;
    
    // 检查是否是二进制包
    if (parse_result.is_binary_packet) {
        // 需要二进制数据
        state_->expected_binaries.resize(parse_result.binary_count);
        state_->received_binaries.clear();
        state_->received_binaries.reserve(parse_result.binary_count);
        state_->expecting_binary = (parse_result.binary_count > 0);
        
        if (parse_result.binary_count == 0) {
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
bool PacketReceiver<T>::receive_binary(const SmartBuffer& binary) {
    if (!state_->expecting_binary) {
        return false;
    }
    
    // SmartBuffer已经使用智能指针，直接使用
    state_->received_binaries.push_back(binary);
    
    // 检查是否已接收完所有预期的二进制数据
    if (state_->received_binaries.size() == state_->expected_binaries.size()) {
        state_->expecting_binary = false;
        check_and_trigger_complete();
    }
    
    return true;
}

template <typename T>
void PacketReceiver<T>::check_and_trigger_complete() {
    if (state_->complete_callback) {
        bool should_complete = false;
        
        if (state_->packet_info.type == PacketType::BINARY_EVENT || state_->packet_info.type == PacketType::BINARY_ACK) {
            // 二进制包：只有当所有二进制数据都接收完成时才合并
            should_complete = !state_->expecting_binary && 
                             state_->received_binaries.size() == state_->expected_binaries.size();
        } else {
            // 普通包：直接合并
            should_complete = true;
        }
        
        if (should_complete) {
            // 如果当前文本为空，说明没有数据
            if (state_->current_text.empty()) {
                state_->complete_callback(std::vector<T>());
                return;
            }
            
            // 临时设置包版本进行解析
            ParserConfig original_config = PacketParser::getInstance().getConfig();
            
            ParserConfig temp_config = original_config;
            temp_config.version = state_->packet_version;
            temp_config.allow_numeric_nsp = (static_cast<int>(state_->packet_version) >= 3);
            PacketParser::getInstance().setConfig(temp_config);
            
            // 解析数据包以获取JSON数据
            auto parse_result = PacketParser::getInstance().parsePacket(state_->current_text);
            
            // 恢复原始配置
            PacketParser::getInstance().setConfig(original_config);
            
            if (!parse_result.success || parse_result.json_data.empty()) {
                // 如果解析失败或没有JSON数据，返回空数组
                state_->complete_callback(std::vector<T>());
                return;
            }
            
            // 合并数据数组
            PacketSplitter<T>::combine_to_data_array_async(
                parse_result.json_data,
                state_->received_binaries,
                state_->complete_callback
            );
        }
    }
}

template <typename T>
void PacketReceiver<T>::reset() {
    state_->current_text.clear();
    state_->received_binaries.clear();
    state_->expected_binaries.clear();
    state_->expecting_binary = false;
    state_->packet_info = Packet();
    state_->namespace_str.clear();
    state_->packet_version = version_;
}

// ============================================================================
// 显式特化常用类型
// ============================================================================

// Json::Value 类型的特化实现

template <>
Json::Value PacketSplitter<Json::Value>::data_to_json(
    const Json::Value& value,
    std::function<void(const SmartBuffer& binary_part, size_t index)> binary_callback,
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
            json_array.append(data_to_json(value[i],
                                         binary_callback,
                                         placeholder_counter));
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
            SmartBuffer buffer(binary_str.data(), binary_str.size());
            
            // 调用二进制回调
            if (binary_callback) {
                binary_callback(buffer, placeholder_counter);
            }
            
            // 创建占位符
            Json::Value placeholder = create_placeholder(placeholder_counter);
            placeholder_counter++;
            return placeholder;
        }
        
        // 检查是否是二进制数据对象 - 格式2：_binary_data + _buffer_ptr
        if (value.isMember("_binary_data") && value["_binary_data"].isBool() &&
            value["_binary_data"].asBool() && value.isMember("_buffer_ptr")) {
            
            // 获取二进制数据智能指针
            auto buffer_ptr = sio::binary_helper::get_binary_shared_ptr(value);
            
            if (buffer_ptr) {
                // 创建SmartBuffer
                SmartBuffer buffer(buffer_ptr);
                
                // 调用二进制回调
                if (binary_callback) {
                    binary_callback(buffer, placeholder_counter);
                }
                
                // 创建占位符
                Json::Value placeholder = create_placeholder(placeholder_counter);
                placeholder_counter++;
                return placeholder;
            }
        }
        
        // 检查是否是二进制数据对象 - 使用binary_helper
        if (sio::binary_helper::is_binary(value)) {
            // 获取二进制数据智能指针
            auto buffer_ptr = sio::binary_helper::get_binary_shared_ptr(value);
            
            if (buffer_ptr) {
                // 创建SmartBuffer
                SmartBuffer buffer(buffer_ptr);
                
                // 调用二进制回调
                if (binary_callback) {
                    binary_callback(buffer, placeholder_counter);
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
            json_obj[key] = data_to_json(value[key],
                                       binary_callback,
                                       placeholder_counter);
        }
        return json_obj;
    }
    
    return Json::Value(Json::nullValue);
}

template <>
Json::Value PacketSplitter<Json::Value>::json_to_data(
    const Json::Value& json,
    const std::vector<SmartBuffer>& binaries) {
    
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
                const SmartBuffer& buffer = binaries[index];
                
                // 使用binary_helper创建包含二进制数据的对象
                if (!buffer.empty()) {
                    return sio::binary_helper::create_binary_value(buffer.buffer());
                }
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

// ============================================================================
// 多版本兼容的发送器实现
// ============================================================================

class VersionAwareSocketIOSender : public SocketIOSender {
public:
    VersionAwareSocketIOSender(
        SocketIOVersion version,
        std::function<bool(const std::string&)> text_handler,
        std::function<bool(const SmartBuffer&)> binary_handler,
        std::function<void(bool, const std::string&)> complete_handler)
        : version_(version),
          text_handler_(text_handler),
          binary_handler_(binary_handler),
          complete_handler_(complete_handler),
          binary_index_(0),
          total_binaries_(0) {}
    
    // 获取支持的Socket.IO版本
    SocketIOVersion get_supported_version() const override {
        return version_;
    }
    
    // 发送文本包
    bool send_text(const std::string& text_packet) override {
        if (text_handler_) {
            return text_handler_(text_packet);
        }
        return false;
    }
    
    // 发送二进制数据
    bool send_binary(const SmartBuffer& binary_data) override {
        if (binary_handler_) {
            bool result = binary_handler_(binary_data);
            binary_index_++;
            
            // 检查是否所有二进制数据都已发送
            if (binary_index_ >= total_binaries_) {
                if (complete_handler_) {
                    complete_handler_(true, "");
                }
            }
            return result;
        }
        return false;
    }
    
    // 发送完成回调
    void on_send_complete(bool success, const std::string& error) override {
        if (complete_handler_) {
            complete_handler_(success, error);
        }
    }
    
    // 设置二进制数据数量
    void set_total_binaries(int count) {
        total_binaries_ = count;
    }
    
private:
    SocketIOVersion version_;
    std::function<bool(const std::string&)> text_handler_;
    std::function<bool(const SmartBuffer&)> binary_handler_;
    std::function<void(bool, const std::string&)> complete_handler_;
    std::atomic<int> binary_index_;
    int total_binaries_;
};

// 显式实例化
template class PacketSender<Json::Value>;
template class PacketReceiver<Json::Value>;

} // namespace sio
