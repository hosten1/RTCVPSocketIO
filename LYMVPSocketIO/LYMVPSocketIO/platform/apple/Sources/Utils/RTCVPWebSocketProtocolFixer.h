//
//  RTCVPWebSocketProtocolFixer.h
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/11.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface RTCVPWebSocketProtocolFixer : NSObject
+ (NSData *)fixWebSocketFrame:(NSData *)frame ;

+ (NSDictionary *)analyzeWebSocketFrame:(NSData *)frame ;

+ (BOOL)isValidWebSocketFrame:(NSData *)frame ;
@end

NS_ASSUME_NONNULL_END
