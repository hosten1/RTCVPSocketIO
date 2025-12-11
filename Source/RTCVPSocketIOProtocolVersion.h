//
//  RTCVPSocketIOProtocolVersion.h
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/11.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, RTCVPSocketIOProtocolVersion) {
    RTCVPSocketIOProtocolVersion2 = 2,  // Engine.IO 3.x
    RTCVPSocketIOProtocolVersion3 = 3,  // Engine.IO 4.x
    RTCVPSocketIOProtocolVersion4 = 4   // Engine.IO 5.x
};

// 默认使用 Engine.IO 4.x (Socket.IO v3)
static const RTCVPSocketIOProtocolVersion kRTCVPSocketIOProtocolVersionDefault = RTCVPSocketIOProtocolVersion3;

// 配置键常量
extern NSString *const kRTCVPSocketIOConfigKeyForcePolling;
extern NSString *const kRTCVPSocketIOConfigKeyForceWebsockets;
extern NSString *const kRTCVPSocketIOConfigKeyReconnects;
extern NSString *const kRTCVPSocketIOConfigKeyReconnectWait;
extern NSString *const kRTCVPSocketIOConfigKeyTimeout;
extern NSString *const kRTCVPSocketIOConfigKeyConnectParams;
extern NSString *const kRTCVPSocketIOConfigKeyNamespace;
extern NSString *const kRTCVPSocketIOConfigKeyIgnoreSSLErrors;
extern NSString *const kRTCVPSocketIOConfigKeyProtocolVersion;
extern NSString *const kRTCVPSocketIOConfigKeyPingInterval;
extern NSString *const kRTCVPSocketIOConfigKeyPingTimeout;
extern NSString *const kRTCVPSocketIOConfigKeyExtraHeaders;
extern NSString *const kRTCVPSocketIOConfigKeyCookies;
extern NSString *const kRTCVPSocketIOConfigKeyPath;
extern NSString *const kRTCVPSocketIOConfigKeySecure;
extern NSString *const kRTCVPSocketIOConfigKeySelfSigned;
extern NSString *const kRTCVPSocketIOConfigKeySecurity;
extern NSString *const kRTCVPSocketIOConfigKeyCompress;

NS_ASSUME_NONNULL_END
