//
//  SocketEngine.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "RTCVPSocketEngineProtocol.h"

@interface RTCVPSocketEngine : NSObject<RTCVPSocketEngineProtocol>

/// Creates a new engine.
-(instancetype)initWithClient:(id<RTCVPSocketEngineClient>)client url:(NSURL*)url options:(NSDictionary*)options;

@end
