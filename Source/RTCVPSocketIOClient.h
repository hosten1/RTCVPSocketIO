//
//  RTCVPSocketIOClient.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "RTCVPSocketAnyEvent.h"
#import "RTCVPSocketOnAckCallback.h"
#import "RTCVPSocketIOClientProtocol.h"

// Socket.IO客户端状态枚举
typedef NS_ENUM(NSUInteger, RTCVPSocketIOClientStatus) {
    RTCVPSocketIOClientStatusNotConnected = 0x1,  // 未连接
    RTCVPSocketIOClientStatusDisconnected = 0x2,  // 已断开连接
    RTCVPSocketIOClientStatusConnecting = 0x3,    // 连接中
    RTCVPSocketIOClientStatusOpened = 0x4,        // 连接已打开
    RTCVPSocketIOClientStatusConnected = 0x5      // 已连接
};

// Socket.IO事件名称常量
extern NSString *const kSocketEventConnect;
extern NSString *const kSocketEventDisconnect;
extern NSString *const kSocketEventError;
extern NSString *const kSocketEventReconnect;
extern NSString *const kSocketEventReconnectAttempt;
extern NSString *const kSocketEventStatusChange;

// 回调类型定义
typedef void (^RTCVPSocketIOVoidHandler)(void);
typedef void (^RTCVPSocketAnyEventHandler)(RTCVPSocketAnyEvent*event);
typedef void (^RTCVPSocketAckHandler)(id _Nullable data, NSError * _Nullable error);
typedef void (^RTCVPSocketConnectHandler)(BOOL connected, NSError * _Nullable error);





// Socket.IO配置类
@interface RTCVPSocketIOConfig : NSObject

// 连接配置
@property (nonatomic, assign) BOOL forceNew;                  ///< 是否强制创建新连接
@property (nonatomic, assign) BOOL reconnects;               ///< 是否自动重连
@property (nonatomic, assign) int reconnectAttempts;         ///< 重连尝试次数
@property (nonatomic, assign) int reconnectWait;             ///< 重连等待时间（秒）
@property (nonatomic, copy) NSString *nsp;                  ///< 命名空间，默认为@"/"
@property (nonatomic, copy) NSString *path;                  ///< Socket.IO路径，默认为@"/socket.io/"
@property (nonatomic, assign) BOOL secure;                  ///< 是否使用安全连接（HTTPS/WSS）
@property (nonatomic, assign) BOOL selfSigned;              ///< 是否允许自签名证书
@property (nonatomic, assign) BOOL ignoreSSLErrors;         ///< 是否忽略SSL错误
@property (nonatomic, assign) BOOL compress;                ///< 是否启用压缩
@property (nonatomic, assign) BOOL forcePolling;            ///< 是否强制使用轮询
@property (nonatomic, assign) BOOL forceWebsockets;         ///< 是否强制使用WebSocket
@property (nonatomic, assign) int protocolVersion;          ///< 协议版本，默认为3.0
@property (nonatomic, assign) double timeout;               ///< 超时时间（秒）

// 日志配置
@property (nonatomic, assign) BOOL log;                     ///< 是否启用日志
@property (nonatomic, strong) id logger;                   ///< 自定义日志器

// 队列和委托
@property (nonatomic, strong) dispatch_queue_t handleQueue; ///< 处理队列
@property (nonatomic, weak) id<NSURLSessionDelegate> sessionDelegate; ///< 会话委托

// 连接参数
@property (nonatomic, strong) NSMutableDictionary *connectParams; ///< 连接参数
@property (nonatomic, strong) NSMutableArray<NSHTTPCookie *> *cookies; ///< Cookie数组
@property (nonatomic, strong) NSMutableDictionary *extraHeaders; ///< 额外头信息
@property (nonatomic, strong) id security;                  ///< 安全配置

// 便捷初始化方法
+ (instancetype)defaultConfig;                     ///< 默认配置
+ (instancetype)configWithBlock:(void(^)(RTCVPSocketIOConfig *config))block; ///< 块配置

// 转换为字典
- (NSDictionary *)toDictionary;                    ///< 转换为字典格式，用于旧版API兼容

@end

@interface RTCVPSocketIOClient : NSObject<RTCVPSocketIOClientProtocol>

@property (nonatomic, readonly) RTCVPSocketIOClientStatus status;
@property (nonatomic) BOOL forceNew;
@property (nonatomic, strong, readonly) NSMutableDictionary *config;
@property (nonatomic) BOOL reconnects;
@property (nonatomic) int reconnectWait;
@property (nonatomic, strong, readonly) NSString *ssid;
@property (nonatomic, strong, readonly) NSURL *socketURL;

@property (nonatomic, strong, readonly) dispatch_queue_t handleQueue;
@property (nonatomic, strong, readonly) NSString* nsp;

-(instancetype)init:(NSURL*)socketURL withConfig:(NSDictionary*)config;
-(void) connect;
-(void) connectWithTimeoutAfter:(double)timeout withHandler:(RTCVPSocketIOVoidHandler)handler;
-(void) disconnect;
-(void) disconnectWithHandler:(RTCVPSocketIOVoidHandler)handler;
-(void) reconnect;
-(void) removeAllHandlers;

-(RTCVPSocketOnAckCallback*) emitWithAck:(NSString*)event items:(NSArray*)items;

/**
 * 增强的emitWithAck方法，直接传递回调block
 * @param event 事件名称
 * @param items 事件参数
 * @param ackBlock 回调block，在收到ack时调用
 * @param timeout 超时时间，单位为秒，默认10秒
 */
-(void) emitWithAck:(NSString*)event 
              items:(NSArray*)items 
           ackBlock:(void(^)(NSArray * _Nullable data, NSError * _Nullable error))ackBlock;

/**
 * 增强的emitWithAck方法，直接传递回调block，带超时时间
 * @param event 事件名称
 * @param items 事件参数
 * @param ackBlock 回调block，在收到ack时调用
 * @param timeout 超时时间，单位为秒
 */
-(void) emitWithAck:(NSString*)event 
              items:(NSArray*)items 
           ackBlock:(void(^)(NSArray * _Nullable data, NSError * _Nullable error))ackBlock
            timeout:(NSTimeInterval)timeout;

-(NSUUID*) on:(NSString*)event callback:(RTCVPSocketOnEventCallback) callback;
-(NSUUID*) once:(NSString*)event callback:(RTCVPSocketOnEventCallback) callback;
-(void) onAny:(RTCVPSocketAnyEventHandler)handler;
-(void) off:(NSString*) event;

@end
