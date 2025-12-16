//
//  RTCVPSocketPacket.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

// RTCVPSocketPacket.h
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// 数据包类型
typedef NS_ENUM(NSUInteger, RTCVPPacketType) {
    RTCVPPacketTypeConnect = 0,
    RTCVPPacketTypeDisconnect,
    RTCVPPacketTypeEvent,
    RTCVPPacketTypeAck,
    RTCVPPacketTypeError,
    RTCVPPacketTypeBinaryEvent,
    RTCVPPacketTypeBinaryAck
};

// ACK回调类型
typedef void (^RTCVPPacketSuccessCallback)(NSArray * _Nullable response);
typedef void (^RTCVPPacketErrorCallback)(NSError * _Nullable error);

// 包状态
typedef NS_ENUM(NSUInteger, RTCVPPacketState) {
    RTCVPPacketStatePending,      // 等待ACK
    RTCVPPacketStateAcknowledged, // 已确认
    RTCVPPacketStateTimeout,      // 超时
    RTCVPPacketStateCancelled     // 已取消
};

@interface RTCVPSocketPacket : NSObject

#pragma mark - 基本属性
@property (nonatomic, readonly) RTCVPPacketType type;
@property (nonatomic, readonly) NSInteger packetId;
@property (nonatomic, copy, readonly) NSString *event;
@property (nonatomic, strong, readonly) NSArray *args;
@property (nonatomic, copy, readonly) NSString *nsp;
@property (nonatomic, strong, readonly) NSArray *data;
@property (nonatomic, strong, readonly) NSMutableArray<NSData *> *binary;
@property (nonatomic, copy, readonly) NSString *packetString;

#pragma mark - ACK相关属性
@property (nonatomic, assign, readonly) BOOL requiresAck;
@property (nonatomic, assign, readonly) RTCVPPacketState state;
@property (nonatomic, copy, nullable) RTCVPPacketSuccessCallback successCallback;
@property (nonatomic, copy, nullable) RTCVPPacketErrorCallback errorCallback;
@property (nonatomic, strong, nullable) NSTimer *timeoutTimer;
@property (nonatomic, assign) NSTimeInterval timeoutInterval;
@property (nonatomic, strong, readonly) NSDate *creationDate;

#pragma mark - 初始化方法
- (instancetype)initWithType:(RTCVPPacketType)type
                         nsp:(NSString *)namespace
                placeholders:(int)placeholders;

- (instancetype)initWithType:(RTCVPPacketType)type
                        data:(NSArray *)data
                    packetId:(NSInteger)packetId
                         nsp:(NSString *)nsp
                placeholders:(int)placeholders
                      binary:(NSArray *)binary;

#pragma mark - 工厂方法
+ (instancetype)eventPacketWithEvent:(NSString *)event
                                items:(NSArray *)items
                             packetId:(NSInteger)packetId
                                  nsp:(NSString *)nsp
                          requiresAck:(BOOL)requiresAck;

+ (instancetype)ackPacketWithId:(NSInteger)ackId
                          items:(NSArray *)items
                            nsp:(NSString *)nsp;

+ (nullable instancetype)packetFromString:(NSString *)message;

#pragma mark - ACK管理
- (void)setupAckCallbacksWithSuccess:(nullable RTCVPPacketSuccessCallback)success
                               error:(nullable RTCVPPacketErrorCallback)error
                             timeout:(NSTimeInterval)timeout;

- (void)acknowledgeWithData:(nullable NSArray *)data;
- (void)failWithError:(nullable NSError *)error;
- (void)cancel;

#pragma mark - 二进制数据处理
- (BOOL)addBinaryData:(NSData *)data;

#pragma mark - 状态查询
- (BOOL)isPending;
- (BOOL)isAcknowledged;
- (BOOL)isTimedOut;
- (BOOL)isCancelled;

#pragma mark - 调试信息
- (NSString *)debugDescription;

@end

NS_ASSUME_NONNULL_END
