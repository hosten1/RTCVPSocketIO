//
//  RTCVPSocketEngine+Private.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/26/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketEngine.h"
#import "RTCJFRWebSocket.h"

typedef enum : NSUInteger{
    RTCVPSocketEnginePacketTypeOpen = 0x0,
    RTCVPSocketEnginePacketTypeClose = 0x1,
    RTCVPSocketEnginePacketTypePing = 0x2,
    RTCVPSocketEnginePacketTypePong = 0x3,
    RTCVPSocketEnginePacketTypeMessage = 0x4,
    RTCVPSocketEnginePacketTypeUpgrade = 0x5,
    RTCVPSocketEnginePacketTypeNoop = 0x6,
} RTCVPSocketEnginePacketType;


@class RTCVPProbe;
@interface RTCVPSocketEngine ()

@property (nonatomic, strong, readonly) NSDictionary *stringEnginePacketType;
@property (nonatomic, readonly) BOOL invalidated;
@property (nonatomic, strong) NSMutableArray<NSString*>* postWait;
@property (nonatomic, strong, readonly) NSURLSession *session;
@property (nonatomic) BOOL waitingForPoll;
@property (nonatomic) BOOL waitingForPost;
@property (nonatomic, strong) NSString* sid;
@property (nonatomic, strong) NSURL *urlPolling;
@property (nonatomic) BOOL websocket;
@property (nonatomic, strong) RTCJFRWebSocket* ws;
@property (nonatomic) BOOL fastUpgrade;
@property (nonatomic) BOOL polling;
@property (nonatomic) BOOL forcePolling;
@property (nonatomic) BOOL forceWebsockets;
@property (nonatomic) BOOL probing;
@property (nonatomic, strong) NSMutableArray<NSHTTPCookie*>* cookies;
@property (nonatomic, strong) NSMutableDictionary *connectParams;
@property (nonatomic, strong) NSMutableDictionary*extraHeaders;
@property (nonatomic, strong) dispatch_queue_t engineQueue;
@property (nonatomic, strong) NSURL *urlWebSocket;
@property (nonatomic) BOOL secure;
@property (nonatomic) BOOL selfSigned;
@property (nonatomic, strong) RTCJFRSecurity* security;
@property (nonatomic) BOOL closed;
@property (nonatomic) BOOL compress;
@property (nonatomic) BOOL connected;
@property (nonatomic, strong, readonly) NSString* logType;
@property (nonatomic, strong) NSMutableArray<RTCVPProbe*>* probeWait;



- (void)didError:(NSString*)reason;
- (void)doFastUpgrade;
- (void)addHeaders:(NSMutableURLRequest *)request;
- (void)parseEngineMessage:(NSString*)message;
-(void) closeOutEngine:(NSString*)reason;
-(void) parseEngineData:(NSData*)data;
@end
