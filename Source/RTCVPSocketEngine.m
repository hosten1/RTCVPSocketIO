//
//  SocketEngine.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketEngine.h"
#import "NSString+RTCVPSocketIO.h"
#import "RTCVPStringReader.h"
#import "RTCDefaultSocketLogger.h"
#import "RTCVPSocketEngine+Private.h"
#import "RTCVPSocketEngine+EnginePollable.h"
#import "RTCVPSocketEngine+EngineWebsocket.h"
#import "RTCVPSocketIOConfig.h"
#import "RTCVPProbe.h"
#import "RTCVPTimeoutManager.h"
#import "RTCVPTimer.h"


@interface RTCVPSocketEngine()<RTCJFRWebSocketDelegate,
NSURLSessionDelegate>


@property (nonatomic, strong) NSString *socketPath;
@property (nonatomic, weak) id<NSURLSessionDelegate> sessionDelegate;


@property (nonatomic, assign) int protocolVersion;

@property(nonatomic, assign)NSUInteger reconnectAttempts;

@end



@implementation RTCVPSocketEngine

@synthesize config = _config;
@synthesize client = _client;


@synthesize onDisconnect;

@synthesize onError;



@synthesize onConnect;

#pragma mark - 生命周期

+ (instancetype)engineWithClient:(id<RTCVPSocketEngineClient>)client
                             url:(NSURL *)url
                          config:(RTCVPSocketIOConfig *)config {
    return [[self alloc] initWithClient:client url:url config:config];
}

- (instancetype)initWithClient:(id<RTCVPSocketEngineClient>)client
                           url:(NSURL *)url
                          config:(RTCVPSocketIOConfig *)config {
    self = [super init];
    if (self) {
        _client = client;
        _url = url;
        _config = config ?: [RTCVPSocketIOConfig defaultConfig];
        
        // 设置日志
        if (self.config.logger) {
            [RTCDefaultSocketLogger setCoustomLogger:self.config.logger];
        }
        [RTCDefaultSocketLogger setEnabled:self.config.loggingEnabled];
        [RTCDefaultSocketLogger setLogLevel:self.config.logLevel];
        
        [self setupEngine];
        [self createURLs];
    }
    return self;
}

- (instancetype)initWithClient:(id<RTCVPSocketEngineClient>)client
                           url:(NSURL *)url
                       options:(NSDictionary *)options {
    RTCVPSocketIOConfig *config = [[RTCVPSocketIOConfig alloc] initWithDictionary:options];
    return [self initWithClient:client url:url config:config];
}

- (void)dealloc {
    [self log:@"Engine is being deallocated" level:RTCLogLevelDebug];
    [self disconnect:@"dealloc"];
}

#pragma mark - 初始化

- (void)setupEngine {
    // 创建串行队列处理引擎事件
    _engineQueue = dispatch_queue_create("com.socketio.engine.queue", DISPATCH_QUEUE_SERIAL);
    dispatch_set_target_queue(_engineQueue, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
    
    // 初始化状态
    _closed = NO;
    _connected = NO;
    _polling = YES;
    _websocket = NO;
    _probing = NO;
    _invalidated = NO;
    _fastUpgrade = NO;
    _waitingForPoll = NO;
    _waitingForPost = NO;
    
    // 初始化数据
    _sid = @"";
    _postWait = [NSMutableArray array];
    _probeWait = [NSMutableArray array];
    
    // 设置心跳参数
    _pingInterval = self.config.pingInterval * 1000; // 转换为毫秒
    _pingTimeout = self.config.pingTimeout * 1000;
    _pongsMissed = 0;
    _pongsMissedMax = MAX(1, _pingTimeout / _pingInterval);
    
    self.reconnectAttempts = 0;
    
    // 创建 URLSession
    NSOperationQueue *queue = [[NSOperationQueue alloc] init];
    queue.underlyingQueue = _engineQueue;
    queue.maxConcurrentOperationCount = 1;
    
    NSURLSessionConfiguration *sessionConfig = [NSURLSessionConfiguration defaultSessionConfiguration];
    sessionConfig.HTTPMaximumConnectionsPerHost = 1;
    sessionConfig.timeoutIntervalForRequest = 30;
    sessionConfig.timeoutIntervalForResource = 300;
    
    _session = [NSURLSession sessionWithConfiguration:sessionConfig
                                             delegate:self.config.sessionDelegate ?: self
                                        delegateQueue:queue];
    // 初始化ACK管理器
    _ackManager = [[RTCVPACKManager alloc] init];
    _ackIdCounter = 0;
}

#pragma mark - URL 创建

- (void)createURLs {
    if (!_url || !_client) {
        [self log:@"Invalid URL or client" level:RTCLogLevelError];
        return;
    }
    
    NSURLComponents *pollingComponents = [NSURLComponents componentsWithURL:_url resolvingAgainstBaseURL:NO];
    NSURLComponents *websocketComponents = [NSURLComponents componentsWithURL:_url resolvingAgainstBaseURL:NO];
    
    // 设置路径
    NSString *path = self.config.path;
    if (![path hasSuffix:@"/"]) {
        path = [path stringByAppendingString:@"/"];
    }
    
    pollingComponents.path = path;
    websocketComponents.path = path;
    
    // 设置协议
    BOOL secure = self.config.secure;
    if ([_url.scheme hasPrefix:@"https"] || [_url.scheme hasPrefix:@"wss"]) {
        secure = YES;
    }
    
    if (secure) {
        websocketComponents.scheme = @"wss";
        pollingComponents.scheme = @"https";
    } else {
        websocketComponents.scheme = @"ws";
        pollingComponents.scheme = @"http";
    }
    
    // 构建查询参数
    NSMutableDictionary *params = [NSMutableDictionary dictionary];
    if (self.config.connectParams) {
        [params addEntriesFromDictionary:self.config.connectParams];
    }
    
    // 添加 EIO 参数
    NSString *eioValue = nil;
    switch (self.config.protocolVersion) {
        case RTCVPSocketIOProtocolVersion2:
            eioValue = @"3"; // Engine.IO 3.x
            break;
        case RTCVPSocketIOProtocolVersion3:
            eioValue = @"4"; // Engine.IO 4.x
            break;
        case RTCVPSocketIOProtocolVersion4:
            eioValue = @"5"; // Engine.IO 5.x (如果支持)
            break;
        default:
            eioValue = @"4"; // 默认使用 Engine.IO 4.x
            break;
    }
    
    params[@"EIO"] = eioValue;
    params[@"transport"] = @"polling";
    
    // 对于 WebSocket，需要额外的参数
    NSMutableDictionary *wsParams = [params mutableCopy];
    wsParams[@"transport"] = @"websocket";
    
    // 构建查询字符串
    NSString *pollingQuery = [self buildQueryString:params];
    NSString *websocketQuery = [self buildQueryString:wsParams];
    
    pollingComponents.percentEncodedQuery = pollingQuery;
    websocketComponents.percentEncodedQuery = websocketQuery;
    
    _urlPolling = pollingComponents.URL;
    _urlWebSocket = websocketComponents.URL;
    
    [self log:[NSString stringWithFormat:@"Polling URL: %@", _urlPolling] level:RTCLogLevelDebug];
    [self log:[NSString stringWithFormat:@"WebSocket URL: %@", _urlWebSocket] level:RTCLogLevelDebug];
}

- (NSString *)buildQueryString:(NSDictionary *)params {
    NSMutableArray *queryItems = [NSMutableArray array];
    
    for (NSString *key in params.allKeys) {
        id value = params[key];
        
        if ([value isKindOfClass:[NSString class]]) {
            NSString *encodedKey = [key urlEncode];
            NSString *encodedValue = [value urlEncode];
            [queryItems addObject:[NSString stringWithFormat:@"%@=%@", encodedKey, encodedValue]];
        } else if ([value isKindOfClass:[NSArray class]]) {
            for (id item in value) {
                if ([item isKindOfClass:[NSString class]]) {
                    NSString *encodedKey = [key urlEncode];
                    NSString *encodedValue = [item urlEncode];
                    [queryItems addObject:[NSString stringWithFormat:@"%@=%@", encodedKey, encodedValue]];
                }
            }
        } else if ([value isKindOfClass:[NSNumber class]]) {
            NSString *encodedKey = [key urlEncode];
            NSString *encodedValue = [[value stringValue] urlEncode];
            [queryItems addObject:[NSString stringWithFormat:@"%@=%@", encodedKey, encodedValue]];
        }
    }
    
    return [queryItems componentsJoinedByString:@"&"];
}

#pragma mark - 日志方法

- (void)log:(NSString *)message level:(RTCLogLevel)level {
    [self log:message type:self.logType level:level];
}

- (void)log:(NSString *)message type:(NSString *)type level:(RTCLogLevel)level {
    if (self.config.loggingEnabled && level <= self.config.logLevel) {
        if (self.config.logger) {
            [self.config.logger logMessage:message type:type level:level];
        } else {
            [RTCDefaultSocketLogger.logger logMessage:message type:type level:level];
        }
    }
}

- (NSString *)logType {
    return @"SocketEngine";
}

#pragma mark - 心跳管理

- (void)startPingTimer {
    if (self.pingInterval <= 0 || !self.connected || self.closed) {
        return;
    }
    
    // 停止现有的心跳定时器
    [self stopPingTimer];
    
    // 创建新的心跳定时器
    __weak typeof(self) weakSelf = self;
    self.pingTimer = [RTCVPTimer timerWithTimeInterval:self.pingInterval / 1000.0
                                               repeats:YES
                                                 queue:self.engineQueue
                                                 block:^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        [strongSelf sendPing];
    }];
    
    [self.pingTimer start];
    
    [self log:@"Ping timer started" level:RTCLogLevelDebug];
}

- (void)stopPingTimer {
    if (self.pingTimer) {
        [self.pingTimer cancel];
        self.pingTimer = nil;
        [self log:@"Ping timer stopped" level:RTCLogLevelDebug];
    }
}

// 修改 sendPing 方法，同时发送两种心跳
- (void)sendPing {
    if (self.pongsMissed >= self.pongsMissedMax) {
        [self log:@"Ping timeout, closing connection" level:RTCLogLevelError];
        [self disconnect:@"ping timeout"];
        return;
    }
    
    self.pongsMissed++;
    
    [self log:[NSString stringWithFormat:@"发送心跳，错过次数: %ld/%ld",
               (long)self.pongsMissed, (long)self.pongsMissedMax]
         level:RTCLogLevelDebug];
    
    // 发送Engine.IO心跳
    NSString *pingMessage = @"";
    if (self.config.protocolVersion >= RTCVPSocketIOProtocolVersion3) {
        pingMessage = @"2"; // Engine.IO 4.x ping
    }
    [self write:pingMessage withType:RTCVPSocketEnginePacketTypePing withData:@[]];
    
    // 同时发送WebSocket协议心跳（如果使用WebSocket）
    if (self.websocket && self.ws && [self.ws isConnected]) {
        [self.ws writePing:nil]; // 发送空的WebSocket Ping
    }
}



#pragma mark - WebSocket 探测超时

- (void)startProbeTimeout {
    // 取消现有的探测超时
    [self cancelProbeTimeout];
    
    __weak typeof(self) weakSelf = self;
    self.probeTimeoutTaskId = [[RTCVPTimeoutManager sharedManager]
                              scheduleTimeout:5.0
                              identifier:@"WebSocketProbe"
                              timeoutBlock:^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        [strongSelf handleProbeTimeout];
    }];
    
    [self log:@"WebSocket probe timeout scheduled" level:RTCLogLevelDebug];
}

- (void)cancelProbeTimeout {
    if (self.probeTimeoutTaskId) {
        [[RTCVPTimeoutManager sharedManager] cancelTask:self.probeTimeoutTaskId];
        self.probeTimeoutTaskId = nil;
        [self log:@"WebSocket probe timeout cancelled" level:RTCLogLevelDebug];
    }
}

- (void)handleProbeTimeout {
    dispatch_async(self.engineQueue, ^{
        if (self.probing && !self.websocket) {
            [self log:@"WebSocket probe timeout" level:RTCLogLevelWarning];
            self.probing = NO;
            // 清理探测等待队列
            [self.probeWait removeAllObjects];
        }
    });
}

#pragma mark - 连接超时

- (void)startConnectionTimeout {
    // 取消现有的连接超时
    [self cancelConnectionTimeout];
    
    __weak typeof(self) weakSelf = self;
    self.connectionTimeoutTaskId = [[RTCVPTimeoutManager sharedManager]
                                   scheduleTimeout:self.config.connectTimeout
                                   identifier:@"Connection"
                                   timeoutBlock:^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        [strongSelf handleConnectionTimeout];
    }];
    
    [self log:@"Connection timeout scheduled" level:RTCLogLevelDebug];
}

- (void)cancelConnectionTimeout {
    if (self.connectionTimeoutTaskId) {
        [[RTCVPTimeoutManager sharedManager] cancelTask:self.connectionTimeoutTaskId];
        self.connectionTimeoutTaskId = nil;
        [self log:@"Connection timeout cancelled" level:RTCLogLevelDebug];
    }
}

- (void)handleConnectionTimeout {
    dispatch_async(self.engineQueue, ^{
        if (!self.connected && !self.closed) {
            [self log:@"Connection timeout" level:RTCLogLevelError];
            [self didError:@"Connection timeout"];
        }
    });
}

#pragma mark - 连接管理

- (void)connect {
    dispatch_async(self.engineQueue, ^{
        [self _connect];
    });
}

- (void)_connect {
    if (self.connected && !self.closed) {
        [self log:@"Engine is already connected" level:RTCLogLevelWarning];
        return;
    }
    
    if (self.closed) {
        [self log:@"Engine is closed, resetting..." level:RTCLogLevelDebug];
        [self resetEngine];
    }
    
    [self log:[NSString stringWithFormat:@"Starting connection to: %@", self.url.absoluteString] level:RTCLogLevelInfo];
    
    // 开始连接超时计时
    [self startConnectionTimeout];
    
    // 确定传输方式
    switch (self.config.transport) {
        case RTCVPSocketIOTransportWebSocket:{
            [self log:@"Using WebSocket transport" level:RTCLogLevelInfo];
            self.polling = NO;
            self.websocket = YES;
            
            // 创建并连接WebSocket
            [self createWebSocketAndConnect];
        }
            break;
            
        case RTCVPSocketIOTransportPolling:{
            [self log:@"Using Polling transport" level:RTCLogLevelInfo];
            // 开始轮询握手
            NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:self.urlPolling];
            request.timeoutInterval = self.config.connectTimeout;
            [self addHeadersToRequest:request];
            
            [self doLongPoll:request];
        }
            break;
            
        case RTCVPSocketIOTransportAuto: {
            [self log:@"Using Auto transport" level:RTCLogLevelInfo];
            // 自动协商传输方式，默认使用轮询握手
            NSMutableURLRequest *autoRequest = [NSMutableURLRequest requestWithURL:self.urlPolling];
            autoRequest.timeoutInterval = self.config.connectTimeout;
            [self addHeadersToRequest:autoRequest];
            
            [self doLongPoll:autoRequest];
        }
            break;
    }
}

/// 延迟重连
- (void)delayReconnect {
    // 计算指数退避延迟
    NSTimeInterval delay = [self calculateReconnectDelay];
    
    [self log:[NSString stringWithFormat:@"计划在 %.1f 秒后重连...", delay] level:RTCLogLevelInfo];
    
    // 使用定时器延迟重连
    __weak typeof(self) weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)),
                  self.engineQueue, ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf && !strongSelf.closed && !strongSelf.connected) {
            strongSelf.reconnectAttempts ++;
            [strongSelf log:@"执行重连..." level:RTCLogLevelInfo];
            [strongSelf connect];
        }
    });
}

- (NSTimeInterval)calculateReconnectDelay {
    // 指数退避算法：base * 2^(attempt-1)，最大60秒
    NSTimeInterval baseDelay = 2.0;
    NSTimeInterval maxDelay = 60.0;
    
    NSTimeInterval delay = baseDelay * pow(2, self.reconnectAttempts - 1);
    return MIN(delay, maxDelay);
}

- (void)disconnect:(NSString *)reason {
    dispatch_async(self.engineQueue, ^{
        [self _disconnect:reason];
    });
}

- (void)_disconnect:(NSString *)reason {
    if (!self.connected && self.closed) {
        return;
    }
    
    [self log:[NSString stringWithFormat:@"Disconnecting: %@", reason] level:RTCLogLevelInfo];
    
    // 发送关闭消息
    if (self.connected && !self.closed) {
        if (self.websocket) {
            [self sendWebSocketMessage:@"" withType:RTCVPSocketEnginePacketTypeClose withData:@[]];
        } else {
            [self disconnectPolling];
        }
    }
    
    [self closeOutEngine:reason];
}

- (void)resetEngine {
    [self log:@"Resetting engine state" level:RTCLogLevelDebug];
    
    // 停止所有定时器
    [self stopPingTimer];
    [self cancelProbeTimeout];
    [self cancelConnectionTimeout];
    
    self.closed = NO;
    self.connected = NO;
    self.fastUpgrade = NO;
    self.polling = YES;
    self.websocket = NO;
    self.probing = NO;
    self.invalidated = NO;
    self.sid = @"";
    self.waitingForPoll = NO;
    self.waitingForPost = NO;
    
    // 清理现有连接
    if (self.ws) {
        [self.ws disconnect];
        self.ws.delegate = nil;
        self.ws = nil;
    }
    
    if (self.session) {
        [self.session invalidateAndCancel];
        self.session = nil;
    }
    
    [self.postWait removeAllObjects];
    [self.probeWait removeAllObjects];
    
    // 重新创建 URLSession
    [self setupEngine];
}

#pragma mark - 数据解析

/// 解析从引擎接收到的原始二进制数据
- (void)parseEngineData:(NSData *)data {
    if (!data || data.length == 0) {
        [self log:@"Received empty binary data" level:RTCLogLevelWarning];
        return;
    }
    
    [self log:[NSString stringWithFormat:@"Got binary data, length: %lu", (unsigned long)data.length] level:RTCLogLevelDebug];
    
    // 根据协议版本处理二进制数据
    if (self.config.protocolVersion == RTCVPSocketIOProtocolVersion2) {
        // Engine.IO 3.x 协议：二进制数据前会有一个字节的标记
        // 第一个字节是 0x04 表示二进制消息
        if (data.length > 1) {
            const Byte *bytes = (const Byte *)data.bytes;
            Byte firstByte = bytes[0];
            
            if (firstByte == 0x04) {
                // 提取实际的二进制数据
                NSData *actualData = [data subdataWithRange:NSMakeRange(1, data.length - 1)];
                [self log:[NSString stringWithFormat:@"Engine.IO 3.x binary data, length: %lu", (unsigned long)actualData.length] level:RTCLogLevelDebug];
                
                // 传递给客户端处理
                if (self.client) {
                    [self.client parseEngineBinaryData:actualData];
                }
            } else {
                [self log:[NSString stringWithFormat:@"Unknown binary packet type: 0x%02X", firstByte] level:RTCLogLevelWarning];
            }
        }
    } else {
        // Engine.IO 4.x+ 协议：直接是二进制数据
        [self log:[NSString stringWithFormat:@"Engine.IO 4.x binary data, length: %lu", (unsigned long)data.length] level:RTCLogLevelDebug];
        
        // 直接传递给客户端处理
        if (self.client) {
            [self.client parseEngineBinaryData:data];
        }
    }
}

/// 处理 Base64 编码的二进制数据
- (void)handleBase64:(NSString *)message {
    if (message.length <= 2) {
        [self log:@"Invalid base64 message, too short" level:RTCLogLevelWarning];
        return;
    }
    
    NSString *base64String = [message substringFromIndex:2];
    NSData *data = [[NSData alloc] initWithBase64EncodedString:base64String options:NSDataBase64DecodingIgnoreUnknownCharacters];
    
    if (data) {
        [self log:[NSString stringWithFormat:@"Decoded base64 data, length: %lu", (unsigned long)data.length] level:RTCLogLevelDebug];
        
        if (self.client) {
            [self.client parseEngineBinaryData:data];
        }
    } else {
        [self log:@"Failed to decode base64 data" level:RTCLogLevelWarning];
    }
}

/// 处理打开消息
- (void)handleOpen:(NSString *)openData {
    NSDictionary *json = [openData toDictionary];
    if (!json) {
        [self didError:@"Invalid open packet"];
        return;
    }
    
    // 解析 session ID
    NSString *sid = json[@"sid"];
    if (![sid isKindOfClass:[NSString class]] || sid.length == 0) {
        [self didError:@"Open packet missing sid"];
        return;
    }
    
    // 连接成功，取消连接超时
    [self cancelConnectionTimeout];
    
    self.sid = sid;
    self.connected = YES;
    self.pongsMissed = 0;
    
    // 解析升级选项
    NSArray<NSString *> *upgrades = json[@"upgrades"];
    BOOL canUpgradeToWebSocket = upgrades && [upgrades containsObject:@"websocket"];
    
    // 解析心跳参数
    NSNumber *pingInterval = json[@"pingInterval"];
    NSNumber *pingTimeout = json[@"pingTimeout"];
    
    if ([pingInterval isKindOfClass:[NSNumber class]] && pingInterval.integerValue > 0) {
        self.pingInterval = pingInterval.integerValue;
    }
    
    if ([pingTimeout isKindOfClass:[NSNumber class]] && pingTimeout.integerValue > 0) {
        self.pingTimeout = pingTimeout.integerValue;
        self.pongsMissedMax = MAX(1, self.pingTimeout / self.pingInterval);
    }
    
    [self log:[NSString stringWithFormat:@"Connected with sid: %@", self.sid] level:RTCLogLevelInfo];
    [self log:[NSString stringWithFormat:@"Ping interval: %ldms, timeout: %ldms", (long)self.pingInterval, (long)self.pingTimeout] level:RTCLogLevelDebug];
    
    // 决定是否使用 WebSocket
    BOOL shouldUseWebSocket = NO;
    
    switch (self.config.transport) {
        case RTCVPSocketIOTransportWebSocket:
            // 强制WebSocket，直接使用
            shouldUseWebSocket = YES;
            break;
            
        case RTCVPSocketIOTransportAuto:
            // 自动模式，根据服务器支持决定
            shouldUseWebSocket = canUpgradeToWebSocket;
            break;
            
        case RTCVPSocketIOTransportPolling:
            // 强制轮询，不使用WebSocket
            shouldUseWebSocket = NO;
            break;
    }
    
    if (shouldUseWebSocket) {
        [self log:@"Using WebSocket transport" level:RTCLogLevelDebug];
        self.websocket = YES;
        self.polling = NO;
        
        // 如果还没有WebSocket连接，创建并连接
        if (!self.ws || ![self.ws isConnected]) {
            [self createWebSocketAndConnect];
        }
        
        // 开始心跳
        [self startPingTimer];
    } else {
        [self log:@"Using polling transport" level:RTCLogLevelDebug];
        // 开始心跳
        [self startPingTimer];
        // 继续轮询
        if (self.polling) {
            [self doPoll];
        }
    }
    
    // 通知客户端
    if (self.client) {
        [self.client engineDidOpen:@"Connected"];
    }
}

/// 处理普通消息
- (void)handleMessage:(NSString *)message {
    [self log:[NSString stringWithFormat:@"Handling message: %@", message] level:RTCLogLevelDebug];
    
    if (self.client) {
        [self.client parseEngineMessage:message];
    }
}

/// 处理关闭消息
- (void)handleClose:(NSString *)reason {
    [self closeOutEngine:reason ?: @"Closed by server"];
}

/// 处理升级消息
- (void)handleUpgrade {
    if (self.probing) {
        [self log:@"WebSocket probe successful, upgrading..." level:RTCLogLevelDebug];
        self.probing = NO;
        self.fastUpgrade = YES;
        [self doFastUpgrade];
    }
}

/// 处理 NOOP 消息
- (void)handleNoop {
    [self log:@"Received NOOP message" level:RTCLogLevelDebug];
    
    // NOOP 消息，继续轮询
    if (self.polling && !self.waitingForPoll) {
        [self doPoll];
    }
}

// 修改 handlePong 方法，处理两种心跳响应
- (void)handlePong:(NSString *)message {
    [self log:[NSString stringWithFormat:@"收到心跳响应: %@", message]
         level:RTCLogLevelDebug];
    
    // 重置心跳计数器
    self.pongsMissed = 0;
    
    // 检查是否为探测响应
    if ([message isEqualToString:@"probe"]) {
        [self log:@"收到WebSocket探测响应，升级传输" level:RTCLogLevelInfo];
        [self upgradeTransport];
    }
}

/// 升级传输方式
- (void)upgradeTransport {
    if ([self.ws isConnected]) {
        [self log:@"Upgrading transport to WebSockets" level:RTCLogLevelInfo];
        self.fastUpgrade = YES;
        
        // 无论是Engine.IO 3.x还是4.x，都发送 "2probe" 作为探测包
        [self.ws writeString:@"2probe"];
        [self log:@"Sent WebSocket probe: 2probe" level:RTCLogLevelDebug];
    } else {
        [self log:@"Cannot upgrade, WebSocket not connected" level:RTCLogLevelWarning];
    }
}

#pragma mark - 错误处理

- (void)didError:(NSString *)reason {
    [self log:[NSString stringWithFormat:@"Engine error: %@", reason] level:RTCLogLevelError];
    
    if (self.client && !self.closed) {
        [self.client engineDidError:reason];
    }
    
    if (self.connected) {
        [self disconnect:reason];
    }
}


#pragma mark - ACK消息发送

/// 生成唯一的ACK ID
- (NSInteger)generateACKId {
    NSInteger ackId = self.ackIdCounter;
    self.ackIdCounter = (self.ackIdCounter + 1) % 1000; // 循环使用，避免溢出
    return ackId;
}

/// 发送消息（带ACK回调）
- (void)send:(NSString *)msg ack:(RTCVPSocketAckCallback)ack {
    [self send:msg withData:@[] ack:ack];
}

/// 发送消息和数据（带ACK回调）
- (void)send:(NSString *)msg withData:(NSArray<NSData *> *)data ack:(RTCVPSocketAckCallback)ack {
    if (!msg || self.closed || !self.connected) {
        if (ack) {
            ack(@[]); // 连接已关闭，立即回调空数据
        }
        return;
    }
    
    NSInteger ackId = [self generateACKId];
    
    // 如果有ACK回调，先存储起来
    if (ack) {
        [self.ackManager addCallback:ack forId:ackId];
    }
    
    // 构建带有ACK ID的消息格式
    // Socket.IO协议格式: [event_name, data, ack_id]
    // 如果没有数据，格式为: [event_name, ack_id]
    // 如果有数据，格式为: [event_name, data, ack_id]
    
    NSMutableArray *messageParts = [NSMutableArray array];
    
    // 解析原始消息（可能是JSON数组）
    NSError *error = nil;
    NSData *msgData = [msg dataUsingEncoding:NSUTF8StringEncoding];
    id jsonObject = [NSJSONSerialization JSONObjectWithData:msgData options:0 error:&error];
    
    if (error) {
        // 如果不是JSON，直接当作事件名处理
        [messageParts addObject:msg];
        if (data.count > 0) {
            // 如果有二进制数据，添加占位符
            [messageParts addObject:@"_placeholder"];
            [messageParts addObject:@(ackId)];
        } else {
            // 没有二进制数据，直接添加ACK ID
            [messageParts addObject:@(ackId)];
        }
    } else if ([jsonObject isKindOfClass:[NSArray class]]) {
        // 已经是JSON数组，需要插入ACK ID
        NSMutableArray *jsonArray = [jsonObject mutableCopy];
        
        // 查找二进制数据占位符
        BOOL hasBinaryPlaceholder = NO;
        for (id item in jsonArray) {
            if ([item isKindOfClass:[NSDictionary class]]) {
                id placeholder = ((NSDictionary *)item)[@"_placeholder"];
                if (placeholder) {
                    hasBinaryPlaceholder = YES;
                    break;
                }
            }
        }
        
        if (hasBinaryPlaceholder) {
            // 有二进制数据占位符，ACK ID在占位符之后
            [jsonArray addObject:@(ackId)];
        } else {
            // 没有二进制数据，ACK ID在最后一个位置
            [jsonArray addObject:@(ackId)];
        }
        
        // 转换为JSON字符串
        NSData *jsonData = [NSJSONSerialization dataWithJSONObject:jsonArray options:0 error:&error];
        if (!error) {
            msg = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
        }
    }
    
    // 发送消息
    [self write:msg withType:RTCVPSocketEnginePacketTypeMessage withData:data];
    
    [self log:[NSString stringWithFormat:@"Sent message with ACK ID: %ld", (long)ackId] level:RTCLogLevelDebug];
}

/// 发送ACK响应
- (void)sendAck:(NSInteger)ackId withData:(NSArray *)data {
    if (self.closed || !self.connected) {
        return;
    }
    
    // 构建ACK响应格式
    // Socket.IO协议格式: [ack_id, data]
    NSArray *ackArray = @[@(ackId)];
    
    // 如果有数据，添加到数组中
    NSMutableArray *responseArray = [ackArray mutableCopy];
    if (data && data.count > 0) {
        // 检查数据中是否有二进制数据
        BOOL hasBinaryData = NO;
        for (id item in data) {
            if ([item isKindOfClass:[NSData class]]) {
                hasBinaryData = YES;
                break;
            }
        }
        
        if (hasBinaryData) {
            // 有二进制数据，添加占位符
            NSMutableArray *processedData = [NSMutableArray array];
            NSInteger placeholderIndex = 0;
            NSMutableArray *binaryDataArray = [NSMutableArray array];
            
            for (id item in data) {
                if ([item isKindOfClass:[NSData class]]) {
                    // 二进制数据，添加占位符
                    NSDictionary *placeholder = @{
                        @"_placeholder": @YES,
                        @"num": @(placeholderIndex)
                    };
                    [processedData addObject:placeholder];
                    [binaryDataArray addObject:item];
                    placeholderIndex++;
                } else {
                    [processedData addObject:item];
                }
            }
            
            [responseArray addObject:processedData];
            
            // 发送消息
            NSError *error = nil;
            NSData *jsonData = [NSJSONSerialization dataWithJSONObject:responseArray options:0 error:&error];
            if (!error) {
                NSString *jsonString = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
                [self write:jsonString withType:RTCVPSocketEnginePacketTypeMessage withData:binaryDataArray];
            }
        } else {
            // 没有二进制数据，直接添加数据
            [responseArray addObjectsFromArray:data];
            
            // 发送消息
            NSError *error = nil;
            NSData *jsonData = [NSJSONSerialization dataWithJSONObject:responseArray options:0 error:&error];
            if (!error) {
                NSString *jsonString = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
                [self write:jsonString withType:RTCVPSocketEnginePacketTypeMessage withData:@[]];
            }
        }
    } else {
        // 没有数据，直接发送ACK ID
        NSError *error = nil;
        NSData *jsonData = [NSJSONSerialization dataWithJSONObject:responseArray options:0 error:&error];
        if (!error) {
            NSString *jsonString = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
            [self write:jsonString withType:RTCVPSocketEnginePacketTypeMessage withData:@[]];
        }
    }
    
    [self log:[NSString stringWithFormat:@"Sent ACK response for ID: %ld", (long)ackId] level:RTCLogLevelDebug];
}

#pragma mark - 消息解析（增强版）

/// 解析原始引擎消息（增强版，支持ACK）
- (void)parseEngineMessage:(NSString *)message {
    if (message.length == 0) {
        [self log:@"Received empty message" level:RTCLogLevelWarning];
        return;
    }
    
    [self log:[NSString stringWithFormat:@"Got message: %@", message] level:RTCLogLevelDebug];
    
    // 检查是否为二进制消息前缀
    if ([message hasPrefix:@"b4"]) {
        [self handleBase64:message];
        return;
    }
    
    // 检查是否为错误消息
    NSDictionary *errorDict = [message toDictionary];
    if (errorDict && errorDict[@"message"]) {
        [self didError:errorDict[@"message"]];
        return;
    }
    
    // 解析消息类型
    if (message.length > 0) {
        unichar firstChar = [message characterAtIndex:0];
        if (firstChar >= '0' && firstChar <= '9') {
            RTCVPSocketEnginePacketType type = firstChar - '0';
            NSString *content = [message substringFromIndex:1];
            
            switch (type) {
                case RTCVPSocketEnginePacketTypeOpen:
                    [self handleOpen:content];
                    break;
                case RTCVPSocketEnginePacketTypeClose:
                    [self handleClose:content];
                    break;
                case RTCVPSocketEnginePacketTypePing:
                    // 服务器发送的 ping，需要回复 pong
                    [self write:@"" withType:RTCVPSocketEnginePacketTypePong withData:@[]];
                    break;
                case RTCVPSocketEnginePacketTypePong:
                    [self handlePong:content];
                    break;
                case RTCVPSocketEnginePacketTypeMessage:
                    [self handleSocketIOMessage:content];
                    break;
                case RTCVPSocketEnginePacketTypeUpgrade:
                    [self handleUpgrade];
                    break;
                case RTCVPSocketEnginePacketTypeNoop:
                    [self handleNoop];
                    break;
                default:
                    [self log:[NSString stringWithFormat:@"Unknown packet type: %c", firstChar] level:RTCLogLevelWarning];
                    break;
            }
        } else {
            // 可能是字符串消息（没有类型前缀）
            [self handleSocketIOMessage:message];
        }
    }
}

/// 处理Socket.IO消息（支持ACK）
- (void)handleSocketIOMessage:(NSString *)message {
    // 尝试解析为JSON数组
    NSError *error = nil;
    NSData *data = [message dataUsingEncoding:NSUTF8StringEncoding];
    id jsonObject = [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
    
    if (error) {
        // 不是JSON，当作普通消息处理
        [self log:[NSString stringWithFormat:@"Non-JSON message: %@", message] level:RTCLogLevelDebug];
        [self.client parseEngineMessage:message];
        return;
    }
    
    if (![jsonObject isKindOfClass:[NSArray class]]) {
        // 不是数组，当作普通消息处理
        [self.client parseEngineMessage:message];
        return;
    }
    
    NSArray *messageArray = (NSArray *)jsonObject;
    if (messageArray.count == 0) {
        [self log:@"Empty message array" level:RTCLogLevelWarning];
        return;
    }
    
    // 检查是否是ACK响应
    if (messageArray.count >= 2) {
        id firstItem = messageArray[0];
        id secondItem = messageArray[1];
        
        // 检查是否是ACK响应格式: [ackId, data...]
        if ([firstItem isKindOfClass:[NSNumber class]]) {
            NSInteger ackId = [firstItem integerValue];
            
            // 提取响应数据
            NSArray *responseData = nil;
            if (messageArray.count > 1) {
                NSMutableArray *tempData = [NSMutableArray array];
                for (NSUInteger i = 1; i < messageArray.count; i++) {
                    [tempData addObject:messageArray[i]];
                }
                responseData = [tempData copy];
            } else {
                responseData = @[];
            }
            
            // 执行ACK回调
            [self.ackManager executeCallbackForId:ackId withData:responseData];
            [self log:[NSString stringWithFormat:@"Executed ACK callback for ID: %ld", (long)ackId] level:RTCLogLevelDebug];
            return;
        }
        
        // 检查是否是带有ACK请求的事件: [event, data..., ackId]
        if (messageArray.count >= 3) {
            id lastItem = [messageArray lastObject];
            if ([lastItem isKindOfClass:[NSNumber class]]) {
                NSInteger ackId = [lastItem integerValue];
                
                // 提取事件和数据
                NSString *event = nil;
                if ([firstItem isKindOfClass:[NSString class]]) {
                    event = firstItem;
                }
                
                NSArray *eventData = nil;
                if (messageArray.count > 2) {
                    NSMutableArray *tempData = [NSMutableArray array];
                    for (NSUInteger i = 1; i < messageArray.count - 1; i++) {
                        id item = messageArray[i];
                        // 检查是否有二进制数据占位符
                        if ([item isKindOfClass:[NSDictionary class]]) {
                            NSDictionary *dict = (NSDictionary *)item;
                            if ([dict[@"_placeholder"] boolValue]) {
                                // 二进制数据占位符，客户端需要处理
                                [tempData addObject:item];
                            } else {
                                [tempData addObject:item];
                            }
                        } else {
                            [tempData addObject:item];
                        }
                    }
                    eventData = [tempData copy];
                }
                
                // 构建ACK请求消息格式
                NSMutableDictionary *ackMessage = [NSMutableDictionary dictionary];
                if (event) {
                    ackMessage[@"event"] = event;
                }
                if (eventData) {
                    ackMessage[@"data"] = eventData;
                }
                ackMessage[@"ackId"] = @(ackId);
                
                // 转换为JSON字符串
                NSData *jsonData = [NSJSONSerialization dataWithJSONObject:ackMessage options:0 error:nil];
                if (jsonData) {
                    NSString *jsonString = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
                    [self log:[NSString stringWithFormat:@"Forwarding ACK request to client: %@", jsonString] level:RTCLogLevelDebug];
                    [self.client parseEngineMessage:jsonString];
                }
                return;
            }
        }
    }
    
    // 普通消息，直接转发给客户端
    [self.client parseEngineMessage:message];
}



- (void)closeOutEngine:(NSString *)reason {
    if (self.closed) {
        return;
    }
    
    [self log:[NSString stringWithFormat:@"Closing engine: %@", reason] level:RTCLogLevelInfo];
    
    // 停止所有定时器
    [self stopPingTimer];
    [self cancelProbeTimeout];
    [self cancelConnectionTimeout];
    
    self.closed = YES;
    self.connected = NO;
    self.invalidated = YES;
    
    // 清理资源
    if (self.ws) {
        [self.ws disconnect];
        self.ws.delegate = nil;
        self.ws = nil;
    }
    
    if (self.session) {
        [self.session invalidateAndCancel];
        self.session = nil;
    }
    
    // 停止心跳
    self.pongsMissed = 0;
    
    // 清理缓冲区
    [self.postWait removeAllObjects];
    [self.probeWait removeAllObjects];
    
    // 通知客户端
    if (self.client) {
        [self.client engineDidClose:reason];
    }
}

#pragma mark - 发送消息

- (void)write:(NSString *)msg withType:(RTCVPSocketEnginePacketType)type withData:(NSArray *)data {
    dispatch_async(self.engineQueue, ^{
        if (!self.connected || self.closed) {
            [self log:@"Cannot write, engine not connected" level:RTCLogLevelWarning];
            return;
        }
        
        if (self.websocket) {
            [self sendWebSocketMessage:msg withType:type withData:data];
        } else if (self.probing) {
            // 在探测期间，缓存消息
            RTCVPProbe *probe = [[RTCVPProbe alloc] init];
            probe.message = msg;
            probe.type = type;
            probe.data = data;
            [self.probeWait addObject:probe];
        } else {
            [self sendPollMessage:msg withType:type withData:data];
        }
    });
}

- (void)send:(NSString *)msg withData:(NSArray<NSData *> *)data {
    [self write:msg withType:RTCVPSocketEnginePacketTypeMessage withData:data];
}

- (void)sendRawData:(NSData *)data {
    dispatch_async(self.engineQueue, ^{
        if (self.websocket && self.ws) {
            [self.ws writeData:data];
        } else {
            [self log:@"Cannot send raw data, WebSocket not available" level:RTCLogLevelWarning];
        }
    });
}

#pragma mark - 工具方法

- (void)addHeadersToRequest:(NSMutableURLRequest *)request {
    // 添加 cookies
    if (self.config.cookies.count > 0) {
        NSDictionary *headers = [NSHTTPCookie requestHeaderFieldsWithCookies:self.config.cookies];
        [request setAllHTTPHeaderFields:headers];
    }
    
    // 添加额外 headers
    if (self.config.extraHeaders) {
        for (NSString *key in self.config.extraHeaders.allKeys) {
            NSString *value = self.config.extraHeaders[key];
            if ([value isKindOfClass:[NSString class]]) {
                [request setValue:value forHTTPHeaderField:key];
            }
        }
    }
    
    // 设置 User-Agent
    NSString *userAgent = [NSString stringWithFormat:@"RTCVPSocketIO/%@ (iOS)", @"1.0.0"];
    [request setValue:userAgent forHTTPHeaderField:@"User-Agent"];
}

- (NSString *)currentTransport {
    if (self.websocket) {
        return @"websocket";
    } else if (self.polling) {
        return @"polling";
    } else {
        return @"none";
    }
}

#pragma mark - NSURLSessionDelegate

- (void)URLSession:(NSURLSession *)session didBecomeInvalidWithError:(NSError *)error {
    if (session == self.session) {
        [self log:@"URLSession became invalid" level:RTCLogLevelError];
        [self didError:error.localizedDescription ?: @"URLSession invalid"];
    }
}

#pragma mark - RTCVPSocketEngineProtocol

- (void)syncResetClient {
    dispatch_sync(self.engineQueue, ^{
        self.client = nil;
    });
}





@end
