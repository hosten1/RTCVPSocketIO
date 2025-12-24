//
//  sio_packet_builder.cpp
//  LYMVPSocketIO
//
//  Created by luoyongmeng on 2025/12/23.
//

#include "lib/sio_packet_builder.h"
#include "lib/sio_packet_impl.h"
#include "lib/sio_packet_parser.h"
#include "lib/sio_ack_manager.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/repeating_task.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>
#include <iostream>

namespace sio {

// ============================================================================
// SioPacketBuilder 类实现
// ============================================================================

SioPacketBuilder::SioPacketBuilder(SocketIOVersion version) : version_(version) {}

SioPacket SioPacketBuilder::build_event_packet(
    const std::string& event_name,
    const std::vector<Json::Value>& args,
    const std::string&  namespace_s,
    int ack_id) {
    
    SioPacket packet;
    packet.type = PacketType::EVENT;
    packet.event_name = event_name;
    packet.args = args;
    packet.namespace_s = namespace_s;
    packet.ack_id = ack_id;
    packet.need_ack = (ack_id > 0);
    packet.version = version_;
    
    // 检查是否有二进制数据
    for (const auto& arg : packet.args) {
        if (binary_helper::is_binary(arg)) {
            packet.type = PacketType::BINARY_EVENT;
            break;
        }
    }
    
    return packet;
}

SioPacket SioPacketBuilder::build_ack_packet(
                        const std::vector<Json::Value>& args,
                          const std::string&  namespace_s ,
                          int ack_id ) {
    
    SioPacket packet;
    packet.type = PacketType::ACK;
    packet.args = args;
    packet.namespace_s = namespace_s;
    packet.ack_id = ack_id;
    packet.need_ack = false;
    packet.version = version_;
    
    // 检查是否有二进制数据
    for (const auto& arg : packet.args) {
        if (binary_helper::is_binary(arg)) {
            packet.type = PacketType::BINARY_ACK;
            break;
        }
    }
    
    return packet;
}

SioPacketBuilder::EncodedPacket SioPacketBuilder::encode_packet(const SioPacket& packet) {
    switch (packet.version) {
        case SocketIOVersion::V2:
            return encode_v2_packet(packet);
        case SocketIOVersion::V3:
            return encode_v3_packet(packet);
        case SocketIOVersion::V4:
        default:
            // V4默认使用V3格式
            return encode_v3_packet(packet);
    }
}

SioPacket SioPacketBuilder::decode_packet(
    const std::string& text_packet,
    const std::vector<SmartBuffer>& binary_parts) {
    
    if (text_packet.empty()) {
        return SioPacket();
    }
    
    // 自动检测协议版本
    SocketIOVersion detected_version = version_;
    
    // 根据包内容检测版本
    if (text_packet.length() >= 2) {
        char first_char = text_packet[0];
        if (first_char >= '0' && first_char <= '9') {
            // 检查是否有减号（V3/V4二进制包格式）
            size_t dash_pos = text_packet.find('-', 1);
            if (dash_pos != std::string::npos) {
                // V3或V4二进制包格式：51-/chat,0[...]
                detected_version = SocketIOVersion::V3;
            } else {
                // 检查命名空间格式
                size_t slash_pos = text_packet.find('/', 1);
                if (slash_pos != std::string::npos) {
                    // 可能是V2或V3
                    // 根据您的日志，V3格式：3/chat,1[...]
                    // V2格式：2/chat,1[...]
                    // 这里根据数据格式判断
                    if (text_packet.find("{\"_placeholder\":") != std::string::npos) {
                        // V3使用_placeholder格式
                        detected_version = SocketIOVersion::V3;
                    } else if (text_packet.find("\"_placeholder\":") != std::string::npos) {
                        // 单引号或双引号格式
                        detected_version = SocketIOVersion::V3;
                    }
                }
            }
        }
    }
    
    // 使用检测到的版本进行解码
    SioPacket result;
    switch (detected_version) {
        case SocketIOVersion::V2:
            result = decode_v2_packet(text_packet, binary_parts);
            break;
        case SocketIOVersion::V3:
            result = decode_v3_packet(text_packet, binary_parts);
            break;
        case SocketIOVersion::V4:
        default:
            result = decode_v3_packet(text_packet, binary_parts);
            result.version = SocketIOVersion::V4;
            break;
    }
    
    return result;
}

SioPacketBuilder::PacketHeader SioPacketBuilder::parse_v2_header(const std::string& packet) {
    PacketHeader header;
    if (packet.empty()) return header;

    size_t pos = 0;
    
    // 1. 包类型
    if (!isdigit(packet[pos])) return header;
    int type_num = packet[pos++] - '0';
    header.type = static_cast<PacketType>(type_num);
    
    // 检查是否为二进制包
    bool is_binary_packet = (header.type == PacketType::BINARY_EVENT ||
                            header.type == PacketType::BINARY_ACK);
    
    // 2. 命名空间（可选）
    if (pos < packet.size() && packet[pos] == '/') {
        size_t nsp_start = pos;
        
        // 查找命名空间结束位置
        while (pos < packet.size()) {
            if (packet[pos] == ',' || packet[pos] == '[') {
                break;
            }
            pos++;
        }
        
        if (pos > nsp_start) {
            header.namespace_str = packet.substr(nsp_start, pos - nsp_start);
        }
    } else {
        header.namespace_str = "/";
    }
    
    // 3. 跳过逗号（如果有）
    if (pos < packet.size() && packet[pos] == ',') {
        pos++;
    }
    
    // 4. ACK ID（可选）
    if (pos < packet.size() && isdigit(packet[pos])) {
        size_t ack_start = pos;
        while (pos < packet.size() && isdigit(packet[pos])) {
            pos++;
        }
        
        if (pos > ack_start) {
            std::string ack_str = packet.substr(ack_start, pos - ack_start);
            try {
                header.ack_id = std::stoi(ack_str);
            } catch (...) {
                header.ack_id = -1;
            }
        }
    }
    
    // V2二进制计数在JSON数据中，不在头部
    header.binary_count = 0;
    
    // 数据起始位置
    header.data_start_pos = pos;
    
    return header;
}

SioPacketBuilder::PacketHeader SioPacketBuilder::parse_v3_header(const std::string& packet) {
    PacketHeader header;
    if (packet.empty()) return header;

    size_t pos = 0;
    
    // 1. 包类型
    if (!isdigit(packet[pos])) return header;
    int type_num = packet[pos++] - '0';
    header.type = static_cast<PacketType>(type_num);
    
    // 检查是否为二进制包
    bool is_binary_packet = (header.type == PacketType::BINARY_EVENT ||
                            header.type == PacketType::BINARY_ACK);
    
    // 2. 二进制计数和减号（二进制包特有）
    if (is_binary_packet) {
        size_t count_start = pos;
        while (pos < packet.size() && isdigit(packet[pos])) {
            pos++;
        }
        
        if (pos > count_start) {
            std::string count_str = packet.substr(count_start, pos - count_start);
            try {
                header.binary_count = std::stoi(count_str);
            } catch (...) {
                header.binary_count = 0;
            }
        }
        
        // 检查减号
        if (pos < packet.size() && packet[pos] == '-') {
            pos++;
        }
    }
    
    // 3. 命名空间（可选）
    if (pos < packet.size() && packet[pos] == '/') {
        size_t nsp_start = pos;
        
        // 查找命名空间结束位置
        while (pos < packet.size()) {
            if (packet[pos] == ',' || packet[pos] == '[') {
                break;
            }
            pos++;
        }
        
        if (pos > nsp_start) {
            header.namespace_str = packet.substr(nsp_start, pos - nsp_start);
        }
    } else {
        header.namespace_str = "/";
    }
    
    // 4. 跳过逗号（如果有）
    if (pos < packet.size() && packet[pos] == ',') {
        pos++;
    }
    
    // 5. ACK ID（可选）
    if (pos < packet.size() && isdigit(packet[pos])) {
        size_t ack_start = pos;
        while (pos < packet.size() && isdigit(packet[pos])) {
            pos++;
        }
        
        if (pos > ack_start) {
            std::string ack_str = packet.substr(ack_start, pos - ack_start);
            try {
                header.ack_id = std::stoi(ack_str);
            } catch (...) {
                header.ack_id = -1;
            }
        }
    }
    
    // 数据起始位置
    header.data_start_pos = pos;
    
    return header;
}

SioPacketBuilder::PacketHeader SioPacketBuilder::parse_v4_header(const std::string& packet) {
    // V4协议与V3非常相似，暂时使用相同的解析逻辑
    return parse_v3_header(packet);
}

SioPacketBuilder::PacketHeader SioPacketBuilder::parse_packet_header(
    const std::string& packet,
    SocketIOVersion version) {
    
    switch (version) {
        case SocketIOVersion::V2:
            return parse_v2_header(packet);
        case SocketIOVersion::V3:
            return parse_v3_header(packet);
        case SocketIOVersion::V4:
            return parse_v4_header(packet);
        default:
            return parse_v3_header(packet);
    }
}

SioPacketBuilder::EncodedPacket SioPacketBuilder::encode_v3_packet(const SioPacket& packet) {
    EncodedPacket result;
    
    // 确定是否为二进制包
    result.is_binary = packet.is_binary();
    result.binary_count = packet.binary_count;
    
    // 构建JSON数据
    Json::Value json_data;
    std::vector<SmartBuffer> binary_parts;
    std::map<std::string, int> binary_map;
    
    if (packet.type == PacketType::ACK || packet.type == PacketType::BINARY_ACK) {
        // ACK包：直接是参数数组
        Json::Value args_array(Json::arrayValue);
        for (const auto& arg : packet.args) {
            Json::Value processed_arg;
            extract_binary_data(arg, processed_arg, binary_parts, binary_map);
            args_array.append(processed_arg);
        }
        json_data = args_array;
    } else {
        // 事件包：["event_name", ...args]
        Json::Value event_array(Json::arrayValue);
        event_array.append(Json::Value(packet.event_name));
        
        for (const auto& arg : packet.args) {
            Json::Value processed_arg;
            extract_binary_data(arg, processed_arg, binary_parts, binary_map);
            event_array.append(processed_arg);
        }
        json_data = event_array;
    }
    
    result.binary_parts = binary_parts;
    result.binary_count = static_cast<int>(binary_parts.size());
    
    // 构建文本包
    std::stringstream ss;
    
    // 确定包类型
    int packet_type = static_cast<int>(packet.type);
    if (result.is_binary) {
        // 二进制包类型转换
        if (packet.type == PacketType::EVENT) {
            packet_type = static_cast<int>(PacketType::BINARY_EVENT);
        } else if (packet.type == PacketType::ACK) {
            packet_type = static_cast<int>(PacketType::BINARY_ACK);
        }
    }
    ss << packet_type;
    
    // 二进制计数（如果是二进制包）
    if (result.is_binary && result.binary_count > 0) {
        ss << result.binary_count << "-";
    }
    
    // 命名空间（如果不是根命名空间）
    if (!packet.namespace_s.empty() && packet.namespace_s != "/") {
        ss << packet.namespace_s;
    }
    
    // ACK ID（如果有）
    if (packet.ack_id >= 0) {
        // 如果有命名空间且不是根，需要逗号分隔
        if (!packet.namespace_s.empty() && packet.namespace_s != "/") {
            ss << ",";
        }
        ss << packet.ack_id;
    }
    
    // JSON数据
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    writer["emitUTF8"] = true;
    writer["precision"] = 17;
    writer["dropNullPlaceholders"] = false;
    writer["enableYAMLCompatibility"] = false;
    
    std::string json_str = Json::writeString(writer, json_data);
    ss << json_str;
    
    result.text_packet = ss.str();
    return result;
}

SioPacket SioPacketBuilder::decode_v3_packet(
    const std::string& text,
    const std::vector<SmartBuffer>& binaries) {
    
    SioPacket packet;
    packet.version = SocketIOVersion::V3;
    
    if (text.empty()) {
        return packet;
    }
    
    // 解析包头
    PacketHeader header = parse_packet_header(text, SocketIOVersion::V3);
    
    packet.type = header.type;
    packet.namespace_s = header.namespace_str;
    packet.ack_id = header.ack_id;
    packet.need_ack = (header.ack_id >= 0);
    packet.binary_count = header.binary_count;
    
    // 获取数据部分
    if (header.data_start_pos >= text.length()) {
        return packet;
    }
    
    std::string json_str = text.substr(header.data_start_pos);
    
    if (json_str.empty()) {
        return packet;
    }
    
    // 解析JSON数据
    Json::CharReaderBuilder reader_builder;
    std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
    Json::Value json_value;
    std::string errors;
    
    bool parsing_successful = reader->parse(
        json_str.data(),
        json_str.data() + json_str.size(),
        &json_value,
        &errors
    );
    
    if (!parsing_successful) {
        std::cerr << "JSON parse error: " << errors << std::endl;
        std::cerr << "JSON string: " << json_str << std::endl;
        return packet;
    }
    
    // 调试输出
    std::cout << "Decoded V3 packet:" << std::endl;
    std::cout << "  Type: " << static_cast<int>(packet.type) << std::endl;
    std::cout << "  Namespace: " << packet.namespace_s << std::endl;
    std::cout << "  Ack ID: " << packet.ack_id << std::endl;
    std::cout << "  Binary count: " << packet.binary_count << std::endl;
    std::cout << "  Binaries received: " << binaries.size() << std::endl;
    std::cout << "  JSON value type: " << (json_value.isArray() ? "Array" : json_value.isObject() ? "Object" : "Other") << std::endl;
    
    if (parsing_successful) {
        if (packet.type == PacketType::ACK || packet.type == PacketType::BINARY_ACK) {
            // ACK包：直接是参数数组
            std::map<std::string, int> binary_map;
            for (Json::ArrayIndex i = 0; i < json_value.size(); i++) {
                Json::Value restored_arg = json_value[i];
                // 重要：传递binaries参数
                restore_binary_data(restored_arg, binaries, binary_map);
                packet.args.push_back(restored_arg);
            }
        } else {
            // 事件包：["event_name", ...args]
            if (json_value.isArray() && json_value.size() > 0) {
                packet.event_name = json_value[0].asString();
                
                // 恢复二进制数据到参数中
                std::map<std::string, int> binary_map;
                for (Json::ArrayIndex i = 1; i < json_value.size(); i++) {
                    Json::Value restored_arg = json_value[i];
                    // 重要：传递binaries参数
                    restore_binary_data(restored_arg, binaries, binary_map);
                    packet.args.push_back(restored_arg);
                }
            } else if (json_value.isObject()) {
                // 可能是CONNECT或DISCONNECT包的响应数据
                // 也需要恢复二进制数据
                Json::Value restored_arg = json_value;
                std::map<std::string, int> binary_map;
                restore_binary_data(restored_arg, binaries, binary_map);
                packet.args.push_back(restored_arg);
            }
        }
    }
    
    // 将二进制数据添加到包中
    packet.binary_parts = binaries;
    
    return packet;
}

SioPacketBuilder::EncodedPacket SioPacketBuilder::encode_v2_packet(const SioPacket& packet) {
    EncodedPacket result;
    
    // 确定是否为二进制包
    result.is_binary = packet.is_binary();
    result.binary_count = packet.binary_count;
    
    // 构建JSON数据
    Json::Value json_obj(Json::objectValue);
    std::vector<SmartBuffer> binary_parts;
    std::map<std::string, int> binary_map;
    
    if (packet.type == PacketType::ACK || packet.type == PacketType::BINARY_ACK) {
        // V2 ACK包：{"args": [...]}
        Json::Value args_array(Json::arrayValue);
        for (const auto& arg : packet.args) {
            Json::Value processed_arg;
            extract_binary_data(arg, processed_arg, binary_parts, binary_map);
            args_array.append(processed_arg);
        }
        json_obj["args"] = args_array;
        
        // V2需要ackId
        if (packet.ack_id >= 0) {
            json_obj["ackId"] = packet.ack_id;
        }
    } else {
        // V2事件包：{"name": "event_name", "args": [...]}
        json_obj["name"] = Json::Value(packet.event_name);
        
        Json::Value args_array(Json::arrayValue);
        for (const auto& arg : packet.args) {
            Json::Value processed_arg;
            extract_binary_data(arg, processed_arg, binary_parts, binary_map);
            args_array.append(processed_arg);
        }
        json_obj["args"] = args_array;
        
        // V2需要ackId
        if (packet.ack_id >= 0) {
            json_obj["ackId"] = packet.ack_id;
        }
        
        // V2二进制包的特殊处理：在args数组中添加二进制映射
        if (result.is_binary && !binary_parts.empty()) {
            // V2格式：在args数组末尾添加一个二进制映射对象
            Json::Value binary_map_obj(Json::objectValue);
            for (size_t i = 0; i < binary_parts.size(); i++) {
                binary_map_obj[std::to_string(i)] = static_cast<Json::Int>(i);
            }
            json_obj["args"].append(binary_map_obj);
        }
    }
    
    result.binary_parts = binary_parts;
    result.binary_count = static_cast<int>(binary_parts.size());
    
    // 构建V2文本包
    std::stringstream ss;
    
    // 包类型
    int packet_type = static_cast<int>(packet.type);
    ss << packet_type;
    
    // 命名空间（如果不是根命名空间）
    if (!packet.namespace_s.empty() && packet.namespace_s != "/") {
        ss << packet.namespace_s;
    }
    
    // ACK ID（如果有）
    if (packet.ack_id >= 0) {
        // 如果有命名空间且不是根，需要逗号分隔
        if (!packet.namespace_s.empty() && packet.namespace_s != "/") {
            ss << ",";
        }
        ss << packet.ack_id;
    }
    
    // V2二进制计数在JSON中处理，不在头部
    
    // JSON数据
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    writer["emitUTF8"] = true;
    writer["precision"] = 17;
    writer["dropNullPlaceholders"] = false;
    writer["enableYAMLCompatibility"] = false;
    
    std::string json_str = Json::writeString(writer, json_obj);
    ss << json_str;
    
    result.text_packet = ss.str();
    return result;
}

SioPacket SioPacketBuilder::decode_v2_packet(
    const std::string& text,
    const std::vector<SmartBuffer>& binaries) {
    
    SioPacket packet;
    packet.version = SocketIOVersion::V2;
    
    if (text.empty()) {
        return packet;
    }
    
    // 解析包头
    PacketHeader header = parse_packet_header(text, SocketIOVersion::V2);
    
    packet.type = header.type;
    packet.namespace_s = header.namespace_str;
    packet.ack_id = header.ack_id;
    packet.need_ack = (header.ack_id >= 0);
    packet.binary_count = header.binary_count;
    
    // 获取数据部分
    if (header.data_start_pos >= text.length()) {
        return packet;
    }
    
    std::string json_str = text.substr(header.data_start_pos);
    
    if (json_str.empty()) {
        return packet;
    }
    
    // 解析JSON数据
    Json::CharReaderBuilder reader_builder;
    std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
    Json::Value json_value;
    std::string errors;
    
    bool parsing_successful = reader->parse(
        json_str.data(),
        json_str.data() + json_str.size(),
        &json_value,
        &errors
    );
    
    if (!parsing_successful) {
        std::cerr << "V2 JSON parse error: " << errors << std::endl;
        return packet;
    }
    
    // 调试输出
    std::cout << "Decoded V2 packet:" << std::endl;
    std::cout << "  Type: " << static_cast<int>(packet.type) << std::endl;
    std::cout << "  Namespace: " << packet.namespace_s << std::endl;
    std::cout << "  Ack ID: " << packet.ack_id << std::endl;
    std::cout << "  JSON value type: " << (json_value.isArray() ? "Array" : json_value.isObject() ? "Object" : "Other") << std::endl;
    
    if (parsing_successful) {
        if (json_value.isObject()) {
            // V2格式：{"name": "event_name", "args": [...], "ackId": 123}
            if (json_value.isMember("name")) {
                packet.event_name = json_value["name"].asString();
            }
            
            if (json_value.isMember("ackId")) {
                packet.ack_id = json_value["ackId"].asInt();
                packet.need_ack = (packet.ack_id >= 0);
            }
            
            if (json_value.isMember("args") && json_value["args"].isArray()) {
                const Json::Value& args = json_value["args"];
                
                // 检查是否是V2二进制格式（最后一个元素是二进制映射对象）
                bool has_binary_map = false;
                Json::Value binary_map_obj;
                
                if (args.size() > 0) {
                    const Json::Value& last_arg = args[args.size() - 1];
                    if (last_arg.isObject()) {
                        // 检查是否是二进制映射对象
                        bool looks_like_binary_map = true;
                        Json::Value::Members members = last_arg.getMemberNames();
                        for (const auto& key : members) {
                            // 检查key是否是数字字符串
                            bool is_numeric = !key.empty();
                            for (char c : key) {
                                if (!isdigit(c)) {
                                    is_numeric = false;
                                    break;
                                }
                            }
                            if (!is_numeric || !last_arg[key].isInt()) {
                                looks_like_binary_map = false;
                                break;
                            }
                        }
                        
                        if (looks_like_binary_map && !members.empty()) {
                            has_binary_map = true;
                            binary_map_obj = last_arg;
                        }
                    }
                }
                
                // 处理参数
                size_t args_to_process = has_binary_map ? args.size() - 1 : args.size();
                
                for (Json::ArrayIndex i = 0; i < args_to_process; i++) {
                    Json::Value restored_arg = args[i];
                    
                    // 处理V2二进制数据（如果有二进制映射）
                    if (has_binary_map && !binaries.empty()) {
                        // 检查并转换V2二进制格式
                        restore_v2_binary_data(restored_arg, binaries, binary_map_obj);
                    }
                    
                    packet.args.push_back(restored_arg);
                }
                
                // 如果是二进制包，设置类型
                if (has_binary_map && !binaries.empty()) {
                    if (packet.type == PacketType::EVENT) {
                        packet.type = PacketType::BINARY_EVENT;
                    } else if (packet.type == PacketType::ACK) {
                        packet.type = PacketType::BINARY_ACK;
                    }
                }
            }
        } else if (json_value.isArray()) {
            // ACK包：直接是参数数组
            std::map<std::string, int> binary_map;
            for (Json::ArrayIndex i = 0; i < json_value.size(); i++) {
                Json::Value restored_arg = json_value[i];
                restore_binary_data(restored_arg, binaries, binary_map);
                packet.args.push_back(restored_arg);
            }
        }
    }
    
    packet.binary_parts = binaries;
    return packet;
}

void SioPacketBuilder::extract_binary_data(
    const Json::Value& data,
    Json::Value& json_without_binary,
    std::vector<SmartBuffer>& binary_parts,
    std::map<std::string, int>& binary_map) {
    
    if (data.isNull() || data.isBool() || data.isInt() ||
        data.isUInt() || data.isDouble() || data.isString()) {
        json_without_binary = data;
        return;
    }
    
    if (data.isArray()) {
        Json::Value array(Json::arrayValue);
        for (Json::ArrayIndex i = 0; i < data.size(); i++) {
            Json::Value processed_element;
            extract_binary_data(data[i], processed_element, binary_parts, binary_map);
            array.append(processed_element);
        }
        json_without_binary = array;
        return;
    }
    
    if (data.isObject()) {
        // 检查是否是二进制数据
        if (binary_helper::is_binary(data)) {
            auto buffer_ptr = binary_helper::get_binary_shared_ptr(data);
            if (buffer_ptr) {
                SmartBuffer buffer(buffer_ptr);
                int index = static_cast<int>(binary_parts.size());
                binary_parts.push_back(buffer);
                
                // 创建占位符（V3/V4格式）
                Json::Value placeholder(Json::objectValue);
                placeholder["_placeholder"] = true;
                placeholder["num"] = index;
                json_without_binary = placeholder;
                return;
            }
        }
        
        // 普通对象
        Json::Value obj(Json::objectValue);
        Json::Value::Members members = data.getMemberNames();
        for (const auto& key : members) {
            Json::Value processed_value;
            extract_binary_data(data[key], processed_value, binary_parts, binary_map);
            obj[key] = processed_value;
        }
        json_without_binary = obj;
        return;
    }
    
    json_without_binary = Json::Value(Json::nullValue);
}

void SioPacketBuilder::restore_binary_data(
    Json::Value& data,
    const std::vector<SmartBuffer>& binary_parts,
    const std::map<std::string, int>& binary_map) {
    
    if (data.isNull() || data.isBool() || data.isInt() ||
        data.isUInt() || data.isDouble() || data.isString()) {
        return;
    }
    
    if (data.isArray()) {
        for (Json::ArrayIndex i = 0; i < data.size(); i++) {
            restore_binary_data(data[i], binary_parts, binary_map);
        }
        return;
    }
    
    if (data.isObject()) {
        // 调试输出：检查对象内容
        // std::cout << "Checking object for placeholder..." << std::endl;
        
        // 检查是否是占位符（V3/V4格式）
        if (data.isMember("_placeholder") && data["_placeholder"].isBool() &&
            data["_placeholder"].asBool() && data.isMember("num") && data["num"].isInt()) {
            
            int index = data["num"].asInt();
            std::cout << "Found placeholder at index: " << index << ", binary_parts size: " << binary_parts.size() << std::endl;
            
            // 检查binary_parts是否为空
            if (binary_parts.empty()) {
                std::cout << "Warning: binary_parts is empty, skipping placeholder replacement" << std::endl;
                return; // 跳过替换，等待完整的二进制数据
            }
            
            if (index >= 0 && index < static_cast<int>(binary_parts.size())) {
                const SmartBuffer& buffer = binary_parts[index];
                if (!buffer.empty()) {
                    std::cout << "Replacing placeholder with binary data, size: " << buffer.size() << std::endl;
                    data = binary_helper::create_binary_value(buffer.buffer());
                } else {
                    std::cout << "Binary buffer at index " << index << " is empty!" << std::endl;
                }
            } else {
                std::cout << "Invalid binary index: " << index << " (max: " << (binary_parts.size() > 0 ? static_cast<int>(binary_parts.size()) - 1 : -1) << ")" << std::endl;
            }
            return;
        }
        
        // 普通对象
        Json::Value::Members members = data.getMemberNames();
        for (const auto& key : members) {
            restore_binary_data(data[key], binary_parts, binary_map);
        }
        return;
    }
}

void SioPacketBuilder::restore_v2_binary_data(
    Json::Value& data,
    const std::vector<SmartBuffer>& binary_parts,
    const Json::Value& binary_map) {
    
    if (data.isNull() || data.isBool() || data.isInt() ||
        data.isUInt() || data.isDouble() || data.isString()) {
        return;
    }
    
    if (data.isArray()) {
        for (Json::ArrayIndex i = 0; i < data.size(); i++) {
            restore_v2_binary_data(data[i], binary_parts, binary_map);
        }
        return;
    }
    
    if (data.isObject()) {
        // V2二进制格式：对象中包含二进制引用
        Json::Value::Members members = data.getMemberNames();
        bool is_binary_ref = false;
        int binary_index = -1;
        
        for (const auto& key : members) {
            Json::Value& value = data[key];
            
            if (value.isInt()) {
                int ref_index = value.asInt();
                // 检查这个引用是否在binary_map中
                if (binary_map.isMember(std::to_string(ref_index))) {
                    binary_index = binary_map[std::to_string(ref_index)].asInt();
                    is_binary_ref = true;
                    break;
                }
            }
        }
        
        if (is_binary_ref && binary_index >= 0 && binary_index < static_cast<int>(binary_parts.size())) {
            const SmartBuffer& buffer = binary_parts[binary_index];
            if (!buffer.empty()) {
                // 替换为实际的二进制数据
                data = binary_helper::create_binary_value(buffer.buffer());
            }
        } else {
            // 递归处理
            Json::Value::Members members = data.getMemberNames();
            for (const auto& key : members) {
                restore_v2_binary_data(data[key], binary_parts, binary_map);
            }
        }
        return;
    }
}

}  // namespace sio end
