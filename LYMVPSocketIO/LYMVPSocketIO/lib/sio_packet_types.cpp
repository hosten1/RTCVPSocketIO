#include "lib/sio_packet_types.h"
#include "lib/sio_jsoncpp_binary_helper.hpp"
#include "rtc_base/logging.h"
#include "rtc_base/checks.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>
#include <json/json.h>

namespace sio {

// ============================================================================// SIOHeader 类实现// ============================================================================

/**
 * @brief 生成调试信息字符串
 * @return 包含头部信息的字符串
 */
std::string SIOHeader::to_string() const {
    std::stringstream ss;
    ss << "SIOHeader {" << std::endl;
    ss << "  version: " << static_cast<int>(version) << std::endl;
    ss << "  type: " << static_cast<int>(type) << std::endl;
    ss << "  namespace: " << namespace_str << std::endl;
    ss << "  nsp: " << nsp << std::endl;
    ss << "  ack_id: " << ack_id << std::endl;
    ss << "  binary_count: " << binary_count << std::endl;
    ss << "  data_start_pos: " << data_start_pos << std::endl;
    ss << "}";
    return ss.str();
}

/**
 * @brief 解析包头部
 * @param packet 完整的数据包字符串
 * @return 解析是否成功
 */
bool SIOHeader::parse(const std::string& packet) {
    RTC_LOG(LS_INFO) << "Parsing SIOHeader from packet";
    
    if (packet.empty()) {
        RTC_LOG(LS_WARNING) << "Empty packet, cannot parse header";
        return false;
    }
    
    // 首先根据包内容检测协议版本
    SocketIOVersion detected_version = version;
    
    // 根据包内容检测版本
    if (packet.length() >= 2) {
        char first_char = packet[0];
        if (first_char >= '0' && first_char <= '9') {
            // 检查是否有减号（V3/V4二进制包格式）
            size_t dash_pos = packet.find('-', 1);
            if (dash_pos != std::string::npos) {
                // V3或V4二进制包格式：51-/chat,0[...]
                detected_version = SocketIOVersion::V3;
                RTC_LOG(LS_INFO) << "Detected V3/V4 packet format (contains dash)";
            } else {
                // 检查命名空间格式
                size_t slash_pos = packet.find('/', 1);
                if (slash_pos != std::string::npos) {
                    // 检查是否包含_placeholder（V3格式）
                    if (packet.find("{\"_placeholder\":") != std::string::npos || packet.find("\"_placeholder\":") != std::string::npos) {
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
    
    this->version = detected_version;
    
    // 根据协议版本选择对应的解析器
    switch (detected_version) {
        case SocketIOVersion::V2:
            return parse_v2(packet);
        case SocketIOVersion::V3:
            return parse_v3(packet);
        case SocketIOVersion::V4:
            return parse_v4(packet);
        default:
            RTC_LOG(LS_WARNING) << "Unknown version, cannot parse header";
            return false;
    }
}

/**
 * @brief 解析V2协议的数据包头部
 * @param packet 完整的数据包字符串
 * @return 解析是否成功
 * @note V2协议头部格式：[type][namespace][,ack_id][data]
 *       - type: 数据包类型（1字节数字）
 *       - namespace: 可选，以"/"开头
 *       - ack_id: 可选，数字字符串
 *       - data: JSON数据部分
 */
bool SIOHeader::parse_v2(const std::string& packet) {
    RTC_LOG(LS_INFO) << "Parsing V2 header from packet";
    
    // Socket.IO v2 header 顺序统一规则：
    // <packetType>[<binaryCount>-][<namespace>][,<ackId>]<json>
    if (packet.empty()) {
        RTC_LOG(LS_WARNING) << "Empty packet, cannot parse V2 header";
        return false;
    }
    
    size_t pos = 0;
    
    // 1. 包类型
    if (!isdigit(packet[pos])) {
        RTC_LOG(LS_WARNING) << "Invalid packet type at start: " << packet[pos];
        return false;
    }
    int type_num = packet[pos++] - '0';
    this->type = static_cast<PacketType>(type_num);
    
    // 检查是否为二进制包
    bool is_binary_packet = (this->type == PacketType::BINARY_EVENT ||
                             this->type == PacketType::BINARY_ACK);
    
    // 2. 二进制计数和减号（二进制包特有）
    if (is_binary_packet) {
        size_t count_start = pos;
        while (pos < packet.size() && isdigit(packet[pos])) {
            pos++;
        }
        
        if (pos > count_start) {
            std::string count_str = packet.substr(count_start, pos - count_start);
            try {
                this->binary_count = std::stoi(count_str);
                RTC_LOG(LS_INFO) << "Parsed V2 binary count: " << this->binary_count;
            } catch (...) {
                RTC_LOG(LS_WARNING) << "Failed to parse V2 binary count: " << count_str;
                this->binary_count = 0;
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
            this->namespace_str = packet.substr(nsp_start, pos - nsp_start);
        }
    } else {
        this->namespace_str = "/";
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
                this->ack_id = std::stoi(ack_str);
            } catch (...) {
                RTC_LOG(LS_WARNING) << "Failed to parse V2 ACK ID: " << ack_str;
                this->ack_id = -1;
            }
        }
    }
    
    // 6. 数据起始位置
    this->data_start_pos = pos;
    
    RTC_LOG(LS_INFO) << "Parsed V2 header: type=" << static_cast<int>(this->type)
    << ", namespace=" << this->namespace_str
    << ", ack_id=" << this->ack_id
    << ", binary_count=" << this->binary_count
    << ", data_start_pos=" << this->data_start_pos;
    
    return true;
}

/**
 * @brief 解析V3协议的数据包头部
 * @param packet 完整的数据包字符串
 * @return 解析是否成功
 * @note V3协议头部格式：[type][binary_count-][namespace][,ack_id][data]
 *       - type: 数据包类型（1字节数字）
 *       - binary_count-: 二进制包特有，格式为数字+"-"，表示二进制部分数量
 *       - namespace: 可选，以"/"开头
 *       - ack_id: 可选，数字字符串
 *       - data: JSON数据部分
 */
bool SIOHeader::parse_v3(const std::string& packet) {
    RTC_LOG(LS_INFO) << "Parsing V3 header from packet";
    
    if (packet.empty()) {
        RTC_LOG(LS_WARNING) << "Empty packet, cannot parse V3 header";
        return false;
    }

    size_t pos = 0;
    
    // 1. 包类型
    if (!isdigit(packet[pos])) {
        RTC_LOG(LS_WARNING) << "Invalid packet type at start: " << packet[pos];
        return false;
    }
    int type_num = packet[pos++] - '0';
    this->type = static_cast<PacketType>(type_num);
    
    // 检查是否为二进制包
    bool is_binary_packet = (this->type == PacketType::BINARY_EVENT ||
                            this->type == PacketType::BINARY_ACK);
    
    // 2. 二进制计数和减号（二进制包特有）
    if (is_binary_packet) {
        size_t count_start = pos;
        while (pos < packet.size() && isdigit(packet[pos])) {
            pos++;
        }
        
        if (pos > count_start) {
            std::string count_str = packet.substr(count_start, pos - count_start);
            try {
                this->binary_count = std::stoi(count_str);
                RTC_LOG(LS_INFO) << "Parsed V3 binary count: " << this->binary_count;
            } catch (...) {
                RTC_LOG(LS_WARNING) << "Failed to parse V3 binary count: " << count_str;
                this->binary_count = 0;
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
            this->namespace_str = packet.substr(nsp_start, pos - nsp_start);
        }
    } else {
        this->namespace_str = "/";
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
                this->ack_id = std::stoi(ack_str);
            } catch (...) {
                RTC_LOG(LS_WARNING) << "Failed to parse V3 ACK ID: " << ack_str;
                this->ack_id = -1;
            }
        }
    }
    
    // 数据起始位置
    this->data_start_pos = pos;
    
    RTC_LOG(LS_INFO) << "Parsed V3 header: type=" << static_cast<int>(this->type) 
                     << ", binary_count=" << this->binary_count 
                     << ", namespace=" << this->namespace_str 
                     << ", ack_id=" << this->ack_id 
                     << ", data_start_pos=" << this->data_start_pos;
    
    return true;
}

/**
 * @brief 解析V4协议的数据包头部
 * @param packet 完整的数据包字符串
 * @return 解析是否成功
 * @note V4协议与V3协议数据包格式几乎相同，直接复用V3的解析逻辑
 */
bool SIOHeader::parse_v4(const std::string& packet) {
    RTC_LOG(LS_INFO) << "Parsing V4 header (using V3 parser)";
    // V4协议与V3非常相似，暂时使用相同的解析逻辑
    return parse_v3(packet);
}

/**
 * @brief 构建包头部字符串
 * @return 构建好的头部字符串
 */
std::string SIOHeader::build() const {
    std::stringstream ss;
    
    // 确定包类型
    int packet_type = static_cast<int>(this->type);
    
    // 二进制计数（如果是二进制包）
    if (this->type == PacketType::BINARY_EVENT || this->type == PacketType::BINARY_ACK) {
        ss << packet_type << this->binary_count << "-";
        RTC_LOG(LS_INFO) << "Added binary count to header: " << this->binary_count;
    } else {
        ss << packet_type;
    }
    
    // 命名空间（如果不是根命名空间）
    if (!this->namespace_str.empty() && this->namespace_str != "/") {
        ss << this->namespace_str;
        RTC_LOG(LS_INFO) << "Added namespace to header: " << this->namespace_str;
    }
    
    // ACK ID（如果有）
    if (this->ack_id >= 0) {
        // 如果有命名空间且不是根，需要逗号分隔
        if (!this->namespace_str.empty() && this->namespace_str != "/") {
            ss << ",";
        }
        ss << this->ack_id;
        RTC_LOG(LS_INFO) << "Added ACK ID to header: " << this->ack_id;
    }
    
    std::string header_str = ss.str();
    RTC_LOG(LS_INFO) << "Built header: " << header_str;
    return header_str;
}

// ============================================================================// sioBody 类实现// ============================================================================

/**
 * @brief 生成调试信息字符串
 * @return 包含包体信息的字符串
 */
std::string sioBody::to_string() const {
    std::stringstream ss;
    ss << "sioBody {" << std::endl;
    ss << "  data: " << (this->data.size() > 50 ? this->data.substr(0, 50) + "..." : this->data) << std::endl;
    ss << "  attachments: " << this->attachments.size() << " 个" << std::endl;
    ss << "  has_binary: " << (this->has_binary() ? "true" : "false") << std::endl;
    ss << "}";
    return ss.str();
}

/**
 * @brief 解析包体
 * @param packet 完整的数据包字符串
 * @param header 已解析的包头部
 * @param binaries 二进制数据部分
 * @return 解析是否成功
 */
bool sioBody::parse(const std::string& packet, const SIOHeader& header, const std::vector<SmartBuffer>& binaries) {
    RTC_LOG(LS_INFO) << "Parsing sioBody from packet";
    
    if (packet.empty() || header.data_start_pos >= packet.size()) {
        RTC_LOG(LS_WARNING) << "Invalid packet for body parsing";
        return false;
    }
    
    // 获取数据部分
    std::string data_str = packet.substr(header.data_start_pos);
    this->data = data_str;
    
    RTC_LOG(LS_INFO) << "Extracted body data: " << (data_str.size() > 50 ? data_str.substr(0, 50) + "..." : data_str);
    
    // 根据协议版本选择对应的解析器
    switch (header.version) {
        case SocketIOVersion::V2:
            return parse_v2(packet, header, binaries);
        case SocketIOVersion::V3:
            return parse_v3(packet, header, binaries);
        case SocketIOVersion::V4:
            return parse_v4(packet, header, binaries);
        default:
            RTC_LOG(LS_WARNING) << "Unknown version, cannot parse body";
            return false;
    }
}

/**
 * @brief 构建包体
 * @param header 包头部信息
 * @return 构建好的包体字符串
 */
std::string sioBody::build(const SIOHeader& header) const {
    RTC_LOG(LS_INFO) << "Building sioBody";
    
    // 根据协议版本选择对应的构建器
    switch (header.version) {
        case SocketIOVersion::V2:
            return build_v2(header);
        case SocketIOVersion::V3:
            return build_v3(header);
        case SocketIOVersion::V4:
            return build_v4(header);
        default:
            RTC_LOG(LS_WARNING) << "Unknown version, cannot build body";
            return "";
    }
}

/**
 * @brief 提取二进制数据
 * @param binary_parts 存储提取出的二进制数据的向量
 */
void sioBody::extract_binary_data(std::vector<SmartBuffer>& binary_parts) const {
    RTC_LOG(LS_INFO) << "Extracting binary data from sioBody";
    
    // 简单实现：直接复制附件
    binary_parts.insert(binary_parts.end(), this->attachments.begin(), this->attachments.end());
    
    RTC_LOG(LS_INFO) << "Extracted " << binary_parts.size() << " binary parts";
}

/**
 * @brief 恢复二进制数据
 * @param binary_parts 二进制数据部分
 */
void sioBody::restore_binary_data(const std::vector<SmartBuffer>& binary_parts) {
    RTC_LOG(LS_INFO) << "Restoring binary data to sioBody";
    
    // 简单实现：直接复制二进制数据
    this->attachments = binary_parts;
    
    RTC_LOG(LS_INFO) << "Restored " << this->attachments.size() << " binary attachments";
}

/**
 * @brief 解析V2协议包体
 * @param packet 完整的数据包字符串
 * @param header 已解析的包头部
 * @param binaries 二进制数据部分
 * @return 解析是否成功
 */
bool sioBody::parse_v2(const std::string& packet, const SIOHeader& header, const std::vector<SmartBuffer>& binaries) {
    RTC_LOG(LS_INFO) << "Parsing V2 body from packet";
    
    if (packet.empty() || header.data_start_pos >= packet.size()) {
        RTC_LOG(LS_WARNING) << "Invalid V2 packet for body parsing";
        return false;
    }
    
    // 获取数据部分
    std::string data_str = packet.substr(header.data_start_pos);
    if (data_str.empty()) {
        RTC_LOG(LS_WARNING) << "Empty V2 body data";
        return true; // 空数据也是有效的
    }
    
    // 解析JSON数据
    Json::CharReaderBuilder reader_builder;
    std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
    Json::Value json_value;
    std::string errors;
    
    bool parsing_successful = reader->parse(
        data_str.data(),
        data_str.data() + data_str.size(),
        &json_value,
        &errors
    );
    
    if (!parsing_successful) {
        RTC_LOG(LS_ERROR) << "V2 JSON parse error: " << errors;
        RTC_LOG(LS_ERROR) << "JSON string: " << data_str;
        return false;
    }
    
    // 处理二进制数据
    if (header.type == PacketType::BINARY_EVENT || header.type == PacketType::BINARY_ACK) {
        this->attachments = binaries;
        RTC_LOG(LS_INFO) << "Set V2 binary attachments: " << this->attachments.size();
    }
    
    return true;
}

/**
 * @brief 解析V3协议包体
 * @param packet 完整的数据包字符串
 * @param header 已解析的包头部
 * @param binaries 二进制数据部分
 * @return 解析是否成功
 */
bool sioBody::parse_v3(const std::string& packet, const SIOHeader& header, const std::vector<SmartBuffer>& binaries) {
    RTC_LOG(LS_INFO) << "Parsing V3 body from packet";
    
    if (packet.empty() || header.data_start_pos >= packet.size()) {
        RTC_LOG(LS_WARNING) << "Invalid V3 packet for body parsing";
        return false;
    }
    
    // 获取数据部分
    std::string data_str = packet.substr(header.data_start_pos);
    if (data_str.empty()) {
        RTC_LOG(LS_WARNING) << "Empty V3 body data";
        return true; // 空数据也是有效的
    }
    
    // 解析JSON数据
    Json::CharReaderBuilder reader_builder;
    std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
    Json::Value json_value;
    std::string errors;
    
    bool parsing_successful = reader->parse(
        data_str.data(),
        data_str.data() + data_str.size(),
        &json_value,
        &errors
    );
    
    if (!parsing_successful) {
        RTC_LOG(LS_ERROR) << "V3 JSON parse error: " << errors;
        RTC_LOG(LS_ERROR) << "JSON string: " << data_str;
        return false;
    }
    
    // 处理二进制数据
    if (header.type == PacketType::BINARY_EVENT || header.type == PacketType::BINARY_ACK) {
        this->attachments = binaries;
        RTC_LOG(LS_INFO) << "Set V3 binary attachments: " << this->attachments.size();
    }
    
    return true;
}

/**
 * @brief 解析V4协议包体
 * @param packet 完整的数据包字符串
 * @param header 已解析的包头部
 * @param binaries 二进制数据部分
 * @return 解析是否成功
 */
bool sioBody::parse_v4(const std::string& packet, const SIOHeader& header, const std::vector<SmartBuffer>& binaries) {
    RTC_LOG(LS_INFO) << "Parsing V4 body (using V3 parser)";
    // V4协议与V3非常相似，暂时使用相同的解析逻辑
    return parse_v3(packet, header, binaries);
}

/**
 * @brief 构建V2协议包体
 * @param header 包头部信息
 * @return 构建好的包体字符串
 */
std::string sioBody::build_v2(const SIOHeader& header) const {
    RTC_LOG(LS_INFO) << "Building V2 body";
    
    // 简单实现：直接返回数据
    RTC_LOG(LS_INFO) << "Built V2 body: " << this->data;
    return this->data;
}

/**
 * @brief 构建V3协议包体
 * @param header 包头部信息
 * @return 构建好的包体字符串
 */
std::string sioBody::build_v3(const SIOHeader& header) const {
    RTC_LOG(LS_INFO) << "Building V3 body";
    
    // 简单实现：直接返回数据
    RTC_LOG(LS_INFO) << "Built V3 body: " << this->data;
    return this->data;
}

/**
 * @brief 构建V4协议包体
 * @param header 包头部信息
 * @return 构建好的包体字符串
 */
std::string sioBody::build_v4(const SIOHeader& header) const {
    RTC_LOG(LS_INFO) << "Building V4 body (using V3 builder)";
    // V4协议与V3非常相似，暂时使用相同的构建逻辑
    return build_v3(header);
}

// ============================================================================// Packet 类实现// ============================================================================

/**
 * @brief 生成调试信息字符串
 * @return 包含完整数据包信息的字符串
 */
std::string Packet::to_string() const {
    std::stringstream ss;
    ss << "Packet {" << std::endl;
    ss << "  Header: " << std::endl;
    ss << this->header.to_string() << std::endl;
    ss << "  Body: " << std::endl;
    ss << this->body.to_string() << std::endl;
    ss << "  has_binary: " << (this->has_binary() ? "true" : "false") << std::endl;
    ss << "}";
    return ss.str();
}

/**
 * @brief 解析完整数据包
 * @param packet 完整的数据包字符串
 * @param binaries 二进制数据部分
 * @return 解析是否成功
 */
bool Packet::parse(const std::string& packet, const std::vector<SmartBuffer>& binaries) {
    RTC_LOG(LS_INFO) << "Parsing complete Packet";
    
    // 1. 解析头部
    if (!this->header.parse(packet)) {
        RTC_LOG(LS_ERROR) << "Failed to parse Packet header";
        return false;
    }
    
    // 2. 解析包体
    if (!this->body.parse(packet, this->header, binaries)) {
        RTC_LOG(LS_ERROR) << "Failed to parse Packet body";
        return false;
    }
    
    RTC_LOG(LS_INFO) << "Successfully parsed Packet";
    return true;
}

/**
 * @brief 构建完整数据包
 * @return 构建好的完整数据包字符串
 */
std::string Packet::build() const {
    std::stringstream ss;
    
    // 1. 构建头部
    ss << this->header.build();
    
    // 2. 构建包体
    ss << this->body.build(this->header);
    
    std::string packet_str = ss.str();
    RTC_LOG(LS_INFO) << "Built complete Packet: " << (packet_str.size() > 100 ? packet_str.substr(0, 100) + "..." : packet_str);
    return packet_str;
}

} // namespace sio
