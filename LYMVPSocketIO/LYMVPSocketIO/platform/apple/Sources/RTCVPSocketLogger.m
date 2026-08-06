//
//  RTCVPSocketLogger.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/26/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketLogger.h"

@interface RTCVPSocketLogger()
@property (nonatomic, copy) void(^logCb)(NSString *message, NSString *type);
@end

@implementation RTCVPSocketLogger

- (instancetype)init {
    self = [super init];
    if (self) {
        self.log = NO;
        self.logLevel = RTCLogLevelInfo;
    }
    return self;
}

- (void)log:(NSString *)message type:(NSString *)type {
    [self printLog:@"LOG" message:message type:type];
}

- (void)error:(NSString *)message type:(NSString *)type {
    [self printLog:@"ERROR" message:message type:type];
}

- (void)logMessage:(NSString *)message type:(NSString *)type level:(RTCLogLevel)level {
    // 根据日志级别决定是否记录
    if (level <= self.logLevel) {
        NSString *levelString = @"";
        switch (level) {
            case RTCLogLevelError: levelString = @"ERROR"; break;
            case RTCLogLevelWarning: levelString = @"WARN"; break;
            case RTCLogLevelInfo: levelString = @"INFO"; break;
            case RTCLogLevelDebug: levelString = @"DEBUG"; break;
        }
        [self printLog:levelString message:message type:type];
    }
}

- (void)printLog:(NSString *)logType message:(NSString *)message type:(NSString *)type {
    if (self.logCb) {
        self.logCb(message, type);
    }
}

- (void)onLogMsgWithCB:(void (^)(NSString *, NSString *))cb {
    self.logCb = cb;
}

@end
