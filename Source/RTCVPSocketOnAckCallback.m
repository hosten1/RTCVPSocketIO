//
//  RTCVPSocketOnAckCallback.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/26/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketOnAckCallback.h"
#import "RTCVPSocketAckManager.h"

@interface RTCVPSocketOnAckCallback()

@property (nonatomic, weak) id<RTCVPSocketIOClientProtocol> socket;
@property (nonatomic, strong) NSArray* items;
@property (nonatomic) int ackNum;

@end

@implementation RTCVPSocketOnAckCallback

-(instancetype)initAck:(int)ack items:(NSArray*)items socket:(id<RTCVPSocketIOClientProtocol>)socket
{
    self = [super init];
    if(self) {
        self.socket = socket;
        self.ackNum = ack;
        self.items = items;
    }
    return self;
}


-(void)timingOutAfter:(double)seconds callback:(RTCVPScoketAckArrayCallback)callback {
    
    if (self.socket != nil && _ackNum != -1) {
        
        [self.socket.ackHandlers addAck:_ackNum callback:callback];
        [self.socket  emitAck:_ackNum withItems:_items isEvent:YES];
        if(seconds >0 ) {
            __weak typeof(self) weakSelf = self;
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(seconds * NSEC_PER_SEC)),(_socket == nil ? dispatch_get_global_queue(0, 0):self.socket.handleQueue), ^{
                @autoreleasepool
                {
                    __strong typeof(weakSelf) strongSelf = weakSelf;
                    if(strongSelf) {
                        [strongSelf.socket.ackHandlers timeoutAck:strongSelf.ackNum
                                                          onQueue:strongSelf.socket.handleQueue];
                    }
                }
            });
        }
    }
}

@end
