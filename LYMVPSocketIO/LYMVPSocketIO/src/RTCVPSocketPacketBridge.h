//
//  RTCVPSocketPacketBridge.h
//  LYMVPSocketIO
//
//  Created by luoyongmeng on 2025/12/22.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// Socket.IO 包类型
typedef NS_ENUM(NSInteger, RTCVPSocketIOPacketType) {
    RTCVPSocketIOPacketTypeConnect = 0,
    RTCVPSocketIOPacketTypeDisconnect = 1,
    RTCVPSocketIOPacketTypeEvent = 2,
    RTCVPSocketIOPacketTypeAck = 3,
    RTCVPSocketIOPacketTypeError = 4,
    RTCVPSocketIOPacketTypeBinaryEvent = 5,
    RTCVPSocketIOPacketTypeBinaryAck = 6
};

// Socket.IO 版本
typedef NS_ENUM(NSInteger, RTCVPSocketIOVersion) {
    RTCVPSocketIOVersionV2 = 2,
    RTCVPSocketIOVersionV3 = 3,
    RTCVPSocketIOVersionV4 = 4
};

// 组合完成回调
typedef void (^RTCVPComposeCompletion)(NSString * _Nullable textPacket,
                                        NSArray<NSData *> * _Nullable binaryParts,
                                        BOOL isBinaryPacket,
                                        NSInteger binaryCount,
                                        NSError * _Nullable error);

// 解析完成回调
typedef void (^RTCVPParseCompletion)(NSArray * _Nullable dataArray,
                                      NSInteger packetType,
                                      NSInteger namespaceId,
                                      NSInteger packetId,
                                      NSError * _Nullable error);

// 带ACK的事件发送回调
typedef void (^RTCVPAckCompletion)(NSArray * _Nullable ackData,
                                    NSError * _Nullable error);

// ACK超时回调
typedef void (^RTCVPAckTimeoutCompletion)(void);

// 二进制数据接收回调
typedef void (^RTCVPBinaryDataCallback)(NSData *binaryData, NSInteger index);

@interface RTCVPSocketPacketBridge : NSObject

// 初始化方法
- (instancetype)initWithSocketIOVersion:(RTCVPSocketIOVersion)version;

// MARK: - 组合方法（异步）
// 组合数据为Socket.IO协议格式
- (void)composePacketWithDataArray:(NSArray *)dataArray
                              type:(RTCVPSocketIOPacketType)type
                               nsp:(NSInteger)nsp
                                id:(NSInteger)packetId
                        completion:(RTCVPComposeCompletion)completion;

// 组合带ACK的事件
- (void)composeEventWithName:(NSString *)eventName
                        data:(id _Nullable)data
                         nsp:(NSInteger)nsp
               needsAckReply:(BOOL)needsAckReply
                  completion:(RTCVPComposeCompletion)completion;

// MARK: - 解析方法（异步）
// 解析Socket.IO包
- (void)parsePacket:(NSString *)textPacket
        binaryParts:(NSArray<NSData *> * _Nullable)binaryParts
         completion:(RTCVPParseCompletion)completion;

// 解析文本包（自动处理二进制）
- (void)parseTextPacket:(NSString *)textPacket
          binaryCallback:(RTCVPBinaryDataCallback _Nullable)binaryCallback
              completion:(RTCVPParseCompletion)completion;

// MARK: - 带ACK的事件发送
// 发送需要ACK响应的事件
- (void)sendEventWithName:(NSString *)eventName
                     data:(id _Nullable)data
                      nsp:(NSInteger)nsp
                ackCallback:(RTCVPAckCompletion _Nullable)ackCallback
           timeoutCallback:(RTCVPAckTimeoutCompletion _Nullable)timeoutCallback
                  timeout:(NSTimeInterval)timeout
               completion:(RTCVPComposeCompletion)completion;

// MARK: - 辅助方法
// 设置ACK回调
- (void)handleAckResponse:(NSArray *)ackData forPacketId:(NSInteger)packetId;

// 获取支持的Socket.IO版本
@property (nonatomic, assign, readonly) RTCVPSocketIOVersion version;

@end

NS_ASSUME_NONNULL_END