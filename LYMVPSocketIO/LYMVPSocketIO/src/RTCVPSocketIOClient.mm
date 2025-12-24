//
//  RTCVPSocketIOClient.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketIOClient.h"
#import "RTCVPSocketEngine.h"
#include "sio_packet_impl.h"
#include "sio_ack_manager.h"
#include "sio_jsoncpp_binary_helper.hpp"
#import "RTCDefaultSocketLogger.h"
#import "RTCVPStringReader.h"
#import "NSString+RTCVPSocketIO.h"
#import "RTCVPAFNetworkReachabilityManager.h"
#import "RTCVPTimer.h"
//#import "RTCVPSocketIOConfig.h"

#include "api/task_queue/default_task_queue_factory.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/time_utils.h"

#pragma mark - 常量定义

NSString *const RTCVPSocketEventConnect = @"connect";
NSString *const RTCVPSocketEventDisconnect = @"disconnect";
NSString *const RTCVPSocketEventError = @"error";
NSString *const RTCVPSocketEventReconnect = @"reconnect";
NSString *const RTCVPSocketEventReconnectAttempt = @"reconnectAttempt";
NSString *const RTCVPSocketEventStatusChange = @"statusChange";

// ACK发射器常量定义
NSString *const kRTCVPSocketAckEmitterErrorDomain = @"RTCVPSocketAckEmitterErrorDomain";
NSInteger const kRTCVPSocketAckEmitterErrorSendFailed = 1;

NSString *const RTCVPSocketStatusNotConnected = @"notconnected";
NSString *const RTCVPSocketStatusDisconnected = @"disconnected";
NSString *const RTCVPSocketStatusConnecting = @"connecting";
NSString *const RTCVPSocketStatusOpened = @"opened";
NSString *const RTCVPSocketStatusConnected = @"connected";


// 辅助函数：递归转换Json::Value到OC对象
static id convertJsonValueToObjC(const Json::Value& jsonValue) {
    if (jsonValue.isNull()) {
        return [NSNull null];
    } else if (jsonValue.isBool()) {
        return [NSNumber numberWithBool:jsonValue.asBool()];
    } else if (jsonValue.isInt() || jsonValue.isUInt() || jsonValue.isInt64() || jsonValue.isUInt64()) {
        return [NSNumber numberWithLongLong:jsonValue.asInt64()];
    } else if (jsonValue.isDouble()) {
        return [NSNumber numberWithDouble:jsonValue.asDouble()];
    } else if (jsonValue.isString()) {
        return [NSString stringWithUTF8String:jsonValue.asCString()];
    } else if (jsonValue.isArray()) {
        NSMutableArray *array = [[NSMutableArray alloc] init];
        for (Json::ArrayIndex i = 0; i < jsonValue.size(); i++) {
            [array addObject:convertJsonValueToObjC(jsonValue[i])];
        }
        return array;
    } else if (jsonValue.isObject()) {
        NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
        Json::Value::Members members = jsonValue.getMemberNames();
        for (const auto& key : members) {
            NSString *ocKey = [NSString stringWithUTF8String:key.c_str()];
            dict[ocKey] = convertJsonValueToObjC(jsonValue[key]);
        }
        return dict;
    }
    
    // 未知类型，返回null
    return [NSNull null];
}

#pragma mark - 事件处理器类

@interface RTCVPSocketEventHandler : NSObject
@property (nonatomic, strong) NSString *event;
@property (nonatomic, strong) NSUUID *uuid;
@property (nonatomic, copy) RTCVPSocketOnEventCallback callback;

- (instancetype)initWithEvent:(NSString *)event uuid:(NSUUID *)uuid andCallback:(RTCVPSocketOnEventCallback)callback;
- (void)executeCallbackWith:(NSArray *)data withAck:(NSInteger)ack withSocket:(id<RTCVPSocketIOClientProtocol>)socket withEmitter:(RTCVPSocketAckEmitter *)emitter;
@end

@implementation RTCVPSocketEventHandler

- (instancetype)initWithEvent:(NSString *)event uuid:(NSUUID *)uuid andCallback:(RTCVPSocketOnEventCallback)callback {
    self = [super init];
    if (self) {
        _event = [event copy];
        _uuid = uuid;
        _callback = [callback copy];
    }
    return self;
}

- (void)executeCallbackWith:(NSArray *)data withAck:(NSInteger)ack withSocket:(id<RTCVPSocketIOClientProtocol>)socket withEmitter:(RTCVPSocketAckEmitter *)emitter {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.callback) {
            self.callback(data, emitter);
        }
    });
    
}

@end

#pragma mark - ACK发射器类

@implementation RTCVPSocketAckEmitter

- (instancetype)initWithAckId:(NSInteger)ackId emitBlock:(void (^_Nullable)(NSArray *_Nullable items))emitBlock {
    self = [super init];
    if (self) {
        _ackId = ackId;
        _emitBlock = [emitBlock copy];
    }
    return self;
}

- (void)send:(NSArray *_Nullable)items {
    if (self.emitBlock) {
        self.emitBlock(items);
    }
}

@end

#pragma mark - 全局事件类

@interface RTCVPSocketAnyEvent : NSObject
@property (nonatomic, strong) NSString *event;
@property (nonatomic, strong) NSArray *items;

- (instancetype)initWithEvent:(NSString *)event andItems:(NSArray *)items;
@end

@implementation RTCVPSocketAnyEvent

- (instancetype)initWithEvent:(NSString *)event andItems:(NSArray *)items {
    self = [super init];
    if (self) {
        _event = [event copy];
        _items = [items copy];
    }
    return self;
}

@end

#pragma mark - 缓存数据模型

@interface RTCVPSocketIOClientCacheData : NSObject
@property (nonatomic, assign) int ack;
@property (nonatomic, strong) NSArray *items;
@property (nonatomic, assign) BOOL isEvent;
@end

@implementation RTCVPSocketIOClientCacheData
@end

#pragma mark - 客户端私有接口

@interface RTCVPSocketIOClient() <RTCVPSocketEngineClient> {
    BOOL _reconnecting;
    NSInteger _currentAck;
    RTCVPSocketAnyEventHandler _anyHandler;
    std::unique_ptr<webrtc::TaskQueueFactory> taskQueueFactory_;
    std::unique_ptr<rtc::TaskQueue> ioClientQueue_;
    webrtc::RepeatingTaskHandle repHanler_;
    std::unique_ptr<sio::PacketSender> pack_sender;
    std::unique_ptr<sio::PacketReceiver> pack_receiver;
    std::shared_ptr<sio::SioAckManager> ack_manager_;
}

@property (nonatomic, strong) NSString *logType;
@property (nonatomic, strong) RTCVPSocketEngine *engine;
@property (nonatomic, strong) NSMutableArray<RTCVPSocketEventHandler *> *handlers;
@property (nonatomic, strong) RTCVPAFNetworkReachabilityManager *networkManager;
@property (nonatomic, assign) RTCVPAFNetworkReachabilityStatus currentNetworkStatus;
@property (nonatomic, strong) NSMutableArray<RTCVPSocketIOClientCacheData *> *dataCache;
@property (nonatomic, assign) NSInteger currentReconnectAttempt;

// 事件映射字典
@property (nonatomic, strong, readonly) NSDictionary *eventMap;
// 状态映射字典
@property (nonatomic, strong, readonly) NSDictionary *statusMap;

@property (nonatomic, strong) NSMutableArray *waitingPackets;
@property (nonatomic, strong) id packetAdapter;

@property (nonatomic, strong) NSString* _Nullable nsp;


@end

#pragma mark - 客户端实现

@implementation RTCVPSocketIOClient

#pragma mark - 生命周期

+ (instancetype)clientWithSocketURL:(NSURL *)socketURL config:(RTCVPSocketIOConfig *)config {
    return [[self alloc] initWithSocketURL:socketURL config:config];
}

- (instancetype)initWithSocketURL:(NSURL *)socketURL config:(RTCVPSocketIOConfig *)config {
    self = [super init];
    if (self) {
        [self setDefaultValues];
        _socketURL = socketURL;
        _config = config ?: [RTCVPSocketIOConfig defaultConfig];
        
        // 设置日志
        if (self.config.logger) {
            [RTCDefaultSocketLogger setCoustomLogger:self.config.logger];
        }
        [RTCDefaultSocketLogger setEnabled:self.config.loggingEnabled];
        [RTCDefaultSocketLogger setLogLevel:(RTCLogLevel)self.config.logLevel];
        
        // 配置重连参数
        _reconnects = self.config.reconnectionEnabled;
        _reconnectAttempts = self.config.reconnectionAttempts;
        _reconnectWait = self.config.reconnectionDelay;
        _nsp = self.config.nsp ?: @"/";
        
        // 设置处理队列
        _handleQueue = dispatch_get_main_queue();
        if (self.config.handleQueue) {
            _handleQueue = self.config.handleQueue;
        }
        
        // 设置命名空间
        if (self.config.nsp) {
            _nsp = self.config.nsp;
        }
        
        // 启动网络监控
        if (self.config.enableNetworkMonitoring) {
            [self startNetworkMonitoring];
        }
        
        // 创建任务队列工厂和文件写入专用队列
        taskQueueFactory_ = webrtc::CreateDefaultTaskQueueFactory();
        ioClientQueue_ = absl::make_unique<rtc::TaskQueue>(
        taskQueueFactory_->CreateTaskQueue(
                      "timerCount", webrtc::TaskQueueFactory::Priority::NORMAL));
//        repHanler_ =  webrtc::RepeatingTaskHandle::Start(ioClientQueue_->Get(), [=]() {
//            NSLog(@"====> ");
//              return webrtc::TimeDelta::ms(1000);
//        });
        
        sio::SocketIOVersion versions = sio::SocketIOVersion::V3;
        
        
        ack_manager_ = sio::SioAckManager::Create(taskQueueFactory_.get());
        // 初始化PacketSender和PacketReceiver，移除模板参数，使用正确的构造函数
        sio::PacketSender::Config sender_config;
        sender_config.version = versions;
        // 将创建的ack_manager_传递给PacketSender
        pack_sender = absl::make_unique<sio::PacketSender>(ack_manager_, taskQueueFactory_.get(), sender_config);
        
        sio::PacketReceiver::Config receiver_config;
        receiver_config.default_version = versions;
        // 将创建的ack_manager_传递给PacketReceiver
        pack_receiver = absl::make_unique<sio::PacketReceiver>(ack_manager_, taskQueueFactory_.get(), receiver_config);
        
        // 设置事件回调函数，将收到的事件推送给上层
        __weak __typeof(self) weakSelf = self;
        pack_receiver->set_event_callback([weakSelf](const sio::SioPacket &packet) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            if (strongSelf) {
                // 将Json::Value数组转换为OC数组
                NSMutableArray *ocArgs = [[NSMutableArray alloc] init];
                for (const auto& jsonValue : packet.args) {
                    [ocArgs addObject:convertJsonValueToObjC(jsonValue)];
                }
                
                if (packet.type == sio::PacketType::CONNECT) {
                    [strongSelf handleConnect:@"/"];
                }else{
                    // 调用上层事件处理器
                    NSString *ocEvent = [NSString stringWithUTF8String:packet.event_name.c_str()];
                    [strongSelf handleEvent:ocEvent withData:ocArgs isInternalMessage:NO withAck:packet.ack_id];
                }
                
                
            }
        });

        
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Client initialized with URL: %@", socketURL.absoluteString]
                                      type:self.logType];
    }
    return self;
}

- (instancetype)initWithSocketURL:(NSURL *)socketURL configDictionary:(NSDictionary *)configDictionary {
    RTCVPSocketIOConfig *config = [[RTCVPSocketIOConfig alloc] initWithDictionary:configDictionary];
    return [self initWithSocketURL:socketURL config:config];
}

- (void)dealloc {
    [RTCDefaultSocketLogger.logger log:@"Client is being released" type:self.logType];
    [self.engine disconnect:@"Client Deinit"];
    [self stopNetworkMonitoring];
//    [self.ackHandlers removeAllPackets];
}

#pragma mark - 初始化配置

- (void)setDefaultValues {
    _status = RTCVPSocketIOClientStatusNotConnected;
    _forceNew = NO;
    _handleQueue = dispatch_get_main_queue();
    _nsp = @"/";
    _reconnects = YES;
    _reconnectWait = 10;
    _reconnectAttempts = -1;
    _currentReconnectAttempt = 0;
    _reconnecting = NO;
    _currentAck = -1;
    
    // 初始化属性
    _handlers = [[NSMutableArray alloc] init];
    _dataCache = [[NSMutableArray alloc] init];
    self.waitingPackets = [[NSMutableArray alloc] init];
    self.packetAdapter = nil; // 暂时设为nil，后续可以根据需要初始化
}

#pragma mark - 映射字典懒加载

- (NSDictionary *)eventMap {
    static NSDictionary *_eventMap = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        _eventMap = @{
            @(RTCVPSocketClientEventConnect): RTCVPSocketEventConnect,
            @(RTCVPSocketClientEventDisconnect): RTCVPSocketEventDisconnect,
            @(RTCVPSocketClientEventError): RTCVPSocketEventError,
            @(RTCVPSocketClientEventReconnect): RTCVPSocketEventReconnect,
            @(RTCVPSocketClientEventReconnectAttempt): RTCVPSocketEventReconnectAttempt,
            @(RTCVPSocketClientEventStatusChange): RTCVPSocketEventStatusChange
        };
    });
    return _eventMap;
}

- (NSDictionary *)statusMap {
    static NSDictionary *_statusMap = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        _statusMap = @{
            @(RTCVPSocketIOClientStatusNotConnected): RTCVPSocketStatusNotConnected,
            @(RTCVPSocketIOClientStatusDisconnected): RTCVPSocketStatusDisconnected,
            @(RTCVPSocketIOClientStatusConnecting): RTCVPSocketStatusConnecting,
            @(RTCVPSocketIOClientStatusOpened): RTCVPSocketStatusOpened,
            @(RTCVPSocketIOClientStatusConnected): RTCVPSocketStatusConnected
        };
    });
    return _statusMap;
}

#pragma mark - 属性

- (void)setStatus:(RTCVPSocketIOClientStatus)status {
    if (_status != status) {
        _status = status;
        
        switch (status) {
            case RTCVPSocketIOClientStatusConnected:
                _reconnecting = NO;
                _currentReconnectAttempt = 0;
                break;
            default:
                break;
        }
        
        NSString *statusString = self.statusMap[@(status)];
        if (statusString) {
            [self handleClientEvent:RTCVPSocketEventStatusChange withData:@[statusString]];
        }
    }
}

- (NSString *)logType {
    return @"RTCVPSocketIOClient";
}

#pragma mark - 工具方法

- (NSString *)eventStringForEvent:(RTCVPSocketClientEvent)event {
    return self.eventMap[@(event)];
}

- (NSString *)statusStringForStatus:(RTCVPSocketIOClientStatus)status {
    return self.statusMap[@(status)];
}

#pragma mark - 连接管理

- (void)connect {
    [self connectWithTimeoutAfter:0 withHandler:^{
        // 默认空处理器
    }];
}

- (void)connectWithTimeoutAfter:(NSTimeInterval)timeout withHandler:(RTCVPSocketIOVoidHandler)handler {
    if (_status != RTCVPSocketIOClientStatusConnected) {
        self.status = RTCVPSocketIOClientStatusConnecting;
        
        // 保留原有引擎连接逻辑，用于向后兼容
        if (self.engine == nil || self.forceNew) {
            [self addEngine];
        }
        
        [self.engine connect];
        
        if (timeout > 0) {
            __weak __typeof(self) weakSelf = self;
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(timeout * NSEC_PER_SEC)),
                          self.handleQueue, ^{
                __strong __typeof(weakSelf) strongSelf = weakSelf;
                if (strongSelf &&
                    (strongSelf.status == RTCVPSocketIOClientStatusConnecting ||
                     strongSelf.status == RTCVPSocketIOClientStatusNotConnected)) {
                    [strongSelf didDisconnect:@"Connection timeout"];
                    if (handler) {
                        handler();
                    }
                }
            });
        }
    } else {
        [RTCDefaultSocketLogger.logger log:@"Tried connecting on an already connected socket"
                                      type:self.logType];
    }
}

- (void)disconnect {
    [RTCDefaultSocketLogger.logger log:@"Closing socket" type:self.logType];
    _reconnects = NO;
    
    [self didDisconnect:@"Disconnect"];
    
    // 停止旧的重复任务（如果还在运行）
    if (repHanler_.Running()) {
        repHanler_.Stop();
    }
    if (ack_manager_) {
        ack_manager_->clear_all_acks();
        ack_manager_->stop();
    }
    if (pack_sender) {
        pack_sender->reset();
    }
    if (pack_receiver) {
        pack_receiver->reset();
    }
    
}

- (void)disconnectWithHandler:(RTCVPSocketIOVoidHandler)handler {
    [self disconnect];
    if (handler) {
        handler();
    }
}

- (void)reconnect {
    if (!_reconnecting) {
        [self tryReconnect:@"manual reconnect"];
    }
}

#pragma mark - 私有方法

- (void)addEngine {
    [RTCDefaultSocketLogger.logger log:@"Adding engine" type:self.logType];
    
    if (self.engine) {
        [self.engine syncResetClient];
        self.engine = nil;
    }
    
    if (!self.socketURL) {
        [RTCDefaultSocketLogger.logger error:@"Socket URL is nil" type:self.logType];
        return;
    }
    
    if (!self.config) {
        [RTCDefaultSocketLogger.logger error:@"Config is nil" type:self.logType];
        return;
    }
    
    // 使用新的配置类创建引擎
    self.engine = [RTCVPSocketEngine engineWithClient:self
                                                 url:self.socketURL
                                              config:self.config];
    
    if (!self.engine) {
        [RTCDefaultSocketLogger.logger error:@"Failed to create engine" type:self.logType];
    }
}

#pragma mark - 断开连接处理

- (void)didDisconnect:(NSString *)reason {
    if (_status != RTCVPSocketIOClientStatusDisconnected) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"断开连接: %@", reason] type:self.logType];
        
        _reconnecting = NO;
        self.status = RTCVPSocketIOClientStatusDisconnected;
        
        // 清理所有ACK包
//        [self.ackHandlers removeAllPackets];
        
        // 确保引擎关闭
        [self.engine disconnect:reason];
        [self handleClientEvent:RTCVPSocketEventDisconnect withData:@[reason]];
    }
}

#pragma mark - 工具方法
- (void)printACKStatistics {
//    NSInteger activeCount = [self.ackHandlers activePacketCount];
//    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"活跃包数量: %ld", (long)activeCount]
//                                  type:self.logType];
//    
//    NSArray<NSNumber *> *allPacketIds = [self.ackHandlers allPacketIds];
//    if (allPacketIds.count > 0) {
//        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"活跃包ID: %@", allPacketIds]
//                                      type:self.logType];
//    }
}


#pragma mark - 事件发射

- (void)emit:(NSString *)event {
    [self emit:event items:nil];
}

- (void)emit:(NSString *)event withArgs:(id)arg1, ... {
    NSMutableArray *items = [NSMutableArray array];
    
    va_list args;
    va_start(args, arg1);
    
    if (arg1) {
        [items addObject:arg1];
        
        id arg;
        while ((arg = va_arg(args, id)) != nil) {
            [items addObject:arg];
        }
    }
    
    va_end(args);
    
    [self emit:event items:items];
}

- (void)emit:(NSString *)event items:(NSArray *)items {
    [self emit:event items:items ack:-1];
}

// OC对象转换为Json::Value的辅助函数
Json::Value convertOCObjectToJsonValue(id obj) {
    if (obj == nil || obj == [NSNull null]) {
        return Json::Value::null;
    }
    
    if ([obj isKindOfClass:[NSString class]]) {
        return Json::Value([(NSString*)obj UTF8String]);
    }
    
    if ([obj isKindOfClass:[NSNumber class]]) {
        NSNumber* num = (NSNumber*)obj;
        if (strcmp(num.objCType, @encode(BOOL)) == 0) {
            return Json::Value([num boolValue]);
        } else if (strcmp(num.objCType, @encode(int)) == 0) {
            return Json::Value([num intValue]);
        } else if (strcmp(num.objCType, @encode(long)) == 0) {
            return Json::Value(static_cast<Json::Int64>([num longValue]));
        } else if (strcmp(num.objCType, @encode(float)) == 0) {
            return Json::Value([num floatValue]);
        } else if (strcmp(num.objCType, @encode(double)) == 0) {
            return Json::Value([num doubleValue]);
        } else {
            return Json::Value([num doubleValue]); // 默认转换为double
        }
    }
    
    if ([obj isKindOfClass:[NSArray class]]) {
        NSArray* array = (NSArray*)obj;
        Json::Value jsonArray(Json::arrayValue);
        for (id item in array) {
            jsonArray.append(convertOCObjectToJsonValue(item));
        }
        return jsonArray;
    }
    
    if ([obj isKindOfClass:[NSDictionary class]]) {
        NSDictionary* dict = (NSDictionary*)obj;
        Json::Value jsonObject(Json::objectValue);
        for (NSString* key in dict) {
            id value = dict[key];
            jsonObject[std::string([key UTF8String])] = convertOCObjectToJsonValue(value);
        }
        return jsonObject;
    }
    
    if ([obj isKindOfClass:[NSData class]]) {
        // 二进制数据处理
        NSData* data = (NSData*)obj;
        Json::Value binary_json = sio::binary_helper::create_binary_value((const uint8_t*)data.bytes, data.length);
        return binary_json;
    }
    
    // 其他类型默认转换为字符串
    return Json::Value([NSString stringWithFormat:@"%@", obj].UTF8String);
}

- (void)emit:(NSString *)event items:(NSArray *)items ack:(int)ack {
    if (!event || event.length == 0) {
        [RTCDefaultSocketLogger.logger error:@"事件名不能为空" type:self.logType];
        return;
    }
    
    // 将OC数组转换为C++ Json::Value数组
    std::vector<Json::Value> data_array;
    if (items && items.count > 0) {
        for (id item in items) {
            data_array.push_back(convertOCObjectToJsonValue(item));
        }
    }
    
    std::string namespace_s = "/"; // 默认命名空间
    
    if (ack > 0) {
        // 如果需要ACK，使用send_event_with_ack方法
        pack_sender->send_event_with_ack(event.UTF8String, data_array, 
            [=](const std::string& text_packet){
                // 发送文本包
                if (self.engine) {
                    [self.engine send:[NSString stringWithUTF8String:text_packet.c_str()] withData:@[]];
                }
                return true;
            },
            [=](const sio::SmartBuffer& binary_data, int index){                // 发送二进制数据
                if (self.engine) {
                    // 将SmartBuffer转换为NSData，使用sendRawData发送原始二进制数据
                    NSData *data = [NSData dataWithBytes:binary_data.data() length:binary_data.size()];
                    [self.engine send:nil withData:@[data]];
                }
                return true;
            },
            [=](const std::vector<Json::Value>& result_data){
                // ACK回调
                [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"收到ACK响应: %@", @(ack)] type:self.logType];
            },
            [=](int ack_id){
                // 超时回调
                [RTCDefaultSocketLogger.logger error:[NSString stringWithFormat:@"ACK超时: %@", @(ack_id)] type:self.logType];
            },
            std::chrono::milliseconds(30000),namespace_s);
    } else {
        // 如果不需要ACK，使用send_event方法
        pack_sender->send_event(event.UTF8String, data_array, 
            [=](const std::string& text_packet){
                // 发送文本包
                if (self.engine) {
                    [self.engine send:[NSString stringWithUTF8String:text_packet.c_str()] withData:@[]];
                }
                return true;
            },
            [=](const sio::SmartBuffer& binary_data, int index){                // 发送二进制数据
                if (self.engine) {
                    // 将SmartBuffer转换为NSData，使用sendRawData发送原始二进制数据
                    NSData *data = [NSData dataWithBytes:binary_data.data() length:binary_data.size()];
                    [self.engine send:nil withData:@[data]];
                }
                return true;
            },
            [=](bool success, const std::string& error){
                // 发送结果回调
                if (!success) {
                    [RTCDefaultSocketLogger.logger error:[NSString stringWithFormat:@"发送失败: %s", error.c_str()] type:self.logType];
                }
            },namespace_s );
    }
//    webrtc::TimeDelta::ms(1000).ms_or(1000)
   //ios 的数据 转 std::vector<T>& data_array
//    std::vector<Json::Value> data_array ;
//    pack_sender->prepare_data_array_async();

}

- (void)emitWithAck:(NSString *)event
              items:(NSArray *)items
           ackBlock:(void(^)(NSArray * _Nullable data, NSError * _Nullable error))ackBlock {
    [self emitWithAck:event items:items ackBlock:ackBlock timeout:10.0];
}

- (void)emitWithAck:(NSString *)event
              items:(NSArray *)items
           ackBlock:(void(^)(NSArray * _Nullable data, NSError * _Nullable error))ackBlock
            timeout:(NSTimeInterval)timeout {
    
    if (!event) {
        if (ackBlock) {
            NSError *error = [NSError errorWithDomain:@"RTCVPSocketIOErrorDomain"
                                                 code:-1
                                             userInfo:@{NSLocalizedDescriptionKey: @"事件名不能为空"}];
            dispatch_async(self.handleQueue, ^{
                ackBlock(nil, error);
            });
        }
        return;
    }
    
    if (_status != RTCVPSocketIOClientStatusConnected) {
        if (ackBlock) {
            NSError *error = [NSError errorWithDomain:@"RTCVPSocketIOErrorDomain"
                                                 code:-2
                                             userInfo:@{NSLocalizedDescriptionKey: @"Socket未连接"}];
            dispatch_async(self.handleQueue, ^{
                ackBlock(nil, error);
            });
        }
        return;
    }

    // 将OC数组转换为C++ Json::Value数组
    std::vector<Json::Value> data_array;
    if (items && items.count > 0) {
        for (id item in items) {
            data_array.push_back(convertOCObjectToJsonValue(item));
        }
    }
    std::string namespace_s = "/"; // 默认命名空间
    
    // 使用send_event_with_ack方法发送带ACK的事件
    pack_sender->send_event_with_ack(event.UTF8String, data_array, 
        [=](const std::string& text_packet){
            // 发送文本包
            if (self.engine) {
                [self.engine send:[NSString stringWithUTF8String:text_packet.c_str()] withData:@[]];
            }
            return true;
        },
        [=](const sio::SmartBuffer& binary_data, int index){
            // 发送二进制数据
            if (self.engine) {
                NSData *data = [NSData dataWithBytes:binary_data.data() length:binary_data.size()];
                [self.engine send:nil withData:@[data]];
            }
            return true;
        },
        [=](const std::vector<Json::Value>& result_data){
            // ACK回调 - 完成C++到OC的回调
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"收到ACK响应"] type:self.logType];
            
            // 将C++ Json::Value数组转换为OC NSArray
            NSMutableArray *ocResultData = [NSMutableArray array];
            for (const auto& jsonValue : result_data) {
                [ocResultData addObject:convertJsonValueToObjC(jsonValue)];
            }
            
            // 调用ACK回调
            if (ackBlock) {
                dispatch_async(self.handleQueue, ^{ 
                    ackBlock(ocResultData, nil);
                });
            }
        },
        [=](int timeout_ack_id){
            // 超时回调 - 完成C++到OC的回调
            [RTCDefaultSocketLogger.logger error:[NSString stringWithFormat:@"ACK超时: %@", @(timeout_ack_id)] type:self.logType];
            
            // 创建超时错误对象
            NSError *error = [NSError errorWithDomain:@"RTCVPSocketIOErrorDomain"
                                                 code:-3
                                             userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"ACK超时 (ID: %d)", timeout_ack_id]}];
            
            // 调用ACK回调，传递错误
            if (ackBlock) {
                dispatch_async(self.handleQueue, ^{ 
                    ackBlock(nil, error);
                });
            }
        },
        std::chrono::milliseconds((int)(timeout * 1000)),
                                     namespace_s);
    
//    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"发送带ACK的事件: %@ (ackId: %@)", str, @(ackId)]
//                                  type:self.logType];
//    
//    // 发送消息
//    [self.engine send:str withData:packet.binary];
}

#pragma mark - 处理ACK响应

- (void)handleAck:(NSInteger)ack withData:(NSArray *)data {
    if (_status == RTCVPSocketIOClientStatusConnected) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"处理ACK响应: %@, 数据: %@", @(ack), data]
                                      type:self.logType];
        
        // 使用PacketReceiver处理ACK响应（简化实现）
    }
}

#pragma mark - 发送ACK响应

- (void)sendAck:(NSInteger)ackId withData:(NSArray *)data {
    if (_status != RTCVPSocketIOClientStatusConnected) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"无法发送ACK %@，Socket未连接", @(ackId)] type:self.logType];
        return;
    }
    
    // 使用新的PacketSender发送ACK响应
    std::vector<Json::Value> data_array;
    pack_sender->send_ack_response((int)ackId, data_array, [=](const std::string& text_packet) {
        // 发送文本包
        if (self.engine) {
            [self.engine send:[NSString stringWithUTF8String:text_packet.c_str()] withData:@[]];
        }
        return true;
    }, [=](const sio::SmartBuffer& binary_data, int index) {
        // 发送二进制数据
        if (self.engine) {
            // TODO: 实现二进制数据发送
        }
        return true;
    }, "/");
}

#pragma mark - RTCVPSocketIOClientProtocol

- (void)handleEvent:(NSString *)event
           withData:(NSArray *)data
  isInternalMessage:(BOOL)internalMessage {
    [self handleEvent:event withData:data isInternalMessage:internalMessage withAck:-1];
}

- (void)handleClientEvent:(NSString *)event withData:(NSArray *)data {
    [self handleEvent:event withData:data isInternalMessage:YES];
}

- (void)handleEvent:(NSString *)event
           withData:(NSArray *)data
  isInternalMessage:(BOOL)internalMessage
            withAck:(NSInteger)ack {
    
    if (_status == RTCVPSocketIOClientStatusConnected || internalMessage) {
        if ([event isEqualToString:RTCVPSocketEventError]) {
            [RTCDefaultSocketLogger.logger error:[NSString stringWithFormat:@"Socket error: %@", data.firstObject]
                                            type:self.logType];
        } else {
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"处理事件: %@, 数据: %@, ack: %@",
                                                event, data, @(ack)]
                                          type:self.logType];
        }
        
        // 调用全局事件处理器
        if (_anyHandler) {
            _anyHandler([[RTCVPSocketAnyEvent alloc] initWithEvent:event andItems:data]);
        }
        
        // 复制处理程序数组以避免在遍历时修改
        NSArray<RTCVPSocketEventHandler *> *handlersCopy = [NSArray arrayWithArray:self.handlers];
        
        // 查找并执行匹配的事件处理器
        for (RTCVPSocketEventHandler *handler in handlersCopy) {
            if ([handler.event isEqualToString:event]) {
                // 创建ACK发射器（如果需要ACK）
                RTCVPSocketAckEmitter *emitter = nil;
                if (ack >= 0) {
                    __weak __typeof(self) weakSelf = self;
                    emitter = [[RTCVPSocketAckEmitter alloc] initWithAckId:ack emitBlock:^(NSArray *items) {
                        __strong __typeof(weakSelf) strongSelf = weakSelf;
                        [strongSelf sendAck:ack withData:items];
                    }];
                }
                
                // 执行事件处理器
                [handler executeCallbackWith:data withAck:ack withSocket:self withEmitter:emitter];
            }
        }
    } else if (!internalMessage) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"忽略未连接时的事件: %@", event]
                                      type:self.logType];
    }
}

#pragma mark - 命名空间管理

- (void)leaveNamespace {
    if (![self.nsp isEqualToString:@"/"]) {
        // 使用新的引擎接口发送离开命名空间消息
        if (self.engine) {
            [self.engine send:[NSString stringWithFormat:@"1%@", self.nsp] withData:@[]];
            _nsp = @"/";
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Left namespace, now in: %@", self.nsp] type:self.logType];
        } else {
            [RTCDefaultSocketLogger.logger error:@"Cannot leave namespace, engine is nil" type:self.logType];
        }
    }
}

- (void)joinNamespace:(NSString *)nsp {
    if (!nsp || nsp.length == 0) {
        [RTCDefaultSocketLogger.logger error:@"Namespace is empty or nil" type:self.logType];
        return;
    }
    
    self.nsp = nsp;
    if (![self.nsp isEqualToString:@"/"]) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Joining namespace: %@", self.nsp] type:self.logType];
        // 使用新的引擎接口发送加入命名空间消息
        if (self.engine) {
            [self.engine send:[NSString stringWithFormat:@"0%@", self.nsp] withData:@[]];
        } else {
            [RTCDefaultSocketLogger.logger error:@"Cannot join namespace, engine is nil" type:self.logType];
        }
    }
}

#pragma mark - 事件监听

- (NSUUID *)on:(NSString *)event callback:(RTCVPSocketOnEventCallback)callback {
    if (!event || event.length == 0) {
        [RTCDefaultSocketLogger.logger error:@"Event name cannot be empty or nil" type:self.logType];
        return [NSUUID UUID];
    }
    
    if (!callback) {
        [RTCDefaultSocketLogger.logger error:@"Callback cannot be nil" type:self.logType];
        return [NSUUID UUID];
    }
    
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Adding handler for event: %@", event]
                                  type:self.logType];
    
    RTCVPSocketEventHandler *handler = [[RTCVPSocketEventHandler alloc] initWithEvent:event
                                                                                 uuid:[NSUUID UUID]
                                                                          andCallback:callback];
    [self.handlers addObject:handler];
    return handler.uuid;
}

- (NSUUID *)once:(NSString *)event callback:(RTCVPSocketOnEventCallback)callback {
    if (!event || event.length == 0) {
        [RTCDefaultSocketLogger.logger error:@"Event name cannot be empty or nil" type:self.logType];
        return [NSUUID UUID];
    }
    
    if (!callback) {
        [RTCDefaultSocketLogger.logger error:@"Callback cannot be nil" type:self.logType];
        return [NSUUID UUID];
    }
    
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Adding once handler for event: %@", event]
                                  type:self.logType];
    
    NSUUID *uuid = [NSUUID UUID];
    
    __weak __typeof(self) weakSelf = self;
    RTCVPSocketEventHandler *handler = [[RTCVPSocketEventHandler alloc] initWithEvent:event
                                                                                 uuid:uuid
                                                                          andCallback:^(NSArray *data, RTCVPSocketAckEmitter *emitter) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf offWithID:uuid];
            callback(data, emitter);
        }
    }];
    
    [self.handlers addObject:handler];
    return handler.uuid;
}

- (void)onAny:(RTCVPSocketAnyEventHandler)handler {
    _anyHandler = handler;
}

- (void)off:(NSString *)event {
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Removing handler for event: %@", event]
                                  type:self.logType];
    
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"SELF.event != %@", event];
    [self.handlers filterUsingPredicate:predicate];
}

- (void)offWithID:(NSUUID *)UUID {
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Removing handler with id: %@", UUID.UUIDString]
                                  type:self.logType];
    
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"SELF.uuid != %@", UUID];
    [self.handlers filterUsingPredicate:predicate];
}

- (void)removeAllHandlers {
    [self.handlers removeAllObjects];
    _anyHandler = nil;
}

#pragma mark - 重连管理

- (void)tryReconnect:(NSString *)reason {
    if (!_reconnecting) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Starting reconnect: %@", reason] type:self.logType];
        [self handleClientEvent:RTCVPSocketEventReconnect withData:@[reason]];
        _reconnecting = YES;
        self.currentReconnectAttempt = 0;
        [self _tryReconnect];
    }
}

- (void)_tryReconnect {
    if (self.reconnects && _reconnecting && _status != RTCVPSocketIOClientStatusDisconnected) {
        if (self.reconnectAttempts != -1 && self.currentReconnectAttempt >= self.reconnectAttempts) {
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Reconnect failed after %ld attempts", (long)self.currentReconnectAttempt] type:self.logType];
            return [self didDisconnect:@"Reconnect Failed"];
        } else {
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Trying to reconnect (attempt %ld/%ld)",
                                                (long)self.currentReconnectAttempt + 1,
                                                self.reconnectAttempts == -1 ? LONG_MAX : (long)self.reconnectAttempts]
                                      type:self.logType];
            
            [self handleClientEvent:RTCVPSocketEventReconnectAttempt
                           withData:@[@(self.currentReconnectAttempt + 1)]];
            
            self.currentReconnectAttempt += 1;
            [self connect];
            
            [self setReconnectTimer];
        }
    }
}

- (void)setReconnectTimer {
    __weak __typeof(self) weakSelf = self;
    
    // 指数退避策略：baseDelay * (2 ^ (attempt - 1))，但不超过最大值60秒
    NSTimeInterval delay = MIN(self.reconnectWait * pow(2, self.currentReconnectAttempt - 1), 60.0);
    
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Setting reconnect timer for %.1f seconds", delay] type:self.logType];
    
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)),
                  self.handleQueue, ^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            if (strongSelf.reconnects && strongSelf->_reconnecting) {
                if (strongSelf.status != RTCVPSocketIOClientStatusConnected) {
                    [strongSelf _tryReconnect];
                } else {
                    [RTCDefaultSocketLogger.logger log:@"Reconnect timer fired but already connected" type:self.logType];
                    strongSelf->_reconnecting = NO;
                }
            } else {
                [RTCDefaultSocketLogger.logger log:@"Reconnect timer fired but reconnect is disabled" type:self.logType];
            }
        }
    });
}

#pragma mark - 网络监控

- (void)startNetworkMonitoring {
    if (!self.networkManager) {
        self.networkManager = [RTCVPAFNetworkReachabilityManager sharedManager];
        [self.networkManager startMonitoring];
        self.currentNetworkStatus = RTCVPAFNetworkReachabilityStatusUnknown;
        
        __weak __typeof(self) weakSelf = self;
        [self.networkManager setReachabilityStatusChangeBlock:^(RTCVPAFNetworkReachabilityStatus status) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            [strongSelf handleNetworkStatusChange:status];
        }];
    }
}

- (void)stopNetworkMonitoring {
    if (self.networkManager) {
        [self.networkManager stopMonitoring];
        self.networkManager = nil;
    }
}

- (void)handleNetworkStatusChange:(RTCVPAFNetworkReachabilityStatus)status {
    if (self.currentNetworkStatus == RTCVPAFNetworkReachabilityStatusUnknown) {
        self.currentNetworkStatus = status;
        return;
    }
    
    switch (status) {
        case RTCVPAFNetworkReachabilityStatusUnknown:
        case RTCVPAFNetworkReachabilityStatusNotReachable: {
            [RTCDefaultSocketLogger.logger log:@"ERROR ==========No network===========" type:self.logType];
            [self.engine disconnect:@"No network or not reachable"];
            break;
        }
        case RTCVPAFNetworkReachabilityStatusReachableViaWWAN: {
            if (self.currentNetworkStatus == RTCVPAFNetworkReachabilityStatusReachableViaWiFi) {
                [RTCDefaultSocketLogger.logger log:@"ERROR ==========Network changed: WiFi to 4G===========" type:self.logType];
                [self.engine disconnect:@"Network changed: WiFi to 4G"];
            }
            break;
        }
        case RTCVPAFNetworkReachabilityStatusReachableViaWiFi: {
            if (self.currentNetworkStatus == RTCVPAFNetworkReachabilityStatusReachableViaWWAN) {
                [RTCDefaultSocketLogger.logger log:@"ERROR ==========Network changed: 4G to WiFi===========" type:self.logType];
                [self.engine disconnect:@"Network changed: 4G to WiFi"];
            }
            break;
        }
    }
    
    self.currentNetworkStatus = status;
}

- (void)emitAck:(int)ack withItems:(NSArray *)items isEvent:(BOOL)isEvent {
    if (items && items.count > 0) {
        // 第一个元素是事件名
        NSString *event = items.firstObject;
        NSArray *eventItems = items.count > 1 ? [items subarrayWithRange:NSMakeRange(1, items.count - 1)] : @[];
        [self emit:event items:eventItems ack:ack];
    }
}

- (void)didConnect:(NSString *)nsp {
    [RTCDefaultSocketLogger.logger log:@"Socket已连接" type:self.logType];
    self.status = RTCVPSocketIOClientStatusConnected;
    
    // 发送缓存的数据
    if (self.dataCache.count > 0) {
        NSArray *tempArr = [NSArray arrayWithArray:self.dataCache];
        for (RTCVPSocketIOClientCacheData *cacheData in tempArr) {
            // 重构缓存数据的发送逻辑
            if (cacheData.isEvent && cacheData.items.count > 0) {
                NSString *event = cacheData.items.firstObject;
                NSArray *items = cacheData.items.count > 1 ? [cacheData.items subarrayWithRange:NSMakeRange(1, cacheData.items.count - 1)] : @[];
                [self emit:event items:items ack:cacheData.ack];
            }
            [self.dataCache removeObject:cacheData];
        }
    }
    
    [self handleClientEvent:RTCVPSocketEventConnect withData:@[nsp]];
}

- (void)didError:(NSString *)reason {
    [self handleClientEvent:RTCVPSocketEventError withData:@[reason]];
}

#pragma mark - RTCVPSocketEngineClient

- (void)engineDidError:(NSString *)reason {
    __weak __typeof(self) weakSelf = self;
    dispatch_async(self.handleQueue, ^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf _engineDidError:reason];
        }
    });
}

- (void)_engineDidError:(NSString *)reason {
    [self handleClientEvent:RTCVPSocketEventError withData:@[reason]];
}

- (void)engineDidOpen:(NSString *)reason {
    self.status = RTCVPSocketIOClientStatusOpened;
    [self handleClientEvent:RTCVPSocketEventConnect withData:@[reason]];
}

- (void)engineDidClose:(NSString *)reason {
    __weak __typeof(self) weakSelf = self;
    dispatch_async(self.handleQueue, ^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf _engineDidClose:reason];
        }
    });
}

- (void)_engineDidClose:(NSString *)reason {
    [self.waitingPackets removeAllObjects];
    if (_status == RTCVPSocketIOClientStatusDisconnected || !self.reconnects) {
        [self didDisconnect:reason];
    } else {
        self.status = RTCVPSocketIOClientStatusNotConnected;
        if (!_reconnecting) {
            _reconnecting = YES;
            [self tryReconnect:reason];
        }
    }
}

- (void)parseSocketMessage:(NSString *)message {
    if (message.length == 0) return;
    
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"解析消息: %@", message]
                                  type:@"SocketParser"];
    
    // 使用PacketReceiver处理文本消息
    if (self->pack_receiver) {
        self->pack_receiver->process_text_packet(message.UTF8String);
    }
}

- (void)parseEngineBinaryData:(NSData *)data {
    __weak __typeof(self) weakSelf = self;
    dispatch_async(self.handleQueue, ^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf parseBinaryData:data];
        }
    });
}

- (void)handleEngineAck:(NSInteger)ackId withData:(nonnull NSArray *)data {
    // 处理引擎ACK
    [self handleAck:(int)ackId withData:data];
}

- (void)parseEngineMessage:(NSString *)msg {
    // 实现缺失的协议方法
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"解析引擎消息: %@", msg]
                                  type:@"SocketParser"];
    
    // 使用PacketReceiver处理消息
    if (self->pack_receiver) {
        self->pack_receiver->process_text_packet(msg.UTF8String);
    }
}



- (void)parseBinaryData:(NSData *)data {
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"收到二进制数据，长度: %ld", (long)data.length]
                                  type:@"SocketParser"];
    
    // 使用PacketReceiver处理二进制数据
    if (self->pack_receiver) {
        // 将NSData转换为SmartBuffer
        sio::SmartBuffer smart_buffer((const uint8_t*)data.bytes, data.length);
        
        // 处理二进制数据
        self->pack_receiver->process_binary_data(smart_buffer);
    } else {
        [RTCDefaultSocketLogger.logger error:@"PacketReceiver未初始化，无法处理二进制数据" type:@"SocketParser"];
    }
}

- (BOOL)isCorrectNamespace:(NSString *)nsp {
    return [nsp isEqualToString:self.nsp];
}

- (void)handleConnect:(NSString *)packetNamespace {
    if ([packetNamespace isEqualToString:@"/"] && ![self.nsp isEqualToString:@"/"]) {
        [self joinNamespace:self.nsp];
    } else {
        [self didConnect:packetNamespace];
    }
}

@end
