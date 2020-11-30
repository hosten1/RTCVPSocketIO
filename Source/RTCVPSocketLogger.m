//
//  RTCVPSocketLogger.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/26/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketLogger.h"
@interface RTCVPSocketLogger()
@property(nonatomic, copy) void(^logCb)(NSString *message ,NSString *type);
@end
@implementation RTCVPSocketLogger

- (instancetype)init
{
    self = [super init];
    if (self) {
        self.log = NO;
    }
    return self;
}

-(void) log:(NSString*)message type:(NSString*)type
{
    [self printLog:@"LOG" message:message type:type];
}
-(void) error:(NSString*)message type:(NSString*)type
{
    [self printLog:@"ERROR" message:message type:type];
}

-(void) printLog:(NSString*)logType message:(NSString*)message type:(NSString*)type
{
    if(_log) {
        NSLog(@"printLog = %@ %@: %@", logType, type, message);
    }
    if (_logCb) {
        _logCb(message,type);
    }
}
-(void)onLogMsgWithCB:(void (^)(NSString *, NSString *))cb{
    self.logCb = cb;
}
-(void)dealloc {
    
}

@end
