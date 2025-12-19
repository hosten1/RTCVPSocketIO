//
//  RTCVPACKManager.h
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/11.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "RTCVPSocketPacket.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCVPACKManager : NSObject

#pragma mark - 配置
@property (nonatomic, assign) NSTimeInterval defaultTimeout;
@property (nonatomic, assign) NSInteger maxPendingPackets;

#pragma mark - 初始化
- (instancetype)initWithDefaultTimeout:(NSTimeInterval)timeout;

#pragma mark - 包管理
- (void)registerPacket:(RTCVPSocketPacket *)packet;
- (BOOL)acknowledgePacketWithId:(NSInteger)packetId data:(nullable NSArray *)data;
- (BOOL)failPacketWithId:(NSInteger)packetId error:(nullable NSError *)error;
- (void)removePacketWithId:(NSInteger)packetId;
- (void)removeAllPackets;

#pragma mark - 查询
- (nullable RTCVPSocketPacket *)packetForId:(NSInteger)packetId;
- (NSInteger)activePacketCount;
- (NSArray<NSNumber *> *)allPacketIds;

#pragma mark - 超时检查
- (void)checkTimeouts;
- (void)startPeriodicTimeoutCheckWithInterval:(NSTimeInterval)interval;
- (void)stopPeriodicTimeoutCheck;

@end

NS_ASSUME_NONNULL_END

