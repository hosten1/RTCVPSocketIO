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

// Socket.IO数据包结构
struct Packet {
    PacketType type;
    int nsp;  // 命名空间索引
    int id;   // 包ID（用于ACK）
    std::string data;  // JSON数据
    std::vector<SmartBuffer> attachments;  // 二进制附件（使用智能指针管理的Buffer）
    
    Packet() : type(PacketType::EVENT), nsp(0), id(-1) {}
    
    // 检查是否包含二进制数据
    bool has_binary() const { return !attachments.empty(); }
    
    // 生成调试信息字符串
    std::string to_string() const;
};

} // namespace sio

#endif // SIO_PACKET_TYPES_H