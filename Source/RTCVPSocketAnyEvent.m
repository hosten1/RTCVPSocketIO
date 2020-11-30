//
//  RTCVPSocketAnyEvent.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketAnyEvent.h"
#import "RTCVPSocketAckEmitter.h"

@implementation RTCVPSocketAnyEvent

-(NSString *)description {
    return [NSString stringWithFormat:@"RTCVPSocketAnyEvent: Event: %d items: %@", (int)_event, _items.description];
}

-(instancetype)initWithEvent:(NSString*)event andItems:(NSArray*)items {
    self = [super init];
    if(self) {
        _event = event;
        _items = items;
    }
    return self;
}

@end


@implementation RTCVPSocketEventHandler : NSObject

-(instancetype)initWithEvent:(NSString*)event uuid:(NSUUID*)uuid andCallback:(RTCVPSocketOnEventCallback)callback{
    self = [super init];
    if(self) {
        _event = event;
        _uuid = uuid;
        _callback = callback;
    }
    return self;
}

-(void)executeCallbackWith:(NSArray*)items withAck:(int)ack withSocket:(id<RTCVPSocketIOClientProtocol>)socket{
    if (self && _callback) {
        self.callback(items, [[RTCVPSocketAckEmitter alloc] initWithSocket:socket ackNum:ack]);
    }
}
@end
