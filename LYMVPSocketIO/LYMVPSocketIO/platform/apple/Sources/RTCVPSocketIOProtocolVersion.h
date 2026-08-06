//
//  RTCVPSocketIOProtocolVersion.h
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/11.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>

#ifndef RTCVPSocketIOProtocolVersion_H
#define RTCVPSocketIOProtocolVersion_H
NS_ASSUME_NONNULL_BEGIN


typedef NS_ENUM(NSInteger, RTCVPSocketIOProtocolVersion) {
    RTCVPSocketIOProtocolVersion2 = 2,  // Engine.IO 3.x
    RTCVPSocketIOProtocolVersion3 = 3,  // Engine.IO 4.x
    RTCVPSocketIOProtocolVersion4 = 4   // Engine.IO 5.x
};

// 默认使用 Engine.IO 3.x (Socket.IO v2)
static const RTCVPSocketIOProtocolVersion kRTCVPSocketIOProtocolVersionDefault = RTCVPSocketIOProtocolVersion2;

NS_ASSUME_NONNULL_END
#endif // RTCVPSocketIOProtocolVersion_H
