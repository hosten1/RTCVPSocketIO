//
//  RTCVPACKManager+Monitoring.m
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/16.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#import "RTCVPACKManager+Monitoring.h"

@implementation RTCVPACKManager (Monitoring)

- (NSDictionary *)ackStatistics {
    __block NSMutableDictionary *stats = [NSMutableDictionary dictionary];
    dispatch_sync(self.queue, ^{
        stats[@"totalPending"] = @(self.ackItems.count);
        
        NSInteger successCount = 0;
        NSInteger timeoutCount = 0;
        NSTimeInterval totalResponseTime = 0;
        
        for (RTCVPACKItem *item in self.ackItems.allValues) {
            if (item.hasExecuted) {
                successCount++;
            } else {
                NSTimeInterval elapsed = [[NSDate date] timeIntervalSince1970] - item.startTime;
                if (elapsed >= item.timeout) {
                    timeoutCount++;
                }
            }
        }
        
        stats[@"successCount"] = @(successCount);
        stats[@"timeoutCount"] = @(timeoutCount);
        stats[@"avgTimeout"] = @(self.defaultTimeout);
    });
    
    return [stats copy];
}

@end
