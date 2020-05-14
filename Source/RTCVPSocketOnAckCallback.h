//
//  VPSocketOnAckCallback.h
//  VPSocketIO
//
//  Created by Vasily Popov on 9/26/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "RTCVPSocketIOClientProtocol.h"

@interface RTCVPSocketOnAckCallback : NSObject
@property (nonatomic,readonly) int ackNum;
-(instancetype)initAck:(int)ack items:(NSArray*)items socket:(id<RTCVPSocketIOClientProtocol>)socket;
-(void)timingOutAfter:(double)seconds callback:(RTCVPScoketAckArrayCallback)callback;

@end
