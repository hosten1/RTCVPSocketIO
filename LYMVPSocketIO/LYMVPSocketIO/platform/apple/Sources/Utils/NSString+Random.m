//
//  NSString+Random.m
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/12.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#import "NSString+Random.h"

@implementation NSString (Random)
/// 生成 t 参数：时间戳 + 随机字符串
- (NSString *)generateTParameter {
    // 获取当前时间戳（秒）
    NSTimeInterval timestamp = [[NSDate date] timeIntervalSince1970];
    NSInteger seconds = (NSInteger)timestamp;
    
    // 生成随机字符串（7个字符）
    NSString *randomString = [self generateRandomStringWithLength:7];
    
    // 组合格式：秒_随机字符串（如：1734010000_abc1234）
    return [NSString stringWithFormat:@"%ld_%@", (long)seconds, randomString];
}

/// 生成指定长度的随机字符串
- (NSString *)generateRandomStringWithLength:(NSInteger)length {
    NSString *characters = @"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    NSMutableString *randomString = [NSMutableString stringWithCapacity:length];
    
    for (NSInteger i = 0; i < length; i++) {
        NSInteger randomIndex = arc4random_uniform((u_int32_t)[characters length]);
        [randomString appendFormat:@"%C", [characters characterAtIndex:randomIndex]];
    }
    
    return randomString;
}
@end
