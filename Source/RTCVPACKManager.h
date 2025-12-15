//
//  RTCVPACKManager.h
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/11.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

// RTCVPACKManager.h - Unified ACK Manager
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// 兼容旧的回调类型
typedef void (^RTCVPScoketAckArrayCallback)(NSArray*array);
typedef void (^RTCVPACKCallback)(NSArray *response);

@interface RTCVPACKManager : NSObject

/// 生成唯一的 ACK ID
- (NSInteger)generateACKId;

/// 添加 ACK 回调（新接口，兼容两种回调类型）
- (void)addCallback:(nullable RTCVPACKCallback)callback forId:(NSInteger)ackId;

/// 添加 ACK 回调（旧接口，兼容RTCVPSocketAckManager）
- (void)addAck:(int)ack callback:(nullable RTCVPScoketAckArrayCallback)callback;

/// 执行并移除 ACK 回调
- (void)executeCallbackForId:(NSInteger)ackId withData:(nullable NSArray *)data;

/// 执行并移除 ACK 回调（旧接口，兼容RTCVPSocketAckManager）
- (void)executeAck:(int)ack withItems:(nullable NSArray*)items onQueue:(dispatch_queue_t)queue;

/// 超时处理（旧接口，兼容RTCVPSocketAckManager）
- (void)timeoutAck:(int)ack onQueue:(dispatch_queue_t)queue;

/// 移除 ACK 回调
- (void)removeCallbackForId:(NSInteger)ackId;

/// 清理所有 ACK 回调
- (void)removeAllCallbacks;

/// 清理所有 ACK 回调（旧接口，兼容RTCVPSocketAckManager）
- (void)removeAllAcks;

/// 获取当前活跃的 ACK 数量
- (NSInteger)activeACKCount;

@end

NS_ASSUME_NONNULL_END

