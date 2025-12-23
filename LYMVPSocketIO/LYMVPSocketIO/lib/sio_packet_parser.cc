#include "sio_packet_parser.h"
#include "rtc_base/logging.h"
#include <sstream>
#include <cctype>
#include <algorithm>
#include <iomanip>
#include <mutex>
#include <regex>
#include <iostream>

namespace sio {

// ==================== ParseResult 方法 ====================

std::string ParseResult::toString() const {
    std::stringstream ss;
    ss << "ParseResult {" << std::endl;
    ss << "  success: " << (success ? "true" : "false") << std::endl;
    if (!success) {
        ss << "  error: " << error << std::endl;
    }
    ss << "  packet.type: " << static_cast<int>(packet.type) << std::endl;
    ss << "  packet.nsp: " << packet.nsp << std::endl;
    ss << "  packet.id: " << packet.id << std::endl;
    ss << "  packet.data size: " << packet.data.size() << std::endl;
    ss << "  json_data: " << json_data.substr(0, 100)
       << (json_data.size() > 100 ? "..." : "") << std::endl;
    ss << "  binary_count: " << binary_count << std::endl;
    ss << "  is_binary_packet: " << (is_binary_packet ? "true" : "false") << std::endl;
    ss << "  namespace_str: " << namespace_str << std::endl;
    ss << "}";
    return ss.str();
}

// ==================== PacketParser 单例管理 ====================

PacketParser& PacketParser::getInstance() {
    static PacketParser instance;
    return instance;
}

PacketParser::PacketParser() {
    // 默认配置
    config_.version = SocketIOVersion::V3;
    config_.support_binary = true;
    config_.allow_numeric_nsp = false;
    config_.default_timeout_ms = 30000;
}

void PacketParser::setConfig(const ParserConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = config;
}

ParserConfig PacketParser::getConfig() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

// ==================== 版本检测 ====================

SocketIOVersion PacketParser::detectVersion(const std::string& packet_str) {
    if (packet_str.empty()) {
        return config_.version;
    }
    
    // 检查是否是v3+的ping/pong包（格式不同）
    if (packet_str == "2" || packet_str == "3") {
        // v2使用数字2表示ping，3表示pong
        // v3使用"2"作为ping，但格式不同，这里简化为v2
        return SocketIOVersion::V2;
    }
    
    // 检查连接包格式
    // v2连接包: 0["namespace",{auth data}]
    // v3连接包: 0{"sid":"...","upgrades":[],"pingInterval":25000,"pingTimeout":5000}
    if (packet_str[0] == '0') {
        if (packet_str.size() > 1) {
            if (packet_str[1] == '[') {
                return SocketIOVersion::V2;
            } else if (packet_str[1] == '{') {
                return SocketIOVersion::V3;
            }
        }
    }
    
    // 默认使用配置的版本
    return config_.version;
}

bool PacketParser::isVersion3OrAbove(SocketIOVersion version) {
    return static_cast<int>(version) >= 3;
}

bool PacketParser::supportsNumericNamespaces(SocketIOVersion version) {
    return static_cast<int>(version) >= 3;
}

// ==================== 基础解析辅助方法 ====================

int PacketParser::readNumber(const std::string& str, size_t& cursor) {
    int result = 0;
    bool negative = false;
    
    if (cursor >= str.length()) {
        return -1;
    }
    
    // 处理负号
    if (str[cursor] == '-') {
        negative = true;
        cursor++;
    }
    
    // 读取数字
    while (cursor < str.length() && std::isdigit(str[cursor])) {
        result = result * 10 + (str[cursor] - '0');
        cursor++;
    }
    
    return negative ? -result : result;
}


std::string PacketParser::readUntil(const std::string& str, size_t& cursor, char delimiter) {
    size_t start = cursor;
    while (cursor < str.length() && str[cursor] != delimiter) {
        cursor++;
    }
    
    std::string result = str.substr(start, cursor - start);
    
    if (cursor < str.length() && str[cursor] == delimiter) {
        cursor++; // 跳过分隔符
    }
    
    return result;
}

std::string PacketParser::readJson(const std::string& str, size_t& cursor) {
    if (cursor >= str.length()) {
        return "";
    }
    
    size_t start = cursor;
    int brace_count = 0;
    int bracket_count = 0;
    bool in_string = false;
    char prev_char = 0;
    
    while (cursor < str.length()) {
        char ch = str[cursor];
        
        // 处理字符串内的转义
        if (in_string) {
            if (ch == '"' && prev_char != '\\') {
                in_string = false;
            }
        } else {
            switch (ch) {
                case '"':
                    in_string = true;
                    break;
                case '{':
                    brace_count++;
                    break;
                case '}':
                    brace_count--;
                    break;
                case '[':
                    bracket_count++;
                    break;
                case ']':
                    bracket_count--;
                    break;
            }
        }
        
        prev_char = ch;
        cursor++;
        
        // 如果所有括号都匹配，结束
        if (!in_string && brace_count == 0 && bracket_count == 0) {
            break;
        }
    }
    
    return str.substr(start, cursor - start);
}

// ==================== 核心解析方法 ====================

ParseResult PacketParser::parseImpl(const std::string& packet_str) {
    ParseResult result;
    result.raw_message = packet_str;
    
    if (packet_str.empty()) {
        result.error = "Empty packet string";
        return result;
    }
    
    // 根据版本选择解析方法
    SocketIOVersion version = detectVersion(packet_str);
    
    if (static_cast<int>(version) < 3) {
        return parseV2Format(packet_str);
    } else {
        return parseV3Format(packet_str);
    }
}

ParseResult PacketParser::parseV2Format(const std::string& packet_str) {
    ParseResult result;
    result.raw_message = packet_str;
    
    if (packet_str.empty()) {
        result.error = "Empty packet string";
        return result;
    }
    
    size_t cursor = 0;
    
    // 1. 解析包类型（只读取一个字符）
    if (cursor >= packet_str.length() || !std::isdigit(packet_str[cursor])) {
        result.error = "Invalid packet type: no numeric prefix found";
        return result;
    }
    int type_int = packet_str[cursor] - '0';
    cursor++;
    
    if (!isValidPacketType(type_int)) {
        result.error = "Invalid packet type: " + std::to_string(type_int);
        return result;
    }
    
    PacketType type = static_cast<PacketType>(type_int);
    result.packet.type = type;
    result.is_binary_packet = isBinaryPacketType(type);
    
    // 2. 解析二进制计数（如果是二进制包）
    if (result.is_binary_packet && cursor < packet_str.length() && std::isdigit(packet_str[cursor])) {
        result.binary_count = readNumber(packet_str, cursor);
        // 检查并跳过 '-' 分隔符
        if (cursor < packet_str.length() && packet_str[cursor] == V2_BINARY_SEPARATOR) {
            cursor++;
        }
    }
    
    // 3. 解析命名空间（V2格式：可选的命名空间）
    if (cursor < packet_str.length() && packet_str[cursor] == '/') {
        // 读取命名空间
        size_t start = cursor;
        while (cursor < packet_str.length() && packet_str[cursor] != ',') {
            cursor++;
        }
        
        result.namespace_str = packet_str.substr(start, cursor - start);
        
        // 跳过逗号分隔符（如果有）
        if (cursor < packet_str.length() && packet_str[cursor] == NAMESPACE_SEPARATOR) {
            cursor++;
        }
    } else {
        result.namespace_str = "/";
    }
    
    // 转换为命名空间索引
    result.packet.nsp = namespaceToIndex(result.namespace_str);
    
    // 4. 解析包ID
    if (cursor < packet_str.length() && std::isdigit(packet_str[cursor])) {
        result.packet.id = readNumber(packet_str, cursor);
    } else {
        result.packet.id = -1;
    }
    
    // 5. 解析数据部分
    if (cursor < packet_str.length()) {
        // V2格式数据总是JSON数组或对象
        if (packet_str[cursor] == '[' || packet_str[cursor] == '{') {
            result.json_data = readJson(packet_str, cursor);
            result.packet.data = result.json_data;
        }
        // 否则可能没有数据部分（如connect/disconnect包）
    }
    
    result.success = true;
    return result;
}

ParseResult PacketParser::parseV3Format(const std::string& packet_str) {
    ParseResult result;
    result.raw_message = packet_str;
    
    if (packet_str.empty()) {
        result.error = "Empty packet string";
        return result;
    }
    
    size_t cursor = 0;
    
    // 1. 解析包类型（只读取一个字符）
    if (cursor >= packet_str.length() || !std::isdigit(packet_str[cursor])) {
        result.error = "Invalid packet type: no numeric prefix found";
        return result;
    }
    int type_int = packet_str[cursor] - '0';
    cursor++;
    
    if (!isValidPacketType(type_int)) {
        result.error = "Invalid packet type: " + std::to_string(type_int);
        return result;
    }
    
    PacketType type = static_cast<PacketType>(type_int);
    result.packet.type = type;
    result.is_binary_packet = isBinaryPacketType(type);
    
    // 2. 解析命名空间（V3格式：命名空间在类型后立即开始）
    // 检查是否是数字命名空间（v3+支持）
    if (config_.allow_numeric_nsp && cursor < packet_str.length() && std::isdigit(packet_str[cursor])) {
        // 数字命名空间
        int nsp_num = readNumber(packet_str, cursor);
        result.namespace_str = "/" + std::to_string(nsp_num);
    } else if (cursor < packet_str.length() && packet_str[cursor] == '/') {
        // 字符串命名空间
        size_t start = cursor;
        while (cursor < packet_str.length() && packet_str[cursor] != ',') {
            cursor++;
        }
        result.namespace_str = packet_str.substr(start, cursor - start);
        
        // 跳过逗号分隔符（如果有）
        if (cursor < packet_str.length() && packet_str[cursor] == NAMESPACE_SEPARATOR) {
            cursor++;
        }
    } else {
        result.namespace_str = "/";
    }
    
    // 转换为命名空间索引
    result.packet.nsp = namespaceToIndex(result.namespace_str);
    
    // 3. 解析二进制计数（V3格式：在命名空间后）
    if (result.is_binary_packet && cursor < packet_str.length() && std::isdigit(packet_str[cursor])) {
        result.binary_count = readNumber(packet_str, cursor);
        // 检查并跳过 '-' 分隔符
        if (cursor < packet_str.length() && packet_str[cursor] == V3_BINARY_SEPARATOR) {
            cursor++;
        }
    }
    
    // 4. 解析包ID（V3格式：在二进制计数后）
    if (cursor < packet_str.length() && std::isdigit(packet_str[cursor])) {
        result.packet.id = readNumber(packet_str, cursor);
    } else {
        result.packet.id = -1;
    }
    
    // 5. 解析数据部分
    if (cursor < packet_str.length()) {
        // V3格式数据可以是JSON数组或对象
        if (packet_str[cursor] == '[' || packet_str[cursor] == '{') {
            result.json_data = readJson(packet_str, cursor);
            result.packet.data = result.json_data;
        } else {
            // 可能没有数据或数据是字符串（如connect包的sid）
            // 读取剩余部分作为数据
            result.json_data = packet_str.substr(cursor);
            result.packet.data = result.json_data;
        }
    }
    
    result.success = true;
    return result;
}

// ==================== 公开解析方法 ====================

ParseResult PacketParser::parsePacket(const std::string& packet_str) {
    return parseImpl(packet_str);
}

ParseResult PacketParser::parsePacketWithVersion(const std::string& packet_str, SocketIOVersion version) {
    ParserConfig saved_config = getConfig();
    ParserConfig temp_config = saved_config;
    temp_config.version = version;
    
    setConfig(temp_config);
    ParseResult result = parseImpl(packet_str);
    setConfig(saved_config);
    
    return result;
}

Packet PacketParser::createPacketFromString(const std::string& packet_str) {
    ParseResult result = parsePacket(packet_str);
    if (result.success) {
        return result.packet;
    }
    
    // 解析失败时返回空包
    Packet empty_packet;
    empty_packet.type = PacketType::ERROR;
    empty_packet.nsp = 0;
    empty_packet.id = -1;
    empty_packet.data = "{\"error\":\"" + result.error + "\"}";
    return empty_packet;
}

Json::Value PacketParser::parseJsonData(const std::string& packet_str) {
    std::string json_str = extractJsonString(packet_str);
    if (json_str.empty()) {
        return Json::Value(Json::nullValue);
    }
    
    Json::Value root;
    Json::Reader reader;
    
    if (reader.parse(json_str, root)) {
        return root;
    }
    
    return Json::Value(Json::nullValue);
}

std::string PacketParser::extractJsonString(const std::string& packet_str) {
    ParseResult result = parsePacket(packet_str);
    if (result.success) {
        return result.json_data;
    }
    return "";
}

// ==================== 构建相关方法 ====================

std::string PacketParser::buildImpl(const Packet& packet, const BuildOptions& options) {
    SocketIOVersion version = config_.version;
    
    // 直接使用当前配置的版本来选择构建方法
    // v2版本使用buildV2Format，v3版本使用buildV3Format
    if (static_cast<int>(version) < 3) {
        return buildV2Format(packet, options);
    } else {
        return buildV3Format(packet, options);
    }
}

// ==================== buildV2Format 修复 ====================
std::string PacketParser::buildV2Format(const Packet& packet, const BuildOptions& options) {
    std::stringstream ss;
    
    // 1. 包类型
    ss << static_cast<int>(packet.type);
    
    // 2. 二进制计数（V2格式：在类型后，命名空间前）
    bool is_binary_type = isBinaryPacketType(packet.type);
    int binary_count = countBinaryPlaceholders(packet.data);
    
    if (is_binary_type && binary_count > 0) {
        // V2格式：51- 表示类型5，有1个二进制
        ss << binary_count << V2_BINARY_SEPARATOR;
    }
    
    // 3. 命名空间
    std::string nsp_str = options.namespace_str.empty() ? indexToNamespace(packet.nsp) : options.namespace_str;
    if (nsp_str != "/" && !nsp_str.empty()) {
        ss << nsp_str;
        // V2格式：命名空间后需要逗号分隔符（如果有包ID或数据）
        if (packet.id >= 0 || !packet.data.empty()) {
            ss << NAMESPACE_SEPARATOR;
        }
    }
    
    // 4. 包ID（ACK ID）
       if (packet.id >= 0) {
           ss << packet.id;
       }
    
    // 5. 数据部分
    if (!packet.data.empty()) {
        ss << packet.data;
    }
    
    return ss.str();
}

// ==================== buildV3Format 修复 ====================
std::string PacketParser::buildV3Format(const Packet& packet, const BuildOptions& options) {
    std::stringstream ss;
    
    // 1. 包类型
    ss << static_cast<int>(packet.type);
    
    // 2. 命名空间（V3格式：命名空间紧跟在类型后）
    std::string nsp_str = options.namespace_str.empty() ? indexToNamespace(packet.nsp) : options.namespace_str;
    if (nsp_str != "/" && !nsp_str.empty()) {
        ss << nsp_str;
        // V3格式：命名空间后需要有逗号分隔符
        if (packet.id >= 0 || !packet.data.empty()) {
            ss << NAMESPACE_SEPARATOR;
        }
    }
    
    // 3. 二进制计数（V3格式：不包含二进制计数）
    // V3版本的Socket.IO数据包不再包含二进制计数
    // 二进制数据通过后续的二进制帧发送，通过占位符索引关联
    
    // 4. 包ID（ACK ID）
    if (packet.id >= 0) {
           // 对于ACK包，ACK ID在二进制计数后
           ss << packet.id;
       }
    
    // 5. 数据部分
    if (!packet.data.empty()) {
        ss << packet.data;
    }
    
    return ss.str();
}

// ==================== 公开构建方法 ====================

std::string PacketParser::buildPacketString(const Packet& packet, const BuildOptions& options) {
    return buildImpl(packet, options);
}

std::string PacketParser::buildEventString(
    const std::string& event_name,
    const Json::Value& data,
    int ack_id,
    const std::string& nsp,
    bool is_binary) {
    
    Packet packet;
    packet.type = is_binary ? PacketType::BINARY_EVENT : PacketType::EVENT;
    packet.nsp = namespaceToIndex(nsp);
    packet.id = ack_id;  // 这里设置ACK ID
    
    // 构建数据数组
    Json::Value data_array(Json::arrayValue);
    data_array.append(Json::Value(event_name));
    
    if (!data.isNull()) {
        if (data.isArray()) {
            for (Json::ArrayIndex i = 0; i < data.size(); i++) {
                data_array.append(data[i]);
            }
        } else {
            data_array.append(data);
        }
    }
    
    // 如果ack_id >= 0，表示需要ACK，将ACK ID添加到JSON数据末尾
    if (ack_id >= 0) {
        // 对于事件包，ACK ID在JSON数组的末尾
        data_array.append(Json::Value(ack_id));
    }
    
    // 序列化JSON
    Json::FastWriter writer;
    packet.data = writer.write(data_array);
    
    BuildOptions options;
    options.namespace_str = nsp;
    options.force_binary_type = is_binary;
    options.include_binary_count = is_binary;  // 二进制包才包含二进制计数
    
    return buildPacketString(packet, options);
}

std::string PacketParser::buildAckString(
    int ack_id,
    const Json::Value& data,
    const std::string& nsp,
    bool is_binary) {
    
    Packet packet;
    packet.type = is_binary ? PacketType::BINARY_ACK : PacketType::ACK;
    packet.nsp = namespaceToIndex(nsp);
    packet.id = ack_id;  // ACK包的ID就是ACK ID
    
    // 构建数据数组 - ACK包格式不同
    Json::Value data_array(Json::arrayValue);
    // ACK包的第一个元素是ACK ID
    data_array.append(Json::Value(ack_id));
    
    if (!data.isNull()) {
        if (data.isArray()) {
            for (Json::ArrayIndex i = 0; i < data.size(); i++) {
                data_array.append(data[i]);
            }
        } else {
            data_array.append(data);
        }
    }
    
    // 序列化JSON
    Json::FastWriter writer;
    packet.data = writer.write(data_array);
    
    BuildOptions options;
    options.namespace_str = nsp;
    options.include_binary_count = is_binary;
    
    return buildPacketString(packet, options);
}

// 其他构建方法类似实现...
std::string PacketParser::buildConnectString(
    const Json::Value& auth_data,
    const std::string& nsp,
    const Json::Value& query_params) {
    
    Packet packet;
    packet.type = PacketType::CONNECT;
    packet.nsp = namespaceToIndex(nsp);
    packet.id = -1;
    
    // V2和V3的connect包格式不同
    SocketIOVersion version = config_.version;
    
    Json::FastWriter writer;
    if (static_cast<int>(version) < 3) {
        // V2格式：数组，包含命名空间和认证数据
        Json::Value data_array(Json::arrayValue);
        data_array.append(Json::Value(nsp));
        
        if (!auth_data.isNull()) {
            data_array.append(auth_data);
        }
        
        packet.data = writer.write(data_array);
    } else {
        // V3格式：对象，包含认证数据和配置
        Json::Value connect_obj(Json::objectValue);
        
        if (!auth_data.isNull()) {
            if (auth_data.isObject() && auth_data.isMember("token")) {
                connect_obj["auth"] = auth_data;
            } else {
                // 简单处理：将auth_data作为认证数据
                Json::Value auth_obj(Json::objectValue);
                auth_obj["token"] = auth_data;
                connect_obj["auth"] = auth_obj;
            }
        }
        
        // 添加查询参数（如果有）
        if (!query_params.isNull() && query_params.isObject()) {
            connect_obj["query"] = query_params;
        }
        
        packet.data = writer.write(connect_obj);
    }
    
    BuildOptions options;
    options.namespace_str = nsp;
    
    return buildPacketString(packet, options);
}

std::string PacketParser::buildDisconnectString(const std::string& nsp) {
    Packet packet;
    packet.type = PacketType::DISCONNECT;
    packet.nsp = namespaceToIndex(nsp);
    packet.id = -1;
    
    BuildOptions options;
    options.namespace_str = nsp;
    
    return buildPacketString(packet, options);
}

std::string PacketParser::buildErrorString(
    const std::string& error_message,
    const Json::Value& error_data,
    const std::string& nsp) {
    
    Packet packet;
    packet.type = PacketType::ERROR;
    packet.nsp = namespaceToIndex(nsp);
    packet.id = -1;
    
    // 构建错误对象
    Json::Value error_obj(Json::objectValue);
    error_obj["message"] = Json::Value(error_message);
    
    if (!error_data.isNull()) {
        if (error_data.isObject()) {
            // 合并错误数据
            Json::Value::Members members = error_data.getMemberNames();
            for (const auto& key : members) {
                error_obj[key] = error_data[key];
            }
        } else {
            error_obj["data"] = error_data;
        }
    }
    
    Json::FastWriter writer;
    packet.data = writer.write(error_obj);
    
    BuildOptions options;
    options.namespace_str = nsp;
    
    return buildPacketString(packet, options);
}

// ==================== 辅助方法 ====================

bool PacketParser::isBinaryPacket(const std::string& packet_str) {
    if (packet_str.empty()) {
        return false;
    }
    
    char first_char = packet_str[0];
    if (!std::isdigit(first_char)) {
        return false;
    }
    
    int type_int = first_char - '0';
    return type_int == 5 || type_int == 6; // BINARY_EVENT 或 BINARY_ACK
}

int PacketParser::countBinaryPlaceholders(const std::string& data) {
    // 直接从数据中计算占位符数量，不进行包解析
    int count = 0;
    size_t pos = 0;
    std::string placeholder = "\"_placeholder\":true";
    
    while ((pos = data.find(placeholder, pos)) != std::string::npos) {
        count++;
        pos += placeholder.length();
    }
    
    return count;
}

PacketType PacketParser::getPacketType(const std::string& packet_str) {
    if (packet_str.empty() || !std::isdigit(packet_str[0])) {
        return PacketType::ERROR;
    }
    
    int type_int = packet_str[0] - '0';
    if (isValidPacketType(type_int)) {
        return static_cast<PacketType>(type_int);
    }
    
    return PacketType::ERROR;
}

// 修复的静态函数调用和类型转换
int PacketParser::getPacketId(const std::string& packet_str) {
    ParseResult result = getInstance().parsePacket(packet_str);
    return result.packet.id;
}

std::string PacketParser::getNamespace(const std::string& packet_str) {
    ParseResult result = getInstance().parsePacket(packet_str);
    return result.namespace_str;
}

bool PacketParser::validatePacket(const std::string& packet_str) {
    ParseResult result = getInstance().parsePacket(packet_str);
    return result.success;
}

// 修复std::stoul返回值类型问题
std::string PacketParser::readString(const std::string& str, size_t& cursor) {
    if (cursor >= str.length() || str[cursor] != '"') {
        return "";
    }
    
    cursor++; // 跳过开头的双引号
    std::string result;
    
    while (cursor < str.length() && str[cursor] != '"') {
        // 处理转义字符
        if (str[cursor] == '\\') {
            cursor++;
            if (cursor < str.length()) {
                switch (str[cursor]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        // Unicode转义序列
                        if (cursor + 4 < str.length()) {
                            std::string hex = str.substr(cursor + 1, 4);
                            try {
                                unsigned long code = std::stoul(hex, nullptr, 16); // 修复这里
                                // 简化处理：只支持基本多文种平面
                                if (code <= 0x7F) {
                                    result += static_cast<char>(code);
                                } else if (code <= 0x7FF) {
                                    result += static_cast<char>(0xC0 | (code >> 6));
                                    result += static_cast<char>(0x80 | (code & 0x3F));
                                } else if (code <= 0xFFFF) {
                                    result += static_cast<char>(0xE0 | (code >> 12));
                                    result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                                    result += static_cast<char>(0x80 | (code & 0x3F));
                                }
                            } catch (...) {
                                // 转换失败
                            }
                            cursor += 4;
                        }
                        break;
                    }
                    default:
                        result += str[cursor];
                        break;
                }
            }
        } else {
            result += str[cursor];
        }
        cursor++;
    }
    
    if (cursor < str.length() && str[cursor] == '"') {
        cursor++; // 跳过结尾的双引号
    }
    
    return result;
}

// 同样的修复在unescapeJsonString中
std::string PacketParser::unescapeJsonString(const std::string& str) {
    std::string result;
    
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '\\' && i + 1 < str.length()) {
            i++;
            switch (str[i]) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u': {
                    // Unicode转义序列
                    if (i + 4 < str.length()) {
                        std::string hex = str.substr(i + 1, 4);
                        try {
                            unsigned long code = std::stoul(hex, nullptr, 16); // 修复这里
                            if (code <= 0x7F) {
                                result += static_cast<char>(code);
                            } else if (code <= 0x7FF) {
                                result += static_cast<char>(0xC0 | (code >> 6));
                                result += static_cast<char>(0x80 | (code & 0x3F));
                            } else if (code <= 0xFFFF) {
                                result += static_cast<char>(0xE0 | (code >> 12));
                                result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                                result += static_cast<char>(0x80 | (code & 0x3F));
                            }
                        } catch (...) {
                            // 转换失败，保留原字符
                            result += "\\u" + hex;
                        }
                        i += 4;
                    }
                    break;
                }
                default:
                    result += str[i];
                    break;
            }
        } else {
            result += str[i];
        }
    }
    
    return result;
}

// ==================== 工具方法 ====================

std::string PacketParser::escapeJsonString(const std::string& str) {
    std::stringstream ss;
    
    for (char c : str) {
        switch (c) {
            case '"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '/': ss << "\\/"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                // 控制字符需要转义为Unicode
                if (c >= 0 && c <= 0x1F) {
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<int>(c);
                } else {
                    ss << c;
                }
                break;
        }
    }
    
    return ss.str();
}

// ==================== 验证和辅助方法 ====================

bool PacketParser::isValidPacketType(int type) {
    return type >= 0 && type <= 6;
}

bool PacketParser::isBinaryPacketType(PacketType type) {
    return type == PacketType::BINARY_EVENT || type == PacketType::BINARY_ACK;
}

std::string PacketParser::normalizeNamespace(const std::string& nsp) {
    if (nsp.empty() || nsp == "/") {
        return "/";
    }
    
    // 确保以斜杠开头
    if (nsp[0] != '/') {
        return "/" + nsp;
    }
    
    return nsp;
}

int PacketParser::namespaceToIndex(const std::string& nsp) {
    // 简化实现：将命名空间映射为索引
    // 在实际项目中，你可能需要维护一个命名空间映射表
    if (nsp == "/") {
        return 0;
    }
    
    // 简单哈希计算
    std::hash<std::string> hasher;
    return static_cast<int>(hasher(nsp) % 1000);
}

std::string PacketParser::indexToNamespace(int index) {
    // 简化实现：只支持默认命名空间
    if (index == 0) {
        return "/";
    }
    
    // 在实际项目中，你可能需要根据索引查找命名空间
    return "/"; // 默认返回根命名空间
}

std::string PacketParser::createBinaryPlaceholder(int index) {
    Json::Value placeholder(Json::objectValue);
    placeholder["_placeholder"] = true;
    placeholder["num"] = index;
    
    Json::FastWriter writer;
    return writer.write(placeholder);
}

int PacketParser::parseBinaryPlaceholder(const Json::Value& json) {
    if (json.isObject() &&
        json.isMember("_placeholder") &&
        json["_placeholder"].isBool() &&
        json["_placeholder"].asBool() &&
        json.isMember("num") &&
        json["num"].isInt()) {
        return json["num"].asInt();
    }
    return -1;
}

// ==================== 日志方法 ====================

void PacketParser::logError(const std::string& message) const {
    // 在实际项目中，这里应该使用日志系统
    RTC_LOG(LS_ERROR) << "[Socket.IO Parser Error] " << message;
}

void PacketParser::logDebug(const std::string& message) const {
    // 在实际项目中，这里应该使用日志系统
    RTC_LOG(LS_INFO) << "[Socket.IO Parser Debug] " << message;
}

} // namespace sio
