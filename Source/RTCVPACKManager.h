//
//  RTCVPACKManager.h
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/11.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

// RTCVPACKManager.h
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef void (^RTCVPACKCallback)(NSArray *response);

@interface RTCVPACKManager : NSObject

/// 生成唯一的 ACK ID
- (NSInteger)generateACKId;

/// 添加 ACK 回调
- (void)addCallback:(RTCVPACKCallback)callback forId:(NSInteger)ackId;

/// 执行并移除 ACK 回调
- (void)executeCallbackForId:(NSInteger)ackId withData:(NSArray *)data;

/// 移除 ACK 回调
- (void)removeCallbackForId:(NSInteger)ackId;

/// 清理所有 ACK 回调
- (void)removeAllCallbacks;

/// 获取当前活跃的 ACK 数量
- (NSInteger)activeACKCount;

@end

NS_ASSUME_NONNULL_END

