//
//  RTCVPSocketLogger.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/26/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface RTCVPSocketLogger:NSObject

@property (nonatomic) BOOL log;

-(void) log:(NSString*)message type:(NSString*)type;
-(void) error:(NSString*)message type:(NSString*)type;
- (void) onLogMsgWithCB:(void(^)(NSString *message ,NSString *type))cb;
@end
