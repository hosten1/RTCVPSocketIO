//
//  RTCVPTimer.h
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/11.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef void (^RTCVPTimerBlock)(void);

/**
 可取消的定时器类
 使用 GCD 的 dispatch_source_t 实现，支持取消和重新调度
 */
@interface RTCVPTimer : NSObject

/// 定时器是否有效
@property (nonatomic, readonly, getter=isValid) BOOL valid;

/// 定时器是否正在运行
@property (nonatomic, readonly, getter=isRunning) BOOL running;

/// 定时器标识符（可选）
@property (nonatomic, copy, nullable) NSString *identifier;

#pragma mark - 创建定时器

/**
 创建定时器（需要手动调用 start 启动）
 
 @param interval 时间间隔（秒）
 @param repeats 是否重复
 @param queue 执行队列（为 nil 则使用主队列）
 @param block 定时器回调
 @return 定时器实例
 */
+ (instancetype)timerWithTimeInterval:(NSTimeInterval)interval
                               repeats:(BOOL)repeats
                                 queue:(dispatch_queue_t _Nullable)queue
                                 block:(RTCVPTimerBlock)block;

/**
 创建并立即启动定时器
 
 @param interval 时间间隔（秒）
 @param repeats 是否重复
 @param queue 执行队列（为 nil 则使用主队列）
 @param block 定时器回调
 @return 定时器实例
 */
+ (instancetype)scheduledTimerWithTimeInterval:(NSTimeInterval)interval
                                        repeats:(BOOL)repeats
                                          queue:(dispatch_queue_t _Nullable)queue
                                          block:(RTCVPTimerBlock)block;

#pragma mark - 控制方法

/// 启动定时器
- (void)start;

/// 暂停定时器
- (void)pause;

/// 恢复定时器
- (void)resume;

/// 取消定时器
- (void)cancel;

/// 重新调度定时器（重新开始计时）
- (void)reschedule;

#pragma mark - 一次性定时器快捷方法

/**
 创建一次性定时器并立即启动
 
 @param interval 延迟时间（秒）
 @param queue 执行队列（为 nil 则使用主队列）
 @param block 定时器回调
 @return 定时器实例
 */
+ (instancetype)after:(NSTimeInterval)interval
                queue:(dispatch_queue_t _Nullable)queue
                block:(RTCVPTimerBlock)block;

@end

NS_ASSUME_NONNULL_END
