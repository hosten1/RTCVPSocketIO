//
//  RTCVPSocketAck.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/26/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "RTCVPSocketIOClientProtocol.h"

@interface RTCVPSocketAck : NSObject

@property (nonatomic, readonly) int ack;
@property (nonatomic, strong, readonly) RTCVPScoketAckArrayCallback callback;

-(instancetype)initWithAck:(int)ack andCallBack:(RTCVPScoketAckArrayCallback)callback;

@end

