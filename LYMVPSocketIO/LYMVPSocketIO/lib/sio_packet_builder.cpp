//
//  sio_packet_builder.cpp
//  LYMVPSocketIO
//
//  Created by luoyongmeng on 2025/12/23.
//
// Socket.IO 协议数据包构建器
// 支持协议版本：V2, V3, V4
//
// 协议格式说明：
// =============================================================
// V2 协议格式：
// - 事件包：{"name": "event_name", "args": [...], "ackId": 123}
// - ACK包：{"args": [...], "ackId": 123}
// - 二进制数据：使用特殊映射对象，在args末尾添加二进制映射
// - 数据包格式：[type][namespace][,ack_id][json_data]
// - 示例：4/chat,1{"name":"chat","args":["hello"]}
//
// V3 协议格式：
// - 事件包：["event_name", ...args]
// - ACK包：[...args]
// - 二进制数据：使用{"_placeholder":true,"num":index}占位符
// - 数据包格式：[type][binary_count-][namespace][,ack_id][json_data]
// - 示例：42["chat","hello"]
//
// V4 协议格式：
// - 与V3兼容，使用相同的解码逻辑
// - 主要区别在于握手阶段的协议版本协商
//
// 包类型说明：
// 0: CONNECT       - 连接请求
// 1: DISCONNECT    - 断开连接
// 2: EVENT         - 事件
// 3: ACK           - 确认
// 4: CONNECT_ERROR - 连接错误
// 5: BINARY_EVENT  - 二进制事件
// 6: BINARY_ACK    - 二进制确认
// =============================================================


#include "lib/sio_packet_builder.h"
#include "lib/sio_packet_impl.h"
#include "lib/sio_packet_parser.h"
#include "lib/sio_ack_manager.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_utils/repeating_task.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>
#include <iostream>

namespace sio {

// ============================================================================// SioPacketBuilder 类实现// ============================================================================

/**
 * @brief 构造函数，初始化数据包构建器
 * @param version Socket.IO协议版本
 * @note 支持V2、V3、V4协议版本
 */SioPacketBuilder::SioPacketBuilder(SocketIOVersion version) : version_(version) {
    RTC_LOG(LS_INFO) << "SioPacketBuilder initialized with version: " << static_cast<int>(version);
}

/***
 @brief 构建事件数据包*
 @param event_name 事件名称*
 @param args 事件参数数组*
 @param namespace_s 命名空间，默认为"/"*
 @param ack_id ACK ID，-1表示不需要ACK*
 @return 构建好的SioPacket对象*
 @\note 根据协议版本自动处理二进制数据，检测到二进制数据时自动转换为BINARY_EVENT类型*/
SioPacket SioPacketBuilder::build_event_packet(
    const std::string& event_name,
    const std::vector<Json::Value>& args,
    const std::string&  namespace_s,
    int ack_id) {
    
    RTC_LOG(LS_INFO) << "Building event packet for: " << event_name << ", namespace: " << namespace_s;
    
    SioPacket packet;
    packet.type = PacketType::EVENT;
    packet.event_name = event_name;
    packet.args = args;
    packet.namespace_s = namespace_s;
    packet.ack_id = ack_id;
    packet.need_ack = (ack_id > 0);
    packet.version = version_;
    
    // 检查是否有二进制数据
    bool has_binary = false;
    for (const auto& arg : packet.args) {
        if (binary_helper::is_binary(arg)) {
            has_binary = true;
            packet.type = PacketType::BINARY_EVENT;
            break;
        }
    }
    
    RTC_LOG(LS_INFO) << "Built packet, type: " << static_cast<int>(packet.type) << ", has_binary: " << has_binary;
    return packet;
}

/*** @brief 构建ACK响应数据包*
 @param args ACK参数数组*
 @param namespace_s 命名空间，默认为"/"*
 @param ack_id ACK ID，用于标识需要响应的请求*
 @return 构建好的SioPacket对象*
 @note 检测到二进制数据时自动转换为BINARY_ACK类型*/
SioPacket SioPacketBuilder::build_ack_packet(
                        const std::vector<Json::Value>& args,
                          const std::string&  namespace_s ,
                          int ack_id ) {
    
    RTC_LOG(LS_INFO) << "Building ACK packet for ID: " << ack_id << ", namespace: " << namespace_s;
    
    SioPacket packet;
    packet.type = PacketType::ACK;
    packet.args = args;
    packet.namespace_s = namespace_s;
    packet.ack_id = ack_id;
    packet.need_ack = false;
    packet.version = version_;
    
    // 检查是否有二进制数据
    bool has_binary = false;
    for (const auto& arg : packet.args) {
        if (binary_helper::is_binary(arg)) {
            has_binary = true;
            packet.type = PacketType::BINARY_ACK;
            break;
        }
    }
    
    RTC_LOG(LS_INFO) << "Built ACK packet, type: " << static_cast<int>(packet.type) << ", has_binary: " << has_binary;
    return packet;
}

/***
 @brief 编码数据包，根据协议版本选择对应编码器*
 @param packet 要编码的SioPacket对象*
 @return 编码后的数据包，包含文本部分和二进制部分*
 @note 根据数据包的version字段选择编码格式，V4默认使用V3格式
 */
SioPacketBuilder::EncodedPacket SioPacketBuilder::encode_packet(const SioPacket& packet) {
    RTC_LOG(LS_INFO) << "Encoding packet with version: " << static_cast<int>(packet.version) << ", type: " << static_cast<int>(packet.type);
    
    EncodedPacket result;
    
    switch (packet.version) {
        case SocketIOVersion::V2:
            RTC_LOG(LS_INFO) << "Using V2 encoder";
            result = encode_v2_packet(packet);
            break;
        case SocketIOVersion::V3:
            RTC_LOG(LS_INFO) << "Using V3 encoder";
            result = encode_v3_packet(packet);
            break;
        case SocketIOVersion::V4:
        default:
            RTC_LOG(LS_INFO) << "Using V3 encoder for V4 packet";
            // V4默认使用V3格式
            result = encode_v3_packet(packet);
            break;
    }
    
    RTC_LOG(LS_INFO) << "Encoded packet: text length=" << result.text_packet.length() << ", binary parts=" << result.binary_parts.size();
    return result;
}

/***
 @brief 解码数据包，自动检测协议版本*
 @param text_packet 文本数据包*
 @param binary_parts 二进制数据部分*
 @return 解码后的SioPacket对象*
 @note 支持自动检测协议版本，根据数据包格式判断是V2、V3还是V4格式*
     - V2: 传统JSON格式，二进制数据通过特殊映射对象传递*
     - V3: 使用"_placeholder"占位符表示二进制数据*
     - V4: 与V3兼容，使用相同解码逻辑*/
SioPacket SioPacketBuilder::decode_packet(
    const std::string& text_packet,
    const std::vector<SmartBuffer>& binary_parts) {
    
    RTC_LOG(LS_INFO) << "Decoding packet, text length: " << text_packet.length() << ", binary parts: " << binary_parts.size();
    
    if (text_packet.empty()) {
        RTC_LOG(LS_WARNING) << "Empty text packet, returning empty packet";
        return SioPacket();
    }
    
    // 自动检测协议版本
    SocketIOVersion detected_version = version_;
    
    // 根据包内容检测版本
   /*
    if (text_packet.length() >= 2) {
        char first_char = text_packet[0];
        if (first_char >= '0' && first_char <= '9') {
            // 检查是否有减号（V3/V4二进制包格式）
            size_t dash_pos = text_packet.find('-', 1);
            if (dash_pos != std::string::npos) {
                // V3或V4二进制包格式：51-/chat,0[...]
                detected_version = SocketIOVersion::V3;
                RTC_LOG(LS_INFO) << "Detected V3/V4 packet format (contains dash)";
            } else {
                // 检查命名空间格式
                size_t slash_pos = text_packet.find('/', 1);
                if (slash_pos != std::string::npos) {
                    // 检查是否包含_placeholder（V3格式）
                    if (text_packet.find("{\"_placeholder\":") != std::string::npos || text_packet.find("\"_placeholder\":") != std::string::npos) {
                        // V3使用_placeholder格式
                        detected_version = SocketIOVersion::V3;
                        RTC_LOG(LS_INFO) << "Detected V3 packet format (contains _placeholder)";
                    } else {
                        // 默认使用V2格式
                        detected_version = SocketIOVersion::V2;
                        RTC_LOG(LS_INFO) << "Detected V2 packet format";
                    }
                }
            }
        }
    }
    */
    
    // 使用检测到的版本进行解码
    SioPacket result;
    switch (detected_version) {
        case SocketIOVersion::V2:
            RTC_LOG(LS_INFO) << "Using V2 decoder";
            result = decode_v2_packet(text_packet, binary_parts);
            break;
        case SocketIOVersion::V3:
            RTC_LOG(LS_INFO) << "Using V3 decoder";
            result = decode_v3_packet(text_packet, binary_parts);
            break;
        case SocketIOVersion::V4:
        default:
            RTC_LOG(LS_INFO) << "Using V3 decoder for V4 packet";
            result = decode_v3_packet(text_packet, binary_parts);
            result.version = SocketIOVersion::V4;
            break;
    }
    
    RTC_LOG(LS_INFO) << "Decoded packet: type=" << static_cast<int>(result.type) << ", namespace=" << result.namespace_s << ", args_count=" << result.args.size();
    return result;
}

/*** @brief 解析V2协议的数据包头部
 * @param packet 完整的数据包字符串
 * @return 解析出的PacketHeader结构体
 * @note V2协议头部格式：[type][namespace][,ack_id][data]
 *       - type: 数据包类型（1字节数字）
 *       - namespace: 可选，以"/"开头
 *       - ack_id: 可选，数字字符串
 *       - data: JSON数据部分
 * @example V2数据包示例：2/chat,1["hello"]
 *          - type: 2 (EVENT)
 *          - namespace: /chat
 *          - ack_id: 1
 *          - data: ["hello"]
 */SioPacketBuilder::PacketHeader SioPacketBuilder::parse_v2_header(const std::string& packet) {
     
     // Socket.IO v2 header 顺序统一规则：
     // <packetType>[-<binaryCount>][<namespace>][,<ackId>]<json>
    PacketHeader header;
    if (packet.empty()) {
        RTC_LOG(LS_WARNING) << "Empty packet, returning empty header";
        return header;
    }

    size_t pos = 0;
    
    // 1. 包类型
    if (!isdigit(packet[pos])) {
        RTC_LOG(LS_WARNING) << "Invalid packet type at start: " << packet[pos];
        return header;
    }
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
                RTC_LOG(LS_WARNING) << "Failed to parse ACK ID: " << ack_str;
                header.ack_id = -1;
            }
        }
    }
    
    // V2二进制计数在JSON数据中，不在头部
    header.binary_count = 0;
    
    // 数据起始位置
    header.data_start_pos = pos;
    
    RTC_LOG(LS_INFO) << "Parsed V2 header: type=" << static_cast<int>(header.type) 
                     << ", namespace=" << header.namespace_str 
                     << ", ack_id=" << header.ack_id 
                     << ", data_start_pos=" << header.data_start_pos;
    
    return header;
}

/*** @brief 解析V3协议的数据包头部
 * @param packet 完整的数据包字符串
 * @return 解析出的PacketHeader结构体
 * @note V3协议头部格式：[type][binary_count-][namespace][,ack_id][data]
 *       - type: 数据包类型（1字节数字）
 *       - binary_count-: 二进制包特有，格式为数字+"-"，表示二进制部分数量
 *       - namespace: 可选，以"/"开头
 *       - ack_id: 可选，数字字符串
 *       - data: JSON数据部分
 * @example V3二进制数据包示例：51-/chat["message",{"_placeholder":true,"num":0}]
 *          - type: 5 (BINARY_EVENT)
 *          - binary_count: 1
 *          - namespace: /chat
 *          - data: ["message",{"_placeholder":true,"num":0}]
 */SioPacketBuilder::PacketHeader SioPacketBuilder::parse_v3_header(const std::string& packet) {
    PacketHeader header;
    if (packet.empty()) {
        RTC_LOG(LS_WARNING) << "Empty packet, returning empty header";
        return header;
    }

    size_t pos = 0;
    
    // 1. 包类型
    if (!isdigit(packet[pos])) {
        RTC_LOG(LS_WARNING) << "Invalid packet type at start: " << packet[pos];
        return header;
    }
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
                RTC_LOG(LS_INFO) << "Parsed binary count: " << header.binary_count;
            } catch (...) {
                RTC_LOG(LS_WARNING) << "Failed to parse binary count: " << count_str;
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
                RTC_LOG(LS_WARNING) << "Failed to parse ACK ID: " << ack_str;
                header.ack_id = -1;
            }
        }
    }
    
    // 数据起始位置
    header.data_start_pos = pos;
    
    RTC_LOG(LS_INFO) << "Parsed V3 header: type=" << static_cast<int>(header.type) 
                     << ", binary_count=" << header.binary_count 
                     << ", namespace=" << header.namespace_str 
                     << ", ack_id=" << header.ack_id 
                     << ", data_start_pos=" << header.data_start_pos;
    
    return header;
}

/*** @brief 解析V4协议的数据包头部
 * @param packet 完整的数据包字符串
 * @return 解析出的PacketHeader结构体
 * @note V4协议与V3协议数据包格式几乎相同，直接复用V3的解析逻辑
 *       未来如果V4协议有变化，可以在这里单独处理
 */SioPacketBuilder::PacketHeader SioPacketBuilder::parse_v4_header(const std::string& packet) {
    RTC_LOG(LS_INFO) << "Parsing V4 header (using V3 parser)";
    // V4协议与V3非常相似，暂时使用相同的解析逻辑
    return parse_v3_header(packet);
}

/*** @brief 根据协议版本解析数据包头部
 * @param packet 完整的数据包字符串
 * @param version Socket.IO协议版本
 * @return 解析出的PacketHeader结构体
 * @note 根据不同的协议版本选择对应的解析器
 *       - V2: 使用parse_v2_header
 *       - V3: 使用parse_v3_header
 *       - V4: 使用parse_v4_header
 *       - 未知版本: 默认使用parse_v3_header
 */SioPacketBuilder::PacketHeader SioPacketBuilder::parse_packet_header(
    const std::string& packet,
    SocketIOVersion version) {
    
    RTC_LOG(LS_INFO) << "Parsing packet header with version: " << static_cast<int>(version);
    
    switch (version) {
        case SocketIOVersion::V2:
            return parse_v2_header(packet);
        case SocketIOVersion::V3:
            return parse_v3_header(packet);
        case SocketIOVersion::V4:
            return parse_v4_header(packet);
        default:
            RTC_LOG(LS_WARNING) << "Unknown version, using V3 parser: " << static_cast<int>(version);
            return parse_v3_header(packet);
    }
}

/***
 @brief 编码V3格式的数据包*
 @param packet 要编码的SioPacket对象*
 @return 编码后的EncodedPacket对象，包含文本部分和二进制部分*
 @note V3协议编码流程：*       1. 构建JSON数据，将二进制数据替换为占位符
*       2. 提取二进制数据到binary_parts
*       3. 构建文本数据包，包含类型、二进制计数、命名空间、ACK ID和JSON数据
* @example V3事件数据包编码：
*          输入：event_name="chat", args=["hello", binary_data]
*          输出：text_packet="42["chat","hello",{"_placeholder":true,"num":0}]", binary_parts=[binary_data]
* @note V3协议格式说明：
*       - 事件包：["event_name", ...args]
*       - ACK包：[...args]
*       - 二进制数据：使用{"_placeholder":true,"num":index}占位符
*/SioPacketBuilder::EncodedPacket SioPacketBuilder::encode_v3_packet(const SioPacket& packet) {
    RTC_LOG(LS_INFO) << "Encoding V3 packet, type: " << static_cast<int>(packet.type) << ", event: " << packet.event_name;
    
    EncodedPacket result;
    
    // 构建JSON数据
    Json::Value json_data;
    std::vector<SmartBuffer> binary_parts;
    std::map<std::string, int> binary_map;
    
    if (packet.type == PacketType::ACK || packet.type == PacketType::BINARY_ACK) {        // ACK包：直接是参数数组
        Json::Value args_array(Json::arrayValue);
        for (const auto& arg : packet.args) {
            Json::Value processed_arg;
            extract_binary_data(arg, processed_arg, binary_parts, binary_map);
            args_array.append(processed_arg);
        }
        json_data = args_array;
        RTC_LOG(LS_INFO) << "Built ACK JSON data with " << args_array.size() << " args";
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
        RTC_LOG(LS_INFO) << "Built EVENT JSON data with event: " << packet.event_name << ", args: " << event_array.size() - 1;
    }
    
    // 确定是否为二进制包 - 基于实际提取的二进制数据数量
    result.binary_parts = binary_parts;
    result.binary_count = static_cast<int>(binary_parts.size());
    result.is_binary = !binary_parts.empty();
    
    RTC_LOG(LS_INFO) << "Extracted binary parts: " << binary_parts.size() << ", is_binary: " << result.is_binary;
    
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
        RTC_LOG(LS_INFO) << "Converted to binary packet type: " << packet_type;
    }
    ss << packet_type;
    
    // 二进制计数（如果是二进制包）
    if (result.is_binary && result.binary_count > 0) {
        ss << result.binary_count << "-";
        RTC_LOG(LS_INFO) << "Added binary count: " << result.binary_count;
    }
    
    // 命名空间（如果不是根命名空间）
    if (!packet.namespace_s.empty() && packet.namespace_s != "/") {
        ss << packet.namespace_s;
        RTC_LOG(LS_INFO) << "Added namespace: " << packet.namespace_s;
    }
    
    // ACK ID（如果有）
    if (packet.ack_id >= 0) {
        // 如果有命名空间且不是根，需要逗号分隔
        if (!packet.namespace_s.empty() && packet.namespace_s != "/") {
            ss << ",";
        }
        ss << packet.ack_id;
        RTC_LOG(LS_INFO) << "Added ACK ID: " << packet.ack_id;
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
    
    RTC_LOG(LS_INFO) << "Encoded V3 packet: text length=" << result.text_packet.length() << ", binary parts=" << result.binary_parts.size();
    return result;
}

/***
 @brief 解码V3格式的数据包*
 @param text 文本数据包*
 @param binaries 二进制数据部分*
 @return 解码后的SioPacket对象*
 @note V3协议解码流程：*
     1. 解析包头，获取类型、命名空间、ACK ID和二进制计数*
     2. 解析JSON数据，获取事件名称和参数*
     3. 恢复二进制数据，将占位符替换为实际二进制数据*
 @example V3事件数据包解码：*
     输入：text="42["chat","hello",{"_placeholder":true,"num":0}]", binaries=[binary_data]*
     输出：packet.event_name="chat", packet.args=["hello", binary_data]*
 @note V3协议格式说明：*
     - 事件包：["event_name", ...args]*       - ACK包：[...args]*
     - 二进制数据：使用{"_placeholder":true,"num":index}占位符*
     - 二进制计数：在包头中指定，与实际二进制部分数量匹配
 */
SioPacket SioPacketBuilder::decode_v3_packet(
    const std::string& text,
    const std::vector<SmartBuffer>& binaries) {
    
    RTC_LOG(LS_INFO) << "Decoding V3 packet, text length: " << text.length() << ", binaries: " << binaries.size();
    
    SioPacket packet;
    packet.version = SocketIOVersion::V3;
    
    if (text.empty()) {
        RTC_LOG(LS_WARNING) << "Empty text packet, returning empty packet";
        return packet;
    }
    
    // 解析包头
    PacketHeader header = parse_packet_header(text, SocketIOVersion::V3);
    
    packet.type = header.type;
    packet.namespace_s = header.namespace_str;
    packet.ack_id = header.ack_id;
    packet.need_ack = (header.ack_id >= 0);
    packet.binary_count = header.binary_count;
    
    RTC_LOG(LS_INFO) << "Parsed V3 header: type=" << static_cast<int>(packet.type) << ", namespace=" << packet.namespace_s << ", ack_id=" << packet.ack_id << ", binary_count=" << packet.binary_count;
    
    // 获取数据部分
    if (header.data_start_pos >= text.length()) {
        RTC_LOG(LS_WARNING) << "Data start position beyond text length, returning packet without data";
        return packet;
    }
    
    std::string json_str = text.substr(header.data_start_pos);
    
    if (json_str.empty()) {
        RTC_LOG(LS_WARNING) << "Empty JSON data, returning packet without data";
        return packet;
    }
    
    RTC_LOG(LS_INFO) << "Extracted JSON data: " << json_str;
    
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
        RTC_LOG(LS_ERROR) << "JSON parse error: " << errors;
        RTC_LOG(LS_ERROR) << "JSON string: " << json_str;
        return packet;
    }
    
    // 调试输出
    RTC_LOG(LS_INFO) << "Decoded V3 packet:";
    RTC_LOG(LS_INFO) << "  Type: " << static_cast<int>(packet.type);
    RTC_LOG(LS_INFO) << "  Namespace: " << packet.namespace_s;
    RTC_LOG(LS_INFO) << "  Ack ID: " << packet.ack_id;
    RTC_LOG(LS_INFO) << "  Binary count: " << packet.binary_count;
    RTC_LOG(LS_INFO) << "  Binaries received: " << binaries.size();
    RTC_LOG(LS_INFO) << "  JSON value type: " << (json_value.isArray() ? "Array" : json_value.isObject() ? "Object" : "Other");
    
    if (parsing_successful) {
        if (packet.type == PacketType::ACK || packet.type == PacketType::BINARY_ACK) {
            // ACK包：直接是参数数组
            std::map<std::string, int> binary_map;
            for (Json::ArrayIndex i = 0; i < json_value.size(); i++) {
                Json::Value restored_arg = json_value[i];
                // 恢复二进制数据
                restore_binary_data(restored_arg, binaries, binary_map);
                packet.args.push_back(restored_arg);
            }
            RTC_LOG(LS_INFO) << "Processed ACK packet, args count: " << packet.args.size();
        } else {
            // 事件包：["event_name", ...args]
            if (json_value.isArray() && json_value.size() > 0) {
                packet.event_name = json_value[0].asString();
                
                // 恢复二进制数据到参数中
                std::map<std::string, int> binary_map;
                for (Json::ArrayIndex i = 1; i < json_value.size(); i++) {
                    Json::Value restored_arg = json_value[i];
                    // 恢复二进制数据
                    restore_binary_data(restored_arg, binaries, binary_map);
                    packet.args.push_back(restored_arg);
                }
                RTC_LOG(LS_INFO) << "Processed EVENT packet, event: " << packet.event_name << ", args count: " << packet.args.size();
            } else if (json_value.isObject()) {
                // 可能是CONNECT或DISCONNECT包的响应数据
                Json::Value restored_arg = json_value;
                std::map<std::string, int> binary_map;
                restore_binary_data(restored_arg, binaries, binary_map);
                packet.args.push_back(restored_arg);
                RTC_LOG(LS_INFO) << "Processed OBJECT packet, args count: " << packet.args.size();
            }
        }
    }
    
    // 将二进制数据添加到包中
    packet.binary_parts = binaries;
    
    RTC_LOG(LS_INFO) << "Decoded V3 packet completed, event: " << packet.event_name << ", args: " << packet.args.size();
    return packet;
}

/***
 @brief 编码V2格式的数据包*
 @param packet 要编码的SioPacket对象*
 @return 编码后的EncodedPacket对象，包含文本部分和二进制部分*
 @note V2协议编码流程：*
     1. 构建JSON数据对象，包含name、args和ackId字段*
     2. 提取二进制数据到binary_parts*
     3. 对于事件包，在args数组末尾添加二进制映射对象*
     4. 构建文本数据包，包含类型、命名空间、ACK ID和JSON数据*
 @example V2事件数据包编码：*
     输入：event_name="chat", args=["hello", binary_data]*
     输出：text_packet="4/chat,1{\"name\":\"chat\",\"args\":[\"hello\",{\"_placeholder\":true,\"num\":0}]}*, binary_parts=[binary_data]*
 @note V2协议格式说明：*
     - 事件包：{"name": "event_name", "args": [...], "ackId": 123}*
     - ACK包：{"args": [...], "ackId": 123}*
     - 二进制数据：使用{"_placeholder":true,"num":index}占位符，在事件包args末尾添加二进制映射对象
 */
SioPacketBuilder::EncodedPacket SioPacketBuilder::encode_v2_packet(const SioPacket& packet) {
    RTC_LOG(LS_INFO) << "Encoding V2 packet, type: " << static_cast<int>(packet.type) << ", event: " << packet.event_name;
    
    EncodedPacket result;
    
    // 构建JSON数据
    Json::Value json_sio_body(Json::arrayValue);
    std::vector<SmartBuffer> binary_parts;
    std::map<std::string, int> binary_map;
    
    // V2协议：EVENT格式为 ["event_name", ...args]
    //         ACK格式为 [...args]
    if (packet.type == PacketType::ACK || packet.type == PacketType::BINARY_ACK) {
        /*****************************
         ACK格式（V2协议）：
         3/chat,22[...args]    // 普通ACK
         6-/chat,22[...args]   // BINARY_ACK（有1个二进制附件）
         
         JSON结构：
         [...args]  // 只包含参数数组，不包含ackId
         
         注意：ackId在header中，不在JSON中
         *****************************/

        RTC_DCHECK(packet.ack_id >= 0) << "ACK包必须有ack_id";
        // ACK包：只有参数数组
        for (const auto& arg : packet.args) {
            Json::Value processed_arg;
            extract_binary_data(arg, processed_arg, binary_parts, binary_map);
            json_sio_body.append(processed_arg);
        }
        
        RTC_LOG(LS_INFO) << "Built V2 ACK JSON data, args count: "
        << packet.args.size() << ", ackId: " << packet.ack_id;
      
    } else {
        /*****************************
         示例：EVENT（BINARY_EVENT，包含 1 个 binary）

         5-/chat,123[["upload",{"file":{"_placeholder":true,"num":0}}]]

         含义：
         5        → PacketType.BINARY_EVENT
         /chat    → namespace
         123      → ackId（位于 header 中）

         JSON 说明：
         [
           "upload",                         // event_name
           {
             "name": "file",
             "data": {
               "_placeholder": true,         // 二进制占位符
               "num": 0                      // 对应 binary_parts[0]
             }
           }
         ]

         协议要点：
         - binary 数据不会出现在 JSON 中
         - 实际 binary 紧随文本包之后单独发送
         - placeholder.num 与 binary frame 顺序严格对应
         *****************************/
        RTC_DCHECK(!packet.event_name.empty()) << "EVENT包必须有event_name";
        // V2事件包：["event_name",  {}]
        // EVENT包：第一个元素是事件名
        json_sio_body.append(packet.event_name);
        
        
        // 然后是参数数组
        for (const auto& arg : packet.args) {
            Json::Value processed_arg;
            extract_binary_data(arg, processed_arg, binary_parts, binary_map);
            json_sio_body.append(processed_arg);
        }
        
        RTC_LOG(LS_INFO) << "Built V2 EVENT JSON data, event: "
        << packet.event_name << ", args count: " << packet.args.size();
    }
    // 确定是否为二进制包 - 基于实际提取的二进制数据数量
    result.binary_parts = binary_parts;
    result.binary_count = static_cast<int>(binary_parts.size());
    result.is_binary = !binary_parts.empty();
    
    RTC_LOG(LS_INFO) << "Extracted V2 binary parts: " << binary_parts.size() << ", is_binary: " << result.is_binary;
    
    // 构建V2文本包
    std::stringstream ss;
    
    // V2协议使用的包类型编号
    int packet_type;
    if (result.is_binary) {
        if (packet.type == PacketType::EVENT || packet.type == PacketType::BINARY_EVENT) {
            packet_type = static_cast<int>(PacketType::BINARY_EVENT); // V2: 5
        } else {
            packet_type = static_cast<int>(PacketType::BINARY_ACK); // V2: 6
        }
    } else {
        if (packet.type == PacketType::EVENT || packet.type == PacketType::BINARY_EVENT) {
            packet_type = static_cast<int>(PacketType::EVENT); // V2: 2
        } else {
            packet_type = static_cast<int>(PacketType::ACK); // V2: 3
        }
    }
    
    
    // 关键修复：V2协议二进制包需要在类型后添加 "{binary_count}-"
     if (result.is_binary) {
         ss << packet_type << result.binary_count << "-";
     } else {
         ss << packet_type;
     }
     
     RTC_LOG(LS_INFO) << "V2 packet type: " << packet_type << " for original type: " << static_cast<int>(packet.type);
    
    bool has_namespace = (!packet.namespace_s.empty() && packet.namespace_s != "/");
    // 命名空间（如果不是根命名空间）
    if (has_namespace) {
        // 确保命名空间以斜杠开头
        if (packet.namespace_s[0] != '/') {
            ss << "/" << packet.namespace_s;
        } else {
            ss << packet.namespace_s;
        }
        RTC_LOG(LS_INFO) << "Added V2 namespace: " << packet.namespace_s;
    }
    
    // 添加ACK ID（如果有）
    bool has_ack_id = (packet.ack_id >= 0);
    
    // ACK ID（如果有）
    if (has_ack_id) {
        // 如果有命名空间且不是根，需要逗号分隔
        if (has_namespace) {
            ss << ",";
        }
        ss << packet.ack_id;
        RTC_LOG(LS_INFO) << "Added V2 ACK ID: " << packet.ack_id;
    }
    // V2二进制计数在JSON中处理，不在头部
    
    // JSON数据
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    writer["emitUTF8"] = true;
    writer["precision"] = 17;
    writer["dropNullPlaceholders"] = false;
    writer["enableYAMLCompatibility"] = false;
    
    std::string json_str = Json::writeString(writer, json_sio_body);
    ss << json_str;
    
    result.text_packet = ss.str();
    
    RTC_LOG(LS_INFO) << "Encoded V2 packet: text length=" << result.text_packet.length() << ", binary parts=" << result.binary_parts.size();
    RTC_LOG(LS_INFO) << "Encoded V2 packet content: " << result.text_packet;
    return result;
}

/***
 @brief 解码V2格式的数据包*
 @param text 文本数据包*
 @param binaries 二进制数据部分*
 @return 解码后的SioPacket对象*
 @note V2协议解码流程：*
     1. 解析包头，获取类型、命名空间、ACK ID和二进制计数*
     2. 解析JSON数据，获取name、args和ackId字段*
     3. 检查是否包含二进制映射对象，处理V2二进制数据*
     4. 恢复二进制数据，将占位符替换为实际二进制数据*
 @example V2事件数据包解码：*
     输入：text="4/chat,1{\"name\":\"chat\",\"args\":[\"hello\",{\"_placeholder\":true,\"num\":0}]}*, binaries=[binary_data]*
     输出：packet.event_name="chat", packet.args=["hello", binary_data]*
 @note V2协议格式说明：*
     - 事件包：{"name": "event_name", "args": [...], "ackId": 123}*
     - ACK包：{"args": [...], "ackId": 123}*
     - 二进制数据：事件包在args末尾添加二进制映射对象，使用{"_placeholder":true,"num":index}占位符*
     - CONNECT包：特殊处理，允许没有数据部分
 */
SioPacket SioPacketBuilder::decode_v2_packet(
    const std::string& text,
    const std::vector<SmartBuffer>& binaries) {
    
    RTC_LOG(LS_INFO) << "Decoding V2 packet, text length: " << text.length() << ", binaries: " << binaries.size();
    
    SioPacket packet;
    packet.version = SocketIOVersion::V2;
    
    if (text.empty()) {
        RTC_LOG(LS_WARNING) << "Empty text packet, returning empty packet";
        return packet;
    }
    
    // 解析包头
    PacketHeader header = parse_packet_header(text, SocketIOVersion::V2);
    
    packet.type = header.type;
    packet.namespace_s = header.namespace_str;
    packet.ack_id = header.ack_id;
    packet.need_ack = (header.ack_id >= 0);
    packet.binary_count = header.binary_count;
    
    RTC_LOG(LS_INFO) << "Parsed V2 header: type=" << static_cast<int>(packet.type) << ", namespace=" << packet.namespace_s << ", ack_id=" << packet.ack_id << ", binary_count=" << packet.binary_count;
    
    // 获取数据部分
    if ((packet.type != PacketType::CONNECT) && header.data_start_pos >= text.length()) {
        RTC_LOG(LS_WARNING) << "Data start position beyond text length for non-CONNECT packet, returning packet without data";
        return packet;
    }
    
    std::string json_str = text.substr(header.data_start_pos);
    
    if ((packet.type != PacketType::CONNECT) && json_str.empty() ) {
        RTC_LOG(LS_WARNING) << "Empty JSON data for non-CONNECT packet, returning packet without data";
        return packet;
    }
    
    if (!json_str.empty()) {
        RTC_LOG(LS_INFO) << "Extracted V2 JSON data: " << json_str;
        
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
            RTC_LOG(LS_ERROR) << "V2 JSON parse error: " << errors;
            RTC_LOG(LS_ERROR) << "JSON string: " << json_str;
            return packet;
        }
        
        // 调试输出
        RTC_LOG(LS_INFO) << "Decoded V2 packet:";
        RTC_LOG(LS_INFO) << "  Type: " << static_cast<int>(packet.type);
        RTC_LOG(LS_INFO) << "  Namespace: " << packet.namespace_s;
        RTC_LOG(LS_INFO) << "  Ack ID: " << packet.ack_id;
        RTC_LOG(LS_INFO) << "  JSON value type: " << (json_value.isArray() ? "Array" : json_value.isObject() ? "Object" : "Other");
        
        if (parsing_successful) {
            if (json_value.isObject()) {
                // V2格式：{"name": "event_name", "args": [...], "ackId": 123}
                if (json_value.isMember("name")) {
                    packet.event_name = json_value["name"].asString();
                    RTC_LOG(LS_INFO) << "Extracted V2 event name: " << packet.event_name;
                }
                
                if (json_value.isMember("ackId")) {
                    packet.ack_id = json_value["ackId"].asInt();
                    packet.need_ack = (packet.ack_id >= 0);
                    RTC_LOG(LS_INFO) << "Extracted V2 ackId: " << packet.ack_id;
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
                                RTC_LOG(LS_INFO) << "Detected V2 binary map with " << members.size() << " entries";
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
                    
                    RTC_LOG(LS_INFO) << "Processed V2 args, count: " << packet.args.size();
                    
                    // 如果是二进制包，设置类型
                    if (has_binary_map && !binaries.empty()) {
                        if (packet.type == PacketType::EVENT) {
                            packet.type = PacketType::BINARY_EVENT;
                            RTC_LOG(LS_INFO) << "Converted to V2 BINARY_EVENT type";
                        } else if (packet.type == PacketType::ACK) {
                            packet.type = PacketType::BINARY_ACK;
                            RTC_LOG(LS_INFO) << "Converted to V2 BINARY_ACK type";
                        }
                    }
                }
            } else if (json_value.isArray()) {
                // 检查包类型
                if (packet.type == PacketType::EVENT || packet.type == PacketType::BINARY_EVENT) {
                    // 事件包数组格式：["event_name", ...args]
                    if (json_value.size() > 0) {
                        // 第一个元素是事件名称
                        packet.event_name = json_value[0].asString();
                        RTC_LOG(LS_INFO) << "Extracted V2 event name from array: " << packet.event_name;
                        
                        // 剩下的元素是参数
                        for (Json::ArrayIndex i = 1; i < json_value.size(); i++) {
                            Json::Value restored_arg = json_value[i];
                            // 处理二进制数据
                            std::map<std::string, int> binary_map;
                            restore_binary_data(restored_arg, binaries, binary_map);
                            packet.args.push_back(restored_arg);
                        }
                        RTC_LOG(LS_INFO) << "Processed V2 array event args, count: " << packet.args.size();
                    }
                } else {
                    // ACK包：直接是参数数组
                    std::map<std::string, int> binary_map;
                    for (Json::ArrayIndex i = 0; i < json_value.size(); i++) {
                        Json::Value restored_arg = json_value[i];
                        restore_binary_data(restored_arg, binaries, binary_map);
                        packet.args.push_back(restored_arg);
                    }
                    RTC_LOG(LS_INFO) << "Processed V2 ACK array, args count: " << packet.args.size();
                }
            }
        }
    }
    
    packet.binary_parts = binaries;
    
    RTC_LOG(LS_INFO) << "Decoded V2 packet completed, event: " << packet.event_name << ", args: " << packet.args.size();
    return packet;
}

/***
 @brief 提取JSON数据中的二进制数据，将其替换为占位符*
 @param data 原始JSON数据*
 @param json_without_binary 处理后的JSON数据，其中二进制数据已被替换为占位符*
 @param binary_parts 存储提取出的二进制数据的向量*
 @param binary_map 二进制数据映射，用于记录二进制数据的位置*
 @note 递归处理JSON数据，支持以下数据类型：*
     - 基本类型（null、bool、int、uint、double、string）：直接复制*
     - 数组：递归处理每个元素*
     - 对象：检查是否为二进制数据，是则替换为占位符，否则递归处理每个属性*
 @note 二进制数据占位符格式：{"_placeholder":true,"num":index}，其中index是二进制数据在binary_parts中的索引
 */
void SioPacketBuilder::extract_binary_data(
    const Json::Value& data,
    Json::Value& json_without_binary,
    std::vector<SmartBuffer>& binary_parts,
    std::map<std::string, int>& binary_map) {
    
    RTC_LOG(LS_VERBOSE) << "Extracting binary data from JSON value";
    
    // 基本数据类型，直接复制
    if (data.isNull() || data.isBool() || data.isInt() ||
        data.isUInt() || data.isDouble() || data.isString()) {
        json_without_binary = data;
        return;
    }
    
    // 数组类型，递归处理每个元素
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
    
    // 对象类型
    if (data.isObject()) {
        // 检查是否是二进制数据
        if (binary_helper::is_binary(data)) {
            auto buffer_ptr = binary_helper::get_binary_shared_ptr(data);
            if (buffer_ptr) {
                SmartBuffer buffer(buffer_ptr);
                int index = static_cast<int>(binary_parts.size());
                binary_parts.push_back(buffer);
                
                RTC_LOG(LS_INFO) << "Extracted binary data, index: " << index << ", size: " << buffer.size();
                
                // 创建占位符（V3/V4格式）
                Json::Value placeholder(Json::objectValue);
                placeholder["_placeholder"] = true;
                placeholder["num"] = index;
                json_without_binary = placeholder;
                
                RTC_LOG(LS_VERBOSE) << "Created placeholder for binary data at index: " << index;
                return;
            }
        }
        
        // 普通对象，递归处理每个属性
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
    
    // 未知类型，返回null
    json_without_binary = Json::Value(Json::nullValue);
    RTC_LOG(LS_WARNING) << "Unknown JSON value type, returning null";
}

/***
 @brief 将JSON数据中的二进制占位符替换为实际的二进制数据*
 @param data 包含占位符的JSON数据，处理后将包含实际的二进制数据*
 @param binary_parts 存储二进制数据的向量，用于替换占位符*
 @param binary_map 二进制数据映射，用于V2协议的二进制数据恢复*
 @note 递归处理JSON数据，支持以下数据类型：*
     - 基本类型（null、bool、int、uint、double、string）：无操作*
     - 数组：递归处理每个元素*
     - 对象：检查是否为二进制占位符，是则替换为实际二进制数据，否则递归处理每个属性*
 @note V3/V4二进制占位符格式：{"_placeholder":true,"num":index}，其中index是二进制数据在binary_parts中的索引*
 @note 如果binary_parts为空或index无效，会跳过替换，等待完整的二进制数据*
 @note 替换操作直接修改传入的data参数，将占位符对象替换为二进制数据对象
 */
void SioPacketBuilder::restore_binary_data(
    Json::Value& data,
    const std::vector<SmartBuffer>& binary_parts,
    const std::map<std::string, int>& binary_map) {
    
    RTC_LOG(LS_VERBOSE) << "Restoring binary data in JSON value";
    
    // 基本数据类型，无需处理
    if (data.isNull() || data.isBool() || data.isInt() ||
        data.isUInt() || data.isDouble() || data.isString()) {
        return;
    }
    
    // 数组类型，递归处理每个元素
    if (data.isArray()) {
        for (Json::ArrayIndex i = 0; i < data.size(); i++) {
            restore_binary_data(data[i], binary_parts, binary_map);
        }
        return;
    }
    
    // 对象类型，检查是否为二进制占位符
    if (data.isObject()) {
        // 检查是否是V3/V4格式的占位符：{"_placeholder":true,"num":index}
        if (data.isMember("_placeholder") && data["_placeholder"].isBool() &&
            data["_placeholder"].asBool() && data.isMember("num") && data["num"].isInt()) {
            
            int index = data["num"].asInt();
            RTC_LOG(LS_INFO) << "Found placeholder at index: " << index << ", binary_parts size: " << binary_parts.size();
            
            // 检查binary_parts是否为空
            if (binary_parts.empty()) {
                RTC_LOG(LS_WARNING) << "Warning: binary_parts is empty, skipping placeholder replacement";
                return; // 跳过替换，等待完整的二进制数据
            }
            
            // 检查index是否有效
            if (index >= 0 && index < static_cast<int>(binary_parts.size())) {
                const SmartBuffer& buffer = binary_parts[index];
                if (!buffer.empty()) {
                    RTC_LOG(LS_INFO) << "Replacing placeholder with binary data, size: " << buffer.size();
                    // 替换为实际的二进制数据
                    data = binary_helper::create_binary_value(buffer.buffer());
                } else {
                    RTC_LOG(LS_WARNING) << "Binary buffer at index " << index << " is empty!";
                }
            } else {
                RTC_LOG(LS_WARNING) << "Invalid binary index: " << index << " (max: " << (binary_parts.size() > 0 ? static_cast<int>(binary_parts.size()) - 1 : -1) << ")";
            }
            return;
        }
        
        // 普通对象，递归处理每个属性
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
