//
//  SocketEngine.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketEngine.h"
#import "NSString+RTCVPSocketIO.h"
#import "NSString+Random.h"
#import "RTCVPStringReader.h"
#import "RTCDefaultSocketLogger.h"
#import "RTCVPSocketEngine+Private.h"
#import "RTCVPSocketIOConfig.h"
#import "RTCVPProbe.h"
#import "RTCVPTimeoutManager.h"
#import "RTCVPTimer.h"
#import "RTCVPWebSocketProtocolFixer.h"

#ifdef USE_CPP_WEBSOCKET
#define RTC_WS_CLASS RTCCPPCWebSocket
#else
#define RTC_WS_CLASS RTCJFRWebSocket
#endif

typedef void (^EngineURLSessionDataTaskCallBack)(NSData* data, NSURLResponse* response, NSError* error);

#ifdef USE_CPP_WEBSOCKET
@interface RTCVPSocketEngine()<RTCCPPCWebSocketDelegate,
NSURLSessionDelegate>
#else
@interface RTCVPSocketEngine()<RTCJFRWebSocketDelegate,
NSURLSessionDelegate>
#endif


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
    
    // 初始化线程安全锁
    _stateLock = [[NSLock alloc] init];
    _stateLock.name = @"com.socketio.engine.stateLock";
    
    // 初始化状态
    [_stateLock lock];
    _closed = NO;
    _connected = NO;
    _polling = YES;
    _websocket = NO;
    _probing = NO;
    _invalidated = NO;
    _fastUpgrade = NO;
    _waitingForPoll = NO;
    _waitingForPost = NO;
    [_stateLock unlock];
    
    // 初始化数据
    _sid = @"";
    _postWait = [NSMutableOrderedSet orderedSet];
    _probeWait = [NSMutableArray array];
    
    // 设置心跳参数
    _pingInterval = self.config.pingInterval * 1000; // 转换为毫秒
    _pingTimeout = self.config.pingTimeout * 1000;
    _pongsMissed = 0;
    _pongsMissedMax = MAX(1, _pingTimeout / _pingInterval);
    
    self.reconnectAttempts = 0;
    
//    dispatch_queue_t networkQueue = dispatch_queue_create("com.vpsocketio.network", DISPATCH_QUEUE_CONCURRENT);
    
    NSOperationQueue *sessionQueue = [[NSOperationQueue alloc] init];
//    sessionQueue.underlyingQueue = networkQueue;
    sessionQueue.maxConcurrentOperationCount = 1;
    sessionQueue.name = @"com.vpsocketio.session.queue";
    
    NSURLSessionConfiguration *sessionConfig = [NSURLSessionConfiguration defaultSessionConfiguration];
    sessionConfig.HTTPMaximumConnectionsPerHost = 4;
    sessionConfig.timeoutIntervalForRequest = 30;
    sessionConfig.timeoutIntervalForResource = 300;
    sessionConfig.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
    sessionConfig.HTTPShouldUsePipelining = YES;
    
    _session = [NSURLSession sessionWithConfiguration:sessionConfig
                                             delegate:self.config.sessionDelegate ?: self
                                        delegateQueue:sessionQueue];
    // 4. 确保安全地访问属性
    dispatch_queue_set_specific(_engineQueue, (__bridge const void *)(_engineQueue), (__bridge void *)(_engineQueue), NULL);
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
    // 仅当URL明确指定https或wss时才使用安全连接
    if ([_url.scheme hasPrefix:@"https"] || [_url.scheme hasPrefix:@"wss"]) {
        secure = YES;
    } else {
        // 非加密连接，强制使用非安全协议
        secure = NO;
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
    
    // 添加压缩参数（如果启用）
    if (self.config.compressionEnabled) {
        params[@"compress"] = @"1"; // 告知服务器客户端支持压缩
    }
    
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
    return @"Engine.IO";
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
    [self _disconnect:reason];
}

- (void)_disconnect:(NSString *)reason {
    [self.stateLock lock];
    BOOL isConnected = self.connected;
    BOOL isClosed = self.closed;
    BOOL isWebsocket = self.websocket;
    [self.stateLock unlock];
    
    if (!isConnected && isClosed) {
        return;
    }
    
    [self log:[NSString stringWithFormat:@"Disconnecting: %@", reason] level:RTCLogLevelInfo];
    
    // 发送关闭消息
    if (isConnected && !isClosed) {
        if (isWebsocket) {
            [self sendWebSocketMessage:@"" withType:RTCVPSocketEnginePacketTypeClose withData:@[]];
        } else {
            [self disconnectPolling];
        }
    }
    
    [self closeOutEngine:reason];
}

- (void)resetEngine {
    [self log:@"Resetting engine state" level:RTCLogLevelDebug];
    
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
    
    // 直接处理数据作为有效负载，因为WebSocket帧的有效负载已经在EngineWebsocket分类中提取过了
    [self processEngineBinaryPayload:data];
}

- (void)processEngineBinaryPayload:(NSData *)payload {
    if (!payload || payload.length == 0) {
        [self log:@"Received empty binary payload" level:RTCLogLevelWarning];
        return;
    }
    
    [self log:[NSString stringWithFormat:@"Processing binary payload, length: %lu", (unsigned long)payload.length] level:RTCLogLevelDebug];
    
    // 根据协议版本处理二进制数据
    if (self.config.protocolVersion == RTCVPSocketIOProtocolVersion2) {
        // Engine.IO 3.x 协议：二进制数据前会有一个字节的标记
        // 第一个字节是 0x04 表示二进制消息
        if (payload.length > 1) {
            const Byte *bytes = (const Byte *)payload.bytes;
            Byte firstByte = bytes[0];
            
            if (firstByte == 0x04) {
                // 提取实际的二进制数据
                NSData *actualData = [payload subdataWithRange:NSMakeRange(1, payload.length - 1)];
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
        [self log:[NSString stringWithFormat:@"Engine.IO 4.x binary data, length: %lu", (unsigned long)payload.length] level:RTCLogLevelDebug];
        
        // 直接传递给客户端处理
        if (self.client) {
            [self.client parseEngineBinaryData:payload];
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
    [self log:[NSString stringWithFormat:@"handleOpen data:%@",openData] level:RTCLogLevelInfo];
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
    
    // 解析压缩支持
    NSNumber *compress = json[@"compress"];
    BOOL serverSupportsCompress = compress && [compress boolValue];
    if (serverSupportsCompress) {
        [self log:[NSString stringWithFormat:@"服务器支持压缩，客户端压缩配置: %@", self.config.compressionEnabled ? @"启用" : @"禁用"] level:RTCLogLevelInfo];
    }
    
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
        [self __sendConnectToServer];

    } else {
        [self log:@"Using polling transport" level:RTCLogLevelDebug];
        [self __sendConnectToServer];
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

- (void)__sendConnectToServer{
    // V2 客户端不用发
    if (_config.protocolVersion < RTCVPSocketIOProtocolVersion3) {
        return;
    }
    // 在handleOpen方法末尾添加命名空间加入逻辑
    // 发送命名空间加入请求（Socket.IO connect packet）
    // 格式：Engine.IO消息类型4 + Socket.IO连接类型0
    NSString *_namespace = self.config.nsp ?: @"/";
    if ([_namespace isEqualToString:@"/"]) {
        // 加入默认命名空间，发送Socket.IO connect packet: "0"
        [self write:@"0" withType:RTCVPSocketEnginePacketTypeMessage withData:@[]];
        [self log:@"📤 已发送默认命名空间加入请求: 0" level:RTCLogLevelInfo];
    } else {
        // 加入自定义命名空间，发送Socket.IO connect packet: "0/_namespace"
        NSString *joinMessage = [NSString stringWithFormat:@"0%@", _namespace];
        [self write:joinMessage withType:RTCVPSocketEnginePacketTypeMessage withData:@[]];
        [self log:[NSString stringWithFormat:@"📤 已发送命名空间加入请求: %@", joinMessage] level:RTCLogLevelInfo];
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

///// 发送消息和数据（带ACK回调）
//- (void)send:(NSString *)msg withData:(NSArray<NSData *> *)data ack:(RTCVPSocketAckCallback)ack {
//    if (!msg || self.closed || !self.connected) {
//        if (ack) {
//            ack(@[]); // 连接已关闭，立即回调空数据
//        }
//        return;
//    }
//    
//    NSInteger ackId = [self generateACKId];
//    
//    // 如果有ACK回调，先存储起来
//    if (ack) {
//        [self.ackManager addCallback:ack forId:ackId];
//    }
//    
//    // 构建带有ACK ID的消息格式
//    // Socket.IO协议格式: [event_name, data, ack_id]
//    // 如果没有数据，格式为: [event_name, ack_id]
//    // 如果有数据，格式为: [event_name, data, ack_id]
//    
//    NSMutableArray *messageParts = [NSMutableArray array];
//    
//    // 解析原始消息（可能是JSON数组）
//    NSError *error = nil;
//    NSData *msgData = [msg dataUsingEncoding:NSUTF8StringEncoding];
//    id jsonObject = [NSJSONSerialization JSONObjectWithData:msgData options:0 error:&error];
//    
//    if (error) {
//        // 如果不是JSON，直接当作事件名处理
//        [messageParts addObject:msg];
//        if (data.count > 0) {
//            // 如果有二进制数据，添加占位符
//            [messageParts addObject:@"_placeholder"];
//            [messageParts addObject:@(ackId)];
//        } else {
//            // 没有二进制数据，直接添加ACK ID
//            [messageParts addObject:@(ackId)];
//        }
//    } else if ([jsonObject isKindOfClass:[NSArray class]]) {
//        // 已经是JSON数组，需要插入ACK ID
//        NSMutableArray *jsonArray = [jsonObject mutableCopy];
//        
//        // 查找二进制数据占位符
//        BOOL hasBinaryPlaceholder = NO;
//        for (id item in jsonArray) {
//            if ([item isKindOfClass:[NSDictionary class]]) {
//                id placeholder = ((NSDictionary *)item)[@"_placeholder"];
//                if (placeholder) {
//                    hasBinaryPlaceholder = YES;
//                    break;
//                }
//            }
//        }
//        
//        if (hasBinaryPlaceholder) {
//            // 有二进制数据占位符，ACK ID在占位符之后
//            [jsonArray addObject:@(ackId)];
//        } else {
//            // 没有二进制数据，ACK ID在最后一个位置
//            [jsonArray addObject:@(ackId)];
//        }
//        
//        // 转换为JSON字符串
//        NSData *jsonData = [NSJSONSerialization dataWithJSONObject:jsonArray options:0 error:&error];
//        if (!error) {
//            msg = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
//        }
//    }
//    
//    // 发送消息
//    [self write:msg withType:RTCVPSocketEnginePacketTypeMessage withData:data];
//    
//    [self log:[NSString stringWithFormat:@"Sent message with ACK ID: %ld", (long)ackId] level:RTCLogLevelDebug];
//}

/// 发送ACK响应
//- (void)sendAck:(NSInteger)ackId withData:(NSArray *)data {
//    if (self.closed || !self.connected) {
//        return;
//    }
//    
//    // 构建ACK响应格式
//    // Socket.IO协议格式: [ack_id, data]
//    NSArray *ackArray = @[@(ackId)];
//    
//    // 如果有数据，添加到数组中
//    NSMutableArray *responseArray = [ackArray mutableCopy];
//    if (data && data.count > 0) {
//        // 检查数据中是否有二进制数据
//        BOOL hasBinaryData = NO;
//        for (id item in data) {
//            if ([item isKindOfClass:[NSData class]]) {
//                hasBinaryData = YES;
//                break;
//            }
//        }
//        
//        if (hasBinaryData) {
//            // 有二进制数据，添加占位符
//            NSMutableArray *processedData = [NSMutableArray array];
//            NSInteger placeholderIndex = 0;
//            NSMutableArray *binaryDataArray = [NSMutableArray array];
//            
//            for (id item in data) {
//                if ([item isKindOfClass:[NSData class]]) {
//                    // 二进制数据，添加占位符
//                    NSDictionary *placeholder = @{
//                        @"_placeholder": @YES,
//                        @"num": @(placeholderIndex)
//                    };
//                    [processedData addObject:placeholder];
//                    [binaryDataArray addObject:item];
//                    placeholderIndex++;
//                } else {
//                    [processedData addObject:item];
//                }
//            }
//            
//            [responseArray addObject:processedData];
//            
//            // 发送消息
//            NSError *error = nil;
//            NSData *jsonData = [NSJSONSerialization dataWithJSONObject:responseArray options:0 error:&error];
//            if (!error) {
//                NSString *jsonString = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
//                [self write:jsonString withType:RTCVPSocketEnginePacketTypeMessage withData:binaryDataArray];
//            }
//        } else {
//            // 没有二进制数据，直接添加数据
//            [responseArray addObjectsFromArray:data];
//            
//            // 发送消息
//            NSError *error = nil;
//            NSData *jsonData = [NSJSONSerialization dataWithJSONObject:responseArray options:0 error:&error];
//            if (!error) {
//                NSString *jsonString = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
//                [self write:jsonString withType:RTCVPSocketEnginePacketTypeMessage withData:@[]];
//            }
//        }
//    } else {
//        // 没有数据，直接发送ACK ID
//        NSError *error = nil;
//        NSData *jsonData = [NSJSONSerialization dataWithJSONObject:responseArray options:0 error:&error];
//        if (!error) {
//            NSString *jsonString = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
//            [self write:jsonString withType:RTCVPSocketEnginePacketTypeMessage withData:@[]];
//        }
//    }
//    
//    [self log:[NSString stringWithFormat:@"Sent ACK response for ID: %ld", (long)ackId] level:RTCLogLevelDebug];
//}

/// 发送ACK响应（由客户端调用）
- (void)sendAckResponse:(NSString *)ackMessage withData:(NSArray<NSData *> *)data {
    dispatch_async(self.engineQueue, ^{
        if (!self.connected || self.closed) {
            [self log:@"Cannot send ACK response, engine not connected" level:RTCLogLevelWarning];
            return;
        }
        
        [self write:ackMessage withType:RTCVPSocketEnginePacketTypeMessage withData:data];
        
        [self log:[NSString stringWithFormat:@"Sent ACK response: %@", ackMessage] level:RTCLogLevelDebug];
    });
}

#pragma mark - 消息解析（增强版）

/// 解析原始引擎消息（增强版，支持ACK）
- (void)parseEngineMessage:(NSString *)message {
    if (message.length == 0) {
        [self log:@"Received empty message" level:RTCLogLevelWarning];
        return;
    }
    
    [self log:[NSString stringWithFormat:@"parseEngineMessage Got message: %@", message] level:RTCLogLevelDebug];
    
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
                    // 服务器发送的 ping，必须立即回复 pong
                    [self log:[NSString stringWithFormat:@"📩 收到Engine.IO ping消息，立即回复pong"] level:RTCLogLevelInfo];
                    
                    // 直接同步发送pong响应，不使用sendWebSocketMessage避免重复添加类型前缀
                    if (self.websocket && self.ws && [self.ws isConnected]) {
                        // 直接发送pong消息: "3"，不使用sendWebSocketMessage避免重复添加类型前缀
                        [self.ws writeString:@"3"];
                        [self log:@"📤 已立即发送pong响应: 3" level:RTCLogLevelInfo];
                    } else {
                        // 那就是轮训发送消息
                        [self.postWait addObject:@"3"];
                        //强制刷新
                        [self flushWaitingForPost];
                        [self log:@"📤 使用异步队列发送pong响应" level:RTCLogLevelInfo];
                    }
                    break;
                case RTCVPSocketEnginePacketTypePong:
                    [self handlePong:content];
                    break;
                case RTCVPSocketEnginePacketTypeMessage:
                    // 直接传递消息给客户端，由客户端处理ACK
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

/// 处理Socket.IO消息（支持ACK）
//- (void)handleSocketIOMessage:(NSString *)message {
//    // 直接传递给客户端处理，包括ACK
//    [self handleMessage:message];
//}



- (void)closeOutEngine:(NSString *)reason {
    [self.stateLock lock];
    BOOL isClosed = self.closed;
    [self.stateLock unlock];
    
    if (isClosed) {
        return;
    }
    
    [self log:[NSString stringWithFormat:@"Closing engine: %@", reason] level:RTCLogLevelInfo];
    
    [self cancelProbeTimeout];
    [self cancelConnectionTimeout];
    
    // 保护状态变量修改
    [self.stateLock lock];
    self.closed = YES;
    self.connected = NO;
    self.invalidated = YES;
    self.pongsMissed = 0;
    [self.stateLock unlock];
    
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
    NSMutableArray *dataArr = [NSMutableArray array];
    for (NSData *dataA in data) {
        [dataArr addObject:[dataA copy]];
    }
    dispatch_async(self.engineQueue, ^{
        if (!self.connected || self.closed) {
            [self log:@"Cannot write, engine not connected" level:RTCLogLevelWarning];
            return;
        }
        
        if (self.websocket) {
            [self sendWebSocketMessage:msg withType:type withData:[dataArr copy]];
        } else if (self.probing) {
            // 在探测期间，缓存消息
            RTCVPProbe *probe = [[RTCVPProbe alloc] init];
            probe.message = msg;
            probe.type = type;
            probe.data = [dataArr copy];
            [self.probeWait addObject:probe];
        } else {
            [self sendPollMessage:msg withType:type withData:[dataArr copy]];
        }
    });
}


#pragma mark - 发送消息

- (void)send:(NSString *)msg withData:(NSArray<NSData *> *)data {
    [self log:[NSString stringWithFormat:@"发送消息: %@ (原始Socket.IO包)", msg] level:RTCLogLevelDebug];
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
    
    [request setValue:@"gzip, deflate, br, zstd" forHTTPHeaderField:@"Accept-encoding"];
    [request setValue:@"zh-CN,zh;q=0.9"          forHTTPHeaderField:@"Accept-language"];
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
-(void)URLSession:(NSURLSession *)session didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential * _Nullable))completionHandler{
    // 根据配置决定是否忽略SSL证书验证
       if ([challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust]) {
           if (self.config.allowSelfSignedCertificates) {
               // 允许自签名证书（开发环境）
               SecTrustRef serverTrust = challenge.protectionSpace.serverTrust;
               NSURLCredential *credential = [NSURLCredential credentialForTrust:serverTrust];
               [self log:@"忽略SSL证书验证（允许自签名证书）" level:RTCLogLevelDebug];
               completionHandler(NSURLSessionAuthChallengeUseCredential, credential);
           } else {
               // 使用默认验证（生产环境）
               [self log:@"使用默认SSL证书验证" level:RTCLogLevelDebug];
               completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
           }
       } else {
           // 其他类型的认证，使用默认处理
           completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
       }
}

#pragma mark - RTCVPSocketEngineProtocol

- (void)syncResetClient {
    dispatch_sync(self.engineQueue, ^{
        self.client = nil;
    });
}






#pragma mark - Polling Transport


#pragma mark - 轮询传输

- (void)doLongPoll:(NSURLRequest *)request {
    if (!self.polling || self.closed || self.invalidated) {
        return;
    }
    
    self.waitingForPoll = YES;
    
    __weak typeof(self) weakSelf = self;
    
    NSURLSessionDataTask *task = [self.session dataTaskWithRequest:request completionHandler:^(NSData * _Nullable data, NSURLResponse * _Nullable response, NSError * _Nullable error) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) return;
        
        dispatch_queue_t engineQueue = strongSelf.engineQueue;
        if (!engineQueue) return;
        
        dispatch_async(engineQueue, ^{
            __strong typeof(weakSelf) strongSelfInQueue = weakSelf;
            if (!strongSelfInQueue) return;
            
            @autoreleasepool {
                BOOL isPolling = strongSelfInQueue.polling;
                BOOL isClosed = strongSelfInQueue.closed;
                BOOL isFastUpgrade = strongSelfInQueue.fastUpgrade;
                
                if (!isPolling || isClosed) {
                    return;
                }
                
                NSInteger statusCode = 200;
                if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
                    statusCode = ((NSHTTPURLResponse *)response).statusCode;
                }
                
                if (error) {
                    [strongSelfInQueue log:[NSString stringWithFormat:@"Polling error: %@", error.localizedDescription] level:RTCLogLevelError];
                    [strongSelfInQueue didError:error.localizedDescription];
                } else if (statusCode != 200) {
                    NSString *errorMsg = [NSString stringWithFormat:@"HTTP %ld", (long)statusCode];
                    [strongSelfInQueue log:[NSString stringWithFormat:@"Polling HTTP error: %@", errorMsg] level:RTCLogLevelError];
                    [strongSelfInQueue didError:errorMsg];
                } else if (!data) {
                    [strongSelfInQueue log:@"Polling received empty data" level:RTCLogLevelError];
                    [strongSelfInQueue didError:@"Empty response"];
                } else {
                    // 解析轮询响应
                    NSString *responseString = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
                    if (responseString) {
                        [strongSelfInQueue log:[NSString stringWithFormat:@"Polling response: %@", responseString] level:RTCLogLevelDebug];
                        [strongSelfInQueue parsePollingMessage:responseString];
                    } else {
                        [strongSelfInQueue log:@"Polling response not UTF-8" level:RTCLogLevelWarning];
                        // 尝试处理二进制数据
                        [strongSelfInQueue parseEngineData:data];
                    }
                }
                
                strongSelfInQueue.waitingForPoll = NO;
                
                isPolling = strongSelfInQueue.polling;
                isClosed = strongSelfInQueue.closed;
                isFastUpgrade = strongSelfInQueue.fastUpgrade;
                
                if (isFastUpgrade) {
                    [strongSelfInQueue doFastUpgrade];
                }
                else if (isPolling && !isClosed) {
                    [strongSelfInQueue doPoll];
                }
            }
        });
    }];
    
    [task resume];
}

- (void)doPoll {
    if (self.waitingForPoll || !self.polling || self.closed || !self.connected) {
        return;
    }
    
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:[self urlPollingWithSid]];
    request.timeoutInterval = 30;
    [self addHeadersToRequest:request];
    
    [self doLongPoll:request];
}

- (void)parsePollingMessage:(NSString *)string {
    if (string.length == 0) {
        return;
    }
    
    if (self.config.protocolVersion >= RTCVPSocketIOProtocolVersion3) {
        // Engine.IO v3/v4 格式：使用 \x1e 分隔多个消息
        NSArray<NSString *> *messages = [string componentsSeparatedByString:@"\x1e"];
        for (NSString *message in messages) {
            if (message.length > 0) {
                [self parseEngineMessage:message];
            }
        }
    } else {
        // Engine.IO v2 格式：length:message
        RTCVPStringReader *reader = [[RTCVPStringReader alloc] init:string];
        
        while (reader.hasNext) {
            NSString *lengthStr = [reader readUntilOccurence:@":"];
            
            if ([lengthStr rangeOfCharacterFromSet:[NSCharacterSet decimalDigitCharacterSet]].location != NSNotFound) {
                NSInteger length = [lengthStr integerValue];
                if (length > 0) {
                    NSString *message = [reader read:(int)length];
                    [self parseEngineMessage:message];
                }
            } else {
                // 没有长度前缀，可能是单个消息
                [self parseEngineMessage:string];
                break;
            }
        }
    }
}

/// 轮训模式发送消息
/// Socket.IO轮询协议参考: https://github.com/socketio/engine.io-protocol#polling
/// 消息格式: 
/// - 普通消息: <type><message> 例如: 40 (心跳), 2["event", {"data": "value"}] (事件)
/// - 二进制消息: 
///   - v2: <type><message_with_placeholders> 例如: 5["binaryEvent",{"_placeholder":true,"num":0}] 后跟 base64编码的二进制数据
///   - v3/v4: <type><message_with_placeholders> 例如: 51-["binaryEvent",{"_placeholder":true,"num":0}] 后跟原始二进制数据
/// @param message 消息内容（不包含类型前缀）
/// @param type 消息类型
/// @param data 二进制数据数组（如果有）
- (void)sendPollMessage:(NSString *)message withType:(RTCVPSocketEnginePacketType)type withData:(NSArray *)data {
    [self log:[NSString stringWithFormat:@"📤 准备发送轮询消息，类型: %ld, 内容: %@, 二进制数据数量: %lu", (long)type, message, (unsigned long)data.count]
        level:RTCLogLevelDebug];
    
    if (!self.connected || self.closed) {
        [self log:@"❌ 连接已关闭或未连接，无法发送消息" level:RTCLogLevelError];
        return;
    }

    // 构建包含占位符的消息
    NSString *placeholderMessage = [self createMessageWithPlaceholderForType:type
                                                                    message:message
                                                                 binaryCount:data.count];
    
    [self log:[NSString stringWithFormat:@"📦 构建轮询消息（包含占位符）: %@", placeholderMessage]
        level:RTCLogLevelDebug];
    
    // 添加到待发送队列
    [self.postWait addObject:placeholderMessage];
    [self log:[NSString stringWithFormat:@"📋 消息已添加到发送队列，当前队列长度: %lu", (unsigned long)self.postWait.count]
        level:RTCLogLevelDebug];
    
    // 根据协议版本处理二进制数据
    if (self.config.enableBinary && data.count > 0) {
        [self log:[NSString stringWithFormat:@"🔄 处理二进制数据，数量: %lu，协议版本: %ld", (unsigned long)data.count, (long)self.config.protocolVersion]
            level:RTCLogLevelDebug];
        
        // 逐个添加二进制数据
        for (NSInteger i = 0; i < data.count; i++) {
            NSData *binaryData = data[i];
            [self log:[NSString stringWithFormat:@"📦 处理第 %ld 个二进制数据，大小: %ld字节", (long)i, (long)binaryData.length]
                level:RTCLogLevelDebug];
            // 无论v2还是v3/v4协议，在polling模式下都需要转换为base64
            NSString *base64String = [binaryData base64EncodedStringWithOptions:0];
            
            if (self.config.protocolVersion == RTCVPSocketIOProtocolVersion2) {
                [self log:@"🔢 使用Socket.IO v2协议，将二进制数据转换为base64" level:RTCLogLevelDebug];
                // v2协议：添加"b4"前缀
                NSString *binaryMessage = [NSString stringWithFormat:@"b4%@", base64String];
                [self.postWait addObject:binaryMessage];
                [self log:[NSString stringWithFormat:@"📋 编码后的二进制消息已添加到队列，长度: %lu字符", (unsigned long)binaryMessage.length] level:RTCLogLevelDebug];
            } else {
                // v3/v4协议：使用特殊的二进制帧格式
                // Engine.IO v3/v4在polling模式下，二进制数据应该作为独立的帧发送，格式为：
                // - 前缀 "b" + base64编码的数据
                [self log:@"🔢 使用Socket.IO v3/v4协议，将二进制数据转换为base64" level:RTCLogLevelDebug];
                NSString *binaryFrame = [NSString stringWithFormat:@"b%@", base64String];
                [self.postWait addObject:binaryFrame];
                [self log:[NSString stringWithFormat:@"📋 编码后的二进制帧已添加到队列，长度: %lu字符", (unsigned long)binaryFrame.length] level:RTCLogLevelDebug];
            }
        }
    }
    
    // 立即发送重要消息
    BOOL isImportantMessage = (type == RTCVPSocketEnginePacketTypeMessage && [message hasPrefix:@"0"]);
    if (isImportantMessage) {
        [self log:@"🔔 检测到重要消息（连接/命名空间消息），立即发送" level:RTCLogLevelInfo];
        [self flushWaitingForPost];
    } else if (self.postWait.count > 0 && !self.waitingForPost) {
        [self log:[NSString stringWithFormat:@"📤 发送队列中有 %lu 条消息，开始发送", (unsigned long)self.postWait.count]
            level:RTCLogLevelInfo];
        [self flushWaitingForPost];
    } else {
        [self log:[NSString stringWithFormat:@"⏳ 等待发送条件满足，当前发送中: %@，队列长度: %lu", self.waitingForPost ? @"是" : @"否", (unsigned long)self.postWait.count]
            level:RTCLogLevelDebug];
    }
    
}

/// 创建包含占位符的消息
/// Socket.IO二进制消息格式参考: https://socket.io/docs/v4/binary-events/
/// 占位符格式: {"_placeholder":true,"num":0}
/// 消息结构: 
/// - v2: <type><message> 例如: 5["binaryEvent",{"_placeholder":true,"num":0}]
/// - v3/v4: <type><attachments>-<message> 例如: 51-["binaryEvent",{"_placeholder":true,"num":0}] 其中1表示有1个二进制附件
/// @param type 消息类型
/// @param message 消息内容（不包含类型前缀）
/// @param binaryCount 二进制数据数量
/// @return 完整的消息字符串（包含类型前缀和占位符）
- (NSString *)createMessageWithPlaceholderForType:(RTCVPSocketEnginePacketType)type
                                          message:(NSString *)message
                                       binaryCount:(NSUInteger)binaryCount {
    [self log:[NSString stringWithFormat:@"🧩 开始创建包含占位符的消息，类型: %ld, 消息内容: %@, 二进制数量: %lu", (long)type, message, (unsigned long)binaryCount]
        level:RTCLogLevelDebug];
    
    NSString *fullMessage = nil;
    
    fullMessage = [NSString stringWithFormat:@"%ld%@", (long)type, message];
    
    [self log:[NSString stringWithFormat:@"✅ 创建完成，完整消息: %@", fullMessage] level:RTCLogLevelDebug];
    return fullMessage;
}

- (void)disconnectPolling {
    if (self.polling && !self.closed) {
        // 添加关闭消息到队列
        NSString *closeMessage = [NSString stringWithFormat:@"%ld", (long)RTCVPSocketEnginePacketTypeClose];
        [self.postWait addObject:closeMessage];
        [self _sendWaitPostWithDisconnect:YES];
        
    }
}

-(void) _sendWaitPostWithDisconnect:(BOOL)isDisconnect{
    // 发送最后的请求
    if (self.postWait.count > 0) {
        NSArray *pstArr = [self.postWait copy];
        [self.postWait removeAllObjects];
        for (NSInteger i = 0; i < pstArr.count; i++) {
            id packet = pstArr[i];
            NSURLRequest *request = [self createRequestForPostWithPostWaitWithData:packet];
            if (isDisconnect) {
                [[self.session dataTaskWithRequest:request] resume];
            }else{
                __weak typeof(self) weakSelf = self;
                NSURLSessionDataTask *task = [self.session dataTaskWithRequest:request completionHandler:^(NSData * _Nullable data, NSURLResponse * _Nullable response, NSError * _Nullable error) {
                    __strong typeof(weakSelf) strongSelf = weakSelf;
                    if (!strongSelf) return;
                    
                    dispatch_queue_t engineQueue = strongSelf.engineQueue;
                    if (!engineQueue) return;
                    
                    dispatch_async(engineQueue, ^{
                        __strong typeof(weakSelf) strongSelfInQueue = weakSelf;
                        if (!strongSelfInQueue) return;
                        
                        strongSelfInQueue.waitingForPost = NO;
                        
                        BOOL isPolling = strongSelfInQueue.polling;
                        BOOL isFastUpgrade = strongSelfInQueue.fastUpgrade;
                        
                        if (error) {
                            [strongSelfInQueue log:[NSString stringWithFormat:@"POST error: %@", error.localizedDescription] level:RTCLogLevelError];
                            if (isPolling) {
                                [strongSelfInQueue didError:error.localizedDescription];
                            }
                        } else {
                            [strongSelfInQueue log:@"POST successful" level:RTCLogLevelDebug];
                            
                            isFastUpgrade = strongSelfInQueue.fastUpgrade;
                            
                            if (!isFastUpgrade) {
                                [strongSelfInQueue flushWaitingForPost];
                                [strongSelfInQueue doPoll];
                            }
                        }
                    });
                }];
                
                [task resume];
            }
            
        }
        
    }
}

- (void)flushWaitingForPost {
    if (self.postWait.count == 0 || self.closed || !self.connected) {
        return;
    }
    
    if (self.websocket) {
        [self flushWaitingForPostToWebSocket];
        return;
    }
    
    self.waitingForPost = YES;
    
    [self _sendWaitPostWithDisconnect:NO];
        
    
}

#pragma mark - URL 构建

- (NSURL *)urlPollingWithSid {
    if (!self.url) {
        return nil;
    }
    // 生成并添加 t 参数（防止缓存）
    NSString *tParam = [self generateTParameter];
    
    if (self.config.protocolVersion > RTCVPSocketIOProtocolVersion2 && self.connected) {
        NSURLComponents *components = [[NSURLComponents alloc] init];
        components.scheme = self.url.scheme;
        components.host = self.url.host;
        components.port = self.url.port;
        components.path = @"/socket.io/";
        
        NSMutableArray<NSURLQueryItem *> *queryItems = [NSMutableArray array];
        
        [queryItems addObject:[[NSURLQueryItem alloc] initWithName:@"EIO" value:@"4"]];
        [queryItems addObject:[[NSURLQueryItem alloc] initWithName:@"transport" value:@"polling"]];
        [queryItems addObject:[[NSURLQueryItem alloc] initWithName:@"t" value:tParam]];
        
        if (self.sid.length > 0) {
            [queryItems addObject:[[NSURLQueryItem alloc] initWithName:@"sid" value:self.sid]];
        }
        
        components.queryItems = queryItems;
        
        return components.URL;
    }
    
    if (!self.urlPolling) {
        return nil;
    }
    
    NSURLComponents *components = [NSURLComponents componentsWithURL:self.urlPolling resolvingAgainstBaseURL:NO];
    NSMutableArray<NSURLQueryItem *> *queryItems = [NSMutableArray array];
    
    if (components.queryItems) {
        [queryItems addObjectsFromArray:components.queryItems];
    }
    [queryItems addObject:[[NSURLQueryItem alloc] initWithName:@"t" value:tParam]];
    
    if (self.sid.length > 0) {
        [queryItems addObject:[[NSURLQueryItem alloc] initWithName:@"sid" value:self.sid]];
    }
    
    components.queryItems = queryItems;
    return components.URL;
}

/// 生成 t 参数：紧凑的base62时间戳+随机字符串，防止重复
- (NSString *)generateTParameter {
    // 浏览器格式：g96ymem3（类似base64编码的时间戳+随机字符）
    // 使用base62编码当前时间戳的毫秒值，确保唯一性
    // 再添加少量随机字符，防止碰撞
    
    static NSString *const kBase62Chars = @"0123456789abcdefghijklmnopqrstuvwxyz";
    const NSUInteger kBase62Count = [kBase62Chars length];
    
    NSMutableString *tParam = [NSMutableString stringWithCapacity:10];
    
    // 1. 获取当前时间戳（毫秒）作为基础，确保唯一性
    uint64_t timestamp = (uint64_t)([[NSDate date] timeIntervalSince1970] * 1000);
    
    // 2. 转换为base62字符串（紧凑格式）
    if (timestamp == 0) {
        [tParam appendString:@"0"];
    } else {
        uint64_t value = timestamp;
        while (value > 0) {
            uint64_t remainder = value % kBase62Count;
            [tParam insertString:[NSString stringWithFormat:@"%C", [kBase62Chars characterAtIndex:(NSUInteger)remainder]] atIndex:0];
            value = value / kBase62Count;
        }
    }
    
    // 3. 生成3-4个随机字符，防止相同时间戳的碰撞
    for (NSInteger i = 0; i < 4; i++) {
        NSUInteger randomIndex = arc4random_uniform((u_int32_t)kBase62Count);
        [tParam appendFormat:@"%C", [kBase62Chars characterAtIndex:randomIndex]];
    }
    
    return tParam;
}

- (NSURLRequest *)createRequestForPostWithPostWaitWithData:(id)packet {
    BOOL isV3 = self.config.protocolVersion >= RTCVPSocketIOProtocolVersion3;
    

    NSURL *url = [self urlPollingWithSid];
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
    [self addHeadersToRequest:request];

    request.HTTPMethod = @"POST";
    
    if (isV3) {
        // Engine.IO v3/v4
        
        if ([packet isKindOfClass:[NSString class]]) {
            // 文本消息
            NSData *textData = [(NSString *)packet dataUsingEncoding:NSUTF8StringEncoding];
            request.HTTPBody = textData;
            [request setValue:@"text/plain; charset=UTF-8" forHTTPHeaderField:@"Content-Type"];


        } else if ([packet isKindOfClass:[NSData class]]) {
            // 二进制数据
            // 对于二进制数据，直接添加到 body 中
            request.HTTPBody = packet;
            [request setValue:@"text/plain; charset=UTF-8" forHTTPHeaderField:@"Content-Type"];
//            [request setValue:@"application/octet-stream" forHTTPHeaderField:@"Content-Type"];

        }
    } else {
        // Engine.IO v2
        if ([packet isKindOfClass:[NSString class]]) {
            NSString *framed = [NSString stringWithFormat:@"%lu:%@",
                               (unsigned long)[(NSString *)packet length],
                               (NSString *)packet];
            request.HTTPBody = [framed dataUsingEncoding:NSUTF8StringEncoding];
            [request setValue:@"text/plain; charset=UTF-8" forHTTPHeaderField:@"Content-Type"];

        }
        // v2 协议中，二进制数据会被编码为字符串，所以这里不会出现 NSData
    }

    return request;
}

- (void)stopPolling {
    self.waitingForPoll = NO;
    self.waitingForPost = NO;
    [self.session finishTasksAndInvalidate];
}


#pragma mark - WebSocket Transport


#pragma mark - WebSocket 管理

- (void)createWebSocketAndConnect {
    if (self.closed || self.invalidated) {
        return;
    }
    
    NSURL *url = [self urlWebSocketWithSid];
    if (!url) {
        [self didError:@"Invalid WebSocket URL"];
        return;
    }
    
    [self log:@"Creating WebSocket connection..." level:RTCLogLevelDebug];
    [self log:[NSString stringWithFormat:@"WebSocket URL: %@", url.absoluteString] level:RTCLogLevelDebug];
    
    self.ws = [[RTC_WS_CLASS alloc] initWithURL:url protocols:@[]];
    self.ws.queue = self.engineQueue;
    self.ws.delegate = self;
    // 配置 WebSocket
    self.ws.voipEnabled = YES;
    self.ws.selfSignedSSL = self.config.allowSelfSignedCertificates;
    self.ws.security = self.config.security;
    self.ws.enableCompression = self.config.compressionEnabled;
    // 添加 headers
    if (self.config.cookies.count > 0) {
        NSDictionary *headers = [NSHTTPCookie requestHeaderFieldsWithCookies:self.config.cookies];
        for (NSString *key in headers.allKeys) {
            [self.ws addHeader:headers[key] forKey:key];
        }
    }
    
    if (self.config.extraHeaders) {
        for (NSString *key in self.config.extraHeaders.allKeys) {
            NSString *value = self.config.extraHeaders[key];
            if ([value isKindOfClass:[NSString class]]) {
                [self.ws addHeader:value forKey:key];
            }
        }
    }
    
    [self.ws connect];
}

- (NSURL *)urlWebSocketWithSid {
    if (!self.urlWebSocket) {
        return nil;
    }
    
    NSURLComponents *components = [NSURLComponents componentsWithURL:self.urlWebSocket resolvingAgainstBaseURL:NO];
    NSMutableString *query = [components.percentEncodedQuery mutableCopy] ?: [NSMutableString string];
    
    if (self.sid.length > 0) {
        NSString *sidParam = [NSString stringWithFormat:@"&sid=%@", [self.sid urlEncode]];
        if (query.length > 0) {
            [query appendString:sidParam];
        } else {
            [query appendString:[sidParam substringFromIndex:1]];
        }
    }
    
    components.percentEncodedQuery = query;
    return components.URL;
}

/**
 * 发送 WebSocket 消息（Engine.IO / Socket.IO 协议）
 *
 * WebSocket 最终发送的内容分两类：
 *  1. 文本帧：用于 Engine.IO 字符包、Socket.IO JSON 包
 *  2. 二进制帧：用于传输二进制 payload（engine v3 与 v4 规则不同）
 *
 * Engine.IO / Socket.IO 消息格式说明：
 *
 * 【文本消息格式】（Engine.IO 文本帧）
 *   [EngineType][Payload]
 *   EngineType：单字符数字，如：
 *      0 open
 *      1 close
 *      2 ping
 *      3 pong
 *      4 message
 *      5 upgrade
 *      6 noop
 *
 *   Payload：通常是 JSON（Socket.IO）或字符串
 *   示例："42["chat","hello"]"
 *       4  -> Engine.IO type: message
 *       2  -> Socket.IO packet type: event
 *       ["chat","hello"] -> event payload
 *
 *
 * 【二进制消息格式】
 *   Engine.IO v3：
 *       0x04 + <binary payload>
 *       0x04 为 Engine.IO 二进制包类型前缀
 *
 *   Engine.IO v4：
 *       直接发送纯二进制 WebSocket 帧，不需要前缀
 *
 */
- (void)sendWebSocketMessage:(NSString *)message
                    withType:(RTCVPSocketEnginePacketType)type
                    withData:(NSArray<NSData *> *)data {

    // 1. 确保 WebSocket 已建立
    if (!self.ws || ![self.ws isConnected]) {
        [self log:@"WebSocket not connected, cannot send message" level:RTCLogLevelWarning];
        return;
    }
    if (message && message.length > 0) {
        // 2. 构建 Engine.IO 文本消息格式
        //    格式：[EngineType][Payload]
        //    例如：@"4{\"msg\":\"hello\"}"
        NSString *fullMessage = [NSString stringWithFormat:@"%ld%@", (long)type, message];

        [self log:[NSString stringWithFormat:@"Sending WebSocket text message: %@", fullMessage]
             level:RTCLogLevelDebug];

        // 3. 发送文本帧
        //    文本帧用于 Socket.IO/Engine.IO 的主控制消息
        [self.ws writeString:fullMessage];
    }

    

    // 4. 若附带二进制数据，则逐个发送二进制帧
    if (self.config.enableBinary && data.count > 0) {

        for (NSData *binaryData in data) {
            NSData *packetData = binaryData;

            // Engine.IO v3 需要加前缀 0x04
            // 0x04 表示 binary message（engine binary packet）
            if (self.config.protocolVersion == RTCVPSocketIOProtocolVersion2) {
                const Byte binaryPrefix = 0x04;

                // 构建 [0x04][binary payload]
                NSMutableData *mutableData = [NSMutableData dataWithBytes:&binaryPrefix length:1];
                [mutableData appendData:binaryData];

                packetData = mutableData;
            }

            [self log:@"Sending WebSocket binary packet" level:RTCLogLevelDebug];

            // Engine.IO v4：发送纯二进制帧
            // Engine.IO v3：发送 0x04 + payload
            [self.ws writeData:packetData];
        }
    }
}


- (void)probeWebSocket {
    if (!self.ws || ![self.ws isConnected] || self.probing || self.websocket) {
        return;
    }
    
    [self log:@"Probing WebSocket connection..." level:RTCLogLevelDebug];
    
    self.probing = YES;
    
    // 发送探测包
    NSString *probeMessage = @"probe";
    [self sendWebSocketMessage:probeMessage withType:RTCVPSocketEnginePacketTypePing withData:@[]];
    
    // 设置探测超时 - 使用超时管理器
    [self startProbeTimeout];
}

- (void)doFastUpgrade {
    if (!self.fastUpgrade || !self.ws || ![self.ws isConnected]) {
        return;
    }
    
    [self log:@"Performing fast upgrade to WebSocket" level:RTCLogLevelDebug];
    
    // 发送升级消息
    [self sendWebSocketMessage:@"" withType:RTCVPSocketEnginePacketTypeUpgrade withData:@[]];
    
    // 更新状态
    self.websocket = YES;
    self.polling = NO;
    self.fastUpgrade = NO;
    self.probing = NO;
    
    // 取消探测超时
    [self cancelProbeTimeout];
    
    
    // 发送缓存在探测期间的消息
    [self flushProbeWait];
}

- (void)flushProbeWait {
    if (self.probeWait.count == 0) {
        return;
    }
    
    [self log:[NSString stringWithFormat:@"Flushing %lu probe wait messages", (unsigned long)self.probeWait.count] level:RTCLogLevelDebug];
    
    for (RTCVPProbe *probe in self.probeWait) {
        [self sendWebSocketMessage:probe.message withType:probe.type withData:probe.data];
    }
    
    [self.probeWait removeAllObjects];
}

- (void)flushWaitingForPostToWebSocket {
    if (self.postWait.count == 0 || !self.ws) {
        return;
    }
    
    [self log:[NSString stringWithFormat:@"Flushing %lu post wait messages to WebSocket", (unsigned long)self.postWait.count] level:RTCLogLevelDebug];
    
    for (id item in self.postWait) {
        if ([item isKindOfClass:[NSString class]]) {
            [self.ws writeString:item];
        } else if ([item isKindOfClass:[NSData class]]) {
//            // 添加 0x04 前缀表示二进制消息（engine binary packet）
//            const Byte binaryPrefix = 0x04;
//            NSMutableData *binaryPacket = [NSMutableData dataWithBytes:&binaryPrefix length:1];
//            [binaryPacket appendData:item];
            [self.ws writeData:item];
        }
    }
    
    [self.postWait removeAllObjects];
}

#pragma mark - RTC_WS_CLASSDelegate

- (void)websocketDidConnect:(RTC_WS_CLASS *)socket {
    [self log:@"WebSocket connected" level:RTCLogLevelInfo];
    
    if (self.config.transport == RTCVPSocketIOTransportWebSocket) {
        // 强制 WebSocket 模式，直接使用
        self.websocket = YES;
        self.polling = NO;
        self.connected = YES;
        
        // 取消连接超时（如果存在）
        [self cancelConnectionTimeout];
        
        // 如果已经有 sid，表示是重连
        if (self.sid.length > 0) {
            // 发送升级消息
            [self sendWebSocketMessage:@"" withType:RTCVPSocketEnginePacketTypeUpgrade withData:@[]];
        } else {
            // 通知客户端连接成功
            if (self.client) {
                [self.client engineDidOpen:@"WebSocket connected"];
            }
        }
    } else if (self.config.transport == RTCVPSocketIOTransportAuto) {
        // 自动模式，需要探测 WebSocket
        [self probeWebSocket];
    } else {
        // 强制轮询，关闭 WebSocket
        [self log:@"WebSocket not needed for polling transport" level:RTCLogLevelDebug];
        [socket disconnect];
    }
}

- (void)websocketDidDisconnect:(RTC_WS_CLASS *)socket error:(NSError *)error {
    NSString *errorDescription = error ? error.localizedDescription : @"Disconnected";
    [self log:[NSString stringWithFormat:@"WebSocket disconnected: %@", errorDescription] level:RTCLogLevelWarning];
    
    // 取消探测超时
    [self cancelProbeTimeout];
    
    if (self.closed) {
        [self closeOutEngine:@"WebSocket closed"];
    } else {
        if (self.websocket) {
            // WebSocket连接断开
            self.websocket = NO;
            
            // 如果配置了只使用WebSocket传输，使用延迟重连
            if (self.config.transport == RTCVPSocketIOTransportWebSocket) {
                [self log:@"WebSocket transport configured, scheduling delayed reconnect..." level:RTCLogLevelInfo];
                // 使用延迟重连，避免频繁连接尝试
                [self delayReconnect];
            } else {
                // WebSocket 断开，尝试回退到轮询
                self.polling = YES;
                
                [self log:@"Falling back to polling" level:RTCLogLevelInfo];
                
                if (self.connected) {
                    [self doPoll];
                }
            }
        } else if (self.connected) {
            // 在探测期间断开，关闭连接
            [self closeOutEngine:errorDescription];
        } else {
            // 连接尚未建立，处理为连接失败
            [self log:@"WebSocket connection failed" level:RTCLogLevelError];
            if (!self.closed) {
                [self didError:errorDescription];
                // 尝试延迟重连
                [self delayReconnect];
            }
        }
    }
}

- (void)websocket:(RTC_WS_CLASS *)socket didReceiveMessage:(NSString *)string {
    [self parseEngineMessage:string];
}

// 在 websocket:didReceiveData: 方法中，添加协议修复
- (void)websocket:(RTC_WS_CLASS *)socket didReceiveData:(NSData *)data {
    if (data.length == 0) {
        [self log:@"WebSocket received empty binary data" level:RTCLogLevelWarning];
        return;
    }
    
    // 分析WebSocket帧
    NSDictionary *frameInfo = [RTCVPWebSocketProtocolFixer analyzeWebSocketFrame:data];
    [self log:[NSString stringWithFormat:@"WebSocket帧分析: %@", frameInfo] level:RTCLogLevelDebug];
    
    // RTC_WS_CLASS 已经正确解析了 WebSocket 帧
    // 我们收到的 data 已经是有效负载（去除了帧头、掩码等）
       
    [self log:[NSString stringWithFormat:@"📦 收到WebSocket二进制数据，长度: %lu", (unsigned long)data.length]
            level:RTCLogLevelInfo];
       
    // 直接传递给 parseEngineData
    [self parseEngineData:data];
}

// 添加处理WebSocket文本帧的方法
- (void)handleWebSocketTextFrame:(NSData *)data {
    // 解析WebSocket帧，提取有效负载
    NSData *payload = [self extractWebSocketPayload:data];
    
    if (payload) {
        NSString *message = [[NSString alloc] initWithData:payload encoding:NSUTF8StringEncoding];
        if (message) {
            [self log:[NSString stringWithFormat:@"WebSocket文本消息: %@", message] level:RTCLogLevelDebug];
            [self parseEngineMessage:message];
        } else {
            [self log:@"无法将WebSocket负载解析为文本" level:RTCLogLevelWarning];
        }
    }
}

// 提取WebSocket帧中的有效负载
- (NSData *)extractWebSocketPayload:(NSData *)frame {
    if (frame.length < 2) return nil;
    
    const uint8_t *bytes = (const uint8_t *)frame.bytes;
    
    // 跳过帧头
    NSUInteger headerLength = 2;
    uint8_t payloadLenByte = bytes[1] & 0x7F;
    
    // 处理扩展长度
    if (payloadLenByte == 126) {
        headerLength += 2;
    } else if (payloadLenByte == 127) {
        headerLength += 8;
    }
    
    // 处理掩码
    BOOL masked = (bytes[1] & 0x80) != 0;
    if (masked) {
        headerLength += 4;
    }
    
    // 检查帧长度
    if (frame.length <= headerLength) {
        return nil;
    }
    
    // 提取负载
    NSData *payload = [frame subdataWithRange:NSMakeRange(headerLength, frame.length - headerLength)];
    
    // 如果被掩码，解码
    if (masked && payload.length > 0) {
        const uint8_t *maskKey = bytes + (headerLength - 4);
        NSMutableData *decodedData = [NSMutableData dataWithData:payload];
        uint8_t *decodedBytes = (uint8_t *)decodedData.mutableBytes;
        
        for (NSUInteger i = 0; i < payload.length; i++) {
            decodedBytes[i] = decodedBytes[i] ^ maskKey[i % 4];
        }
        
        return decodedData;
    }
    
    return payload;
}

// 处理WebSocket Ping
- (void)handleWebSocketPing:(NSData *)pingFrame {
    // 发送Pong响应
    [self sendWebSocketPong:pingFrame];
    
    
    // 同时重置Engine.IO心跳计数器
    [self handlePong:@"WebSocket Ping"];
}

// 发送WebSocket Pong
- (void)sendWebSocketPong:(NSData *)pingFrame {
    if (!self.ws || ![self.ws isConnected]) {
        return;
    }
    
    // 构建Pong帧：操作码0xA，负载与Ping相同
    NSData *payload = [self extractWebSocketPayload:pingFrame];
    
    // 创建Pong帧
    NSMutableData *pongFrame = [NSMutableData data];
    
    // 第一个字节：FIN=1，RSV=0，操作码=0xA
    uint8_t firstByte = 0x80 | 0xA; // FIN=1, Opcode=0xA
    [pongFrame appendBytes:&firstByte length:1];
    
    // 第二个字节：掩码=0，负载长度
    uint64_t payloadLength = payload ? payload.length : 0;
    
    if (payloadLength <= 125) {
        uint8_t secondByte = (uint8_t)payloadLength;
        [pongFrame appendBytes:&secondByte length:1];
    } else if (payloadLength <= 65535) {
        uint8_t secondByte = 126;
        [pongFrame appendBytes:&secondByte length:1];
        
        uint16_t len16 = CFSwapInt16HostToBig((uint16_t)payloadLength);
        [pongFrame appendBytes:&len16 length:2];
    } else {
        uint8_t secondByte = 127;
        [pongFrame appendBytes:&secondByte length:1];
        
        uint64_t len64 = CFSwapInt64HostToBig(payloadLength);
        [pongFrame appendBytes:&len64 length:8];
    }
    
    // 添加负载
    if (payload) {
        [pongFrame appendData:payload];
    }
    
    [self.ws writeData:pongFrame];
    [self log:@"发送WebSocket Pong响应" level:RTCLogLevelDebug];
}

// 处理WebSocket Pong
- (void)handleWebSocketPong:(NSData *)pongFrame {
    // 重置心跳计数器
    [self handlePong:@"WebSocket Pong"];
}

// 处理WebSocket关闭帧
- (void)handleWebSocketClose:(NSData *)closeFrame {
    uint16_t closeCode = 1000; // 默认正常关闭
    
    if (closeFrame.length >= 4) {
        const uint8_t *bytes = (const uint8_t *)closeFrame.bytes;
        
        // 跳过帧头，提取关闭代码
        NSUInteger offset = 2; // 基本头
        uint8_t payloadLenByte = bytes[1] & 0x7F;
        
        if (payloadLenByte == 126) {
            offset += 2;
        } else if (payloadLenByte == 127) {
            offset += 8;
        }
        
        if ((bytes[1] & 0x80) != 0) { // 如果有掩码
            offset += 4;
        }
        
        if (closeFrame.length >= offset + 2) {
            closeCode = (bytes[offset] << 8) | bytes[offset + 1];
        }
    }
    
    NSString *reason = [NSString stringWithFormat:@"WebSocket关闭 (代码: %d)", closeCode];
    [self log:reason level:RTCLogLevelInfo];
    
    // 如果未主动关闭，尝试重连
    if (!self.closed) {
//        [self handleConnectionError:reason];
    }
}



@end
