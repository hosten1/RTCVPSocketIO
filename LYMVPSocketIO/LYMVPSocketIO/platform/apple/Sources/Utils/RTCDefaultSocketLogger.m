//
//  RTCDefaultSocketLogger.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/23/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCDefaultSocketLogger.h"


@implementation RTCDefaultSocketLogger

static RTCVPSocketLogger *logInstance;

static RTCVPSocketLogger *_sharedLogger = nil;

+(void)setCoustomLogger:(RTCVPSocketLogger *)logger{
    if (_sharedLogger) {
        return;
    }
    _sharedLogger = logger;
}

+ (RTCVPSocketLogger *)logger {
    return _sharedLogger;
}

+ (void)setEnabled:(BOOL)enabled {
    [self logger].log = enabled;
}

+ (void)setLogLevel:(RTCLogLevel)level {
    [self logger].logLevel = level;
}

@end
