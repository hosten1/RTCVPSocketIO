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

@implementation RTCVPSocketEngine (EngineWebsocket)
- (void)createWebSocketAndConnect {
    self.ws = [[RTCJFRWebSocket alloc] initWithURL:self.urlWebSocket protocols:nil];
    self.ws.queue = self.engineQueue;
    self.ws.delegate = self;
    self.ws.voipEnabled = YES;
    self.ws.selfSignedSSL = self.selfSigned;
    self.ws.security = self.security;

    // 添加cookies和extraHeaders
    if (self.cookies.count > 0) {
        NSDictionary *headers = [NSHTTPCookie requestHeaderFieldsWithCookies:self.cookies];
        for (NSString *key in headers) {
            [self.ws addHeader:headers[key] forKey:key];
        }
    }

    for (NSString *key in self.extraHeaders) {
        [self.ws addHeader:self.extraHeaders[key] forKey:key];
    }

    [self.ws connect];
}

-(void) sendWebSocketMessage:(NSString*)message withType:(RTCVPSocketEnginePacketType)type withData:(NSArray*)datas
{
//    NSLog(@"===================type:%@ message:%@ datas:%@===============",@(type),message,datas);
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Sending ws: %@ as type:%@", message, self.stringEnginePacketType[@(type)]] type:@"SocketEngineWebSocket"];
    if (datas.count > 0) {
        [self.ws writeString:[NSString stringWithFormat:@"%lu%@",(unsigned long)type, message]];
    }else{
        [self.ws writeString:[NSString stringWithFormat:@"%lu%@",(unsigned long)type, message]];
    }
    
    if(self.websocket)
    {
        for (NSData *data in datas)
        {
            NSData *binData = [self createBinaryDataForSend:data];
            [self.ws writeData:binData];
        }
    }
}
-(void) probeWebSocket
{
    if (self.websocket) {
          return;
      }
    self.probing = YES;
    if([self.ws isConnected])
    {
        [self sendWebSocketMessage:@"" withType:RTCVPSocketEnginePacketTypePing withData:@[]];
    }
}

- (NSData*)createBinaryDataForSend:(NSData *)data
{
    const Byte byte = 0x4;
    NSMutableData *byteData = [NSMutableData dataWithBytes:&byte length:sizeof(Byte)];
    [byteData appendData:data];
    return  byteData;
}

- (void)flushProbeWait {
    [RTCDefaultSocketLogger.logger log:@"Flushing probe wait" type:self.logType];
    // 这里需要将probeWait中的消息发送出去
    // 由于probeWait是原有设计，我们暂时按照原有逻辑，将消息写入WebSocket
    for (NSDictionary *probe in self.probeWait) {
        NSString *msg = probe[@"message"];
        RTCVPSocketEnginePacketType type = [probe[@"type"] integerValue];
        NSArray *data = probe[@"data"];
        [self sendWebSocketMessage:msg withType:type withData:data];
    }
    [self.probeWait removeAllObjects];
}

- (void)flushWaitingForPostToWebSocket {
    for (NSString *packet in self.postWait) {
        [self.ws writeString:packet];
    }
    [self.postWait removeAllObjects];
}

#pragma mark - RTCJFRWebSocketDelegate

- (void)websocketDidConnect:(RTCJFRWebSocket *)socket {
    if (!self.forceWebsockets) {
        self.probing = YES;
        [self probeWebSocket];
    } else {
        self.connected = YES;
        self.probing = NO;
        self.polling = NO;
    }
}

- (void)websocketDidDisconnect:(RTCJFRWebSocket *)socket error:(NSError *)error {
    self.probing = NO;

    if (self.closed) {
        [self closeOutEngine:@"Disconnect"];
    } else {
        if (self.websocket) {
            [self flushProbeWait];
        } else {
            self.connected = NO;
            self.websocket = NO;

            NSString *reason = @"Socket Disconnected";
            if (error.localizedDescription.length > 0) {
                reason = error.localizedDescription;
            }
            [self closeOutEngine:reason];
        }
    }
}

- (void)websocket:(RTCJFRWebSocket *)socket didReceiveMessage:(NSString *)string {
    [self parseEngineMessage:string];
}

- (void)websocket:(RTCJFRWebSocket *)socket didReceiveData:(NSData *)data {
    [self parseEngineData:data];
}

@end
