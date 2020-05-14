//
//  RTCVPSocketAnyEvent.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "RTCVPSocketIOClientProtocol.h"

@interface RTCVPSocketAnyEvent : NSObject

@property (nonatomic, strong, readonly) NSString* event;
@property (nonatomic, strong, readonly) NSArray *items;

-(instancetype)initWithEvent:(NSString*)event andItems:(NSArray*)items;

@end


@interface RTCVPSocketEventHandler : NSObject

@property (nonatomic, strong, readonly) NSString* event;
@property (nonatomic, strong, readonly) NSUUID *uuid;
@property (nonatomic, strong, readonly) RTCVPSocketOnEventCallback callback;

-(instancetype)initWithEvent:(NSString*)event uuid:(NSUUID*)uuid andCallback:(RTCVPSocketOnEventCallback)callback;

-(void)executeCallbackWith:(NSArray*)items withAck:(int)ack withSocket:(id<RTCVPSocketIOClientProtocol>)socket;
@end
