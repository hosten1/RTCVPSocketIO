//
//  RTCVPSocketEngine+EnginePollable.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/26/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketEngine.h"
#import "RTCVPSocketEngine+Private.h"

@interface RTCVPSocketEngine (EnginePollable)

- (void) doPoll;
- (void) stopPolling;
- (void) doLongPoll:(NSURLRequest *)request;
- (void) disconnectPolling;
- (void) flushWaitingForPost;
- (void)sendPollMessage:(NSString *)message withType:(RTCVPSocketEnginePacketType)type withData:(NSArray *)array;
@end
