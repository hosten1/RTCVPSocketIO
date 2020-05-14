//
//  RTCVPSocketAckEmitter.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketAckEmitter.h"

@interface RTCVPSocketAckEmitter()

@property (nonatomic, strong) id<RTCVPSocketIOClientProtocol> socket;
@property (nonatomic) int ackNum;

@end

@implementation RTCVPSocketAckEmitter

-(instancetype)initWithSocket:(id<RTCVPSocketIOClientProtocol>)socket ackNum:(int)ack
{
    self = [super init];
    if(self) {
        self.socket = socket;
        self.ackNum = ack;
    }
    return self;
}


-(void)emitWith:(NSArray*) items {
    if(_ackNum != -1) {
        [_socket emitAck:_ackNum withItems:items isEvent:NO];
    }
}

@end

