//
//  RTCVPACKManager+Monitoring.h
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/16.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#import "RTCVPACKManager.h"

NS_ASSUME_NONNULL_BEGIN

// 添加监控扩展
@interface RTCVPACKManager (Monitoring)

@property (nonatomic, copy) void(^ackAddedHandler)(NSInteger ackId, NSTimeInterval timeout);
@property (nonatomic, copy) void(^ackExecutedHandler)(NSInteger ackId, BOOL success, NSArray * _Nullable data);
@property (nonatomic, copy) void(^ackTimeoutHandler)(NSInteger ackId);

/// 获取ACK统计信息
- (NSDictionary *)ackStatistics;

@end

NS_ASSUME_NONNULL_END
