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

typedef enum : NSUInteger {
    RTCVPSocketIOClientStatusNotConnected = 0x1,
    RTCVPSocketIOClientStatusDisconnected = 0x2,
    RTCVPSocketIOClientStatusConnecting = 0x3,
    RTCVPSocketIOClientStatusOpened = 0x4,
    RTCVPSocketIOClientStatusConnected = 0x5
} RTCVPSocketIOClientStatus;


extern NSString *const kSocketEventConnect;
extern NSString *const kSocketEventDisconnect;
extern NSString *const kSocketEventError;
extern NSString *const kSocketEventReconnect;
extern NSString *const kSocketEventReconnectAttempt;
extern NSString *const kSocketEventStatusChange;


typedef void (^RTCVPSocketIOVoidHandler)(void);
typedef void (^RTCVPSocketAnyEventHandler)(RTCVPSocketAnyEvent*event);

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
-(void) reconnect;
-(void) removeAllHandlers;

-(RTCVPSocketOnAckCallback*) emitWithAck:(NSString*)event items:(NSArray*)items;

-(NSUUID*) on:(NSString*)event callback:(RTCVPSocketOnEventCallback) callback;
-(NSUUID*) once:(NSString*)event callback:(RTCVPSocketOnEventCallback) callback;
-(void) onAny:(RTCVPSocketAnyEventHandler)handler;
-(void) off:(NSString*) event;

@end
