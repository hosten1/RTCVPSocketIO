//
//  RTCVPSocketAckEmitter.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "RTCVPSocketIOClientProtocol.h"

@interface RTCVPSocketAckEmitter : NSObject

-(instancetype)initWithSocket:(id<RTCVPSocketIOClientProtocol>)socket ackNum:(int)ack;
-(void)emitWith:(NSArray*) items;

@end

