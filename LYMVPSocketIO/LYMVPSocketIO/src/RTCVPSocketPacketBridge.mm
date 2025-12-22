//
//  RTCVPSocketPacketBridge.mm
//  LYMVPSocketIO
//
//  Created by luoyongmeng on 2025/12/22.
//

#import "RTCVPSocketPacketBridge.h"

// 引入 C++ 核心头文件
#include "lib/sio_packet_impl.h"

@implementation RTCVPSocketPacketBridge

// 数据组合成 Socket.IO 协议的方法
+ (NSDictionary<NSString *, id> *)composeSocketIOPacketWithDataArray:(NSArray *)dataArray
                                                              type:(NSInteger)type
                                                              nsp:(NSInteger)nsp
                                                               id:(NSInteger)id {
    
    NSMutableDictionary *result = [NSMutableDictionary dictionary];
    
    @try {
        // 将 Objective-C NSArray 转换为 C++ std::vector<Json::Value>
        std::vector<Json::Value> cDataArray;
        
        // 遍历 NSArray，转换每个元素
        for (id item in dataArray) {
            if ([item isKindOfClass:[NSString class]]) {
                cDataArray.push_back(std::string([(NSString *)item UTF8String]));
            } else if ([item isKindOfClass:[NSNumber class]]) {
                NSNumber *number = (NSNumber *)item;
                if (strcmp([number objCType], @encode(int)) == 0) {
                    cDataArray.push_back(number.intValue);
                } else if (strcmp([number objCType], @encode(float)) == 0) {
                    cDataArray.push_back(number.floatValue);
                } else if (strcmp([number objCType], @encode(double)) == 0) {
                    cDataArray.push_back(number.doubleValue);
                } else if (strcmp([number objCType], @encode(BOOL)) == 0) {
                    cDataArray.push_back(number.boolValue);
                } else {
                    cDataArray.push_back(number.longValue);
                }
            } else if ([item isKindOfClass:[NSDictionary class]]) {
                // 简化处理，实际应该转换为 Json::Value
                Json::Value jsonObj;
                cDataArray.push_back(jsonObj);
            } else if ([item isKindOfClass:[NSArray class]]) {
                // 简化处理，实际应该转换为 Json::Value
                Json::Value jsonArray;
                cDataArray.push_back(jsonArray);
            } else if ([item isKindOfClass:[NSData class]]) {
                // 简化处理，实际应该转换为二进制数据
                Json::Value jsonData;
                cDataArray.push_back(jsonData);
            }
        }
        
        // 创建 PacketSender 实例
        sio::PacketSender<Json::Value> sender(sio::SocketIOVersion::V3);
        
        // 使用 C++ 类生成 Socket.IO 协议包
        // 注意：这里简化实现，实际应该使用异步回调或其他方式获取结果
        // 暂时返回空字典，后续完善
        
        [result setValue:@"" forKey:@"text_packet"];
        [result setValue:@[] forKey:@"binary_parts"];
        [result setValue:@NO forKey:@"is_binary_packet"];
        [result setValue:@0 forKey:@"binary_count"];
        
    } @catch (NSException *exception) {
        NSLog(@"Error composing Socket.IO packet: %@", exception);
    }
    
    return result;
}

// 解析 Socket.IO 协议成数组的方法
+ (NSArray *)parseSocketIOPacket:(NSString *)packet {
    NSMutableArray *result = [NSMutableArray array];
    
    @try {
        // 创建 PacketReceiver 实例
        sio::PacketReceiver<Json::Value> receiver(sio::SocketIOVersion::V3);
        
        // 设置接收完成回调
        __block BOOL callbackCalled = NO;
        __block std::vector<Json::Value> receivedData;
        
        receiver.set_complete_callback([&](const std::vector<Json::Value>& data) {
            receivedData = data;
            callbackCalled = YES;
        });
        
        // 接收文本包
        const char *packetCStr = [packet UTF8String];
        std::string packetStr(packetCStr);
        BOOL success = receiver.receive_text(packetStr);
        
        // 简化处理，实际应该处理异步回调
        if (success && callbackCalled) {
            // 将 C++ std::vector<Json::Value> 转换为 Objective-C NSArray
            for (const Json::Value& item : receivedData) {
                if (item.isString()) {
                    [result addObject:[NSString stringWithUTF8String:item.asCString()]];
                } else if (item.isInt()) {
                    [result addObject:@(item.asInt())];
                } else if (item.isDouble()) {
                    [result addObject:@(item.asDouble())];
                } else if (item.isBool()) {
                    [result addObject:@(item.asBool())];
                } else {
                    // 其他类型，简化处理
                    [result addObject:@{}];
                }
            }
        }
        
    } @catch (NSException *exception) {
        NSLog(@"Error parsing Socket.IO packet: %@", exception);
    }
    
    return result;
}

@end