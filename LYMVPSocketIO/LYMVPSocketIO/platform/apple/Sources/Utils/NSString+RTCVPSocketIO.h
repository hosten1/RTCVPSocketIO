//
//  NSString+RTCVPSocketIO.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/23/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface NSString (RTCVPSocketIO)

-(NSDictionary*)toDictionary;
-(NSString*)urlEncode;
-(NSArray*)toArray;

@end
