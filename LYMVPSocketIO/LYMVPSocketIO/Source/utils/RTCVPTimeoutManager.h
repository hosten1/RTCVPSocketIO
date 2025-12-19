//
//  RTCVPTimeoutManager.h
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/11.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef void (^RTCVPTimeoutBlock)(void);
typedef void (^RTCVPTimeoutCompleteBlock)(BOOL timeout);

/**
 超时管理器
 统一管理各种超时任务，支持取消和重置
 */
@interface RTCVPTimeoutManager : NSObject

#pragma mark - 单例

+ (instancetype)sharedManager;

#pragma mark - 超时任务管理

/**
 创建超时任务
 
 @param timeout 超时时间（秒）
 @param identifier 任务标识符
 @param timeoutBlock 超时回调
 @return 超时任务ID（可用于取消）
 */
- (NSString *)scheduleTimeout:(NSTimeInterval)timeout
                   identifier:(NSString *)identifier
                 timeoutBlock:(RTCVPTimeoutBlock)timeoutBlock;

/**
 创建带完成回调的超时任务
 
 @param timeout 超时时间（秒）
 @param identifier 任务标识符
 @param completeBlock 完成回调（timeout参数表示是否超时）
 @return 超时任务ID
 */
- (NSString *)scheduleTimeout:(NSTimeInterval)timeout
                   identifier:(NSString *)identifier
                completeBlock:(RTCVPTimeoutCompleteBlock)completeBlock;

/**
 完成超时任务
 
 @param taskId 任务ID
 @param success 是否成功完成（如果为YES，将取消超时计时）
 */
- (void)completeTask:(NSString *)taskId success:(BOOL)success;

/**
 取消超时任务
 
 @param taskId 任务ID
 */
- (void)cancelTask:(NSString *)taskId;

/**
 取消所有超时任务
 */
- (void)cancelAllTasks;

/**
 取消指定标识符的所有超时任务
 
 @param identifier 任务标识符
 */
- (void)cancelAllTasksWithIdentifier:(NSString *)identifier;

/**
 重置指定标识符的超时任务
 
 @param identifier 任务标识符
 */
- (void)resetTasksWithIdentifier:(NSString *)identifier;

/**
 获取当前活跃的超时任务数量
 
 @return 任务数量
 */
- (NSUInteger)activeTaskCount;

/**
 获取指定标识符的超时任务数量
 
 @param identifier 任务标识符
 @return 任务数量
 */
- (NSUInteger)taskCountForIdentifier:(NSString *)identifier;

@end

NS_ASSUME_NONNULL_END
