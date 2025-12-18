//
//  sio_packet.cpp
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/18.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

// sio_packet.cpp
#include "sio_packet.h"
#include "rtc_base/strings/json.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

// 如果WebRTC不可用，提供简单的Buffer实现
#if !defined(USE_WEBRTC_BUFFER)
namespace rtc {
    class ByteBufferWriter {
    public:
        void WriteUInt8(uint8_t value) { data_.push_back(value); }
        void WriteUInt16(uint16_t value) {
            data_.push_back(value >> 8);
            data_.push_back(value & 0xFF);
        }
        void WriteUInt32(uint32_t value) {
            data_.push_back((value >> 24) & 0xFF);
            data_.push_back((value >> 16) & 0xFF);
            data_.push_back((value >> 8) & 0xFF);
            data_.push_back(value & 0xFF);
        }
        void WriteString(const std::string& str) {
            WriteUInt32(static_cast<uint32_t>(str.size()));
            data_.insert(data_.end(), str.begin(), str.end());
        }
        void WriteBytes(const uint8_t* bytes, size_t size) {
            WriteUInt32(static_cast<uint32_t>(size));
            data_.insert(data_.end(), bytes, bytes + size);
        }
        const std::vector<uint8_t>& data() const { return data_; }
        
    private:
        std::vector<uint8_t> data_;
    };
    
    class ByteBufferReader {
    public:
        ByteBufferReader(const uint8_t* data, size_t size)
            : data_(data), size_(size), pos_(0) {}
            
        bool ReadUInt8(uint8_t* value) {
            if (pos_ >= size_) return false;
            *value = data_[pos_++];
            return true;
        }
        
        bool ReadUInt16(uint16_t* value) {
            if (pos_ + 2 > size_) return false;
            *value = (data_[pos_] << 8) | data_[pos_ + 1];
            pos_ += 2;
            return true;
        }
        
        bool ReadUInt32(uint32_t* value) {
            if (pos_ + 4 > size_) return false;
            *value = (data_[pos_] << 24) | (data_[pos_ + 1] << 16) |
                     (data_[pos_ + 2] << 8) | data_[pos_ + 3];
            pos_ += 4;
            return true;
        }
        
        bool ReadString(std::string* value) {
            uint32_t len;
            if (!ReadUInt32(&len)) return false;
            if (pos_ + len > size_) return false;
            value->assign(reinterpret_cast<const char*>(data_ + pos_), len);
            pos_ += len;
            return true;
        }
        
        bool ReadBytes(std::vector<uint8_t>* value) {
            uint32_t len;
            if (!ReadUInt32(&len)) return false;
            if (pos_ + len > size_) return false;
            value->assign(data_ + pos_, data_ + pos_ + len);
            pos_ += len;
            return true;
        }
        
    private:
        const uint8_t* data_;
        size_t size_;
        size_t pos_;
    };
}
#endif

using json = nlohmann::json;

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
    
    // 解析包头部
    bool decode_packet_header(const std::string& encoded, Packet& packet) {
        if (encoded.empty()) return false;
        
        size_t pos = 0;
        
        // 解析类型
        if (!isdigit(encoded[pos])) return false;
        packet.type = static_cast<PacketType>(encoded[pos] - '0');
        pos++;
        
        // 如果类型是二进制事件或二进制ACK，需要特殊处理
        if (packet.type == PacketType::BINARY_EVENT ||
            packet.type == PacketType::BINARY_ACK) {
            // 这里会处理"_placeholder"标记
            return true;
        }
        
        // 解析命名空间
        if (pos < encoded.size() && encoded[pos] == '/') {
            // 有命名空间
            size_t nsp_end = encoded.find(',', pos);
            if (nsp_end == std::string::npos) {
                packet.nsp = 0;  // 使用默认值
            } else {
                // 解析命名空间索引（简化处理）
                packet.nsp = 1;  // 非默认命名空间
                pos = nsp_end + 1;
            }
        }
        
        // 解析包ID
        if (pos < encoded.size() && isdigit(encoded[pos])) {
            size_t id_end = encoded.find(',', pos);
            if (id_end == std::string::npos) {
                packet.id = std::stoi(encoded.substr(pos));
                pos = encoded.size();
            } else {
                packet.id = std::stoi(encoded.substr(pos, id_end - pos));
                pos = id_end + 1;
            }
        } else {
            packet.id = -1;
        }
        
        // 剩余部分是数据
        if (pos < encoded.size()) {
            packet.data = encoded.substr(pos);
        }
        
        return true;
    }
    
    // 在JSON数据中查找和提取二进制数据
    void extract_binaries_from_json(json& j, std::vector<std::vector<uint8_t>>& binaries) {
        if (j.is_array()) {
            for (auto& elem : j) {
                extract_binaries_from_json(elem, binaries);
            }
        } else if (j.is_object()) {
            // 检查是否是二进制占位符
            if (j.contains("_placeholder") && j["_placeholder"] == true && j.contains("num")) {
                int num = j["num"];
                // 这里应该已经是占位符了，不需要提取
                return;
            }
            
            // 遍历对象
            for (auto& elem : j.items()) {
                extract_binaries_from_json(elem.value(), binaries);
            }
        } else if (j.is_binary()) {
            // 提取二进制数据
            auto binary = j.get<std::vector<uint8_t>>();
            binaries.push_back(binary);
            
            // 替换为占位符
            j = json::object({
                {"_placeholder", true},
                {"num", static_cast<int>(binaries.size() - 1)}
            });
        }
    }
    
    // 将占位符替换为二进制数据
    void replace_placeholders_in_json(json& j, const std::vector<std::vector<uint8_t>>& binaries) {
        if (j.is_array()) {
            for (auto& elem : j) {
                replace_placeholders_in_json(elem, binaries);
            }
        } else if (j.is_object()) {
            // 检查是否是二进制占位符
            if (j.contains("_placeholder") && j["_placeholder"] == true && j.contains("num")) {
                int num = j["num"];
                if (num >= 0 && num < static_cast<int>(binaries.size())) {
                    j = json::binary(binaries[num]);
                }
            } else {
                // 遍历对象
                for (auto& elem : j.items()) {
                    replace_placeholders_in_json(elem.value(), binaries);
                }
            }
        }
    }
}

// PacketSplitter实现
PacketSplitter::SplitResult PacketSplitter::split(const Packet& packet) {
    SplitResult result;
    
    if (!packet.has_binary()) {
        // 没有二进制数据，直接编码
        result.text_part = encode_packet(packet);
        return result;
    }
    
    // 有二进制数据，需要创建副本进行处理
    Packet modified_packet = packet;
    
    // 解析JSON数据
    json j;
    try {
        if (!packet.data.empty()) {
            j = json::parse(packet.data);
        } else {
            j = json::array();  // 空数组
        }
        
        // 提取二进制数据并替换为占位符
        std::vector<std::vector<uint8_t>> extracted_binaries;
        extract_binaries_from_json(j, extracted_binaries);
        
        // 更新包的数据
        modified_packet.data = j.dump();
        
        // 更新包类型为二进制类型
        if (packet.type == PacketType::EVENT) {
            modified_packet.type = PacketType::BINARY_EVENT;
        } else if (packet.type == PacketType::ACK) {
            modified_packet.type = PacketType::BINARY_ACK;
        }
        
        // 二进制部分就是我们的附件
        result.binary_parts = packet.attachments;
        
    } catch (const std::exception& e) {
        // 解析失败，使用原始数据
        result.text_part = encode_packet(packet);
        return result;
    }
    
    // 编码修改后的包
    result.text_part = encode_packet(modified_packet);
    
    return result;
}

Packet PacketSplitter::combine(const std::string& text_part,
                               const std::vector<std::vector<uint8_t>>& binary_parts) {
    Packet packet;
    
    // 先解码文本部分
    if (!decode_packet_header(text_part, packet)) {
        return packet;
    }
    
    // 检查是否是二进制包
    if (packet.type == PacketType::BINARY_EVENT || packet.type == PacketType::BINARY_ACK) {
        // 将类型转换回普通类型
        if (packet.type == PacketType::BINARY_EVENT) {
            packet.type = PacketType::EVENT;
        } else {
            packet.type = PacketType::ACK;
        }
        
        // 解析JSON数据
        if (!packet.data.empty()) {
            try {
                json j = json::parse(packet.data);
                
                // 将占位符替换为二进制数据
                replace_placeholders_in_json(j, binary_parts);
                
                // 更新包的数据
                packet.data = j.dump();
                
                // 保存二进制附件
                packet.attachments = binary_parts;
                
            } catch (const std::exception& e) {
                // 保持原始数据不变
            }
        }
    } else {
        // 普通包，数据已经是完整的
        if (!packet.data.empty()) {
            // 尝试解析JSON以确保格式正确
            try {
                json::parse(packet.data);
            } catch (...) {
                // 如果不是有效JSON，保持原样
            }
        }
    }
    
    return packet;
}

// PacketSender实现
PacketSender::PacketSender() : state_(std::make_unique<SendState>()) {
    reset();
}

PacketSender::~PacketSender() = default;

void PacketSender::prepare(const Packet& packet) {
    reset();
    
    if (!packet.has_binary()) {
        // 没有二进制数据，直接发送
        std::string encoded = encode_packet(packet);
        state_->text_queue.push(encoded);
        state_->expecting_binary = false;
    } else {
        // 有二进制数据，需要分包
        auto split_result = PacketSplitter::split(packet);
        
        // 文本部分
        state_->text_queue.push(split_result.text_part);
        
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

bool PacketSender::get_next_binary(std::vector<uint8_t>& binary) {
    if (state_->binary_queue.empty()) {
        return false;
    }
    
    binary = state_->binary_queue.front();
    state_->binary_queue.pop();
    return true;
}

void PacketSender::reset() {
    state_->text_queue = std::queue<std::string>();
    state_->binary_queue = std::queue<std::vector<uint8_t>>();
    state_->current_attachments.clear();
    state_->expecting_binary = false;
}

// PacketReceiver实现
PacketReceiver::PacketReceiver() : state_(std::make_unique<ReceiveState>()) {
    reset();
}

PacketReceiver::~PacketReceiver() = default;

int PacketReceiver::parse_binary_count(const std::string& text) {
    // 简单解析：查找二进制占位符的数量
    int count = 0;
    size_t pos = 0;
    
    while ((pos = text.find("\"_placeholder\":true", pos)) != std::string::npos) {
        // 向前查找"num"字段
        size_t num_pos = text.find("\"num\":", pos);
        if (num_pos != std::string::npos) {
            num_pos += 6;  // 跳过"\"num\":"
            size_t end_pos = text.find_first_of(",}", num_pos);
            if (end_pos != std::string::npos) {
                std::string num_str = text.substr(num_pos, end_pos - num_pos);
                try {
                    int num = std::stoi(num_str);
                    count = std::max(count, num + 1);
                } catch (...) {
                    // 忽略解析错误
                }
            }
        }
        pos += 18;  // 跳过已搜索的部分
    }
    
    return count;
}

bool PacketReceiver::parse_packet_header(const std::string& text, Packet& packet) {
    return decode_packet_header(text, packet);
}

bool PacketReceiver::receive_text(const std::string& text) {
    reset();  // 每次接收新文本都重置状态
    
    state_->current_text = text;
    state_->expecting_binary = false;
    state_->has_complete = false;
    
    // 检查是否是二进制包
    Packet temp_packet;
    if (!parse_packet_header(text, temp_packet)) {
        return false;
    }
    
    if (temp_packet.type == PacketType::BINARY_EVENT ||
        temp_packet.type == PacketType::BINARY_ACK) {
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

bool PacketReceiver::receive_binary(const std::vector<uint8_t>& binary) {
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

bool PacketReceiver::get_complete_packet(Packet& packet) {
    if (!state_->has_complete) {
        return false;
    }
    
    // 组合包
    packet = PacketSplitter::combine(state_->current_text, state_->received_binaries);
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
