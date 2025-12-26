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

} // namespace sio

#endif // SIO_PACKET_TYPES_H
