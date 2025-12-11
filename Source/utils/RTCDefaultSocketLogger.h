//
//  RTCDefaultSocketLogger.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/23/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "RTCVPSocketLogger.h"

@interface RTCDefaultSocketLogger:NSObject


/// 共享日志记录器实例
@property (class, nonatomic, strong, readonly) RTCVPSocketLogger *logger;


+ (void) setCoustomLogger:(RTCVPSocketLogger *)logger;

/// 设置日志是否启用
+ (void)setEnabled:(BOOL)enabled;

/// 设置日志级别
+ (void)setLogLevel:(RTCLogLevel)level;

@end
