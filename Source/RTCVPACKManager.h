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

// ACK回调类型
typedef void (^RTCVPScoketAckArrayCallback)(NSArray* array);
typedef void (^RTCVPACKCallback)(NSArray *response);
typedef void (^RTCVPACKErrorCallback)(NSArray * _Nullable data, NSError * _Nullable error);

@interface RTCVPACKManager : NSObject

/// ACK配置
@property (nonatomic, assign) NSTimeInterval defaultTimeout; // 默认超时时间
@property (nonatomic, assign) NSInteger maxPendingAcks;      // 最大挂起ACK数量

#pragma mark - 初始化

/// 初始化方法
- (instancetype)initWithDefaultTimeout:(NSTimeInterval)timeout;

#pragma mark - ACK管理（新接口）

/// 添加ACK回调（带错误处理）
- (NSInteger)addErrorCallback:(RTCVPACKErrorCallback)callback
                      timeout:(NSTimeInterval)timeout;

/// 执行ACK回调（带错误处理）
- (BOOL)executeErrorCallbackForId:(NSInteger)ackId
                         withData:(nullable NSArray *)data
                            error:(nullable NSError *)error;

#pragma mark - 兼容旧接口


/// 生成唯一的 ACK ID
- (NSInteger)generateACKId;

/// 添加 ACK 回调（兼容旧接口）
- (void)addCallback:(nullable RTCVPACKCallback)callback forId:(NSInteger)ackId;

/// 添加 ACK 回调（旧接口，兼容RTCVPSocketAckManager）
- (void)addAck:(NSInteger)ack callback:(nullable RTCVPScoketAckArrayCallback)callback;

/// 执行并移除 ACK 回调
- (void)executeCallbackForId:(NSInteger)ackId withData:(nullable NSArray *)data;

/// 执行并移除 ACK 回调（旧接口，兼容RTCVPSocketAckManager）
- (void)executeAck:(NSInteger)ack withItems:(nullable NSArray*)items onQueue:(dispatch_queue_t)queue;

/// 超时处理（旧接口，兼容RTCVPSocketAckManager）
- (void)timeoutAck:(NSInteger)ack onQueue:(dispatch_queue_t)queue;

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

