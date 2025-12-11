//
//  SocketEngine.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "RTCVPSocketEngineProtocol.h"
#import "RTCVPSocketIOConfig.h"


@interface RTCVPSocketEngine : NSObject<RTCVPSocketEngineProtocol>

/// 配置对象
@property (nonatomic, strong, readonly) RTCVPSocketIOConfig *config;

/// 创建引擎
+ (instancetype)engineWithClient:(id<RTCVPSocketEngineClient>)client
                             url:(NSURL *)url
                          config:(RTCVPSocketIOConfig *)config;

/// 向后兼容的初始化方法
- (instancetype)initWithClient:(id<RTCVPSocketEngineClient>)client
                           url:(NSURL *)url
                       options:(NSDictionary *)options DEPRECATED_MSG_ATTRIBUTE("Use engineWithClient:url:config: instead");

/// 发送原始数据
- (void)sendRawData:(NSData *)data;

/// 获取当前传输类型
- (NSString *)currentTransport;
@end
