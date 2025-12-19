//
//  RTCVPSocketLogger.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/26/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#ifndef RTCVPSocketLogger_H
#define RTCVPSocketLogger_H

// RTCVPSocketLogger.h
#import <Foundation/Foundation.h>



NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, RTCLogLevel) {
    RTCLogLevelError = 0,
    RTCLogLevelWarning,
    RTCLogLevelInfo,
    RTCLogLevelDebug
};

@interface RTCVPSocketLogger : NSObject

@property (nonatomic, assign) BOOL log;
@property (nonatomic, assign) RTCLogLevel logLevel;

/// 记录日志消息
- (void)log:(NSString *)message type:(NSString *)type;
/// 记录错误消息
- (void)error:(NSString *)message type:(NSString *)type;
/// 设置日志回调
- (void)onLogMsgWithCB:(void(^)(NSString *message, NSString *type))cb;

/// 简化的日志方法
- (void)logMessage:(NSString *)message type:(NSString *)type level:(RTCLogLevel)level;

@end

NS_ASSUME_NONNULL_END
#endif // RTCVPSocketLogger_H
