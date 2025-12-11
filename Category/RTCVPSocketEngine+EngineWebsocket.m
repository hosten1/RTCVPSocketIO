//
//  RTCVPSocketEngine+EngineWebsocket.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/26/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketEngine+EngineWebsocket.h"
#import "RTCDefaultSocketLogger.h"
#import "RTCVPSocketEngine+Private.h"
#import "RTCVPSocketEngine+EnginePollable.h"
#import "NSString+RTCVPSocketIO.h"
#import "RTCVPProbe.h"

@implementation RTCVPSocketEngine (EngineWebsocket)

#pragma mark - WebSocket 管理

- (void)createWebSocketAndConnect {
    if (self.closed || self.invalidated) {
        return;
    }
    
    NSURL *url = [self urlWebSocketWithSid];
    if (!url) {
        [self didError:@"Invalid WebSocket URL"];
        return;
    }
    
    [self log:@"Creating WebSocket connection..." level:RTCLogLevelDebug];
    [self log:[NSString stringWithFormat:@"WebSocket URL: %@", url.absoluteString] level:RTCLogLevelDebug];
    
    self.ws = [[RTCJFRWebSocket alloc] initWithURL:url protocols:@[]];
    self.ws.queue = self.engineQueue;
    self.ws.delegate = self;
    
    // 配置 WebSocket
    self.ws.voipEnabled = YES;
    self.ws.selfSignedSSL = self.config.allowSelfSignedCertificates;
    self.ws.security = self.config.security;
    
    // 添加 headers
    if (self.config.cookies.count > 0) {
        NSDictionary *headers = [NSHTTPCookie requestHeaderFieldsWithCookies:self.config.cookies];
        for (NSString *key in headers.allKeys) {
            [self.ws addHeader:headers[key] forKey:key];
        }
    }
    
    if (self.config.extraHeaders) {
        for (NSString *key in self.config.extraHeaders.allKeys) {
            NSString *value = self.config.extraHeaders[key];
            if ([value isKindOfClass:[NSString class]]) {
                [self.ws addHeader:value forKey:key];
            }
        }
    }
    
    [self.ws connect];
}

- (NSURL *)urlWebSocketWithSid {
    if (!self.urlWebSocket) {
        return nil;
    }
    
    NSURLComponents *components = [NSURLComponents componentsWithURL:self.urlWebSocket resolvingAgainstBaseURL:NO];
    NSMutableString *query = [components.percentEncodedQuery mutableCopy] ?: [NSMutableString string];
    
    if (self.sid.length > 0) {
        NSString *sidParam = [NSString stringWithFormat:@"&sid=%@", [self.sid urlEncode]];
        if (query.length > 0) {
            [query appendString:sidParam];
        } else {
            [query appendString:[sidParam substringFromIndex:1]];
        }
    }
    
    components.percentEncodedQuery = query;
    return components.URL;
}

- (void)sendWebSocketMessage:(NSString *)message withType:(RTCVPSocketEnginePacketType)type withData:(NSArray *)data {
    if (!self.ws || ![self.ws isConnected]) {
        [self log:@"WebSocket not connected, cannot send message" level:RTCLogLevelWarning];
        return;
    }
    
    // 构建完整消息：类型 + 消息内容
    NSString *fullMessage = [NSString stringWithFormat:@"%ld%@", (long)type, message];
    
    [self log:[NSString stringWithFormat:@"Sending WebSocket message: %@", fullMessage] level:RTCLogLevelDebug];
    
    // 发送文本消息
    [self.ws writeString:fullMessage];
    
    // 发送二进制数据（如果需要）
    if (self.config.enableBinary && data.count > 0) {
        for (NSData *binaryData in data) {
            // Engine.IO 4.x 使用原生二进制
            NSData *packetData = binaryData;
            
            // Engine.IO 3.x 需要添加前缀
            if (self.config.protocolVersion == RTCVPSocketIOProtocolVersion2) {
                const Byte binaryPrefix = 0x04;
                NSMutableData *mutableData = [NSMutableData dataWithBytes:&binaryPrefix length:1];
                [mutableData appendData:binaryData];
                packetData = mutableData;
            }
            
            [self.ws writeData:packetData];
        }
    }
}

- (void)probeWebSocket {
    if (!self.ws || ![self.ws isConnected] || self.probing || self.websocket) {
        return;
    }
    
    [self log:@"Probing WebSocket connection..." level:RTCLogLevelDebug];
    
    self.probing = YES;
    
    // 发送探测包
    NSString *probeMessage = @"probe";
    [self sendWebSocketMessage:probeMessage withType:RTCVPSocketEnginePacketTypePing withData:@[]];
    
    // 设置探测超时 - 使用超时管理器
    [self startProbeTimeout];
}

- (void)doFastUpgrade {
    if (!self.fastUpgrade || !self.ws || ![self.ws isConnected]) {
        return;
    }
    
    [self log:@"Performing fast upgrade to WebSocket" level:RTCLogLevelDebug];
    
    // 发送升级消息
    [self sendWebSocketMessage:@"" withType:RTCVPSocketEnginePacketTypeUpgrade withData:@[]];
    
    // 更新状态
    self.websocket = YES;
    self.polling = NO;
    self.fastUpgrade = NO;
    self.probing = NO;
    
    // 取消探测超时
    [self cancelProbeTimeout];
    
    // 开始心跳
    [self startPingTimer];
    
    // 发送缓存在探测期间的消息
    [self flushProbeWait];
}

- (void)flushProbeWait {
    if (self.probeWait.count == 0) {
        return;
    }
    
    [self log:[NSString stringWithFormat:@"Flushing %lu probe wait messages", (unsigned long)self.probeWait.count] level:RTCLogLevelDebug];
    
    for (RTCVPProbe *probe in self.probeWait) {
        [self sendWebSocketMessage:probe.message withType:probe.type withData:probe.data];
    }
    
    [self.probeWait removeAllObjects];
}

- (void)flushWaitingForPostToWebSocket {
    if (self.postWait.count == 0 || !self.ws) {
        return;
    }
    
    [self log:[NSString stringWithFormat:@"Flushing %lu post wait messages to WebSocket", (unsigned long)self.postWait.count] level:RTCLogLevelDebug];
    
    for (NSString *packet in self.postWait) {
        [self.ws writeString:packet];
    }
    
    [self.postWait removeAllObjects];
}

#pragma mark - RTCJFRWebSocketDelegate

- (void)websocketDidConnect:(RTCJFRWebSocket *)socket {
    [self log:@"WebSocket connected" level:RTCLogLevelInfo];
    
    if (self.config.transport == RTCVPSocketIOTransportWebSocket) {
        // 强制 WebSocket 模式，直接使用
        self.websocket = YES;
        self.polling = NO;
        self.connected = YES;
        
        // 取消连接超时（如果存在）
        [self cancelConnectionTimeout];
        
        // 开始心跳
        [self startPingTimer];
        
        // 如果已经有 sid，表示是重连
        if (self.sid.length > 0) {
            // 发送升级消息
            [self sendWebSocketMessage:@"" withType:RTCVPSocketEnginePacketTypeUpgrade withData:@[]];
        } else {
            // 通知客户端连接成功
            if (self.client) {
                [self.client engineDidOpen:@"WebSocket connected"];
            }
        }
    } else if (self.config.transport == RTCVPSocketIOTransportAuto) {
        // 自动模式，需要探测 WebSocket
        [self probeWebSocket];
    } else {
        // 强制轮询，关闭 WebSocket
        [self log:@"WebSocket not needed for polling transport" level:RTCLogLevelDebug];
        [socket disconnect];
    }
}

- (void)websocketDidDisconnect:(RTCJFRWebSocket *)socket error:(NSError *)error {
    NSString *errorDescription = error ? error.localizedDescription : @"Disconnected";
    [self log:[NSString stringWithFormat:@"WebSocket disconnected: %@", errorDescription] level:RTCLogLevelWarning];
    
    // 取消探测超时
    [self cancelProbeTimeout];
    
    if (self.closed) {
        [self closeOutEngine:@"WebSocket closed"];
    } else {
        if (self.websocket) {
            // WebSocket 断开，尝试回退到轮询
            self.websocket = NO;
            self.polling = YES;
            
            [self log:@"Falling back to polling" level:RTCLogLevelInfo];
            
            if (self.connected) {
                [self doPoll];
            }
        } else if (self.connected) {
            // 在探测期间断开，关闭连接
            [self closeOutEngine:errorDescription];
        } else {
            // 连接尚未建立，处理为连接失败
            [self log:@"WebSocket connection failed" level:RTCLogLevelError];
            if (!self.closed) {
                [self didError:errorDescription];
            }
        }
    }
}

- (void)websocket:(RTCJFRWebSocket *)socket didReceiveMessage:(NSString *)string {
    [self parseEngineMessage:string];
}

- (void)websocket:(RTCJFRWebSocket *)socket didReceiveData:(NSData *)data {
    if (data.length == 0) {
        [self log:@"WebSocket received empty binary data" level:RTCLogLevelWarning];
        return;
    }
    
    [self log:[NSString stringWithFormat:@"WebSocket received binary data, length: %lu", (unsigned long)data.length] level:RTCLogLevelDebug];
    
    // 解析二进制数据
    [self parseEngineData:data];
}


@end
