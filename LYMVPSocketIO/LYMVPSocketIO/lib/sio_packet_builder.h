//
//  sio_packet_builder.hpp
//  LYMVPSocketIO
//
//  Created by luoyongmeng on 2025/12/23.
//

#ifndef sio_packet_builder_hpp
#define sio_packet_builder_hpp

#include <string>
#include <cstdint>
#include "lib/sio_packet_parser.h"
#include "lib/sio_packet_types.h"
#include "lib/sio_jsoncpp_binary_helper.hpp"


namespace sio {

// Socket.IO 协议包结构
struct SioPacket {
    PacketType type;
    std::string event_name;
    std::vector<Json::Value> args;
    std::vector<SmartBuffer> binary_parts;
    std::string namespace_s;
    int ack_id;
    bool need_ack;
    SocketIOVersion version;
    
    int binary_count = 0;           // 二进制数据数量

    
    SioPacket() : type(PacketType::EVENT), namespace_s("/"),
    ack_id(-1), need_ack(false), version(SocketIOVersion::V4) {}
    
    bool is_binary() const {
        return !binary_parts.empty() ||
               type == PacketType::BINARY_EVENT ||
               type == PacketType::BINARY_ACK;
    }
    
    std::string to_string() const {
        std::stringstream ss;
        ss << "SioPacket {" << std::endl;
        ss << "  type: " << static_cast<int>(type) << std::endl;
        ss << "  event: " << event_name << std::endl;
        ss << "  args: " << args.size() << " 个" << std::endl;
        ss << "  binary_parts: " << binary_parts.size() << " 个" << std::endl;
        ss << "  namespace_s: " << namespace_s << std::endl;
        ss << "  ack_id: " << ack_id << std::endl;
        ss << "  need_ack: " << (need_ack ? "true" : "false") << std::endl;
        ss << "  version: " << static_cast<int>(version) << std::endl;
        ss << "}";
        return ss.str();
    }
};
// Socket.IO 协议构建器（专注于V2和V3）
class SioPacketBuilder {
public:
    explicit SioPacketBuilder(SocketIOVersion version = SocketIOVersion::V3);
    
    // 设置协议版本
    void set_version(SocketIOVersion version) { version_ = version; }
    SocketIOVersion get_version() const { return version_; }
    
    // 构建事件包
    SioPacket build_event_packet(
                                 const std::string& event_name,
                                 const std::vector<Json::Value>& args,
                                 const std::string&  namespace_s = "/",
                                 int ack_id = -1);
    
    // 构建ACK包
    SioPacket build_ack_packet(
                               const std::vector<Json::Value>& args,
                               const std::string&  namespace_s = "/",
                               int ack_id = -1);
    
    // 将包编码为Socket.IO协议格式
    struct EncodedPacket {
        std::string text_packet;
        std::vector<SmartBuffer> binary_parts;
        bool is_binary;
        int binary_count;
        
        EncodedPacket() : is_binary(false), binary_count(0) {}
    };
    
    EncodedPacket encode_packet(const SioPacket& packet);
    
    // 解码Socket.IO协议包
    SioPacket decode_packet(
                            const std::string& text_packet,
                            const std::vector<SmartBuffer>& binary_parts = std::vector<SmartBuffer>());
    
    // 检测协议版本（基于包内容）
//    static SocketIOVersion detect_version(const std::string& packet);
    
private:
    // V2协议构建
    EncodedPacket encode_v2_packet(const SioPacket& packet);
    SioPacket decode_v2_packet(const std::string& text, const std::vector<SmartBuffer>& binaries);
    
    // V3协议构建
    EncodedPacket encode_v3_packet(const SioPacket& packet);
    SioPacket decode_v3_packet(const std::string& text, const std::vector<SmartBuffer>& binaries);
    
    // 辅助方法
    void extract_binary_data(const Json::Value& data,
                             Json::Value& json_without_binary,
                             std::vector<SmartBuffer>& binary_parts,
                             std::map<std::string, int>& binary_map);
    
    void restore_binary_data(Json::Value& data,
                             const std::vector<SmartBuffer>& binary_parts,
                             const std::map<std::string, int>& binary_map);
    
    Json::Value create_binary_placeholder(int index);
    bool is_binary_placeholder(const Json::Value& value);
    int get_placeholder_index(const Json::Value& placeholder);
    
    // 解析V2/V3包头部
    struct PacketHeader {
        PacketType type;
        std::string namespace_str;
        int ack_id;
        int binary_count;
        size_t data_start_pos;
        
        PacketHeader() : type(PacketType::EVENT),
        ack_id(-1), binary_count(0), data_start_pos(0) {}
    };
    
    PacketHeader parse_v2_header(const std::string& packet);
    
    PacketHeader parse_v3_header(const std::string& packet);
    
    PacketHeader parse_packet_header(const std::string& packet, SocketIOVersion version);
    
    SocketIOVersion version_;
    
    // 禁止拷贝
    SioPacketBuilder(const SioPacketBuilder&) = delete;
    SioPacketBuilder& operator=(const SioPacketBuilder&) = delete;
};
}  // namespace sio end
#endif /* sio_packet_builder_hpp */
