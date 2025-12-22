//
//  RTCVPSocketPacketBridge.mm
//  LYMVPSocketIO
//
//  Created by luoyongmeng on 2025/12/22.
//

#import "RTCVPSocketPacketBridge.h"

// 引入 C++ 核心头文件
#include "lib/sio_packet_impl.h"
#include "lib/sio_packet_parser.h"
#include "lib/sio_jsoncpp_binary_helper.hpp"
#include "lib/sio_packet.h"

// 转换工具函数
namespace {

// 将 Objective-C 对象转换为 Json::Value
Json::Value ConvertToJsonValue(id obj) {
    if ([obj isKindOfClass:[NSString class]]) {
        return std::string([(NSString *)obj UTF8String]);
    } else if ([obj isKindOfClass:[NSNumber class]]) {
        NSNumber *number = (NSNumber *)obj;
        const char *type = [number objCType];
        
        if (strcmp(type, @encode(BOOL)) == 0) {
            return number.boolValue;
        } else if (strcmp(type, @encode(int)) == 0) {
            return number.intValue;
        } else if (strcmp(type, @encode(long)) == 0) {
            return number.longValue;
        } else if (strcmp(type, @encode(long long)) == 0) {
            return number.longLongValue;
        } else if (strcmp(type, @encode(float)) == 0) {
            return number.floatValue;
        } else if (strcmp(type, @encode(double)) == 0) {
            return number.doubleValue;
        } else {
            return number.intValue;
        }
    } else if ([obj isKindOfClass:[NSDictionary class]]) {
        NSDictionary *dict = (NSDictionary *)obj;
        Json::Value jsonObj(Json::objectValue);
        
        for (NSString *key in dict) {
            id value = dict[key];
            jsonObj[[key UTF8String]] = ConvertToJsonValue(value);
        }
        return jsonObj;
    } else if ([obj isKindOfClass:[NSArray class]]) {
        NSArray *array = (NSArray *)obj;
        Json::Value jsonArray(Json::arrayValue);
        
        for (id item in array) {
            jsonArray.append(ConvertToJsonValue(item));
        }
        return jsonArray;
    } else if ([obj isKindOfClass:[NSData class]]) {
        // 二进制数据
        NSData *data = (NSData *)obj;
        return sio::binary_helper::create_binary_value((const uint8_t *)data.bytes, data.length);
    } else if ([obj isKindOfClass:[NSNull class]]) {
        return Json::Value(Json::nullValue);
    }
    
    // 默认返回null
    return Json::Value(Json::nullValue);
}

// 将 Json::Value 转换为 Objective-C 对象
id ConvertFromJsonValue(const Json::Value& jsonValue) {
    if (jsonValue.isNull()) {
        return [NSNull null];
    } else if (jsonValue.isBool()) {
        return @(jsonValue.asBool());
    } else if (jsonValue.isInt()) {
        return @(jsonValue.asInt());
    } else if (jsonValue.isUInt()) {
        return @(jsonValue.asUInt());
    } else if (jsonValue.isDouble()) {
        return @(jsonValue.asDouble());
    } else if (jsonValue.isString()) {
        return [NSString stringWithUTF8String:jsonValue.asCString()];
    } else if (jsonValue.isArray()) {
        NSMutableArray *array = [NSMutableArray array];
        for (Json::ArrayIndex i = 0; i < jsonValue.size(); i++) {
            [array addObject:ConvertFromJsonValue(jsonValue[i])];
        }
        return array;
    } else if (jsonValue.isObject()) {
        // 检查是否是二进制数据
        if (sio::binary_helper::is_binary(jsonValue)) {
            try {
                auto buffer = sio::binary_helper::get_binary(jsonValue);
                return [NSData dataWithBytes:buffer.data() length:buffer.size()];
            } catch (...) {
                return [NSData data];
            }
        }
        
        // 普通对象
        NSMutableDictionary *dict = [NSMutableDictionary dictionary];
        Json::Value::Members members = jsonValue.getMemberNames();
        
        for (const auto& member : members) {
            NSString *key = [NSString stringWithUTF8String:member.c_str()];
            dict[key] = ConvertFromJsonValue(jsonValue[member]);
        }
        return dict;
    }
    
    return [NSNull null];
}

// 转换 SmartBuffer 到 NSData
NSData* ConvertSmartBufferToNSData(const sio::SmartBuffer& buffer) {
    if (buffer.empty()) {
        return [NSData data];
    }
    return [NSData dataWithBytes:buffer.data() length:buffer.size()];
}

// 转换 NSData 到 SmartBuffer
sio::SmartBuffer ConvertNSDataToSmartBuffer(NSData* data) {
    if (!data || data.length == 0) {
        return sio::SmartBuffer();
    }
    return sio::SmartBuffer((const uint8_t *)data.bytes, data.length);
}

} // namespace

// 内部实现类
@interface RTCVPSocketPacketBridge () {
    // C++ 对象
    std::shared_ptr<sio::PacketSender<Json::Value>> _packetSender;
    std::shared_ptr<sio::PacketReceiver<Json::Value>> _packetReceiver;
    sio::SocketIOVersion _cppVersion;
}

@property (nonatomic, strong) dispatch_queue_t callbackQueue;

@end

@implementation RTCVPSocketPacketBridge

- (instancetype)initWithSocketIOVersion:(RTCVPSocketIOVersion)version {
    self = [super init];
    if (self) {
        // 创建回调队列
        _callbackQueue = dispatch_queue_create("com.rtc.vp.socketio.packet.bridge", DISPATCH_QUEUE_SERIAL);
        
        // 转换版本
        switch (version) {
            case RTCVPSocketIOVersionV2:
                _cppVersion = sio::SocketIOVersion::V2;
                break;
            case RTCVPSocketIOVersionV3:
                _cppVersion = sio::SocketIOVersion::V3;
                break;
            case RTCVPSocketIOVersionV4:
                _cppVersion = sio::SocketIOVersion::V4;
                break;
            default:
                _cppVersion = sio::SocketIOVersion::V3;
                break;
        }
        
        // 创建TaskQueueFactory（简化版本，实际中可能需要传递）
        // 这里使用默认配置
        
        // 创建PacketSender和PacketReceiver
        // 注意：这里简化了TaskQueueFactory的创建，实际项目中需要传入
        _packetSender = std::make_shared<sio::PacketSender<Json::Value>>(nullptr, _cppVersion);
        _packetReceiver = std::make_shared<sio::PacketReceiver<Json::Value>>(nullptr, _cppVersion);
        
        // 设置完成回调
        __weak typeof(self) weakSelf = self;
        _packetReceiver->set_complete_callback([weakSelf](const std::vector<Json::Value>& data_array) {
            [weakSelf handleDataArrayReceived:data_array];
        });
    }
    return self;
}

- (RTCVPSocketIOVersion)version {
    switch (_cppVersion) {
        case sio::SocketIOVersion::V2:
            return RTCVPSocketIOVersionV2;
        case sio::SocketIOVersion::V3:
            return RTCVPSocketIOVersionV3;
        case sio::SocketIOVersion::V4:
            return RTCVPSocketIOVersionV4;
        default:
            return RTCVPSocketIOVersionV3;
    }
}

// MARK: - 组合方法

- (void)composePacketWithDataArray:(NSArray *)dataArray
                              type:(RTCVPSocketIOPacketType)type
                               nsp:(NSInteger)nsp
                                id:(NSInteger)packetId
                        completion:(RTCVPComposeCompletion)completion {
    
    if (!completion) return;
    
    // 转换数据类型
    sio::PacketType packetType = static_cast<sio::PacketType>(type);
    
    // 转换为C++数据数组
    std::vector<Json::Value> cppDataArray;
    for (id item in dataArray) {
        cppDataArray.push_back(ConvertToJsonValue(item));
    }
    
    // 使用异步发送（但只是生成包，不实际发送）
    _packetSender->send_data_array_async(
        cppDataArray,
        // 文本回调 - 获取生成的文本包
        [completion, self](const std::string& text_packet) -> bool {
            dispatch_async(self.callbackQueue, ^{                
                NSString *textPacket = [NSString stringWithUTF8String:text_packet.c_str()];
                
                // 这里只返回文本包，二进制数据在另一个回调中处理
                // 注意：实际实现中，文本包和二进制数据应该一起返回
                completion(textPacket, nil, NO, 0, nil);
            });
            return true; // 表示"发送"成功
        },
        // 二进制回调 - 获取二进制数据
        [completion, self](const sio::SmartBuffer& binary_data, int index) -> bool {
            // 这里无法单独返回二进制数据，需要重构
            return true;
        },
        // 完成回调
        [completion, self](bool success, const std::string& error) {
            dispatch_async(self.callbackQueue, ^{                
                if (!success) {
                    NSError *nsError = [NSError errorWithDomain:@"RTCVPSocketPacketBridge" 
                                                           code:-1
                                                       userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithUTF8String:error.c_str()]}];
                    completion(nil, nil, NO, 0, nsError);
                }
            });
        },
        packetType,
        static_cast<int>(nsp),
        static_cast<int>(packetId)
    );
}

// 更好的组合方法 - 使用PacketParser直接构建
- (void)composeEventWithName:(NSString *)eventName
                        data:(id _Nullable)data
                         nsp:(NSInteger)nsp
               needsAckReply:(BOOL)needsAckReply
                  completion:(RTCVPComposeCompletion)completion {
    
    if (!completion) return;
    
    dispatch_async(self.callbackQueue, ^{        
        @try {
            // 获取PacketParser单例
            auto& parser = sio::PacketParser::getInstance();
            
            // 准备参数
            std::string nspStr = "/";
            if (nsp != 0) {
                nspStr = "/" + std::to_string(nsp);
            }
            
            // 转换数据
            Json::Value jsonData = ConvertToJsonValue(data);
            int packetId = needsAckReply ? -2 : -1; // -2表示需要ACK，实际中会生成真实ID
            
            // 构建事件包
            std::string textPacket = parser.buildEventString(
                [eventName UTF8String],
                jsonData,
                packetId,
                nspStr,
                false // 不是二进制包
            );
            
            // 计算二进制占位符数量
            int binaryCount = 0;
            size_t pos = 0;
            std::string placeholder = "\"_placeholder\":true";
            while ((pos = textPacket.find(placeholder, pos)) != std::string::npos) {
                binaryCount++;
                pos += placeholder.length();
            }
            
            BOOL isBinaryPacket = (binaryCount > 0);
            
            // 返回结果
            NSString *resultTextPacket = [NSString stringWithUTF8String:textPacket.c_str()];
            completion(resultTextPacket, nil, isBinaryPacket, binaryCount, nil);
            
        } @catch (const std::exception& e) {
            NSError *error = [NSError errorWithDomain:@"RTCVPSocketPacketBridge" 
                                                 code:-2
                                             userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithUTF8String:e.what()]}];
            completion(nil, nil, NO, 0, error);
        }
    });
}

// MARK: - 解析方法

- (void)parsePacket:(NSString *)textPacket
        binaryParts:(NSArray<NSData *> * _Nullable)binaryParts
         completion:(RTCVPParseCompletion)completion {
    
    if (!completion) return;
    
    dispatch_async(self.callbackQueue, ^{        
        @try {
            // 转换参数
            std::string textPacketStr = [textPacket UTF8String];
            
            // 转换二进制数据
            std::vector<sio::SmartBuffer> cppBinaryParts;
            if (binaryParts) {
                for (NSData *data in binaryParts) {
                    cppBinaryParts.push_back(ConvertNSDataToSmartBuffer(data));
                }
            }
            
            // 使用PacketSplitter合并数据
            sio::PacketSplitter<Json::Value>::combine_to_data_array_async(
                textPacketStr,
                cppBinaryParts,
                [completion, textPacketStr](const std::vector<Json::Value>& data_array) {
                    // 解析包信息
                    auto parseResult = sio::PacketParser::getInstance().parsePacket(textPacketStr);
                    
                    // 转换数据数组
                    NSMutableArray *resultArray = [NSMutableArray array];
                    for (const auto& item : data_array) {
                        [resultArray addObject:ConvertFromJsonValue(item)];
                    }
                    
                    // 调用完成回调
                    completion(resultArray,
                              static_cast<NSInteger>(parseResult.packet.type),
                              parseResult.packet.nsp,
                              parseResult.packet.id,
                              nil);
                }
            );
            
        } @catch (const std::exception& e) {
            NSError *error = [NSError errorWithDomain:@"RTCVPSocketPacketBridge" 
                                                 code:-3
                                             userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithUTF8String:e.what()]}];
            completion(nil, 0, 0, -1, error);
        }
    });
}

- (void)parseTextPacket:(NSString *)textPacket
          binaryCallback:(RTCVPBinaryDataCallback _Nullable)binaryCallback
              completion:(RTCVPParseCompletion)completion {
    
    if (!completion) return;
    
    dispatch_async(self.callbackQueue, ^{        
        @try {
            // 转换文本包
            std::string textPacketStr = [textPacket UTF8String];
            
            // 使用PacketReceiver解析
            if (_packetReceiver->receive_text(textPacketStr)) {
                // 解析成功，等待二进制数据
                // 这里需要保存回调，等二进制数据到达时调用
                
                // 临时方案：解析但不等待二进制
                auto parseResult = sio::PacketParser::getInstance().parsePacket(textPacketStr);
                
                // 如果有二进制占位符，通过binaryCallback请求
                if (parseResult.is_binary_packet && binaryCallback) {
                    for (int i = 0; i < parseResult.binary_count; i++) {
                        binaryCallback(nil, i);
                    }
                }
                
                // 如果没有二进制数据，直接调用完成回调
                if (parseResult.binary_count == 0) {
                    // 解析JSON数据
                    if (!parseResult.json_data.empty()) {
                        Json::Value jsonData;
                        Json::Reader reader;
                        if (reader.parse(parseResult.json_data, jsonData)) {
                            NSArray *resultArray = @[ConvertFromJsonValue(jsonData)];
                            completion(resultArray, 
                                      static_cast<NSInteger>(parseResult.packet.type),
                                      parseResult.packet.nsp,
                                      parseResult.packet.id,
                                      nil);
                            return;
                        }
                    }
                    
                    // 空数据
                    completion(@[], 
                              static_cast<NSInteger>(parseResult.packet.type),
                              parseResult.packet.nsp,
                              parseResult.packet.id,
                              nil);
                }
            } else {
                // 解析失败
                NSError *error = [NSError errorWithDomain:@"RTCVPSocketPacketBridge" 
                                                 code:-4
                                             userInfo:@{NSLocalizedDescriptionKey: @"解析文本包失败"}];
                completion(nil, 0, 0, -1, error);
            }
            
        } @catch (const std::exception& e) {
            NSError *error = [NSError errorWithDomain:@"RTCVPSocketPacketBridge" 
                                                 code:-5
                                             userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithUTF8String:e.what()]}];
            completion(nil, 0, 0, -1, error);
        }
    });
}

// MARK: - 带ACK的事件发送

- (void)sendEventWithName:(NSString *)eventName
                     data:(id _Nullable)data
                      nsp:(NSInteger)nsp
               ackCallback:(RTCVPAckCompletion _Nullable)ackCallback
          timeoutCallback:(RTCVPAckTimeoutCompletion _Nullable)timeoutCallback
                 timeout:(NSTimeInterval)timeout
              completion:(RTCVPComposeCompletion)completion {
    
    if (!completion) return;
    
    dispatch_async(self.callbackQueue, ^{        
        @try {
            // 构建事件数据
            Json::Value jsonData = ConvertToJsonValue(data);
            std::vector<Json::Value> dataArray = {Json::Value([eventName UTF8String]), jsonData};
            
            // 使用带ACK的发送
            // 注意：这里简化了实现，实际应该使用PacketSender的ACK功能
            auto& parser = sio::PacketParser::getInstance();
            
            // 生成ACK ID（简化：使用时间戳）
            int ackId = static_cast<int>([[NSDate date] timeIntervalSince1970] * 1000) % 1000000;
            
            // 构建事件包（带ACK ID）
            std::string nspStr = "/";
            if (nsp != 0) {
                nspStr = "/" + std::to_string(nsp);
            }
            
            std::string textPacket = parser.buildEventString(
                [eventName UTF8String],
                jsonData,
                ackId,
                nspStr,
                false
            );
            
            // 计算二进制占位符
            int binaryCount = 0;
            size_t pos = 0;
            std::string placeholder = "\"_placeholder\":true";
            while ((pos = textPacket.find(placeholder, pos)) != std::string::npos) {
                binaryCount++;
                pos += placeholder.length();
            }
            
            BOOL isBinaryPacket = (binaryCount > 0);
            
            // 返回结果
            NSString *resultTextPacket = [NSString stringWithUTF8String:textPacket.c_str()];
            completion(resultTextPacket, nil, isBinaryPacket, binaryCount, nil);
            
            // 如果有ACK回调，存储起来（实际应该使用ACK管理器）
            if (ackCallback || timeoutCallback) {
                // 这里需要实现ACK管理器来跟踪ACK响应
                NSLog(@"警告：ACK回调功能需要实现ACK管理器");
            }
            
        } @catch (const std::exception& e) {
            NSError *error = [NSError errorWithDomain:@"RTCVPSocketPacketBridge" 
                                                 code:-6
                                             userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithUTF8String:e.what()]}];
            completion(nil, nil, NO, 0, error);
        }
    });
}

// MARK: - 辅助方法

- (void)handleAckResponse:(NSArray *)ackData forPacketId:(NSInteger)packetId {
    // 处理ACK响应
    // 这里应该调用ACK管理器来处理
    NSLog(@"收到ACK响应，packetId: %ld, data: %@", (long)packetId, ackData);
}

- (void)handleDataArrayReceived:(const std::vector<Json::Value>&)data_array {
    // 处理接收到的数据数组
    // 这里可以由PacketReceiver触发
    NSLog(@"收到数据数组，数量: %lu", data_array.size());
}

@end