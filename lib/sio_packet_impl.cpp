//
//  sio_packet_impl.cpp
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/19.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#include "sio_packet_impl.h"

namespace sio {
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
                                                  state_->text_queue
                                                      .push(encoded);
                                              },
                                              [this](const rtc::Buffer& binary_part,
                                                     size_t index) {
                                                         // 创建数据的拷贝
                                                         rtc::Buffer buffer_copy;
                                                         buffer_copy
                                                             .SetData(binary_part.data(),
                                                                      binary_part
                                                                      .size());
                                                         state_->binary_queue
                                                             .push(std::move(buffer_copy));
                                                     }
                                              );
    
    // 开始处理队列
    process_next_item();
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
            rtc::Buffer binary = std::move(state_->binary_queue.front());
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
        
        // 1. 解析数据包
        auto parse_result = PacketParser::parsePacket(state_->current_text);
        if (!parse_result.success || parse_result.json_data.empty()) {
            state_->complete_callback(std::vector<T>());
            return;
        }
        
        // 2. 解析JSON数据
        Json::Value json_root;
        Json::Reader reader;
        
        if (!reader.parse(parse_result.json_data, json_root)) {
            state_->complete_callback(std::vector<T>());
            return;
        }
        
        // 3. 检查JSON是否为数组
        if (!json_root.isArray()) {
            // 如果不是数组，包装成数组
            Json::Value array(Json::arrayValue);
            array.append(json_root);
            json_root = array;
        }
        
        // 4. 将JSON数组序列化为字符串（供后续处理）
        Json::FastWriter writer;
        std::string json_str = writer.write(json_root);
        
        // 5. 合并数据数组
        PacketSplitter<T>::combine_to_data_array_async(
                                                       json_str,
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
            json_array
                .append(data_to_json(value[i],
                                     binary_callback,
                                     placeholder_counter));
        }
        return json_array;
    }
    
    // 处理对象
    if (value.isObject()) {
        // 检查是否是二进制数据对象 - 格式1：_is_binary + data
        if (value.isMember("_is_binary") && value["_is_binary"].isBool() &&
            value["_is_binary"]
            .asBool() && value
            .isMember("data") && value["data"]
            .isString()) {
            
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
        
        // 检查是否是二进制数据对象 - 格式2：_binary_data + _buffer_ptr
        if (value.isMember("_binary_data") && value["_binary_data"].isBool() &&
            value["_binary_data"].asBool() && value.isMember("_buffer_ptr")) {
            
            // 获取二进制数据指针
            uint64_t buffer_ptr_val = value["_buffer_ptr"].asUInt64();
            rtc::Buffer* buffer_ptr = reinterpret_cast<rtc::Buffer*>(buffer_ptr_val);
            
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
template class PacketSender<Json::Value>;
template class PacketReceiver<Json::Value>;
} //namespace sio end
