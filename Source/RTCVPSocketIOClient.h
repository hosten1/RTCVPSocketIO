//
//  RTCVPSocketIOClient.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "RTCVPSocketAnyEvent.h"
#import "RTCVPSocketOnAckCallback.h"
#import "RTCVPSocketIOClientProtocol.h"

// Socket.IO客户端状态枚举
typedef NS_ENUM(NSUInteger, RTCVPSocketIOClientStatus) {
    RTCVPSocketIOClientStatusNotConnected = 0x1,  // 未连接
    RTCVPSocketIOClientStatusDisconnected = 0x2,  // 已断开连接
    RTCVPSocketIOClientStatusConnecting = 0x3,    // 连接中
    RTCVPSocketIOClientStatusOpened = 0x4,        // 连接已打开
    RTCVPSocketIOClientStatusConnected = 0x5      // 已连接
};

// Socket.IO事件名称常量
extern NSString *const kSocketEventConnect;
extern NSString *const kSocketEventDisconnect;
extern NSString *const kSocketEventError;
extern NSString *const kSocketEventReconnect;
extern NSString *const kSocketEventReconnectAttempt;
extern NSString *const kSocketEventStatusChange;

// 回调类型定义
typedef void (^RTCVPSocketIOVoidHandler)(void);
typedef void (^RTCVPSocketAnyEventHandler)(RTCVPSocketAnyEvent*event);
typedef void (^RTCVPSocketAckHandler)(id _Nullable data, NSError * _Nullable error);
typedef void (^RTCVPSocketConnectHandler)(BOOL connected, NSError * _Nullable error);




@interface RTCVPSocketIOClient : NSObject<RTCVPSocketIOClientProtocol>

@property (nonatomic, readonly) RTCVPSocketIOClientStatus status;
@property (nonatomic) BOOL forceNew;
@property (nonatomic, strong, readonly) NSMutableDictionary *config;
@property (nonatomic) BOOL reconnects;
@property (nonatomic) int reconnectWait;
@property (nonatomic, strong, readonly) NSString *ssid;
@property (nonatomic, strong, readonly) NSURL *socketURL;

@property (nonatomic, strong, readonly) dispatch_queue_t handleQueue;
@property (nonatomic, strong, readonly) NSString* nsp;

-(instancetype)init:(NSURL*)socketURL withConfig:(NSDictionary*)config;
-(void) connect;
-(void) connectWithTimeoutAfter:(double)timeout withHandler:(RTCVPSocketIOVoidHandler)handler;
-(void) disconnect;
-(void) disconnectWithHandler:(RTCVPSocketIOVoidHandler)handler;
-(void) reconnect;
-(void) removeAllHandlers;

-(RTCVPSocketOnAckCallback*) emitWithAck:(NSString*)event items:(NSArray*)items;

/**
 * 增强的emitWithAck方法，直接传递回调block
 * @param event 事件名称
 * @param items 事件参数
 * @param ackBlock 回调block，在收到ack时调用
 * @param timeout 超时时间，单位为秒，默认10秒
 */
-(void) emitWithAck:(NSString*)event 
              items:(NSArray*)items 
           ackBlock:(void(^)(NSArray * _Nullable data, NSError * _Nullable error))ackBlock;

/**
 * 增强的emitWithAck方法，直接传递回调block，带超时时间
 * @param event 事件名称
 * @param items 事件参数
 * @param ackBlock 回调block，在收到ack时调用
 * @param timeout 超时时间，单位为秒
 */
-(void) emitWithAck:(NSString*)event 
              items:(NSArray*)items 
           ackBlock:(void(^)(NSArray * _Nullable data, NSError * _Nullable error))ackBlock
            timeout:(NSTimeInterval)timeout;

-(NSUUID*) on:(NSString*)event callback:(RTCVPSocketOnEventCallback) callback;
-(NSUUID*) once:(NSString*)event callback:(RTCVPSocketOnEventCallback) callback;
-(void) onAny:(RTCVPSocketAnyEventHandler)handler;
-(void) off:(NSString*) event;

@end
