//
//  RTCVPSocketEngineProtocol.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#ifndef RTCVPSocketEngineProtocol_H
#define RTCVPSocketEngineProtocol_H

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol RTCVPSocketEngineClient;

/// SocketEngine 错误类型
typedef NS_ENUM(NSInteger, RTCVPSocketEngineError) {
    RTCVPSocketEngineErrorUnknown = 0,
    RTCVPSocketEngineErrorInvalidURL,
    RTCVPSocketEngineErrorHandshakeFailed,
    RTCVPSocketEngineErrorPingTimeout,
    RTCVPSocketEngineErrorWebSocketFailed,
    RTCVPSocketEngineErrorPollingFailed
};

/// ACK 回调类型
typedef void (^RTCVPSocketAckCallback)(NSArray * _Nullable response);

/// SocketEngine 协议
@protocol RTCVPSocketEngineProtocol <NSObject>

@required

@property (nonatomic, weak) id<RTCVPSocketEngineClient> client;
@property (nonatomic, readonly) BOOL closed;
@property (nonatomic, readonly) BOOL connected;
@property (nonatomic, readonly) NSString *sid;

/// 连接状态回调
@property (nonatomic, copy, nullable) void (^onConnect)(void);
@property (nonatomic, copy, nullable) void (^onDisconnect)(NSString *reason);
@property (nonatomic, copy, nullable) void (^onError)(NSString *error);

/// 创建引擎
- (instancetype)initWithClient:(id<RTCVPSocketEngineClient>)client
                           url:(NSURL*)url
                       options:(NSDictionary*)options;

/// 连接到服务器
- (void)connect;

/// 断开连接
- (void)disconnect:(NSString*)reason;

/// 重置客户端
- (void)syncResetClient;

///// 发送消息和数据
- (void)send:(NSString*)msg withData:(NSArray<NSData*>*) data;
///// 发送消息（可选ACK）
//- (void)send:(NSString *)msg ack:(RTCVPSocketAckCallback)ack;
///// 发送消息和数据（可选ACK）
//- (void)send:(NSString *)msg withData:(NSArray<NSData *> *)data ack:(RTCVPSocketAckCallback)ack;

/// 发送ACK响应（由客户端调用）
- (void)sendAckResponse:(NSString *)ackMessage withData:(NSArray<NSData *> *)data;

@end

/// SocketEngine 客户端协议
@protocol RTCVPSocketEngineClient <NSObject>

@required

/// 引擎错误
- (void)engineDidError:(NSString*)reason;

/// 引擎打开
- (void)engineDidOpen:(NSString*)reason;

/// 引擎关闭
- (void)engineDidClose:(NSString*)reason;

/// 解析引擎消息
- (void)parseEngineMessage:(NSString*)msg;

/// 解析引擎二进制数据
- (void)parseEngineBinaryData:(NSData*)data;

/// 处理ACK消息
- (void)handleEngineAck:(NSInteger)ackId withData:(NSArray *)data;

@optional

/// 心跳超时
- (void)enginePingTimeout;

@end

NS_ASSUME_NONNULL_END

#endif

