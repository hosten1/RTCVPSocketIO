//
//  RTCVPSocketEngine+EngineWebsocket.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/26/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketEngine.h"
#import "RTCVPSocketEngine+Private.h"

#ifdef USE_CPP_WEBSOCKET
@interface RTCVPSocketEngine (EngineWebsocket) <RTCCPPCWebSocketDelegate>
#else
@interface RTCVPSocketEngine (EngineWebsocket) <RTCJFRWebSocketDelegate>
#endif

-(void)sendWebSocketMessage:(nullable NSString*)message withType:(RTCVPSocketEnginePacketType)type withData:(nullable NSArray*)datas;

/// 探测WebSocket连接
- (void)probeWebSocket;
/// 创建WebSocket并连接
- (void)createWebSocketAndConnect;



/// 刷新WebSocket等待队列
- (void)flushProbeWait;

/// 刷新等待发送到WebSocket的消息
- (void)flushWaitingForPostToWebSocket;


- (void)doFastUpgrade;

@end
