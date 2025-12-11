//
//  RTCVPSocketIOConfig.h
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/10.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

// RTCVPSocketIOConfig.h
#import <Foundation/Foundation.h>
#import "RTCVPSocketIOProtocolVersion.h"

NS_ASSUME_NONNULL_BEGIN

@class RTCVPSocketLogger;

typedef NS_ENUM(NSInteger, RTCVPSocketIOTransport) {
    RTCVPSocketIOTransportAuto,      // 自动选择
    RTCVPSocketIOTransportWebSocket, // 强制WebSocket
    RTCVPSocketIOTransportPolling    // 强制轮询
};

@interface RTCVPSocketIOConfig : NSObject

#pragma mark - 连接配置

/// 服务器URL路径（默认：@"/socket.io/"）
@property (nonatomic, copy) NSString *path;

/// 命名空间（默认：@"/"）
@property (nonatomic, copy, nullable) NSString *namespace;

/// 是否使用安全连接（根据URL自动检测，可覆盖）
@property (nonatomic, assign) BOOL secure;

/// 连接超时时间（秒，默认：10）
@property (nonatomic, assign) NSTimeInterval connectTimeout;

/// 传输方式（默认：自动选择）
@property (nonatomic, assign) RTCVPSocketIOTransport transport;

/// 协议版本（默认：RTCVPSocketIOProtocolVersion3）
@property (nonatomic, assign) RTCVPSocketIOProtocolVersion protocolVersion;

/// 是否强制轮训协议
@property (nonatomic, assign) BOOL forcePolling;

/// 是否强制使用WebSocket
@property (nonatomic, assign) BOOL forceWebsockets;

/// 心跳间隔（秒，默认：25）
@property (nonatomic, assign) NSInteger pingInterval;

/// 心跳超时时间（秒，默认：20）
@property (nonatomic, assign) NSInteger pingTimeout;

/// 是否启用二进制传输
@property (nonatomic, assign) BOOL enableBinary;

/// 是否启用重连
@property (nonatomic, assign) BOOL reconnectionEnabled;

/// 重连尝试次数（默认：无限重连）
@property (nonatomic, assign) NSInteger reconnectionAttempts;

/// 重连延迟（秒，默认：1）
@property (nonatomic, assign) NSTimeInterval reconnectionDelay;

/// 最大重连延迟（秒，默认：5）
@property (nonatomic, assign) NSTimeInterval reconnectionDelayMax;

/// 随机化因子（0.0-1.0，默认：0.5）
@property (nonatomic, assign) double randomizationFactor;

#pragma mark - 安全配置

/// 是否允许自签名证书
@property (nonatomic, assign) BOOL allowSelfSignedCertificates;

/// 是否忽略SSL错误
@property (nonatomic, assign) BOOL ignoreSSLErrors;

/// 安全配置对象
@property (nonatomic, strong, nullable) id security;

#pragma mark - 高级配置

/// 是否启用压缩
@property (nonatomic, assign) BOOL compressionEnabled;

/// 是否强制创建新连接
@property (nonatomic, assign) BOOL forceNewConnection;

/// 额外请求头
@property (nonatomic, strong, nullable) NSDictionary<NSString *, NSString *> *extraHeaders;

/// 连接参数
@property (nonatomic, strong, nullable) NSDictionary<NSString *, id> *connectParams;

/// Cookies
@property (nonatomic, strong, nullable) NSArray<NSHTTPCookie *> *cookies;

/// Session委托
@property (nonatomic, weak, nullable) id<NSURLSessionDelegate> sessionDelegate;

#pragma mark - 日志配置

/// 日志记录器
@property (nonatomic, strong, nullable) RTCVPSocketLogger *logger;

/// 是否启用日志
@property (nonatomic, assign) BOOL loggingEnabled;

/// 日志级别（0:错误，1:警告，2:信息，3:调试）
@property (nonatomic, assign) NSInteger logLevel;


@property(nonatomic, strong) dispatch_queue_t handleQueue;

@property(nonatomic, assign) BOOL enableNetworkMonitoring;

#pragma mark - 初始化方法

/// 默认配置
+ (instancetype)defaultConfig;

/// 生产环境配置
+ (instancetype)productionConfig;

/// 开发环境配置
+ (instancetype)developmentConfig;

/// 从字典初始化（向后兼容）
- (instancetype)initWithDictionary:(NSDictionary *)dict;

/// 使用构建器模式初始化
- (instancetype)initWithBuilder:(void(^)(RTCVPSocketIOConfig *config))builder;

@end

// 构建器模式扩展
@interface RTCVPSocketIOConfig (Builder)

/// 快速配置方法
+ (instancetype)configWithBlock:(void(^)(RTCVPSocketIOConfig *config))block;

@end

NS_ASSUME_NONNULL_END
