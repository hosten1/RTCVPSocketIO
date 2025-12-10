//
//  RTCVPSocketIOClient.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketIOClient.h"
#import "RTCVPSocketAnyEvent.h"
#import "RTCVPSocketEngineProtocol.h"
#import "RTCVPSocketEngine.h"
#import "RTCVPSocketPacket.h"
#import "RTCVPSocketAckManager.h"
#import "RTCDefaultSocketLogger.h"
#import "RTCVPStringReader.h"
#import "NSString+RTCVPSocketIO.h"
//lym
#import "RTCVPAFNetworkReachabilityManager.h"

typedef enum : NSUInteger {
    /// Called when the client connects. This is also called on a successful reconnection. A connect event gets one
    RTCVPSocketClientEventConnect = 0x0,
    /// Called when the socket has disconnected and will not attempt to try to reconnect.
    RTCVPSocketClientEventDisconnect,
    /// Called when an error occurs.
    RTCVPSocketClientEventError,
    /// Called when the client begins the reconnection process.
    RTCVPSocketClientEventReconnect,
    /// Called each time the client tries to reconnect to the server.
    RTCVPSocketClientEventReconnectAttempt,
    /// Called every time there is a change in the client's status.
    RTCVPSocketClientEventStatusChange,
} RTCVPSocketClientEvent;


NSString *const kSocketEventConnect            = @"connect";
NSString *const kSocketEventDisconnect         = @"disconnect";
NSString *const kSocketEventError              = @"error";
NSString *const kSocketEventReconnect          = @"reconnect";
NSString *const kSocketEventReconnectAttempt   = @"reconnectAttempt";
NSString *const kSocketEventStatusChange       = @"statusChange";



@interface  RTCVPSocketIOClientCacheData: NSObject
@property(nonatomic, assign) int ack;
@property(nonatomic, strong) NSArray *items;
@property(nonatomic, assign) BOOL isEvent;
@end
@implementation RTCVPSocketIOClientCacheData



@end
@interface RTCVPSocketIOClient() <RTCVPSocketEngineClient>
{
    int currentAck;
    int reconnectAttempts;
    int currentReconnectAttempt;
    BOOL reconnecting;
    RTCVPSocketAnyEventHandler anyHandler;
    
    NSDictionary *eventStrings;
    NSDictionary *statusStrings;
}

@property (nonatomic, strong, readonly) NSString* logType;
@property (nonatomic, strong) id<RTCVPSocketEngineProtocol> engine;
@property (nonatomic, strong) NSMutableArray<RTCVPSocketEventHandler*>* handlers;
@property (nonatomic, strong) NSMutableArray<RTCVPSocketPacket*>* waitingPackets;
// lym
@property(nonatomic,strong) RTCVPAFNetworkReachabilityManager *manager;
@property(nonatomic,assign) RTCVPAFNetworkReachabilityStatus currentNetWorkStatus;

@property(nonatomic, strong) NSMutableArray<RTCVPSocketIOClientCacheData*> *dataCache;
@end

@implementation RTCVPSocketIOClient

@synthesize ackHandlers;

-(instancetype)init:(NSURL*)socketURL withConfig:(NSDictionary*)config
{
    self = [super init];
    if (self) {
        [self setDefaultValues];
        _config = [config mutableCopy];
        _socketURL = socketURL;
        
        BOOL logEnabled = NO;
        
        if([socketURL.absoluteString hasPrefix:@"https://"])
        {
            [self.config setValue:@YES forKey:@"secure"];
        }
        
        for (NSString*key in self.config.allKeys)
        {
            id value = [self.config valueForKey:key];
            if([key isEqualToString:@"reconnects"])
            {
                _reconnects = [value boolValue];
            }
            if([key isEqualToString:@"reconnectAttempts"])
            {
                reconnectAttempts = [value intValue];
            }
            if([key isEqualToString:@"reconnectWait"])//秒 s
            {
                _reconnectWait = [value intValue];
            }
            if([key isEqualToString:@"nsp"])
            {
                _nsp = value;
            }
            
            if([key isEqualToString:@"log"])
            {
                logEnabled = [value boolValue];
            }
            
            if([key isEqualToString:@"logger"])
            {
                [RTCDefaultSocketLogger setLogger:value];
            }
            
            if([key isEqualToString:@"handleQueue"])
            {
                _handleQueue = value;
            }
            
            if([key isEqualToString:@"forceNew"])
            {
                _forceNew = [value boolValue];
            }

        }
        
        if([self.config objectForKey:@"path"] == nil) {
            [self.config setValue:@"/socket.io/" forKey:@"path"];
        }
        
        if(RTCDefaultSocketLogger.logger == nil) {
            [RTCDefaultSocketLogger setLogger:[RTCVPSocketLogger new]];
        }
        
        RTCDefaultSocketLogger.logger.log = logEnabled;
        // 检测指定服务器是否可达
        if (!_manager) {
            //创建网络监听对象
            self.manager = [RTCVPAFNetworkReachabilityManager sharedManager];
            //开始监听
            [_manager startMonitoring];
            self.currentNetWorkStatus = RTCVPAFNetworkReachabilityStatusUnknown;
            //监听改变
            __weak typeof(self)weakSelf = self;
            [_manager setReachabilityStatusChangeBlock:^(RTCVPAFNetworkReachabilityStatus status) {
                __strong typeof(weakSelf)strongSelf = weakSelf;
                [strongSelf rtc_changNetWorkWithStatus:status];
            }];
        }
    }
    return self;
}
- (void)rtc_changNetWorkWithStatus:(RTCVPAFNetworkReachabilityStatus)status{
//    if (_status == RTCVPSocketIOClientStatusDisconnected) {
//
//    }
    if (_currentNetWorkStatus == RTCVPAFNetworkReachabilityStatusUnknown) {//这种情况一般只有第一次初始化的时候出现
        self.currentNetWorkStatus = status;
        return;
    }
    switch (status) {
        case RTCVPAFNetworkReachabilityStatusUnknown:
          
        case RTCVPAFNetworkReachabilityStatusNotReachable:{
            [RTCDefaultSocketLogger.logger log:@"ERROR ==========No network===========" type:self.logType];

            [self.engine disconnect:@"Unknown network or not reachable"];
        }
            break;
        case RTCVPAFNetworkReachabilityStatusReachableViaWWAN:{
            if (_currentNetWorkStatus == RTCVPAFNetworkReachabilityStatusReachableViaWiFi ) {//网络状态改变 WiFi to 4G
                //                        [strongSelf.protoo reconnect];
                [RTCDefaultSocketLogger.logger log:@"ERROR ==========网络状态改变 WiFi to 4G===========" type:self.logType];
                [self.engine disconnect:@"network change WiFi to 4G"];
                
            }else if(self.currentNetWorkStatus == RTCVPAFNetworkReachabilityStatusReachableViaWWAN){
                [RTCDefaultSocketLogger.logger log:@"ERROR ==========网络状态改变 4G to 4G===========" type:self.logType];
            }

            
        }
            break;
        case RTCVPAFNetworkReachabilityStatusReachableViaWiFi:{
            if (self.currentNetWorkStatus == RTCVPAFNetworkReachabilityStatusReachableViaWWAN) {//网络状态改变 4G to WiFi
                //                        [strongSelf.protoo reconnect];
                [RTCDefaultSocketLogger.logger log:@"ERROR ==========/网络状态改变 4G to WiFi===========" type:self.logType];
                [self.engine disconnect:@"network change with 4G to WiFi"];
                
            }else if (self.currentNetWorkStatus == RTCVPAFNetworkReachabilityStatusReachableViaWiFi) {//网络状态改变 WiFi to WiFi
                //                        [strongSelf.protoo reconnect];
                [RTCDefaultSocketLogger.logger log:@"ERROR ==========/网络状态改变 WiFi to WiFi===========" type:self.logType];
            }
           
        }
            break;
    }
    self.currentNetWorkStatus = status;
}
-(void) connect
{
    [self connectWithTimeoutAfter:0 withHandler:nil];
}

-(void) connectWithTimeoutAfter:(double)timeout withHandler:(RTCVPSocketIOVoidHandler)handler
{
    if(_status != RTCVPSocketIOClientStatusConnected) {
        self.status = RTCVPSocketIOClientStatusConnecting;
        
        if (_engine == nil || _forceNew) {
            [self addEngine];
        }
        
        [_engine connect];
        
        if(timeout <= 0) {
            return;
        }
        __weak typeof(self) weakSelf1 = self;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(timeout * NSEC_PER_SEC)), _handleQueue, ^
        {
            @autoreleasepool
            {
                __strong typeof(weakSelf1) strongSelf = weakSelf1;
                if(strongSelf != nil &&
                   (strongSelf.status == RTCVPSocketIOClientStatusConnecting ||
                    strongSelf.status == RTCVPSocketIOClientStatusNotConnected))
                {
                    [strongSelf didDisconnect:@"Connection timeout"];
                    if(handler) {
                        handler();
                    }
                }
            }
        });
    } else {
        [RTCDefaultSocketLogger.logger log:@"Tried connecting on an already connected socket" type:self.logType];
    }
}

/// Disconnects the socket.
-(void)disconnect
{
    [RTCDefaultSocketLogger.logger log:@"Closing socket" type:self.logType];
    _reconnects = NO;
    [self didDisconnect:@"Disconnect"];
}
-(void)disconnectWithHandler:(RTCVPSocketIOVoidHandler)handler{
    [self disconnect];
    if(handler)handler();
}

-(void)dealloc
{
    [RTCDefaultSocketLogger.logger log:@"Client is being released" type:self.logType];
    [_engine disconnect: @"Client Deinit"];
    if (_manager) {
        [_manager stopMonitoring];
        self.manager = nil;
    }
    [RTCDefaultSocketLogger setLogger:nil];
}


-(void)setDefaultValues {
    _status = RTCVPSocketIOClientStatusNotConnected;
    _forceNew = NO;
    _handleQueue = dispatch_get_main_queue();
    _nsp = @"/";
    _reconnects = YES;
    _reconnectWait = 10;
    reconnecting = NO;
    currentAck = -1;
    reconnectAttempts = -1;
    currentReconnectAttempt = 0;
    ackHandlers = [[RTCVPSocketAckManager alloc] init];
    _handlers = [[NSMutableArray alloc] init];
    _waitingPackets = [[NSMutableArray alloc] init];
    
    
    eventStrings =@{ @(RTCVPSocketClientEventConnect)          : kSocketEventConnect,
                     @(RTCVPSocketClientEventDisconnect)       : kSocketEventDisconnect,
                     @(RTCVPSocketClientEventError)            : kSocketEventError,
                     @(RTCVPSocketClientEventReconnect)        : kSocketEventReconnect,
                     @(RTCVPSocketClientEventReconnectAttempt) : kSocketEventReconnectAttempt,
                     @(RTCVPSocketClientEventStatusChange)     : kSocketEventStatusChange};
    
    
    statusStrings = @{ @(RTCVPSocketIOClientStatusNotConnected) : @"notconnected",
                       @(RTCVPSocketIOClientStatusDisconnected) : @"disconnected",
                       @(RTCVPSocketIOClientStatusConnecting) : @"connecting",
                       @(RTCVPSocketIOClientStatusOpened) : @"opened",
                       @(RTCVPSocketIOClientStatusConnected) : @"connected"};
}

#pragma mark - property

-(void)setStatus:(RTCVPSocketIOClientStatus)status
{
    _status = status;
    switch (status) {
        case RTCVPSocketIOClientStatusConnected:
            reconnecting = NO;
            currentReconnectAttempt = 0;
            break;
            
        default:
            break;
    }
    [self handleClientEvent:eventStrings[@(RTCVPSocketClientEventStatusChange)]
                   withData:@[statusStrings[@(status)]]];
    
}

-(NSString *)logType
{
    return @"RTCVPSocketIOClient";
}
#pragma mark - private

-(void) addEngine
{
    [RTCDefaultSocketLogger.logger log:@"Adding engine" type:self.logType];
    if(_engine)
    {
        [_engine syncResetClient];
    }
    if (!_socketURL || ! _config) {
        return;
    }
    _engine = [[RTCVPSocketEngine alloc] initWithClient: self url: _socketURL options: _config];
}

-(RTCVPSocketOnAckCallback*) createOnAck:(NSArray*)items
{
    currentAck += 1;
    return [[RTCVPSocketOnAckCallback alloc] initAck:currentAck items:items socket:self];
}


-(void) didDisconnect:(NSString*)reason {
    
    if(_status != RTCVPSocketIOClientStatusDisconnected) {
        
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Disconnected: %@", reason] type:self.logType];
        
        reconnecting = NO;
        self.status = RTCVPSocketIOClientStatusDisconnected;
        
        // Make sure the engine is actually dead.
        [_engine disconnect:reason];
        [self handleClientEvent:eventStrings[@(RTCVPSocketClientEventDisconnect)]
                       withData:@[reason]];
    }
}

#pragma mark - emitter

/// Send an event to the server, with optional data items.
-(void)emit:(NSString*)event items:(NSArray*)items
{
    if(_status == RTCVPSocketIOClientStatusConnected) {
        NSMutableArray *array = [NSMutableArray arrayWithObject:event];
        [array addObjectsFromArray:items];
        [self emitData:array ack:-1];
    }
    else
    {
        [self handleClientEvent:eventStrings[@(RTCVPSocketClientEventError)]
                       withData:@[@"Tried emitting \(event) when not connected"]];
    }
}

/// Sends a message to the server, requesting an ack.
-(RTCVPSocketOnAckCallback*) emitWithAck:(NSString*)event items:(NSArray*)items
{
    NSMutableArray *array = [NSMutableArray arrayWithObject:event];
    [array addObjectsFromArray:items];
    if ([self isKindOfClass:[RTCVPSocketIOClient class]]) {
        return [self createOnAck:array];
    }
    return nil;
    
}

-(void) emitWithAck:(NSString*)event 
              items:(NSArray*)items 
           ackBlock:(void(^)(NSArray * _Nullable data, NSError * _Nullable error))ackBlock {
    [self emitWithAck:event items:items ackBlock:ackBlock timeout:10.0];
}

-(void) emitWithAck:(NSString*)event 
              items:(NSArray*)items 
           ackBlock:(void(^)(NSArray * _Nullable data, NSError * _Nullable error))ackBlock
            timeout:(NSTimeInterval)timeout {
    if (!event) {
        if (ackBlock) {
            NSError *error = [NSError errorWithDomain:@"RTCVPSocketIOErrorDomain" 
                                                 code:-1 
                                             userInfo:@{NSLocalizedDescriptionKey: @"Event name cannot be nil"}];
            ackBlock(nil, error);
        }
        return;
    }
    
    if (!items) {
        items = @[];
    }
    
    RTCVPSocketOnAckCallback *callback = [self emitWithAck:event items:items];
    if (callback) {
        [callback timingOutAfter:timeout callback:^(NSArray * _Nullable array) {
            if (ackBlock) {
                if (array) {
                    ackBlock(array, nil);
                } else {
                    NSError *error = [NSError errorWithDomain:@"RTCVPSocketIOErrorDomain" 
                                                         code:-1 
                                                     userInfo:@{NSLocalizedDescriptionKey: @"ACK timeout"}];
                    ackBlock(nil, error);
                }
            }
        }];
    } else {
        if (ackBlock) {
            NSError *error = [NSError errorWithDomain:@"RTCVPSocketIOErrorDomain" 
                                                 code:-1 
                                             userInfo:@{NSLocalizedDescriptionKey: @"Failed to create ACK callback"}];
            ackBlock(nil, error);
        }
    }
}

-(void)emitData:(NSArray*)data ack:(int) ack
{
    if(_status == RTCVPSocketIOClientStatusConnected) {
        RTCVPSocketPacket *packet = [RTCVPSocketPacket packetFromEmit:data ID:ack nsp:_nsp ack:NO isEvent:NO];
        NSString* str = packet.packetString;
        
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Emitting: %@", str] type:self.logType];
        [_engine send:str withData:packet.binary];
    }
    else {
        [self handleClientEvent:eventStrings[@(RTCVPSocketClientEventError)]
                       withData:@[@"Tried emitting when not connected"]];
    }
}

// If the server wants to know that the client received data
-(void) emitAck:(int)ack withItems:(NSArray*)items isEvent:(BOOL)isEvent
{
    if(_status == RTCVPSocketIOClientStatusConnected) {
        
        RTCVPSocketPacket *packet = [RTCVPSocketPacket packetFromEmit:items ID:ack nsp:_nsp ack:YES isEvent:isEvent];
        NSString *str = packet.packetString;
        
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Emitting Ack: %@", str] type:self.logType];
        
        [_engine send:str withData:packet.binary];
    }else{
        if (!_dataCache) {
            self.dataCache = [NSMutableArray array];
        }
        RTCVPSocketIOClientCacheData *cacheData = [[RTCVPSocketIOClientCacheData alloc]init];
        cacheData.ack = ack;
        cacheData.items = items;
        cacheData.isEvent = isEvent;
        [self.dataCache addObject:cacheData];
    }
}

#pragma mark - namespace

/// Leaves nsp and goes back to the default namespace.
-(void) leaveNamespace {
    if(![_nsp isEqualToString: @"/"]) {
        [_engine send:@"1\(nsp)" withData: @[]];
        _nsp = @"/";
    }
}

/// Joins `namespace`.
-(void) joinNamespace:(NSString*) namespace
{
    _nsp = namespace;
    if(![_nsp isEqualToString: @"/"])
    {
        [RTCDefaultSocketLogger.logger log:@"Joining namespace" type:self.logType];
        [_engine send:[NSString stringWithFormat:@"0\%@",_nsp] withData: @[]];
    }
}

#pragma mark - off

/// Removes handler(s) for a client event.
-(void) off:(NSString*) event
{
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Removing handler for event: %@", event] type:self.logType];
    NSPredicate *predicate= [NSPredicate predicateWithFormat:@"SELF.event != %@", event];
    [_handlers filterUsingPredicate:predicate];
}


/// Removes a handler with the specified UUID gotten from an `on` or `once`
-(void)offWithID:(NSUUID*)UUID {
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Removing handler with id: %@", UUID.UUIDString] type:self.logType];
    
    NSPredicate *predicate= [NSPredicate predicateWithFormat:@"SELF.uuid != %@", UUID];
    [_handlers filterUsingPredicate:predicate];
}

#pragma mark - on

/// Adds a handler for an event.
-(NSUUID*) on:(NSString*)event callback:(RTCVPSocketOnEventCallback) callback
{
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Adding handler for event: %@", event] type:self.logType];
    RTCVPSocketEventHandler *handler = [[RTCVPSocketEventHandler alloc] initWithEvent:event
                                                                           uuid:[NSUUID UUID]
                                                                    andCallback:callback];
    [_handlers addObject:handler];
    return handler.uuid;
}

/// Adds a single-use handler for a client event.
-(NSUUID*) once:(NSString*)event callback:(RTCVPSocketOnEventCallback) callback
{
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Adding once handler for event: %@", event] type:self.logType];
    
    NSUUID *uuid = [NSUUID UUID];
    
    __weak typeof(self) weakSelf = self;
    RTCVPSocketEventHandler *handler = [[RTCVPSocketEventHandler alloc] initWithEvent:event
                                                                           uuid:uuid
                                                                    andCallback:^(NSArray *data, RTCVPSocketAckEmitter *emiter) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if(strongSelf) {
            [strongSelf offWithID:uuid];
            callback(data, emiter);
        }
    }];
    [_handlers addObject:handler];
    return handler.uuid;
}

/// Adds a handler that will be called on every event.
-(void) onAny:(RTCVPSocketAnyEventHandler)handler
{
    anyHandler = handler;
}

#pragma mark - reconnect

/// Tries to reconnect to the server.
-(void) reconnect {
    if(!reconnecting) {
        [self tryReconnect:@"manual reconnect"];
    }
}

/// Removes all handlers.
-(void) removeAllHandlers {
    [_handlers removeAllObjects];
}

-(void) tryReconnect:(NSString*)reason
{
    if(reconnecting)
    {
        [RTCDefaultSocketLogger.logger log:@"Starting reconnect" type:self.logType];
        [self handleClientEvent:eventStrings[@(RTCVPSocketClientEventReconnect)]
                       withData:@[reason]];
        [self _tryReconnect];
    }
}

-(void) _tryReconnect
{
    if( _reconnects && reconnecting && _status != RTCVPSocketIOClientStatusDisconnected)
    {
        if(reconnectAttempts != -1 && currentReconnectAttempt + 1 > reconnectAttempts)
        {
            return [self didDisconnect: @"Reconnect Failed"];
        }
        else
        {
            [RTCDefaultSocketLogger.logger log:@"Trying to reconnect" type:self.logType];
            [self handleClientEvent:eventStrings[@(RTCVPSocketClientEventReconnectAttempt)]
                           withData:@[@(reconnectAttempts - currentReconnectAttempt)]];
            
            currentReconnectAttempt += 1;
            [self connect];
            
            [self setTimer];
        }
    }
}

-(void)setTimer
{
    __weak typeof(self) weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(_reconnectWait * NSEC_PER_SEC)), _handleQueue, ^
    {
       @autoreleasepool
       {
           __strong typeof(weakSelf) strongSelf = weakSelf;
           if(strongSelf != nil)
           {
               if(strongSelf.status != RTCVPSocketIOClientStatusDisconnected &&
                  strongSelf.status != RTCVPSocketIOClientStatusOpened)
               {
                   [strongSelf _tryReconnect];
               }
               else if(strongSelf.status != RTCVPSocketIOClientStatusConnected)
               {
                   [strongSelf setTimer];
               }
           }
       }
    });
}

#pragma mark - RTCVPSocketIOClientProtocol

/// Causes an event to be handled, and any event handlers for that event to be called.

-(void)handleEvent:(NSString*)event
          withData:(NSArray*) data
 isInternalMessage:(BOOL)internalMessage
{
    [self handleEvent:event withData:data
    isInternalMessage:internalMessage withAck:-1];
}

-(void)handleEvent:(NSString*)event
          withData:(NSArray*) data
 isInternalMessage:(BOOL)internalMessage
           withAck:(int)ack
{
    
    if(_status == RTCVPSocketIOClientStatusConnected || internalMessage)
    {
        if([event isEqualToString:kSocketEventError])
        {
            [RTCDefaultSocketLogger.logger error:data.firstObject type:self.logType];
        }
        else
        {
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Handling event: %@ with data: %@", event, data] type:self.logType];
        }
        
        if(anyHandler) {
            anyHandler([[RTCVPSocketAnyEvent alloc] initWithEvent: event andItems: data]);
        }
//        __weak typeof(self) weakSelf = self;
        if (_handlers.count > 0) {
            NSArray<RTCVPSocketEventHandler*> *enumArr = [NSArray arrayWithArray:_handlers];
            [enumArr enumerateObjectsUsingBlock:^(RTCVPSocketEventHandler * _Nonnull hdl, NSUInteger idx, BOOL * _Nonnull stop) {
    //            __strong typeof(weakSelf) strongSelf = weakSelf;
                if([hdl.event isEqualToString: event])
                {
                    [hdl executeCallbackWith:data withAck:ack withSocket:self];
                }
            }];
        }
       
//        for (RTCVPSocketEventHandler *hdl in _handlers)
//        {
//
//        }
    }
}

-(void) handleClientEvent:(NSString*)event withData:(NSArray*) data {
    [self handleEvent:event withData:data isInternalMessage:YES];
}

// Called when the socket gets an ack for something it sent
-(void) handleAck:(int)ack withData:(NSArray*)data {
    
    if(_status == RTCVPSocketIOClientStatusConnected) {
        
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Handling ack: %d with data: %@", ack, data] type:self.logType];
        [ackHandlers executeAck:ack withItems:data onQueue: _handleQueue];
    }
}

-(void) didConnect:(NSString*) namespace
{
    [RTCDefaultSocketLogger.logger log:@"Socket connected" type:self.logType];
    self.status = RTCVPSocketIOClientStatusConnected;
    if (_dataCache.count > 0) {//如果发送消息过程中重连则继续发送消息
        NSArray *tempArr = [NSArray arrayWithArray:_dataCache];
        [tempArr  enumerateObjectsUsingBlock:^(RTCVPSocketIOClientCacheData * _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
            [self emitAck:obj.ack withItems:obj.items isEvent:obj.isEvent];
            [self.dataCache removeObject:obj];
        }];
    }else{
        [self handleClientEvent:eventStrings[@(RTCVPSocketClientEventConnect)]
                       withData:@[namespace]];
    }
   
}

-(void)didError:(NSString*)reason {
    
    [self handleClientEvent:eventStrings[@(RTCVPSocketClientEventError)] withData:@[reason]];
}

#pragma mark - RTCVPSocketEngineClient

-(void) engineDidError:(NSString*)reason {
    
    __weak typeof(self) weakSelf = self;
    dispatch_async(_handleQueue, ^
    {
        @autoreleasepool
        {
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if(strongSelf) {
                [strongSelf _engineDidError:reason];
            }
        }
    });
}

-(void) _engineDidError:(NSString*)reason {
    [self handleClientEvent:eventStrings[@(RTCVPSocketClientEventError)]
                   withData:@[reason]];
}

-(void) engineDidOpen:(NSString*)reason {
    self.status = RTCVPSocketIOClientStatusOpened;
    [self handleClientEvent:eventStrings[@(RTCVPSocketIOClientStatusOpened)]
                   withData:@[reason]];
}

-(void)engineDidClose:(NSString*)reason
{
    __weak typeof(self) weakSelf = self;
    dispatch_async(_handleQueue, ^
    {
        @autoreleasepool
        {
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if(strongSelf) {
               [strongSelf _engineDidClose:reason];
            }
        }
    });
}

-(void) _engineDidClose:(NSString*)reason
{
    [_waitingPackets removeAllObjects];
    if (_status == RTCVPSocketIOClientStatusDisconnected || !_reconnects)
    {
        [self didDisconnect:reason];
    }
    else
    {
        self.status = RTCVPSocketIOClientStatusNotConnected;
        if (!reconnecting)
        {
            reconnecting = YES;
            [self tryReconnect:reason];
        }
    }
}

/// Called when the engine has a message that must be parsed.
-(void)parseEngineMessage:(NSString*)msg
{
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Should parse message: %@", msg] type:self.logType];
    __weak typeof(self) weakSelf = self;
    dispatch_async(_handleQueue, ^
    {
        @autoreleasepool
        {
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if(strongSelf) {
                [strongSelf parseSocketMessage:msg];
            }
        }
    });
}

/// Called when the engine receives binary data.
-(void)parseEngineBinaryData:(NSData*)data
{
    __weak typeof(self) weakSelf = self;
    dispatch_async(_handleQueue, ^{
        @autoreleasepool
        {
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if(strongSelf) {
                [strongSelf parseBinaryData:data];
            }
        }
    });
}

#pragma mark - RTCVPSocketParsable

// Parses messages recieved
-(void)parseSocketMessage:(NSString*)message {
    
    if(message.length > 0)
    {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Parsing %@", message] type:@"SocketParser"];
        
        RTCVPSocketPacket *packet = [self parseString:message];
        
        if(packet) {
        
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Decoded packet as: %@", packet.description] type:@"SocketParser"];
            [self handlePacket:packet];
        }
        else {
            [RTCDefaultSocketLogger.logger error:@"invalidPacketType" type: @"SocketParser"];
        }
    }
}

-(void)parseBinaryData:(NSData*)data
{
    if(_waitingPackets.count > 0)
    {
        RTCVPSocketPacket *lastPacket = _waitingPackets.lastObject;
        BOOL success = [lastPacket addData:data];
        if(success) {
            [_waitingPackets removeLastObject];
            
            if(lastPacket.type != RTCVPPacketTypeBinaryAck) {
                [self handleEvent:lastPacket.event
                         withData:lastPacket.args
                isInternalMessage:NO
                          withAck:lastPacket.ID];
            }
            else {
                [self handleAck:lastPacket.ID withData:lastPacket.args];
            }
        }
    }
    else {
        [RTCDefaultSocketLogger.logger error:@"Got data when not remaking packet"
                                     type:@"SocketParser"];
    }
}


-(RTCVPSocketPacket*)parseString:(NSString*)message
{
    NSCharacterSet* digits = [NSCharacterSet decimalDigitCharacterSet];
    RTCVPStringReader *reader = [[RTCVPStringReader alloc] init:message];
    
    NSString *packetType = [reader read:1];
    if ([packetType rangeOfCharacterFromSet:digits].location != NSNotFound) {
        RTCVPPacketType type = [packetType integerValue];
        if(![reader hasNext]) {
            return [[RTCVPSocketPacket alloc] init:type nsp:@"/" placeholders:0];
        }
        
        NSString *namespace = @"/";
        int placeholders = -1;
        
        if(type == RTCVPPacketTypeBinaryAck || type == RTCVPPacketTypeBinaryEvent) {
            NSString *value = [reader readUntilOccurence:@"-"];
            if ([value rangeOfCharacterFromSet:digits].location == NSNotFound) {
                return nil;
            }
            else {
                placeholders = [value intValue];
            }
        }
        
        NSString *charStr = [reader currentCharacter];
        if([charStr isEqualToString:namespace]) {
            namespace = [reader readUntilOccurence:@","];
        }
        
        if(![reader hasNext]) {
            return [[RTCVPSocketPacket alloc] init:type nsp:namespace placeholders:placeholders];
        }
        
        NSMutableString *idString = [NSMutableString string];
        
        if(type == RTCVPPacketTypeError) {
            [reader advance:-1];
        }
        else {
            while ([reader hasNext]) {
                NSString *value = [reader read:1];
                if ([value rangeOfCharacterFromSet:digits].location == NSNotFound) {
                    [reader advance:-2];
                    break;
                }
                else {
                    [idString appendString:value];
                }
            }
        }
        
        NSString *dataArray = [message substringFromIndex:reader.currentIndex+1];
        
        if (type == RTCVPPacketTypeError && ![dataArray hasPrefix:@"["] && ![dataArray hasSuffix:@"]"])
        {
            dataArray =  [NSString stringWithFormat:@"[%@]", dataArray];
        }
        
        NSArray *data = [dataArray toArray];
        if(data.count > 0) {
            int idValue = -1;
            if(idString.length > 0)
            {
                idValue = [idString intValue];
            }
            return [[RTCVPSocketPacket alloc] init:type
                                           data:data
                                             ID:idValue
                                            nsp:namespace
                                   placeholders:placeholders
                                         binary:[NSArray array]];
        }
    }
    return nil;
}

#pragma mark - handle packet

-(BOOL) isCorrectNamespace:(NSString*) nsp
{
    return [nsp isEqualToString: self.nsp];
}

-(void)handlePacket:(RTCVPSocketPacket*) packet
{
    switch (packet.type)
    {
        case RTCVPPacketTypeEvent:
            if([self isCorrectNamespace:packet.nsp])
            {
                [self handleEvent:packet.event withData:packet.args isInternalMessage:NO
                          withAck:packet.ID];
            }
            else
            {
                [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Got invalid packet: %@", packet.description]
                                           type:@"SocketParser"];
            }
            break;
        case RTCVPPacketTypeAck:
            if([self isCorrectNamespace:packet.nsp])
            {
                [self handleAck:packet.ID withData:packet.data];
            }
            else
            {
                [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Got invalid packet: %@", packet.description]
                                           type:@"SocketParser"];
            }
            break;
        case RTCVPPacketTypeBinaryEvent:
        case RTCVPPacketTypeBinaryAck:
            if([self isCorrectNamespace:packet.nsp]) {
                [_waitingPackets addObject:packet];
            }
            else {
                [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Got invalid packet: %@", packet.description]
                                           type:@"SocketParser"];
            }
            break;
        case RTCVPPacketTypeConnect:
            [self handleConnect:packet.nsp];
            break;
        case RTCVPPacketTypeDisconnect:
            [self didDisconnect:@"Got Disconnect"];
            break;
        case RTCVPPacketTypeError:
            
            [self handleEvent:@"error" withData:packet.data isInternalMessage:YES
                      withAck:packet.ID];
            break;
        default:
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Got invalid packet: %@", packet.description]
                                       type:@"SocketParser"];
            break;
    }
}

-(void)handleConnect:(NSString*)packetNamespace {
    if ([packetNamespace isEqualToString: @"/"] && ![_nsp isEqualToString:@"/"]) {
        [self joinNamespace:_nsp];
    } else {
        [self didConnect:packetNamespace];
    }
}

@end

