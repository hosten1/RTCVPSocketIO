
//
//  RTCVPSocketIOConfig.m
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/10.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketIOConfig.h"
#import "RTCDefaultSocketLogger.h"

// 配置键常量
NSString *const kRTCVPSocketIOConfigKeyForceNew = @"forceNew";
NSString *const kRTCVPSocketIOConfigKeyReconnects = @"reconnects";
NSString *const kRTCVPSocketIOConfigKeyReconnectWait = @"reconnectWait";
NSString *const kRTCVPSocketIOConfigKeyTimeout = @"timeout";
NSString *const kRTCVPSocketIOConfigKeyQuery = @"query";
NSString *const kRTCVPSocketIOConfigKeyNsp = @"nsp";
NSString *const kRTCVPSocketIOConfigKeyIgnoreSSLErrors = @"ignoreSSLErrors";
NSString *const kRTCVPSocketIOConfigKeyProtocolVersion = @"protocolVersion";

// 缺失的配置键常量

NSString *const kRTCVPSocketIOConfigKeyPath = @"path";
NSString *const kRTCVPSocketIOConfigKeySecure = @"secure";
NSString *const kRTCVPSocketIOConfigKeyPingInterval = @"pingInterval";
NSString *const kRTCVPSocketIOConfigKeyPingTimeout = @"pingTimeout";
NSString *const kRTCVPSocketIOConfigKeyExtraHeaders = @"extraHeaders";
NSString *const kRTCVPSocketIOConfigKeyCookies = @"cookies";
NSString *const kRTCVPSocketIOConfigKeyConnectParams = @"connectParams";
NSString *const kRTCVPSocketIOConfigKeySelfSigned = @"selfSigned";
NSString *const kRTCVPSocketIOConfigKeySecurity = @"security";
NSString *const kRTCVPSocketIOConfigKeyCompress = @"compress";
NSString *const kRTCVPSocketIOConfigKeyNamespace = @"namespace";

// Socket.IO 3.0协议支持常量
const int kRTCVPSocketIOProtocolVersion2 = 2;
const int kRTCVPSocketIOProtocolVersion3 = 3;

@implementation RTCVPSocketIOConfig

+ (instancetype)defaultConfig {
    return [[self alloc] init];
}

+ (instancetype)productionConfig {
    RTCVPSocketIOConfig *config = [self defaultConfig];
    config.logLevel = 1; // 仅警告
    config.reconnectionEnabled = YES;
    config.reconnectionDelay = 2;
    config.reconnectionDelayMax = 10;
    return config;
}

+ (instancetype)developmentConfig {
    RTCVPSocketIOConfig *config = [self defaultConfig];
    config.logLevel = 3; // 调试级别
    config.loggingEnabled = YES;
    return config;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _path = @"/socket.io/";
        _secure = NO;
        _connectTimeout = 10;
        _transport = RTCVPSocketIOTransportAuto;
        _protocolVersion = kRTCVPSocketIOProtocolVersionDefault;

        _pingInterval = 25;
        _pingTimeout = 20;
        _enableBinary = YES;
        _reconnectionEnabled = YES;
        _reconnectionAttempts = -1; // 无限重连
        _reconnectionDelay = 1;
        _reconnectionDelayMax = 5;
        _randomizationFactor = 0.5;
        _allowSelfSignedCertificates = NO;
        _ignoreSSLErrors = NO;
        _compressionEnabled = NO;
        _forceNewConnection = NO;
        _loggingEnabled = NO;
        _logLevel = 2; // 信息级别
    }
    return self;
}

- (instancetype)initWithDictionary:(NSDictionary *)dict {
    self = [self init];
    if (self) {
        [self applyDictionary:dict];
    }
    return self;
}

- (instancetype)initWithBuilder:(void (^)(RTCVPSocketIOConfig *))builder {
    self = [self init];
    if (self && builder) {
        builder(self);
    }
    return self;
}

+ (instancetype)configWithBlock:(void (^)(RTCVPSocketIOConfig *))block {
    return [[self alloc] initWithBuilder:block];
}

- (void)applyDictionary:(NSDictionary *)dict {
    for (NSString *key in dict.allKeys) {
        id value = dict[key];
        
        if ([key isEqualToString:kRTCVPSocketIOConfigKeyPath]) {
            self.path = value;
        } else if ([key isEqualToString:kRTCVPSocketIOConfigKeySecure]) {
            self.secure = [value boolValue];
        } else if ([key isEqualToString:kRTCVPSocketIOConfigKeyProtocolVersion]) {
            self.protocolVersion = [value integerValue];
        } else if ([key isEqualToString:kRTCVPSocketIOConfigKeyPingInterval]) {
            self.pingInterval = [value integerValue];
        } else if ([key isEqualToString:kRTCVPSocketIOConfigKeyPingTimeout]) {
            self.pingTimeout = [value integerValue];
        } else if ([key isEqualToString:kRTCVPSocketIOConfigKeyExtraHeaders]) {
            self.extraHeaders = value;
        } else if ([key isEqualToString:kRTCVPSocketIOConfigKeyCookies]) {
            self.cookies = value;
        } else if ([key isEqualToString:kRTCVPSocketIOConfigKeyConnectParams]) {
            self.connectParams = value;
        } else if ([key isEqualToString:kRTCVPSocketIOConfigKeySelfSigned]) {
            self.allowSelfSignedCertificates = [value boolValue];
        } else if ([key isEqualToString:kRTCVPSocketIOConfigKeySecurity]) {
            self.security = value;
        } else if ([key isEqualToString:kRTCVPSocketIOConfigKeyCompress]) {
            self.compressionEnabled = [value boolValue];
        } else if ([key isEqualToString:kRTCVPSocketIOConfigKeyNamespace]) {
            self.nsp = value;
        }
    }
}

@end
