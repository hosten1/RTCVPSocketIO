
//
//  RTCVPSocketIOConfig.m
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/10.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketIOConfig.h"

// 配置键常量
NSString *const kRTCVPSocketIOConfigKeyForceNew = @"forceNew";
NSString *const kRTCVPSocketIOConfigKeyReconnects = @"reconnects";
NSString *const kRTCVPSocketIOConfigKeyReconnectWait = @"reconnectWait";
NSString *const kRTCVPSocketIOConfigKeyTimeout = @"timeout";
NSString *const kRTCVPSocketIOConfigKeyQuery = @"query";
NSString *const kRTCVPSocketIOConfigKeyNsp = @"nsp";
NSString *const kRTCVPSocketIOConfigKeyIgnoreSSLErrors = @"ignoreSSLErrors";
NSString *const kRTCVPSocketIOConfigKeyProtocolVersion = @"protocolVersion";

// Socket.IO 3.0协议支持常量
const int kRTCVPSocketIOProtocolVersion2 = 2;
const int kRTCVPSocketIOProtocolVersion3 = 3;

@implementation RTCVPSocketIOConfig

#pragma mark - 初始化方法

- (instancetype)init {
    self = [super init];
    if (self) {
        [self setupDefaultValues];
    }
    return self;
}

- (void)setupDefaultValues {
    // 连接配置
    _path = @"/socket.io/";
    _namespace = @"/";
    _secure = NO; // 由URL自动检测
    _timeout = 10.0;
    _transport = RTCVPSocketIOTransportAuto;
    _protocolVersion = RTCVPSocketIOProtocolVersion4;
    _forcePolling = NO;
    _forceWebsockets = NO;
    _pingInterval = 25;
    _pingTimeout = 20;
    
    // 重连配置
    _reconnectionEnabled = YES;
    _reconnectionAttempts = 0; // 0表示无限重连
    _reconnectionDelay = 1.0;
    _reconnectionDelayMax = 5.0;
    _randomizationFactor = 0.5;
    
    // 安全配置
    _allowSelfSignedCertificates = NO;
    _ignoreSSLErrors = NO;
    _security = nil;
    
    // 高级配置
    _compressionEnabled = NO;
    _forceNewConnection = NO;
    _extraHeaders = @{};
    _queryParameters = @{};
    _cookies = @[];
    _authToken = nil;
    
    // 日志配置
    _logger = [RTCVPSocketLogger new];
    _loggingEnabled = NO;
    _logLevel = 0; // 错误级别
}

#pragma mark - 类方法

+ (instancetype)defaultConfig {
    return [[self alloc] init];
}

+ (instancetype)productionConfig {
    RTCVPSocketIOConfig *config = [[self alloc] init];
    config.loggingEnabled = NO;
    config.logLevel = 0;
    config.compressionEnabled = YES;
    config.reconnectionEnabled = YES;
    return config;
}

+ (instancetype)developmentConfig {
    RTCVPSocketIOConfig *config = [[self alloc] init];
    config.loggingEnabled = YES;
    config.logLevel = 3; // 调试级别
    config.compressionEnabled = NO;
    config.reconnectionEnabled = YES;
    return config;
}

- (instancetype)initWithBuilder:(void(^)(RTCVPSocketIOConfig *config))builder {
    self = [self init];
    if (self) {
        if (builder) {
            builder(self);
        }
    }
    return self;
}

#pragma mark - 描述方法

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@: %p>\n"
            "path: %@\n"
            "namespace: %@\n"
            "secure: %@\n"
            "timeout: %.1f\n"
            "transport: %ld\n"
            "protocolVersion: %ld\n"
            "forcePolling: %@\n"
            "forceWebsockets: %@\n"
            "pingInterval: %ld\n"
            "pingTimeout: %ld\n"
            "reconnectionEnabled: %@\n"
            "reconnectionAttempts: %ld\n"
            "reconnectionDelay: %.1f\n"
            "reconnectionDelayMax: %.1f\n"
            "randomizationFactor: %.1f\n"
            "allowSelfSignedCertificates: %@\n"
            "ignoreSSLErrors: %@\n"
            "compressionEnabled: %@\n"
            "forceNewConnection: %@\n"
            "extraHeaders: %@\n"
            "queryParameters: %@\n"
            "cookies: %ld items\n"
            "authToken: %@\n"
            "loggingEnabled: %@\n"
            "logLevel: %ld",
            NSStringFromClass([self class]), self,
            _path,
            _namespace,
            _secure ? @"YES" : @"NO",
            _timeout,
            (long)_transport,
            (long)_protocolVersion,
            _forcePolling ? @"YES" : @"NO",
            _forceWebsockets ? @"YES" : @"NO",
            (long)_pingInterval,
            (long)_pingTimeout,
            _reconnectionEnabled ? @"YES" : @"NO",
            (long)_reconnectionAttempts,
            _reconnectionDelay,
            _reconnectionDelayMax,
            _randomizationFactor,
            _allowSelfSignedCertificates ? @"YES" : @"NO",
            _ignoreSSLErrors ? @"YES" : @"NO",
            _compressionEnabled ? @"YES" : @"NO",
            _forceNewConnection ? @"YES" : @"NO",
            _extraHeaders,
            _queryParameters,
            (unsigned long)_cookies.count,
            _authToken ? @"(set)" : @"nil",
            _loggingEnabled ? @"YES" : @"NO",
            (long)_logLevel];
}

#pragma mark - NSCopying

- (id)copyWithZone:(nullable NSZone *)zone {
    RTCVPSocketIOConfig *copy = [[[self class] allocWithZone:zone] init];
    copy.path = [_path copy];
    copy.namespace = [_namespace copy];
    copy.secure = _secure;
    copy.timeout = _timeout;
    copy.transport = _transport;
    copy.protocolVersion = _protocolVersion;
    copy.forcePolling = _forcePolling;
    copy.forceWebsockets = _forceWebsockets;
    copy.pingInterval = _pingInterval;
    copy.pingTimeout = _pingTimeout;
    copy.reconnectionEnabled = _reconnectionEnabled;
    copy.reconnectionAttempts = _reconnectionAttempts;
    copy.reconnectionDelay = _reconnectionDelay;
    copy.reconnectionDelayMax = _reconnectionDelayMax;
    copy.randomizationFactor = _randomizationFactor;
    copy.allowSelfSignedCertificates = _allowSelfSignedCertificates;
    copy.ignoreSSLErrors = _ignoreSSLErrors;
    copy.security = _security;
    copy.compressionEnabled = _compressionEnabled;
    copy.forceNewConnection = _forceNewConnection;
    copy.extraHeaders = [_extraHeaders copy];
    copy.queryParameters = [_queryParameters copy];
    copy.cookies = [_cookies copy];
    copy.authToken = [_authToken copy];
    copy.logger = _logger;
    copy.loggingEnabled = _loggingEnabled;
    copy.logLevel = _logLevel;
    return copy;
}

@end

#pragma mark - 构建器模式扩展

@implementation RTCVPSocketIOConfig (Builder)

+ (instancetype)configWithBlock:(void(^)(RTCVPSocketIOConfig *config))block {
    return [[self alloc] initWithBuilder:block];
}

@end
