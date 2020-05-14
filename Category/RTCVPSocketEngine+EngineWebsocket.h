//
//  RTCVPSocketEngine+EngineWebsocket.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/26/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketEngine.h"
#import "RTCVPSocketEngine+Private.h"

@interface RTCVPSocketEngine (EngineWebsocket)

-(void) sendWebSocketMessage:(NSString*)message withType:(RTCVPSocketEnginePacketType)type withData:(NSArray*)datas;
-(void) probeWebSocket;

@end
