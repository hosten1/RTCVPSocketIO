//
//  sio_packet.cpp
//  LYMVPSocketIO
//
//  Created by luoyongmeng on 2025/12/26.
//

#include "lib/sio_packet.h"
#include "rtc_base/logging.h"


namespace sio {
SIOHeader::SIOHeader() : version_(SocketIOVersion::V3), type_(PacketType::EVENT), namespace_str_("/"),ack_id_(-1), binary_count_(0), data_start_pos_(0) {}

SIOHeader::SIOHeader(SocketIOVersion versionIn) : version_(versionIn), type_(PacketType::EVENT), namespace_str_("/"), ack_id_(-1), binary_count_(0), data_start_pos_(0) {}

SIOHeader::SIOHeader(const SIOHeader& other)
    : version_(other.version_)
    , type_(other.type_)
    , namespace_str_(other.namespace_str_)
    , ack_id_(other.ack_id_)
    , binary_count_(other.binary_count_)
    , has_binary_(other.has_binary_)
    , data_start_pos_(other.data_start_pos_) {
}


// 解析包头部
bool SIOHeader::parse(const std::string& packet){
    RTC_LOG(LS_INFO) << "Parsing packet header with version: " << static_cast<int>(version_);
    // 检查是否有二进制数据
    switch (version_) {
        case SocketIOVersion::V2:
            return parse_v2_header(packet);
        case SocketIOVersion::V3:
            return parse_v3_header(packet);
        case SocketIOVersion::V4:
            return parse_v4_header(packet);
        default:
            RTC_LOG(LS_WARNING) << "Unknown version, using V3 parser: " << static_cast<int>(version_);
            return parse_v3_header(packet);
    }
}

// 构建包头部字符串
bool SIOHeader::build_sio_string(
                                        SocketIOVersion version,
                                        PacketType type,
                                        std::string namespace_str,
                                        int ack_id,
                                        int binary_count,
                                        std::stringstream& ss
                                    ){
    version_        = version;
    type_           = type;
    namespace_str_  = std::move(namespace_str);
    ack_id_         = ack_id;
    binary_count_   = binary_count;
    if (version_ == SocketIOVersion::V2) {
        build_v2_sio_str(binary_count, ss);
    }else if(version_ == SocketIOVersion::V3){
        build_v3_sio_str(binary_count, ss);
    }else{
        build_v3_sio_str(binary_count, ss);
    }
}

// 生成调试信息字符串
std::string SIOHeader::to_string() const{
    std::ostringstream oss;
       oss << "Parsed header: version:"<< static_cast<int>(version_)
           << " type=" << static_cast<int>(type_)
           << ", namespace=" << namespace_str_
           << ", ack_id=" << ack_id_
           << ", binary_count=" << binary_count_
           << ", data_start_pos=" << data_start_pos_;
       return oss.str();
}


/*** @brief 解析V2协议的数据包头部
 * @param packet 完整的数据包字符串
 * @return bool 是否成功
 * @note V2协议头部格式：[type][namespace][,ack_id][data]
 *       - type: 数据包类型（1字节数字）
 *       - namespace: 可选，以"/"开头
 *       - ack_id: 可选，数字字符串
 *       - data: JSON数据部分
 * @example V2数据包示例： 51-/chat,0["binaryEvent",{"text":"testData",...},{"_placeholder":true,"num":0}]
 *          - type: 5 (EVENT binary)
 *          - attachments: 1 有一个二进制
 *          - namespace: /chat
 *          - ack_id: 1
 *          - data: ["hello"]
 */
bool SIOHeader::parse_v2_header(const std::string& packet) {
    
    // Socket.IO v2 header 顺序统一规则：
    // <packetType>[<binaryCount>-][<namespace>][,<ackId>]<json>
    
    size_t pos = 0;
    
    // 1. 包类型
    if (!isdigit(packet[pos])) {
        RTC_LOG(LS_WARNING) << "Invalid packet type at start: " << packet[pos];
        return false;
    }
    int type_num = packet[pos++] - '0';
    type_ = static_cast<PacketType>(type_num);
    
    // 检查是否为二进制包
    bool is_binary_packet = (type_ == PacketType::BINARY_EVENT ||
                             type_ == PacketType::BINARY_ACK);
    
    // 2. 二进制计数和减号（二进制包特有）
    if (is_binary_packet) {
        size_t count_start = pos;
        while (pos < packet.size() && isdigit(packet[pos])) {
            pos++;
        }
        
        if (pos > count_start) {
            std::string count_str = packet.substr(count_start, pos - count_start);
            try {
                binary_count_ = std::stoi(count_str);
                RTC_LOG(LS_INFO) << "Parsed binary count: " << binary_count_;
            } catch (...) {
                RTC_LOG(LS_WARNING) << "Failed to parse binary count: " << count_str;
                binary_count_ = 0;
            }
        }
        
        // 检查减号
        if (pos < packet.size() && packet[pos] == '-') {
            pos++;
        }
    }
    RTC_LOG(LS_INFO) << "V2 binary packet detected, binary_count=" << binary_count_;
    
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
            namespace_str_ = packet.substr(nsp_start, pos - nsp_start);
        }
    } else {
        namespace_str_ = "/";
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
                ack_id_ = std::stoi(ack_str);
            } catch (...) {
                RTC_LOG(LS_WARNING) << "Failed to parse ACK ID: " << ack_str;
                ack_id_ = -1;
            }
        }
    }
    
    // 6. 数据起始位置
    data_start_pos_ = pos;
    
    RTC_LOG(LS_INFO) << "Parsed V2 "  << to_string();
    
    return true;
}

/*** @brief 解析V3协议的数据包头部
 * @param packet 完整的数据包字符串
 * @return bool 是否成功
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
 */

bool SIOHeader::parse_v3_header(const std::string& packet) {

    size_t pos = 0;
    
    // 1. 包类型
    if (!isdigit(packet[pos])) {
        RTC_LOG(LS_WARNING) << "Invalid packet type at start: " << packet[pos];
        return false;
    }
    int type_num = packet[pos++] - '0';
    type_ = static_cast<PacketType>(type_num);
    
    // 检查是否为二进制包
    bool is_binary_packet = (type_ == PacketType::BINARY_EVENT ||
                             type_ == PacketType::BINARY_ACK);
    
    // 2. 二进制计数和减号（二进制包特有）
    if (is_binary_packet) {
        size_t count_start = pos;
        while (pos < packet.size() && isdigit(packet[pos])) {
            pos++;
        }
        
        if (pos > count_start) {
            std::string count_str = packet.substr(count_start, pos - count_start);
            try {
                binary_count_ = std::stoi(count_str);
                RTC_LOG(LS_INFO) << "Parsed binary count: " << binary_count_;
            } catch (...) {
                RTC_LOG(LS_WARNING) << "Failed to parse binary count: " << count_str;
                binary_count_ = 0;
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
            namespace_str_ = packet.substr(nsp_start, pos - nsp_start);
        }
    } else {
        namespace_str_ = "/";
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
                ack_id_ = std::stoi(ack_str);
            } catch (...) {
                RTC_LOG(LS_WARNING) << "Failed to parse ACK ID: " << ack_str;
                ack_id_ = -1;
            }
        }
    }
    
    // 数据起始位置
    data_start_pos_ = pos;
    
    RTC_LOG(LS_INFO) << "Parsed V3 " << to_string();
    
    return true;
}

/*** @brief 解析V4协议的数据包头部
 * @param packet 完整的数据包字符串
 * @return bool 是否成功
 * @note V4协议与V3协议数据包格式几乎相同，直接复用V3的解析逻辑
 *       未来如果V4协议有变化，可以在这里单独处理
 */
bool SIOHeader::parse_v4_header(const std::string& packet) {
    RTC_LOG(LS_INFO) << "Parsing V4 header (using V3 parser)";
    // V4协议与V3非常相似，暂时使用相同的解析逻辑
    return parse_v3_header(packet);
}


bool SIOHeader::build_v2_sio_str(const int binary_count,std::stringstream& ss) {
    RTC_LOG(LS_INFO) << "Encoding V2 packet " << to_string();
    
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
    
    // 构建V2文本包
    
    // V2协议使用的包类型编号
    int packet_type;
    if (is_binary()) {
        if (type_ == PacketType::EVENT || type_ == PacketType::BINARY_EVENT) {
            packet_type = static_cast<int>(PacketType::BINARY_EVENT); // V2: 5
        } else {
            packet_type = static_cast<int>(PacketType::BINARY_ACK); // V2: 6
        }
    } else {
        if (type_ == PacketType::EVENT || type_ == PacketType::BINARY_EVENT) {
            packet_type = static_cast<int>(PacketType::EVENT); // V2: 2
        } else {
            packet_type = static_cast<int>(PacketType::ACK); // V2: 3
        }
    }
    
    
    // 关键修复：V2协议二进制包需要在类型后添加 "{binary_count}-"
     if (is_binary()) {
         ss << packet_type << binary_count << "-";
     } else {
         ss << packet_type;
     }
     
     RTC_LOG(LS_INFO) << "V2 packet type: " << packet_type << " for original type: " << static_cast<int>(type_);
    
    bool has_namespace = (!namespace_str_.empty() && namespace_str_ != "/");
    // 命名空间（如果不是根命名空间）
    if (has_namespace) {
        // 确保命名空间以斜杠开头
        if (namespace_str_[0] != '/') {
            ss << "/" << namespace_str_;
        } else {
            ss << namespace_str_;
        }
        RTC_LOG(LS_INFO) << "Added V2 namespace: " << namespace_str_;
    }
    
    // 添加ACK ID（如果有）
    bool has_ack_id = (ack_id_ >= 0);
    
    // ACK ID（如果有）
    if (has_ack_id) {
        // 如果有命名空间且不是根，需要逗号分隔
        if (has_namespace) {
            ss << ",";
        }
        ss << ack_id_;
        RTC_LOG(LS_INFO) << "Added V2 ACK ID: " << ack_id_;
    }
    // V2二进制计数在JSON中处理，不在头部
    
   
    RTC_LOG(LS_INFO) << "Encoded V2 packet content: " << ss;
    return true;
}


bool SIOHeader::build_v3_sio_str(const int binary_count,std::stringstream& ss){
    // 确定包类型
    int packet_type = static_cast<int>(type_);
    if (is_binary()) {
        // 二进制包类型转换
        if (type_ == PacketType::EVENT) {
            packet_type = static_cast<int>(PacketType::BINARY_EVENT);
        } else if (type_ == PacketType::ACK) {
            packet_type = static_cast<int>(PacketType::BINARY_ACK);
        }
        RTC_LOG(LS_INFO) << "Converted to binary packet type: " << packet_type;
    }
    ss << packet_type;
    
    // 二进制计数（如果是二进制包）
    if (is_binary() && binary_count > 0) {
        ss << binary_count << "-";
        RTC_LOG(LS_INFO) << "Added binary count: " << binary_count;
    }
    
    bool has_namespace = (!namespace_str_.empty() && namespace_str_ != "/");
    // 命名空间（如果不是根命名空间）
    if (has_namespace) {
        if (namespace_str_[0] != '/') {
            ss << "/" << namespace_str_;
        } else {
            ss << namespace_str_;
        }
        RTC_LOG(LS_INFO) << "Added namespace: " <<namespace_str_;
    }
    
    // ACK ID（如果有）
    if (ack_id_ >= 0) {
        // 如果有命名空间且不是根，需要逗号分隔
        if (has_namespace) {
            ss << ",";
        }
        ss << ack_id_;
        RTC_LOG(LS_INFO) << "Added ACK ID: " << ack_id_;
    }
    return true;
}



bool SIOHeader::need_ack(){
    return ack_id_ >= 0;
}




SIOBody::SIOBody(SocketIOVersion versionIn):version_(versionIn) {}

// 解析包体
bool SIOBody::parse(const std::string& text_packet, const SIOHeader& header, const std::vector<SmartBuffer>& binary_parts){
    RTC_LOG(LS_INFO) << "Decoding packet, text length: " << text_packet.length() << ", binary parts: " << binary_parts.size();
    
    if (text_packet.empty()) {
        RTC_LOG(LS_WARNING) << "Empty text packet, returning empty packet";
        return false;
    }
    switch (version_) {
        case SocketIOVersion::V2:
            RTC_LOG(LS_INFO) << "Using V2 encoder";
            return parse_v2(text_packet, header, binary_parts);
            break;
        case SocketIOVersion::V3:
            RTC_LOG(LS_INFO) << "Using V3 encoder";
            return parse_v3(text_packet, header, binary_parts);
            break;
        case SocketIOVersion::V4:
        default:
            RTC_LOG(LS_INFO) << "Using V3 encoder for V4 packet";
            // V4默认使用V3格式
            return parse_v3(text_packet, header, binary_parts);
            break;
    }
    
}

// 构建包体
std::string SIOBody::build(const SIOHeader& header,bool isEvent){
    PacketType type = isEvent ? PacketType::EVENT : PacketType::ACK;
    // 检查是否有二进制数据
    int binary_count = 0;
    for (const auto& arg : args_) {
        if (binary_helper::is_binary(arg)) {
            has_binary_ = true;
            binary_count++;
        
        }
    }
    if (isEvent) {
        if (has_binary_) {
            type = PacketType::BINARY_EVENT;
        }else{
            type = PacketType::EVENT;
        }
    }else{
        if (has_binary_) {
            type = PacketType::BINARY_ACK;
        }else{
            type = PacketType::ACK;
        }
    }
    header_ = std::make_shared<SIOHeader>(header);
    std::string result;
    switch (version_) {
        case SocketIOVersion::V2:
            RTC_LOG(LS_INFO) << "Using V2 encoder";
            result = build_v2(type);
            break;
        case SocketIOVersion::V3:
            RTC_LOG(LS_INFO) << "Using V3 encoder";
            result = build_v3(type);
            break;
        case SocketIOVersion::V4:
        default:{
            RTC_LOG(LS_INFO) << "Using V3 encoder for V4 packet";
            // V4默认使用V3格式
            result = build_v4(type);
            break;
        }
            
    }
    return result;
}


// 生成调试信息字符串
std::string SIOBody::to_string() const{
    
}



// 解析V2协议包体
bool SIOBody::parse_v2(const std::string& packet, const SIOHeader& header,const std::vector<SmartBuffer>& binaries){
    // 获取数据部分
    if ((header.type() != PacketType::CONNECT) && header.data_start_pos() >= packet.length()) {
        RTC_LOG(LS_WARNING) << "Data start position beyond text length for non-CONNECT packet, returning packet without data";
        return false;
    }
    std::string text = packet;
    
    std::string json_str = text.substr(header.data_start_pos());
    json_data_ = json_str;
    
    if ((header.type() != PacketType::CONNECT) && json_str.empty() ) {
        RTC_LOG(LS_WARNING) << "Empty JSON data for non-CONNECT packet, returning packet without data";
        return false;
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
            return false;
        }
        
        // 调试输出
        RTC_LOG(LS_INFO) << "Decoded V2 packet:" << header.to_string();
    
        RTC_LOG(LS_INFO) << "  JSON value type: " << (json_value.isArray() ? "Array" : json_value.isObject() ? "Object" : "Other");
        
        
        if (parsing_successful) {
            if (json_value.isObject()) {
                RTC_CHECK(false) << "v2 is not object!!!";
            } else if (json_value.isArray()) {
                // 检查包类型
                if (header.type() == PacketType::EVENT || header.type()== PacketType::BINARY_EVENT) {
                    // 事件包数组格式：["event_name", ...args]
                    if (json_value.size() > 0) {
                        // 第一个元素是事件名称
                       event_name_ = json_value[0].asString();
                        RTC_LOG(LS_INFO) << "Extracted V2 event name from array: " << event_name_;
                        
                        // 剩下的元素是参数
                        for (Json::ArrayIndex i = 1; i < json_value.size(); i++) {
                            Json::Value restored_arg = json_value[i];
                            // 处理二进制数据
                            std::map<std::string, int> binary_map;
                            restore_binary_data(restored_arg, binaries, binary_map);
                            args_.push_back(restored_arg);
                        }
                        RTC_LOG(LS_INFO) << "Processed V2 array event args, count: " << args_.size();
                    }
                } else {
                    // ACK包：直接是参数数组
                    std::map<std::string, int> binary_map;
                    for (Json::ArrayIndex i = 0; i < json_value.size(); i++) {
                        Json::Value restored_arg = json_value[i];
                        restore_binary_data(restored_arg, binaries, binary_map);
                        args_.push_back(restored_arg);
                    }
                    RTC_LOG(LS_INFO) << "Processed V2 ACK array, args count: " << args_.size();
                }
            }
        }
    }
    RTC_LOG(LS_INFO) << "Decoded V2 packet completed, event: " << event_name_ << ", args: " << args_.size();
}

// 解析V3协议包体
bool SIOBody::parse_v3(const std::string& packet, const SIOHeader& header, const std::vector<SmartBuffer>& binaries){
    std::string text = packet;
    // 获取数据部分
    if (header.data_start_pos() >= text.length()) {
        RTC_LOG(LS_WARNING) << "Data start position beyond text length, returning packet without data";
        return false;
    }
    
    std::string json_str = text.substr(header.data_start_pos());
    json_data_ = json_str;
    if (json_str.empty()) {
        RTC_LOG(LS_WARNING) << "Empty JSON data, returning packet without data";
        return false;
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
        return false;
    }
    
    // 调试输出
    RTC_LOG(LS_INFO) << "Decoded V3 packet:" << header.to_string();
    RTC_LOG(LS_INFO) << "  Binaries received: " << binaries.size();
    RTC_LOG(LS_INFO) << "  JSON value type: " << (json_value.isArray() ? "Array" : json_value.isObject() ? "Object" : "Other");
    
    if (parsing_successful) {
        if (header.type() == PacketType::ACK || header.type() == PacketType::BINARY_ACK) {
            // ACK包：直接是参数数组
            std::map<std::string, int> binary_map;
            for (Json::ArrayIndex i = 0; i < json_value.size(); i++) {
                Json::Value restored_arg = json_value[i];
                // 恢复二进制数据
                restore_binary_data(restored_arg, binaries, binary_map);
                args_.push_back(restored_arg);
            }
            RTC_LOG(LS_INFO) << "Processed ACK packet, args count: " << args_.size();
        } else {
            // 事件包：["event_name", ...args]
            if (json_value.isArray() && json_value.size() > 0) {
                event_name_ = json_value[0].asString();
                
                // 恢复二进制数据到参数中
                std::map<std::string, int> binary_map;
                for (Json::ArrayIndex i = 1; i < json_value.size(); i++) {
                    Json::Value restored_arg = json_value[i];
                    // 恢复二进制数据
                    restore_binary_data(restored_arg, binaries, binary_map);
                    args_.push_back(restored_arg);
                }
                RTC_LOG(LS_INFO) << "Processed EVENT packet, event: " << event_name_ << ", args count: " << args_.size();
            } else if (json_value.isObject()) {
                // 可能是CONNECT或DISCONNECT包的响应数据
                Json::Value restored_arg = json_value;
                std::map<std::string, int> binary_map;
                restore_binary_data(restored_arg, binaries, binary_map);
                args_.push_back(restored_arg);
                RTC_LOG(LS_INFO) << "Processed OBJECT packet, args count: " << args_.size();
            }
        }
    }
    
    RTC_LOG(LS_INFO) << "Decoded V3 packet completed, event: " << event_name_ << ", args: " << args_.size();
}

// 解析V4协议包体
bool SIOBody::parse_v4(const std::string& packet, const SIOHeader& header, const std::vector<SmartBuffer>& binaries){
    
}

// 构建V2协议包体
std::string SIOBody::build_v2(const PacketType type) {
    // 构建JSON数据
    Json::Value json_sio_body(Json::arrayValue);
    std::vector<SmartBuffer> binary_parts;
    std::map<std::string, int> binary_map;
    
    // V2协议：EVENT格式为 ["event_name", ...args]
    //         ACK格式为 [...args]
    if (type == PacketType::ACK || type == PacketType::BINARY_ACK) {
        /*****************************
         ACK格式（V2协议）：
         3/chat,22[...args]    // 普通ACK
         6-/chat,22[...args]   // BINARY_ACK（有1个二进制附件）
         
         JSON结构：
         [...args]  // 只包含参数数组，不包含ackId
         
         注意：ackId在header中，不在JSON中
         *****************************/

        RTC_DCHECK(ack_id_ >= 0) << "ACK包必须有ack_id";
        // ACK包：只有参数数组
        for (const auto& arg : args_) {
            Json::Value processed_arg;
            extract_binary_data(arg, processed_arg, binary_parts, binary_map);
            json_sio_body.append(processed_arg);
        }
        
        RTC_LOG(LS_INFO) << "Built V2 ACK JSON data, args count: "
        << args_.size() << ", ackId: " << ack_id_;
      
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
        RTC_DCHECK(!event_name_.empty()) << "EVENT包必须有event_name";
        // V2事件包：["event_name",  {}]
        // EVENT包：第一个元素是事件名
        json_sio_body.append(event_name_);
        
        
        // 然后是参数数组
        for (const auto& arg : args_) {
            Json::Value processed_arg;
            extract_binary_data(arg, processed_arg, binary_parts, binary_map);
            json_sio_body.append(processed_arg);
        }
        
        RTC_LOG(LS_INFO) << "Built V2 EVENT JSON data, event: "
        << event_name_ << ", args count: " << args_.size();
    }
    // 确定是否为二进制包 - 基于实际提取的二进制数据数量
    attachments = binary_parts;
    int binary_count = static_cast<int>(binary_parts.size());
    bool is_binary = !binary_parts.empty();
    
    RTC_LOG(LS_INFO) << "Extracted V2 binary parts: " << binary_parts.size() << ", is_binary: " << is_binary;
    
    // 构建V2文本包
    std::stringstream ss;
    
    // V2协议使用的包类型编号
    int packet_type;
    if (has_binary_) {
        if (type == PacketType::EVENT || type == PacketType::BINARY_EVENT) {
            packet_type = static_cast<int>(PacketType::BINARY_EVENT); // V2: 5
        } else {
            packet_type = static_cast<int>(PacketType::BINARY_ACK); // V2: 6
        }
    } else {
        if (type == PacketType::EVENT || type == PacketType::BINARY_EVENT) {
            packet_type = static_cast<int>(PacketType::EVENT); // V2: 2
        } else {
            packet_type = static_cast<int>(PacketType::ACK); // V2: 3
        }
    }
    
    
    // 关键修复：V2协议二进制包需要在类型后添加 "{binary_count}-"
     if (has_binary_) {
         ss << packet_type << binary_count << "-";
     } else {
         ss << packet_type;
     }
     
    RTC_LOG(LS_INFO) << "V2 packet type: " << packet_type << " for original type: " << static_cast<int>(header_->type());
    
    bool has_namespace = (!header_->namespace_str().empty() && header_->namespace_str() != "/");
    // 命名空间（如果不是根命名空间）
    if (has_namespace) {
        // 确保命名空间以斜杠开头
        if (header_->namespace_str()[0] != '/') {
            ss << "/" << header_->namespace_str();
        } else {
            ss << header_->namespace_str();
        }
        RTC_LOG(LS_INFO) << "Added V2 namespace: " << header_->namespace_str();
    }
    
    // 添加ACK ID（如果有）
    bool has_ack_id = (header_->ack_id() >= 0);
    
    // ACK ID（如果有）
    if (has_ack_id) {
        // 如果有命名空间且不是根，需要逗号分隔
        if (has_namespace) {
            ss << ",";
        }
        ss << header_->ack_id();
        RTC_LOG(LS_INFO) << "Added V2 ACK ID: " << header_->ack_id();
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
    
    return ss.str();
    
//    RTC_LOG(LS_INFO) << "Encoded V2 packet: text length=" << result.text_packet.length() << ", binary parts=" << result.binary_parts.size();
//    RTC_LOG(LS_INFO) << "Encoded V2 packet content: " << result.text_packet;
}

// 构建V3协议包体
std::string SIOBody::build_v3(const PacketType type) {
    return "";
}

// 构建V4协议包体
std::string SIOBody::build_v4(const PacketType type) {
    return "";
}


void SIOBody::restore_binary_data(
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

void SIOBody::restore_v2_binary_data(
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
    

// 提取二进制数据
void SIOBody::extract_binary_data(const Json::Value& data,
                                  Json::Value& json_without_binary,
                                  std::vector<SmartBuffer>& binary_parts,
                                  std::map<std::string, int>& binary_map){
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


// 检查是否包含二进制数据
bool SIOBody::has_binary() const {
    return !attachments.empty();
}

}//namespace sio
