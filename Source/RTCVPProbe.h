//
//  RTCVPProbe.h
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/10.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "RTCVPSocketEngine+Private.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCVPProbe : NSObject

@property (nonatomic, strong) NSString *message;
@property (nonatomic) RTCVPSocketEnginePacketType type;
@property (nonatomic, strong) NSArray *data;
@end

NS_ASSUME_NONNULL_END
