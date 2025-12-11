//
//  SocketAckManager.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/20/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketAckManager.h"

// 简单的ACK回调存储结构
@interface RTCVPSocketAckItem : NSObject
@property (nonatomic, assign) int ack;
@property (nonatomic, copy) RTCVPScoketAckArrayCallback callback;
@end

@implementation RTCVPSocketAckItem

- (instancetype)initWithAck:(int)ack andCallBack:(RTCVPScoketAckArrayCallback)callback {
    self = [super init];
    if (self) {
        _ack = ack;
        _callback = [callback copy];
    }
    return self;
}

@end

@interface RTCVPSocketAckManager()
{
    NSMutableSet<RTCVPSocketAckItem*>*acks;
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
    if (!callback) {
        return;
    }
    
    dispatch_semaphore_wait(ackSemaphore,DISPATCH_TIME_FOREVER);
    [acks addObject:[[RTCVPSocketAckItem alloc] initWithAck:ack andCallBack:callback]];
    dispatch_semaphore_signal(ackSemaphore);
}

-(void)executeAck:(int)ack withItems:(NSArray*)items onQueue:(dispatch_queue_t)queue
{
    RTCVPScoketAckArrayCallback callback = [self removeAckCallbackWithId:ack];
    if (callback) {
        dispatch_async(queue, ^
        {
            @autoreleasepool
            {
                callback(items);
            }
        });
    }
}

-(void)timeoutAck:(int)ack onQueue:(dispatch_queue_t)queue
{
    RTCVPScoketAckArrayCallback callback = [self removeAckCallbackWithId:ack];
    if (callback) {
        dispatch_async(queue, ^
        {
            @autoreleasepool
            {
                callback(@[@"NO ACK"]);
            }
        });
    }
}

-(RTCVPScoketAckArrayCallback)removeAckCallbackWithId:(int)ack {
    dispatch_semaphore_wait(ackSemaphore,DISPATCH_TIME_FOREVER);
    RTCVPSocketAckItem *ackItem = nil;
    for (RTCVPSocketAckItem *item in acks) {
        if(item.ack == ack) {
            ackItem = item;
            break;
        }
    }
    
    RTCVPScoketAckArrayCallback callback = nil;
    if (ackItem) {
        callback = [ackItem.callback copy];
        [acks removeObject:ackItem];
    }
    
    dispatch_semaphore_signal(ackSemaphore);
    return callback;
}

-(void)removeAllAcks{
    dispatch_semaphore_wait(ackSemaphore,DISPATCH_TIME_FOREVER);
    [acks removeAllObjects];
    dispatch_semaphore_signal(ackSemaphore);
}

@end

