//
//  RTCVPSocketPacket.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import <Foundation/Foundation.h>

typedef enum : NSUInteger {
    RTCVPPacketTypeConnect = 0,
    RTCVPPacketTypeDisconnect,
    RTCVPPacketTypeEvent,
    RTCVPPacketTypeAck,
    RTCVPPacketTypeError,
    RTCVPPacketTypeBinaryEvent,
    RTCVPPacketTypeBinaryAck
    
} RTCVPPacketType;

@interface RTCVPSocketPacket : NSObject

@property (nonatomic, strong, readonly) NSString *packetString;
@property (nonatomic, strong, readonly) NSMutableArray<NSData*> *binary;
@property (nonatomic, readonly) RTCVPPacketType type;
@property (nonatomic, readonly) int ID;
@property (nonatomic, strong, readonly) NSString *event;
@property (nonatomic, strong, readonly) NSArray *args;
@property (nonatomic, strong, readonly) NSString *nsp;
@property (nonatomic, strong, readonly) NSMutableArray *data;

-(instancetype)init:(RTCVPPacketType)type
                nsp:(NSString*)namespace
       placeholders:(int)plholders;
-(instancetype)init:(RTCVPPacketType)type
               data:(NSArray*)data
                 ID:(int)ID
                nsp:(NSString*)nsp
       placeholders:(int)plholders
             binary:(NSArray*)binary;

+(RTCVPSocketPacket*)packetFromEmit:(NSArray*)items ID:(int)ID nsp:(NSString*)nsp ack:(BOOL)ack isEvent:(BOOL)isEvent;

-(BOOL)addData:(NSData*)data;

@end
