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

+(void)setLogger:(RTCVPSocketLogger*)newLogger {
    logInstance = newLogger;
}

+(RTCVPSocketLogger*)logger {
    return logInstance;
}

@end
