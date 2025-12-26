//
//  sio_packet_types.h
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/19.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#ifndef SIO_PACKET_TYPES_H
#define SIO_PACKET_TYPES_H

#include <vector>
#include <string>
#include <cstdint>
#include "rtc_base/buffer.h"
#include "sio_smart_buffer.hpp"

namespace sio {

// 数据包类型
enum class PacketType {
    CONNECT = 0,
    DISCONNECT = 1,
    EVENT = 2,
    ACK = 3,
    ERROR = 4,
    BINARY_EVENT = 5,
    BINARY_ACK = 6
};

/**
 * Socket.IO版本枚举
 * 
 * 不同版本的Socket.IO协议差异:
 * - V2: 使用数组格式的连接包，二进制计数直接在包类型后
 * - V3/V4: 使用对象格式的连接包，不再需要二进制计数
 * - V3+: 支持数字命名空间，简化了一些包格式
 */
enum class SocketIOVersion {
    V2 = 2,  // Socket.IO v2.x
    V3 = 3,  // Socket.IO v3.x
    V4 = 4   // Socket.IO v4.x (与v3兼容)
};

// Socket.IO数据包头部
struct SIOHeader {
    SocketIOVersion version;
    PacketType type;
    std::string namespace_str;
    int nsp;  // 命名空间索引
    int ack_id;   // 包ID（用于ACK）
    int binary_count;
    size_t data_start_pos;
    
    SIOHeader() : version(SocketIOVersion::V4), type(PacketType::EVENT), namespace_str("/"), nsp(0), ack_id(-1), binary_count(0), data_start_pos(0) {}
    
    SIOHeader(SocketIOVersion versionIn) : version(versionIn), type(PacketType::EVENT), namespace_str("/"), nsp(0), ack_id(-1), binary_count(0), data_start_pos(0) {}
    
    // 生成调试信息字符串
    std::string to_string() const;
};

// Socket.IO数据包体
struct sioBody {
    std::string data;  // JSON数据
    std::vector<SmartBuffer> attachments;  // 二进制附件（使用智能指针管理的Buffer）
    
    sioBody() {}
    
    // 检查是否包含二进制数据
    bool has_binary() const { return !attachments.empty(); }
    
    // 生成调试信息字符串
    std::string to_string() const;
};

// Socket.IO数据包结构
struct Packet {
    SIOHeader header;
    sioBody body;
    
    Packet() {}
    
    // 检查是否包含二进制数据
    bool has_binary() const { return body.has_binary(); }
    
    // 生成调试信息字符串
    std::string to_string() const;
};

} // namespace sio

#endif // SIO_PACKET_TYPES_H