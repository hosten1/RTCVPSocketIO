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
#include <chrono>

#include "absl/memory/memory.h"

namespace sio {
// ============================================================================
// AckManager 类实现
// ============================================================================

AckManager::AckManager(std::shared_ptr<rtc::TaskQueue> task_queue):
            task_queue_(task_queue),
            next_ack_id_(0),
            default_timeout_(std::chrono::milliseconds(30000)) {
}

AckManager::~AckManager() {
    clear_all_acks();
}

int AckManager::generate_ack_id() {
    // 生成唯一的ACK ID，原子操作确保线程安全
    return next_ack_id_++;
}

void AckManager::register_ack_callback(int ack_id, AckCallback callback, std::chrono::milliseconds timeout) {
    webrtc::MutexLock lock(&mutex_);
    
    // 创建ACK信息
    auto ack_info = absl::make_unique<AckInfo>();
    ack_info->callback = callback;
    ack_info->timeout = timeout;
    ack_info->start_time = std::chrono::steady_clock::now();
    
    // 保存ACK信息
    acks_[ack_id] = std::move(ack_info);
    
    // 如果超时时间大于0，启动超时检查
    if (timeout > std::chrono::milliseconds(0)) {
        // 使用线程池或当前线程进行超时检查
        task_queue_->PostDelayedTask(
            [this, ack_id]() {
                // 超时检查
                webrtc::MutexLock lock(&mutex_);
                auto it = acks_.find(ack_id);
                if (it != acks_.end()) {
                    // 检查是否真的超时
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second->start_time);
                    if (elapsed >= it->second->timeout) {
                        // 调用超时回调
                        if (it->second->timeout_callback) {
                            // 在超时线程中执行回调
                            AckTimeoutCallback callback = it->second->timeout_callback;
                            task_queue_->PostTask(
                                [callback]() {
                                    if (callback) {
                                        callback();
                                    }
                                }
                            );
                        }
                        // 移除ACK
                        acks_.erase(it);
                    }
                }
            },
            static_cast<uint32_t>(timeout.count())
        );
    }
}

void AckManager::register_ack_timeout_callback(int ack_id, AckTimeoutCallback callback) {
    webrtc::MutexLock lock(&mutex_);
    
    auto it = acks_.find(ack_id);
    if (it != acks_.end()) {
        it->second->timeout_callback = callback;
    }
}

bool AckManager::handle_ack_response(int ack_id, const std::vector<Json::Value>& data_array) {
    webrtc::MutexLock lock(&mutex_);
    
    auto it = acks_.find(ack_id);
    if (it == acks_.end()) {
        // ACK ID 不存在或已超时
        return false;
    }
    
    // 保存回调
    AckCallback callback = it->second->callback;
    
    // 移除ACK
    acks_.erase(it);

    // 在当前线程中执行回调
    if (callback) {
        callback(data_array);
    }
    
    return true;
}

bool AckManager::cancel_ack(int ack_id) {
    webrtc::MutexLock lock(&mutex_);
    
    auto it = acks_.find(ack_id);
    if (it == acks_.end()) {
        return false;
    }
    
    // 移除ACK
    acks_.erase(it);
    return true;
}

void AckManager::set_default_timeout(std::chrono::milliseconds timeout) {
    webrtc::MutexLock lock(&mutex_);
    default_timeout_ = timeout;
}

std::chrono::milliseconds AckManager::get_default_timeout() const {
    webrtc::MutexLock lock(&mutex_);
    return default_timeout_;
}

void AckManager::clear_all_acks() {
    webrtc::MutexLock lock(&mutex_);
    acks_.clear();
}

// ============================================================================
// PacketSender 模板类实现
// ============================================================================

template <typename T>
PacketSender<T>::PacketSender(webrtc::TaskQueueFactory* task_queue_factory, SocketIOVersion version)
    : task_queue_factory_(task_queue_factory),
      task_queue_(absl::make_unique<rtc::TaskQueue>(
          task_queue_factory_->CreateTaskQueue("packet_sender", webrtc::TaskQueueFactory::Priority::NORMAL))),
      version_(version),
      ack_manager_(task_queue_) {  // 使用task_queue_初始化ack_manager_
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
void PacketSender<T>::send_data_async(
    const std::vector<T>& data_array,
    std::function<bool(const std::string& text_packet)> text_callback,
    std::function<bool(const SmartBuffer& binary_data, int index)> binary_callback,
    std::function<void(bool success, const std::string& error)> complete_callback,
    std::function<void(const std::vector<T>& data_array)> ack_callback,
    std::function<void()> timeout_callback,
    std::chrono::milliseconds timeout,
    PacketType type,
    int nsp,
    int id) {
    // 实现异步发送数据的逻辑
    // 这里简化实现，直接调用text_callback和binary_callback
    // 实际实现应该包含数据拆分、异步发送等逻辑
    
    if (!text_callback) {
        if (complete_callback) {
            complete_callback(false, "text_callback is required");
        }
        return;
    }
    
    // 简化实现：直接创建一个简单的文本包
    std::string text_packet = std::to_string(static_cast<int>(type)) + std::to_string(nsp) + std::to_string(id) + "[" + data_array[0].toStyledString() + "]";
    
    bool text_sent = text_callback(text_packet);
    if (!text_sent) {
        if (complete_callback) {
            complete_callback(false, "Failed to send text packet");
        }
        return;
    }
    
    if (complete_callback) {
        complete_callback(true, "");
    }
}

// ============================================================================
// PacketReceiver 模板类实现
// ============================================================================

template <typename T>
PacketReceiver<T>::PacketReceiver(webrtc::TaskQueueFactory* task_queue_factory, SocketIOVersion version)
    : task_queue_factory_(task_queue_factory),
      task_queue_(absl::make_unique<rtc::TaskQueue>(
          task_queue_factory_->CreateTaskQueue("packet_receiver", webrtc::TaskQueueFactory::Priority::NORMAL))),
      version_(version),
      state_(new typename PacketReceiver<T>::ReceiveState()),
      ack_manager_(nullptr) {  // 初始化为nullptr
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
void PacketReceiver<T>::set_ack_manager(AckManager* ack_manager) {
    ack_manager_ = ack_manager;
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
                [this](const std::vector<T>& data_array) {
                    // 检查是否是ACK包
                    if ((state_->packet_info.type == PacketType::ACK ||
                         state_->packet_info.type == PacketType::BINARY_ACK) &&
                        state_->packet_info.id != -1 &&
                        ack_manager_) {
                        
                        // 转换为Json::Value数组
                        std::vector<Json::Value> json_array;
                        for (const auto& data : data_array) {
                            json_array.push_back(data);
                        }
                        
                        // 处理ACK响应
                        ack_manager_->handle_ack_response(state_->packet_info.id, json_array);
                    }
                    
                    // 调用原始回调
                    state_->complete_callback(data_array);
                }
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

// SocketIOSender 类的定义被移除，因为它不是测试程序的核心部分
// VersionAwareSocketIOSender 类也被移除，因为它依赖于未定义的 SocketIOSender 类

// 显式实例化
template class PacketSender<Json::Value>;
template class PacketReceiver<Json::Value>;

} // namespace sio
