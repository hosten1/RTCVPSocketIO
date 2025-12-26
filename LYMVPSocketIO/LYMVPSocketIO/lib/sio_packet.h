//
//  sio_packet.hpp
//  LYMVPSocketIO
//
//  Created by luoyongmeng on 2025/12/26.
//

#ifndef sio_packet_hpp
#define sio_packet_hpp

#include "lib/sio_packet_types.h"
#include "lib/sio_jsoncpp_binary_helper.hpp"
#include "lib/sio_smart_buffer.hpp"

namespace sio {
// Socket.IO数据包头部
class SIOHeader {
public:
    SIOHeader();
    
    SIOHeader(const SIOHeader& other);
    
    SIOHeader(SocketIOVersion versionIn);
    
    // 解析包头部
    bool parse(const std::string& packet);
    
    // 构建包头部字符串
    bool build_sio_string(
            SocketIOVersion version,
            PacketType type,
            std::string namespace_str,
            int ack_id,
            int binary_count,
            std::stringstream& ss
        );
    
    // 生成调试信息字符串
    std::string to_string() const;
    
    // ===== getters =====
    SocketIOVersion version() const { return version_; }
    PacketType type() const { return type_; }
    
    const std::string& namespace_str() const { return namespace_str_; }
    
    int ack_id() const { return ack_id_; }
    int binary_count() const { return binary_count_; }
    size_t data_start_pos() const { return data_start_pos_; }
    
    bool has_ack() const { return ack_id_ >= 0; }
    
    bool is_binary() const {
        return type_ == PacketType::BINARY_EVENT ||
        type_ == PacketType::BINARY_ACK;
    }
    
    // ===== setters =====
    void set_version(SocketIOVersion version) {
        version_ = version;
    }
    
    void set_type(PacketType type) {
        type_ = type;
    }
    
    void set_namespace(const std::string& ns) {
        namespace_str_ = ns.empty() ? "/" : ns;
    }
    
    void set_namespace(std::string&& ns) {
        namespace_str_ = ns.empty() ? "/" : std::move(ns);
    }
    
    void set_ack_id(int ack_id) {
        ack_id_ = ack_id;
    }
    
    void clear_ack() {
        ack_id_ = -1;
    }
    
    void set_binary_count(int count) {
        binary_count_ = count;
    }
    
    void set_data_start_pos(size_t pos) {
        data_start_pos_ = pos;
    }
    
    bool has_binary(){
        return has_binary_;
    }

    
private:
    // 解析V2协议头部
    bool parse_v2_header(const std::string& packet);
    
    // 解析V3协议头部
    bool parse_v3_header(const std::string& packet);
    
    // 解析V4协议头部
    bool parse_v4_header(const std::string& packet);
    
    bool need_ack();
    
    bool build_v2_sio_str(const int binary_count,std::stringstream& ss);
    
    bool build_v3_sio_str(const int binary_count,std::stringstream& ss);
    
    
    SocketIOVersion version_{SocketIOVersion::V2};
    PacketType type_{PacketType::EVENT};
    std::string namespace_str_{"/"};
    int ack_id_{-1};
    int binary_count_{0};
    bool has_binary_{false};
    size_t data_start_pos_{0};
};

// Socket.IO数据包体
class SIOBody {
    
public:
    SIOBody(SocketIOVersion versionIn);
    
    // 解析包体
    bool parse(const std::string& text_packet, const SIOHeader& header, const std::vector<SmartBuffer>& binary_parts = std::vector<SmartBuffer>());
    
    // 构建包体
    std::string build(const SIOHeader& header, bool isEvent);
    
    // 提取二进制数据
    void extract_binary_data(const Json::Value& data,
                             Json::Value& json_without_binary,
                             std::vector<SmartBuffer>& binary_parts,
                             std::map<std::string, int>& binary_map);
    
    // 恢复二进制数据
    void restore_binary_data(Json::Value& data,
                                           const std::vector<SmartBuffer>& binary_parts,
                                           const std::map<std::string, int>& binary_map);
    
    // 检查是否包含二进制数据
    bool has_binary() const ;
    
    // 生成调试信息字符串
    std::string to_string() const;
    
    
    std::vector<SmartBuffer> attachments;  // 二进制附件（使用智能指针管理的Buffer）
    
private:
    // 解析V2协议包体
    bool parse_v2(const std::string& packet, const SIOHeader& header, const std::vector<SmartBuffer>& binaries);
    
    // 解析V3协议包体
    bool parse_v3(const std::string& packet, const SIOHeader& header, const std::vector<SmartBuffer>& binaries);
    
    // 解析V4协议包体
    bool parse_v4(const std::string& packet, const SIOHeader& header, const std::vector<SmartBuffer>& binaries);
    
    // 构建V2协议包体
    std::string build_v2(const PacketType type);
    
    // 构建V3协议包体
    std::string build_v3(const PacketType type);
    
    // 构建V4协议包体
    std::string build_v4(const PacketType type);
    
    void restore_v2_binary_data(
        Json::Value& data,
        const std::vector<SmartBuffer>& binary_parts,
                                const Json::Value& binary_map);
    
    
    SocketIOVersion version_;
    
    std::string event_name_ = nullptr;
    
    int ack_id_{-1};
    std::string json_data_ = nullptr;  // JSON数据
    
    std::vector<Json::Value> args_;
    
    bool has_binary_{false};
    
    std::shared_ptr<SIOHeader> header_{nullptr};
};

// Socket.IO数据包结构
class SIOPacket {
    
public:
    
    SIOPacket(SocketIOVersion versionIn) {}
    
    // 解析完整数据包
    bool parse(const std::string& packet, const std::vector<SmartBuffer>& binaries = std::vector<SmartBuffer>());
    
    // 构建完整数据包
    std::string build() const;
    
    // 检查是否包含二进制数据
//    bool has_binary() const { return body.has_binary(); }
    
    // 生成调试信息字符串
    std::string to_string() const;
private:
//    SIOHeader header{nullptr};
//    SIOBody body{nullptr};
};
}//namespace sio



#endif /* sio_packet_hpp */
