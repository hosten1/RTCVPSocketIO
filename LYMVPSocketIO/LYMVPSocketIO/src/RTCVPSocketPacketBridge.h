//
//  RTCVPSocketPacketBridge.h
//  LYMVPSocketIO
//
//  Created by luoyongmeng on 2025/12/22.
//

#ifndef RTCVPSocketPacketBridge_h
#define RTCVPSocketPacketBridge_h

#include <Foundation/Foundation.h>

// 前向声明 C++ 类
namespace sio {
    template <typename T>
    class PacketSender;
    
    template <typename T>
    class PacketReceiver;
    
    struct SocketIOPacketResult;
}

// 定义 Objective-C++ 桥接类
@interface RTCVPSocketPacketBridge : NSObject

// 数据组合成 Socket.IO 协议的方法
+ (NSDictionary<NSString *, id> *)composeSocketIOPacketWithDataArray:(NSArray *)dataArray
                                                              type:(NSInteger)type
                                                              nsp:(NSInteger)nsp
                                                               id:(NSInteger)id;

// 解析 Socket.IO 协议成数组的方法
+ (NSArray *)parseSocketIOPacket:(NSString *)packet;

@end

#endif /* RTCVPSocketPacketBridge_h */