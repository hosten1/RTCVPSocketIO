//
//  RTCVPSocketEngine+EngineWebsocket.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/26/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketEngine+EngineWebsocket.h"
#import "RTCDefaultSocketLogger.h"

@implementation RTCVPSocketEngine (EngineWebsocket)


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
    if([self.ws isConnected])
    {
        [self sendWebSocketMessage:@"probe" withType:RTCVPSocketEnginePacketTypePing withData:@[]];
    }
}

- (NSData*)createBinaryDataForSend:(NSData *)data
{
    const Byte byte = 0x4;
    NSMutableData *byteData = [NSMutableData dataWithBytes:&byte length:sizeof(Byte)];
    [byteData appendData:data];
    return  byteData;
}

@end
