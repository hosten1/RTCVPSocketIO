//
//  SocketAckManager.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/20/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "RTCVPSocketIOClientProtocol.h"

@interface RTCVPSocketAckManager : NSObject

-(void)addAck:(int)ack callback:(RTCVPScoketAckArrayCallback)callback;
-(void)executeAck:(int)ack withItems:(NSArray*)items onQueue:(dispatch_queue_t)queue;
-(void)timeoutAck:(int)ack onQueue:(dispatch_queue_t)queue;

- (void)removeAllAcks;


@end

