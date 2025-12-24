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
        default:
            return encode_v3_packet(packet);
    }
}

SioPacket SioPacketBuilder::decode_packet(
    const std::string& text_packet,
    const std::vector<SmartBuffer>& binary_parts) {
    
    if (text_packet.empty()) {
        return SioPacket();
    }
    
    switch (version_) {
        case SocketIOVersion::V2:
            return decode_v2_packet(text_packet, binary_parts);
        case SocketIOVersion::V3:
        default:
            return decode_v3_packet(text_packet, binary_parts);
    }
}

/*
SocketIOVersion SioPacketBuilder::detect_version(const std::string& packet) {
    if (packet.empty()) {
        return SocketIOVersion::V3; // 默认返回 V3
    }
    
    // 简单的版本检测逻辑
    // V2格式通常包含字符串命名空间（以"/"开头）
    // V3格式支持数字命名空间
    
    size_t pos = 0;
    
    // 跳过包类型
    if (packet[0] >= '0' && packet[0] <= '9') {
        pos = 1;
    }
    
    // 检查是否有命名空间
    if (pos < packet.length()) {
        // 检查是否是V2的字符串命名空间（以"/"开头）
        if (packet[pos] == '/') {
            return SocketIOVersion::V2;
        }
        
        // 检查是否是V3的数字命名空间
        if (packet[pos] >= '0' && packet[pos] <= '9') {
            // 查找数字结束位置
            size_t num_end = pos;
            while (num_end < packet.length() && packet[num_end] >= '0' && packet[num_end] <= '9') {
                num_end++;
            }
            
            // 如果数字后面是逗号或结束，则是V3的数字命名空间
            if (num_end < packet.length() && (packet[num_end] == ',' || packet[num_end] == '[')) {
                return SocketIOVersion::V3;
            }
        }
    }
    
    // 默认返回V3
    return SocketIOVersion::V3;
}
*/

SioPacketBuilder::PacketHeader SioPacketBuilder::parse_v2_header(const std::string& packet) {
    PacketHeader header;
    if (packet.empty()) return header;

    size_t pos = 0;

    // 1. packet type
    if (!isdigit(packet[pos])) return header;
    header.type = static_cast<PacketType>(packet[pos++] - '0');

    // 2. namespace (optional)
    if (pos < packet.size() && packet[pos] == '/') {
        size_t comma = packet.find(',', pos);
        if (comma != std::string::npos) {
            header.namespace_str = packet.substr(pos, comma - pos);
            pos = comma + 1;
        } else {
            // namespace only, no payload
            header.namespace_str = packet.substr(pos);
            pos = packet.size();
        }
    } else {
        header.namespace_str = "/";
    }

    // 3. v2: ackId 不在 header 中
    header.ack_id = -1;

    // 4. v2: binary_count 不在 header 中
    header.binary_count = 0;

    // 5. JSON 起点
    header.data_start_pos = pos;
    return header;
}
SioPacketBuilder::PacketHeader SioPacketBuilder::parse_v3_header(const std::string& packet) {
    PacketHeader header;
    if (packet.empty()) return header;

    size_t pos = 0;

    // 1. type
    if (!isdigit(packet[pos])) return header;
    header.type = static_cast<PacketType>(packet[pos++] - '0');

    bool is_binary =
        header.type == PacketType::BINARY_EVENT ||
        header.type == PacketType::BINARY_ACK;

    // 2. attachments
    if (is_binary) {
        size_t start = pos;
        while (pos < packet.size() && isdigit(packet[pos])) pos++;
        if (start < pos) {
            header.binary_count = std::stoi(packet.substr(start, pos - start));
        }
    }

    // 3. ackId (optional)
    size_t ack_start = pos;
    while (pos < packet.size() && isdigit(packet[pos])) pos++;
    if (pos > ack_start) {
        header.ack_id = std::stoi(packet.substr(ack_start, pos - ack_start));
    }

    // 4. namespace (optional, must start with '/')
    if (pos < packet.size() && packet[pos] == '/') {
        size_t nsp_start = pos;
        while (pos < packet.size() && packet[pos] != '-') pos++;
        header.namespace_str = packet.substr(nsp_start, pos - nsp_start);
    } else {
        header.namespace_str = "/";
    }

    // 5. '-'
    if (pos >= packet.size() || packet[pos] != '-') {
        header.data_start_pos = packet.size();
        return header;
    }
    pos++;

    // 6. JSON start
    header.data_start_pos = pos;
    return header;
}

 
SioPacketBuilder::PacketHeader SioPacketBuilder::parse_packet_header(
    const std::string& packet,
    SocketIOVersion version) {
    
    if (version == SocketIOVersion::V2) {
           return parse_v2_header(packet);
       }

    // V3 / V4
    return parse_v3_header(packet);
    
//    PacketHeader header;
//    
//    if (packet.empty()) {
//        return header;
//    }
//    
//    size_t pos = 0;
//    
//    // 1. 解析包类型
//    if (packet[0] >= '0' && packet[0] <= '9') {
//        header.type = static_cast<PacketType>(packet[0] - '0');
//        pos = 1;
//    }
//    
//    bool is_binary = (header.type == PacketType::BINARY_EVENT ||
//                      header.type == PacketType::BINARY_ACK);
//    
//    // 2. 对于二进制包，解析二进制计数
//    if (is_binary) {
//        // 查找减号分隔符
//        size_t dash_pos = packet.find('-', pos);
//        if (dash_pos != std::string::npos && dash_pos > pos) {
//            // 提取二进制计数
//            std::string count_str = packet.substr(pos, dash_pos - pos);
//            try {
//                header.binary_count = std::stoi(count_str);
//            } catch (...) {
//                header.binary_count = 0;
//            }
//            pos = dash_pos + 1; // 跳过减号
//        }
//    }
//    
//    if (pos >= packet.length()) {
//        header.data_start_pos = pos;
//        return header;
//    }
//    
//    // 3. 解析命名空间（V3/V4格式）
//    if (version == SocketIOVersion::V3 || version == SocketIOVersion::V4) {
//        // 检查是否有命名空间
//        if (packet[pos] == '/') {
//            size_t nsp_end = pos;
//            while (nsp_end < packet.length() &&
//                   packet[nsp_end] != ',' &&
//                   packet[nsp_end] != '[') {
//                nsp_end++;
//            }
//            
//            header.namespace_str = packet.substr(pos, nsp_end - pos);
//            pos = nsp_end;
//        } else {
//            header.namespace_str = "/";
//        }
//        
//        // 如果有逗号分隔符，跳过它
//        if (pos < packet.length() && packet[pos] == ',') {
//            pos++;
//        }
//    }
//    
//    // 4. 解析ACK ID（可选）
//    if (pos < packet.length() && packet[pos] >= '0' && packet[pos] <= '9') {
//        size_t ack_start = pos;
//        while (pos < packet.length() && packet[pos] >= '0' && packet[pos] <= '9') {
//            pos++;
//        }
//        
//        std::string ack_str = packet.substr(ack_start, pos - ack_start);
//        try {
//            header.ack_id = std::stoi(ack_str);
//        } catch (...) {
//            header.ack_id = -1;
//        }
//    }
//    
//    header.data_start_pos = pos;
//    return header;
//}

SioPacketBuilder::EncodedPacket SioPacketBuilder::encode_v3_packet(const SioPacket& packet) {
    EncodedPacket result;
    result.is_binary = packet.is_binary();
    
    // 构建JSON数据
    Json::Value json_data;
    std::vector<SmartBuffer> binary_parts;
    std::map<std::string, int> binary_map;
    
    if (packet.type == PacketType::ACK || packet.type == PacketType::BINARY_ACK) {
        // V3 ACK包：直接是参数数组
        Json::Value args_array(Json::arrayValue);
        for (const auto& arg : packet.args) {
            Json::Value processed_arg;
            extract_binary_data(arg, processed_arg, binary_parts, binary_map);
            args_array.append(processed_arg);
        }
        json_data = args_array;
    } else {
        // V3事件包：["event_name", ...args]
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
    
    // 构建V3文本包
    std::stringstream ss;
    
    // 包类型（如果是二进制包，使用二进制包类型）
    PacketType packet_type = packet.type;
    if (packet.is_binary()) {
        if (packet.type == PacketType::EVENT) {
            packet_type = PacketType::BINARY_EVENT;
        } else if (packet.type == PacketType::ACK) {
            packet_type = PacketType::BINARY_ACK;
        }
    }
    ss << static_cast<int>(packet_type);
    
    // V3命名空间（如果是数字命名空间）
    if (!packet.namespace_s.empty()) {
        ss << packet.namespace_s;
        
        // 如果有包ID，需要逗号分隔
        if (packet.ack_id > 0) {
            ss << ",";
        }
    }
    
    // 包ID（如果需要ACK）
    if (packet.ack_id > 0) {
        ss << packet.ack_id;
    }
    
    // V3二进制计数（如果是二进制包）
    if (packet.is_binary()) {
        ss << binary_parts.size();
    }
    
    // JSON数据
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    ss << Json::writeString(writer, json_data);
    
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
    packet.need_ack = (header.ack_id > 0);
    packet.binary_count = header.binary_count;
    
    // 获取数据部分
    std::string json_str = text.substr(header.data_start_pos);
    
    if (json_str.empty()) {
        return packet;
    }
    
    // 解析JSON数据
    Json::CharReaderBuilder reader_builder;
    std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
    Json::Value json_value;
    std::string errors;
    
    if (reader->parse(json_str.data(),
                     json_str.data() + json_str.size(),
                     &json_value, &errors)) {
        
        if (json_value.isArray()) {
            if (packet.type == PacketType::ACK || packet.type == PacketType::BINARY_ACK) {
                // ACK包：直接是参数数组
                std::map<std::string, int> binary_map;
                for (Json::ArrayIndex i = 0; i < json_value.size(); i++) {
                    Json::Value restored_arg = json_value[i];
                    restore_binary_data(restored_arg, binaries, binary_map);
                    packet.args.push_back(restored_arg);
                }
            } else {
                // 事件包：["event_name", ...args]
                if (json_value.size() > 0) {
                    packet.event_name = json_value[0].asString();
                    
                    // 恢复二进制数据到参数中
                    std::map<std::string, int> binary_map;
                    for (Json::ArrayIndex i = 1; i < json_value.size(); i++) {
                        Json::Value restored_arg = json_value[i];
                        restore_binary_data(restored_arg, binaries, binary_map);
                        packet.args.push_back(restored_arg);
                    }
                }
            }
        }else{
            //有可能是 "{\"sid\":\"qh_5Nkfe39jbMX56AAAk\"}" 对象
            packet.args.push_back(json_value);
        }
    }
    
    packet.binary_parts = binaries;
    return packet;
}

SioPacketBuilder::EncodedPacket SioPacketBuilder::encode_v2_packet(const SioPacket& packet) {
    EncodedPacket result;
    result.is_binary = packet.is_binary();
    
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
        if (packet.ack_id > 0) {
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
        if (packet.ack_id > 0) {
            json_obj["ackId"] = packet.ack_id;
        }
    }
    
    result.binary_parts = binary_parts;
    result.binary_count = static_cast<int>(binary_parts.size());
    
    // 构建V2文本包
    std::stringstream ss;
    
    // 包类型（如果是二进制包，使用二进制包类型）
    PacketType packet_type = packet.type;
    if (packet.is_binary()) {
        if (packet.type == PacketType::EVENT) {
            packet_type = PacketType::BINARY_EVENT;
        } else if (packet.type == PacketType::ACK) {
            packet_type = PacketType::BINARY_ACK;
        }
    }
    ss << static_cast<int>(packet_type);
    
    // V2命名空间（字符串格式，可选）
    if (!packet.namespace_s.empty()) {
        ss << packet.namespace_s;
        
        // 如果有包ID，需要逗号分隔
        if (packet.ack_id > 0) {
            ss << ",";
        }
    }
    
    // 包ID（如果需要ACK） - V2在JSON中处理，这里只处理二进制计数
    // V2二进制计数（如果是二进制包）
    if (packet.is_binary()) {
        ss << binary_parts.size();
    }
    
    // JSON数据
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    ss << Json::writeString(writer, json_obj);
    
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
    
//    // 解析V2命名空间字符串
//    if (!header.namespace_str.empty() && header.namespace_str.length() > 1) {
//        // 假设格式为 "/nsp123"，提取数字部分
//        std::string nsp_num = header.namespace_str.substr(4); // 跳过 "/nsp"
//        try {
//            packet.namespace_s = std::stoi(nsp_num);
//        } catch (...) {
//            packet.namespace_s = 0;
//        }
//    }
    
    // 获取数据部分
    std::string json_str = text.substr(header.data_start_pos);
    
    if (json_str.empty()) {
        return packet;
    }
    
    // 解析JSON数据
    Json::CharReaderBuilder reader_builder;
    std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
    Json::Value json_value;
    std::string errors;
    
    if (reader->parse(json_str.data(),
                     json_str.data() + json_str.size(),
                     &json_value, &errors)) {
        
        if (json_value.isObject()) {
            // V2格式：{"name": "event_name", "args": [...], "ackId": 123}
            if (json_value.isMember("name")) {
                packet.event_name = json_value["name"].asString();
            }
            
            if (json_value.isMember("args") && json_value["args"].isArray()) {
                const Json::Value& args = json_value["args"];
                std::map<std::string, int> binary_map;
                for (Json::ArrayIndex i = 0; i < args.size(); i++) {
                    Json::Value restored_arg = args[i];
                    restore_binary_data(restored_arg, binaries, binary_map);
                    packet.args.push_back(restored_arg);
                }
            }
            
            if (json_value.isMember("ackId")) {
                packet.ack_id = json_value["ackId"].asInt();
                packet.need_ack = (packet.ack_id > 0);
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
                
                // 创建占位符
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
        // 检查是否是占位符
        if (data.isMember("_placeholder") && data["_placeholder"].isBool() &&
            data["_placeholder"].asBool() && data.isMember("num") && data["num"].isInt()) {
            int index = data["num"].asInt();
            if (index >= 0 && index < static_cast<int>(binary_parts.size())) {
                const SmartBuffer& buffer = binary_parts[index];
                if (!buffer.empty()) {
                    data = binary_helper::create_binary_value(buffer.buffer());
                }
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

}  // namespace sio end
