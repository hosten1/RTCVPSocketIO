//
//  NSString+Random.h
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/12.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSString (Random)
- (NSString *)generateTParameter;
- (NSString *)generateRandomStringWithLength:(NSInteger)length;
@end

NS_ASSUME_NONNULL_END
