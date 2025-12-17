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
#import "RTCVPWebSocketProtocolFixer.h"

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

/**
 * 发送 WebSocket 消息（Engine.IO / Socket.IO 协议）
 *
 * WebSocket 最终发送的内容分两类：
 *  1. 文本帧：用于 Engine.IO 字符包、Socket.IO JSON 包
 *  2. 二进制帧：用于传输二进制 payload（engine v3 与 v4 规则不同）
 *
 * Engine.IO / Socket.IO 消息格式说明：
 *
 * 【文本消息格式】（Engine.IO 文本帧）
 *   [EngineType][Payload]
 *   EngineType：单字符数字，如：
 *      0 open
 *      1 close
 *      2 ping
 *      3 pong
 *      4 message
 *      5 upgrade
 *      6 noop
 *
 *   Payload：通常是 JSON（Socket.IO）或字符串
 *   示例："42["chat","hello"]"
 *       4  -> Engine.IO type: message
 *       2  -> Socket.IO packet type: event
 *       ["chat","hello"] -> event payload
 *
 *
 * 【二进制消息格式】
 *   Engine.IO v3：
 *       0x04 + <binary payload>
 *       0x04 为 Engine.IO 二进制包类型前缀
 *
 *   Engine.IO v4：
 *       直接发送纯二进制 WebSocket 帧，不需要前缀
 *
 */
- (void)sendWebSocketMessage:(NSString *)message
                    withType:(RTCVPSocketEnginePacketType)type
                    withData:(NSArray<NSData *> *)data {

    // 1. 确保 WebSocket 已建立
    if (!self.ws || ![self.ws isConnected]) {
        [self log:@"WebSocket not connected, cannot send message" level:RTCLogLevelWarning];
        return;
    }

    // 2. 构建 Engine.IO 文本消息格式
    //    格式：[EngineType][Payload]
    //    例如：@"4{\"msg\":\"hello\"}"
    NSString *fullMessage = [NSString stringWithFormat:@"%ld%@", (long)type, message];

    [self log:[NSString stringWithFormat:@"Sending WebSocket text message: %@", fullMessage]
         level:RTCLogLevelDebug];

    // 3. 发送文本帧
    //    文本帧用于 Socket.IO/Engine.IO 的主控制消息
    [self.ws writeString:fullMessage];

    // 4. 若附带二进制数据，则逐个发送二进制帧
    if (self.config.enableBinary && data.count > 0) {

        for (NSData *binaryData in data) {
            NSData *packetData = binaryData;

            // Engine.IO v3 需要加前缀 0x04
            // 0x04 表示 binary message（engine binary packet）
            if (self.config.protocolVersion == RTCVPSocketIOProtocolVersion2) {
                const Byte binaryPrefix = 0x04;

                // 构建 [0x04][binary payload]
                NSMutableData *mutableData = [NSMutableData dataWithBytes:&binaryPrefix length:1];
                [mutableData appendData:binaryData];

                packetData = mutableData;
            }

            [self log:@"Sending WebSocket binary packet" level:RTCLogLevelDebug];

            // Engine.IO v4：发送纯二进制帧
            // Engine.IO v3：发送 0x04 + payload
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
            // WebSocket连接断开
            self.websocket = NO;
            
            // 如果配置了只使用WebSocket传输，使用延迟重连
            if (self.config.transport == RTCVPSocketIOTransportWebSocket) {
                [self log:@"WebSocket transport configured, scheduling delayed reconnect..." level:RTCLogLevelInfo];
                // 使用延迟重连，避免频繁连接尝试
                [self delayReconnect];
            } else {
                // WebSocket 断开，尝试回退到轮询
                self.polling = YES;
                
                [self log:@"Falling back to polling" level:RTCLogLevelInfo];
                
                if (self.connected) {
                    [self doPoll];
                }
            }
        } else if (self.connected) {
            // 在探测期间断开，关闭连接
            [self closeOutEngine:errorDescription];
        } else {
            // 连接尚未建立，处理为连接失败
            [self log:@"WebSocket connection failed" level:RTCLogLevelError];
            if (!self.closed) {
                [self didError:errorDescription];
                // 尝试延迟重连
                [self delayReconnect];
            }
        }
    }
}

- (void)websocket:(RTCJFRWebSocket *)socket didReceiveMessage:(NSString *)string {
    // 打印收到的消息字符串
    [self log:[NSString stringWithFormat:@"📩 Socket层收到字符串数据: %@", string] level:RTCLogLevelInfo];
    [self parseEngineMessage:string];
}

// 在 websocket:didReceiveData: 方法中，添加协议修复
- (void)websocket:(RTCJFRWebSocket *)socket didReceiveData:(NSData *)data {
    if (data.length == 0) {
        [self log:@"WebSocket received empty binary data" level:RTCLogLevelWarning];
        return;
    }
    
    // 分析WebSocket帧
    NSDictionary *frameInfo = [RTCVPWebSocketProtocolFixer analyzeWebSocketFrame:data];
    [self log:[NSString stringWithFormat:@"WebSocket帧分析: %@", frameInfo] level:RTCLogLevelDebug];
    
    // RTCJFRWebSocket 已经正确解析了 WebSocket 帧
    // 我们收到的 data 已经是有效负载（去除了帧头、掩码等）
       
    [self log:[NSString stringWithFormat:@"📦 收到WebSocket二进制数据，长度: %lu", (unsigned long)data.length]
            level:RTCLogLevelInfo];
       
    // 直接传递给 parseEngineData
    [self parseEngineData:data];
}

// 添加处理WebSocket文本帧的方法
- (void)handleWebSocketTextFrame:(NSData *)data {
    // 解析WebSocket帧，提取有效负载
    NSData *payload = [self extractWebSocketPayload:data];
    
    if (payload) {
        NSString *message = [[NSString alloc] initWithData:payload encoding:NSUTF8StringEncoding];
        if (message) {
            [self log:[NSString stringWithFormat:@"WebSocket文本消息: %@", message] level:RTCLogLevelDebug];
            [self parseEngineMessage:message];
        } else {
            [self log:@"无法将WebSocket负载解析为文本" level:RTCLogLevelWarning];
        }
    }
}

// 提取WebSocket帧中的有效负载
- (NSData *)extractWebSocketPayload:(NSData *)frame {
    if (frame.length < 2) return nil;
    
    const uint8_t *bytes = (const uint8_t *)frame.bytes;
    
    // 跳过帧头
    NSUInteger headerLength = 2;
    uint8_t payloadLenByte = bytes[1] & 0x7F;
    
    // 处理扩展长度
    if (payloadLenByte == 126) {
        headerLength += 2;
    } else if (payloadLenByte == 127) {
        headerLength += 8;
    }
    
    // 处理掩码
    BOOL masked = (bytes[1] & 0x80) != 0;
    if (masked) {
        headerLength += 4;
    }
    
    // 检查帧长度
    if (frame.length <= headerLength) {
        return nil;
    }
    
    // 提取负载
    NSData *payload = [frame subdataWithRange:NSMakeRange(headerLength, frame.length - headerLength)];
    
    // 如果被掩码，解码
    if (masked && payload.length > 0) {
        const uint8_t *maskKey = bytes + (headerLength - 4);
        NSMutableData *decodedData = [NSMutableData dataWithData:payload];
        uint8_t *decodedBytes = (uint8_t *)decodedData.mutableBytes;
        
        for (NSUInteger i = 0; i < payload.length; i++) {
            decodedBytes[i] = decodedBytes[i] ^ maskKey[i % 4];
        }
        
        return decodedData;
    }
    
    return payload;
}

// 处理WebSocket Ping
- (void)handleWebSocketPing:(NSData *)pingFrame {
    // 发送Pong响应
    [self sendWebSocketPong:pingFrame];
    
    
    // 同时重置Engine.IO心跳计数器
    [self handlePong:@"WebSocket Ping"];
}

// 发送WebSocket Pong
- (void)sendWebSocketPong:(NSData *)pingFrame {
    if (!self.ws || ![self.ws isConnected]) {
        return;
    }
    
    // 构建Pong帧：操作码0xA，负载与Ping相同
    NSData *payload = [self extractWebSocketPayload:pingFrame];
    
    // 创建Pong帧
    NSMutableData *pongFrame = [NSMutableData data];
    
    // 第一个字节：FIN=1，RSV=0，操作码=0xA
    uint8_t firstByte = 0x80 | 0xA; // FIN=1, Opcode=0xA
    [pongFrame appendBytes:&firstByte length:1];
    
    // 第二个字节：掩码=0，负载长度
    uint64_t payloadLength = payload ? payload.length : 0;
    
    if (payloadLength <= 125) {
        uint8_t secondByte = (uint8_t)payloadLength;
        [pongFrame appendBytes:&secondByte length:1];
    } else if (payloadLength <= 65535) {
        uint8_t secondByte = 126;
        [pongFrame appendBytes:&secondByte length:1];
        
        uint16_t len16 = CFSwapInt16HostToBig((uint16_t)payloadLength);
        [pongFrame appendBytes:&len16 length:2];
    } else {
        uint8_t secondByte = 127;
        [pongFrame appendBytes:&secondByte length:1];
        
        uint64_t len64 = CFSwapInt64HostToBig(payloadLength);
        [pongFrame appendBytes:&len64 length:8];
    }
    
    // 添加负载
    if (payload) {
        [pongFrame appendData:payload];
    }
    
    [self.ws writeData:pongFrame];
    [self log:@"发送WebSocket Pong响应" level:RTCLogLevelDebug];
}

// 处理WebSocket Pong
- (void)handleWebSocketPong:(NSData *)pongFrame {
    // 重置心跳计数器
    [self handlePong:@"WebSocket Pong"];
}

// 处理WebSocket关闭帧
- (void)handleWebSocketClose:(NSData *)closeFrame {
    uint16_t closeCode = 1000; // 默认正常关闭
    
    if (closeFrame.length >= 4) {
        const uint8_t *bytes = (const uint8_t *)closeFrame.bytes;
        
        // 跳过帧头，提取关闭代码
        NSUInteger offset = 2; // 基本头
        uint8_t payloadLenByte = bytes[1] & 0x7F;
        
        if (payloadLenByte == 126) {
            offset += 2;
        } else if (payloadLenByte == 127) {
            offset += 8;
        }
        
        if ((bytes[1] & 0x80) != 0) { // 如果有掩码
            offset += 4;
        }
        
        if (closeFrame.length >= offset + 2) {
            closeCode = (bytes[offset] << 8) | bytes[offset + 1];
        }
    }
    
    NSString *reason = [NSString stringWithFormat:@"WebSocket关闭 (代码: %d)", closeCode];
    [self log:reason level:RTCLogLevelInfo];
    
    // 如果未主动关闭，尝试重连
    if (!self.closed) {
//        [self handleConnectionError:reason];
    }
}


@end
