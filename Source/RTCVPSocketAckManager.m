//
//  SocketAckManager.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/20/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketAckManager.h"
#import "RTCVPSocketAck.h"

@interface RTCVPSocketAckManager()
{
    NSMutableSet<RTCVPSocketAck*>*acks;
    dispatch_semaphore_t ackSemaphore;
}

@end

@implementation RTCVPSocketAckManager

-(instancetype)init
{
    self = [super init];
    if(self) {
        acks = [NSMutableSet set];
        ackSemaphore = dispatch_semaphore_create(1);
    }
    return self;
}
-(void)addAck:(int)ack callback:(RTCVPScoketAckArrayCallback)callback
{
    dispatch_semaphore_wait(ackSemaphore,DISPATCH_TIME_FOREVER);
    [acks addObject:[[RTCVPSocketAck alloc] initWithAck:ack andCallBack:callback]];
    dispatch_semaphore_signal(ackSemaphore);
}
-(void)executeAck:(int)ack withItems:(NSArray*)items onQueue:(dispatch_queue_t)queue
{
    RTCVPSocketAck *socketAck = [self removeAckWithId:ack];
    dispatch_async(queue, ^
    {
        @autoreleasepool
        {
            if(socketAck && socketAck.callback) {
                socketAck.callback(items);
            }
        }
    });
    
}
-(void)timeoutAck:(int)ack onQueue:(dispatch_queue_t)queue
{
    RTCVPSocketAck *socketAck = [self removeAckWithId:ack];
    dispatch_async(queue, ^
    {
        @autoreleasepool
        {
            if(socketAck && socketAck.callback) {
                socketAck.callback(@[@"NO ACK"]);
            }
        }
    });
}

-(RTCVPSocketAck*)removeAckWithId:(int)ack {
    dispatch_semaphore_wait(ackSemaphore,DISPATCH_TIME_FOREVER);
    RTCVPSocketAck *socketAck = nil;
    for (RTCVPSocketAck *vpack in acks) {
        if(vpack.ack == ack) {
            socketAck = vpack;
        }
    }
    if (socketAck) {
        [acks removeObject:socketAck];
    }
    dispatch_semaphore_signal(ackSemaphore);
    return socketAck;
}

@end

