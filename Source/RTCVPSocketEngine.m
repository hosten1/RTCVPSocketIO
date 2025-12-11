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

#pragma mark - 连接管理

- (void)connect {
    dispatch_async(_engineQueue, ^{
        [self _connect];
    });
}

- (void)_connect {
    if (_connected && !_closed) {
        [self log:@"Engine is already connected" level:RTCLogLevelWarning];
        return;
    }
    
    if (_closed) {
        [self log:@"Engine is closed, resetting..." level:RTCLogLevelDebug];
        [self resetEngine];
    }
    
    [self log:[NSString stringWithFormat:@"Starting connection to: %@", _url.absoluteString] level:RTCLogLevelInfo];
    [self log:@"Initiating handshake..." level:RTCLogLevelDebug];
    
    // 确定传输方式
    BOOL forceWebSocket = self.config.forceWebsockets;
    BOOL forcePolling = self.config.forcePolling;
    
    if (forceWebSocket && forcePolling) {
        [self log:@"Both forceWebsockets and forcePolling are set, defaulting to WebSocket" level:RTCLogLevelWarning];
        forcePolling = NO;
    }
    
    if (forceWebSocket) {
        _polling = NO;
        _websocket = YES;
        [self createWebSocketAndConnect];
    } else {
        // 开始轮询握手
        NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:_urlPolling];
        request.timeoutInterval = self.config.connectTimeout;
        [self addHeadersToRequest:request];
        
        [self doLongPoll:request];
    }
}

- (void)disconnect:(NSString *)reason {
    dispatch_async(_engineQueue, ^{
        [self _disconnect:reason];
    });
}

- (void)send:(nonnull NSString *)msg ack:(RTCVPSocketAckCallback _Nullable)ack { 
    <#code#>
}


- (void)send:(nonnull NSString *)msg withData:(nonnull NSArray<NSData *> *)data ack:(RTCVPSocketAckCallback _Nullable)ack { 
    <#code#>
}


- (void)sendAck:(NSInteger)ackId withData:(nonnull NSArray *)data { 
    <#code#>
}


- (void)syncResetClient { 
    <#code#>
}


- (void)_disconnect:(NSString *)reason {
    if (!_connected && _closed) {
        return;
    }
    
    [self log:[NSString stringWithFormat:@"Disconnecting: %@", reason] level:RTCLogLevelInfo];
    
    // 发送关闭消息
    if (_connected && !_closed) {
        if (_websocket) {
            [self sendWebSocketMessage:@"" withType:RTCVPSocketEnginePacketTypeClose withData:@[]];
        } else {
            [self disconnectPolling];
        }
    }
    
    [self closeOutEngine:reason];
}

- (void)resetEngine {
    [self log:@"Resetting engine state" level:RTCLogLevelDebug];
    
    _closed = NO;
    _connected = NO;
    _fastUpgrade = NO;
    _polling = YES;
    _websocket = NO;
    _probing = NO;
    _invalidated = NO;
    _sid = @"";
    _waitingForPoll = NO;
    _waitingForPost = NO;
    
    // 清理现有连接
    if (_ws) {
        [_ws disconnect];
        _ws.delegate = nil;
        _ws = nil;
    }
    
    if (_session) {
        [_session invalidateAndCancel];
        _session = nil;
    }
    
    [_postWait removeAllObjects];
    [_probeWait removeAllObjects];
    
    // 重新创建 URLSession
    [self setupEngine];
}

#pragma mark - 心跳管理

- (void)startPingTimer {
    if (_pingInterval <= 0 || !_connected || _closed) {
        return;
    }
    
    __weak typeof(self) weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(_pingInterval * NSEC_PER_MSEC)), _engineQueue, ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf && strongSelf->_connected && !strongSelf->_closed) {
            [strongSelf sendPing];
        }
    });
}

- (void)sendPing {
    if (_pongsMissed >= _pongsMissedMax) {
        [self log:@"Ping timeout, closing connection" level:RTCLogLevelError];
        [self disconnect:@"ping timeout"];
        return;
    }
    
    _pongsMissed++;
    
    NSString *pingMessage = @"";
    if (self.config.protocolVersion >= RTCVPSocketIOProtocolVersion3) {
        // Engine.IO 4.x+ 使用 "2" 作为 ping
        pingMessage = @"2";
    }
    
    [self write:pingMessage withType:RTCVPSocketEnginePacketTypePing withData:@[]];
    [self startPingTimer];
}

- (void)handlePong {
    _pongsMissed = 0;
}

#pragma mark - 消息处理

/// 解析原始引擎消息
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
                    [self handleMessage:content];
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
            [self handleMessage:message];
        }
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
    
    // 决定是否升级到 WebSocket
    BOOL shouldUpgrade = canUpgradeToWebSocket &&
                       !self.config.forcePolling &&
                       (self.config.forceWebsockets || self.config.transport == RTCVPSocketIOTransportWebSocket);
    
    if (shouldUpgrade) {
        [self log:@"Upgrading to WebSocket..." level:RTCLogLevelDebug];
        [self createWebSocketAndConnect];
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

/// 处理 Pong 消息
- (void)handlePong:(NSString *)message {
    [self log:@"Received Pong message" level:RTCLogLevelDebug];
    
    self.pongsMissed = 0;
    
    // 检查是否为探测响应
    if (self.config.protocolVersion == RTCVPSocketIOProtocolVersion3) {
        // Engine.IO 4.x 使用字符串格式
        if ([message isEqualToString:@"probe"]) {
            [self upgradeTransport];
        }
    } else {
        // Engine.IO 3.x 使用数字格式
        if ([message isEqualToString:@"probe"]) {
            [self upgradeTransport];
        }
    }
}

/// 升级传输方式
- (void)upgradeTransport {
    if ([self.ws isConnected]) {
        [self log:@"Upgrading transport to WebSockets" level:RTCLogLevelInfo];
        self.fastUpgrade = YES;
        
        if (self.config.protocolVersion == RTCVPSocketIOProtocolVersion3) {
            // Engine.IO 4.x 发送 "2probe" 作为探测包
            [self.ws writeString:@"2probe"];
        } else {
            // Engine.IO 3.x 发送空字符串作为探测包
            [self sendPollMessage:@"" withType:RTCVPSocketEnginePacketTypeNoop withData:@[]];
        }
    }
}

#pragma mark - 错误处理

- (void)didError:(NSString *)reason {
    [self log:[NSString stringWithFormat:@"Engine error: %@", reason] level:RTCLogLevelError];
    
    if (_client && !_closed) {
        [_client engineDidError:reason];
    }
    
    if (_connected) {
        [self disconnect:reason];
    }
}

- (void)closeOutEngine:(NSString *)reason {
    if (_closed) {
        return;
    }
    
    [self log:[NSString stringWithFormat:@"Closing engine: %@", reason] level:RTCLogLevelInfo];
    
    _closed = YES;
    _connected = NO;
    _invalidated = YES;
    
    // 清理资源
    if (_ws) {
        [_ws disconnect];
        _ws.delegate = nil;
        _ws = nil;
    }
    
    if (_session) {
        [_session invalidateAndCancel];
        _session = nil;
    }
    
    // 停止心跳
    _pongsMissed = 0;
    
    // 清理缓冲区
    [_postWait removeAllObjects];
    [_probeWait removeAllObjects];
    
    // 通知客户端
    if (_client) {
        [_client engineDidClose:reason];
    }
}

#pragma mark - 发送消息

- (void)write:(NSString *)msg withType:(RTCVPSocketEnginePacketType)type withData:(NSArray *)data {
    dispatch_async(_engineQueue, ^{
        if (!self->_connected || self->_closed) {
            [self log:@"Cannot write, engine not connected" level:RTCLogLevelWarning];
            return;
        }
        
        if (self->_websocket) {
            [self sendWebSocketMessage:msg withType:type withData:data];
        } else if (self->_probing) {
            // 在探测期间，缓存消息
            RTCVPProbe *probe = [[RTCVPProbe alloc] init];
            probe.message = msg;
            probe.type = type;
            probe.data = data;
            [self->_probeWait addObject:probe];
        } else {
            [self sendPollMessage:msg withType:type withData:data];
        }
    });
}

- (void)send:(NSString *)msg withData:(NSArray<NSData *> *)data {
    [self write:msg withType:RTCVPSocketEnginePacketTypeMessage withData:data];
}

- (void)sendRawData:(NSData *)data {
    dispatch_async(_engineQueue, ^{
        if (self->_websocket && self->_ws) {
            [self->_ws writeData:data];
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
    if (_websocket) {
        return @"websocket";
    } else if (_polling) {
        return @"polling";
    } else {
        return @"none";
    }
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

#pragma mark - NSURLSessionDelegate

- (void)URLSession:(NSURLSession *)session didBecomeInvalidWithError:(NSError *)error {
    if (session == _session) {
        [self log:@"URLSession became invalid" level:RTCLogLevelError];
        [self didError:error.localizedDescription ?: @"URLSession invalid"];
    }
}



@end
