//
//  RTCVPSocketIOClientProtocol.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#ifndef RTCVPSocketIOClientProtocol_H
#define RTCVPSocketIOClientProtocol_H

#import <Foundation/Foundation.h>
@class RTCVPSocketAckManager;
@class RTCVPSocketAckEmitter;

typedef void (^RTCVPScoketAckArrayCallback)(NSArray*array);
typedef void (^RTCVPSocketOnEventCallback)(NSArray*array, RTCVPSocketAckEmitter*emitter);

@protocol RTCVPSocketIOClientProtocol <NSObject>

@required

@property (nonatomic, strong) RTCVPSocketAckManager *ackHandlers;
@property (nonatomic, strong, readonly) dispatch_queue_t handleQueue;

-(void)emit:(NSString*)event items:(NSArray*)items;
-(void)emitAck:(int)ack withItems:(NSArray*)items isEvent:(BOOL)isEvent;


@end

#endif
