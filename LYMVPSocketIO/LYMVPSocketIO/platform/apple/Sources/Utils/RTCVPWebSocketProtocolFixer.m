//
//  RTCVPWebSocketProtocolFixer.m
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/11.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#import "RTCVPWebSocketProtocolFixer.h"
#import "RTCDefaultSocketLogger.h"

@implementation RTCVPWebSocketProtocolFixer

+ (NSData *)fixWebSocketFrame:(NSData *)frame {
    if (frame.length < 2) {
        return frame;
    }
    
    const uint8_t *bytes = (const uint8_t *)frame.bytes;
    
    // 复制数据
    NSMutableData *fixedData = [NSMutableData dataWithData:frame];
    uint8_t *fixedBytes = (uint8_t *)fixedData.mutableBytes;
    
    // 修复第一个字节（操作码和RSV位）
    uint8_t firstByte = fixedBytes[0];
    
    // 1. 清除RSV位（0x70）
    if ((firstByte & 0x70) != 0) {
        firstByte &= 0x8F; // 保留FIN（0x80）和操作码（0x0F）
        fixedBytes[0] = firstByte;
        
        [RTCDefaultSocketLogger.logger logMessage:@"清除WebSocket帧RSV位" type:@"WebSocketProtocolFixer" level:RTCLogLevelWarning];
    }
    
    // 2. 修复未知操作码
    uint8_t opcode = firstByte & 0x0F;
    
    // 有效的操作码范围：0x0-0x2, 0x8-0xA
    switch (opcode) {
        case 0x0: // 延续帧
        case 0x1: // 文本帧
        case 0x2: // 二进制帧
        case 0x8: // 关闭连接
        case 0x9: // Ping
        case 0xA: // Pong
            // 标准操作码，无需修改
            break;
            
        default:
            // 未知操作码，转换为文本帧
            if (opcode >= 0x3 && opcode <= 0x7) {
                // 保留的非控制帧，转换为文本帧
                fixedBytes[0] = (firstByte & 0xF0) | 0x1; // 设置操作码为文本帧
                
                [RTCDefaultSocketLogger.logger logMessage:[NSString stringWithFormat:@"转换未知非控制帧操作码 0x%02X -> 0x01", opcode] type:@"WebSocketProtocolFixer" level:RTCLogLevelWarning];
            } else if (opcode >= 0xB && opcode <= 0xF) {
                // 保留的控制帧，转换为Pong（0xA）
                fixedBytes[0] = (firstByte & 0xF0) | 0xA; // 设置操作码为Pong
                [RTCDefaultSocketLogger.logger logMessage:[NSString stringWithFormat:@"转换未知控制帧操作码 0x%02X -> 0x0A", opcode] type:@"WebSocketProtocolFixer" level:RTCLogLevelWarning];
            }
            break;
    }
    
    return fixedData;
}

+ (NSDictionary *)analyzeWebSocketFrame:(NSData *)frame {
    if (frame.length < 2) {
        return @{@"error": @"Frame too short"};
    }
    
    const uint8_t *bytes = (const uint8_t *)frame.bytes;
    
    BOOL fin = (bytes[0] & 0x80) != 0;
    uint8_t rsv1 = (bytes[0] & 0x40) != 0;
    uint8_t rsv2 = (bytes[0] & 0x20) != 0;
    uint8_t rsv3 = (bytes[0] & 0x10) != 0;
    uint8_t opcode = bytes[0] & 0x0F;
    BOOL masked = (bytes[1] & 0x80) != 0;
    uint64_t payloadLength = bytes[1] & 0x7F;
    
    NSString *opcodeString;
    switch (opcode) {
        case 0x0: opcodeString = @"Continuation"; break;
        case 0x1: opcodeString = @"Text"; break;
        case 0x2: opcodeString = @"Binary"; break;
        case 0x8: opcodeString = @"Close"; break;
        case 0x9: opcodeString = @"Ping"; break;
        case 0xA: opcodeString = @"Pong"; break;
        default: opcodeString = [NSString stringWithFormat:@"Unknown (0x%02X)", opcode]; break;
    }
    
    return @{
        @"fin": @(fin),
        @"rsv1": @(rsv1),
        @"rsv2": @(rsv2),
        @"rsv3": @(rsv3),
        @"opcode": opcodeString,
        @"opcodeRaw": @(opcode),
        @"masked": @(masked),
        @"payloadLength": @(payloadLength),
        @"totalLength": @(frame.length)
    };
}

+ (BOOL)isValidWebSocketFrame:(NSData *)frame {
    if (frame.length < 2) return NO;
    
    const uint8_t *bytes = (const uint8_t *)frame.bytes;
    
    // 检查操作码
    uint8_t opcode = bytes[0] & 0x0F;
    
    // 允许的操作码
    BOOL isValidOpcode = (opcode <= 0x2) || (opcode >= 0x8 && opcode <= 0xA);
    
    // 检查RSV位（应全部为0）
    BOOL hasRSV = (bytes[0] & 0x70) != 0;
    
    // 对于控制帧，检查FIN位（应始终为1）
    BOOL isControlFrame = (opcode == 0x8 || opcode == 0x9 || opcode == 0xA);
    BOOL controlFrameValid = !isControlFrame || (bytes[0] & 0x80) != 0;
    
    return isValidOpcode && !hasRSV && controlFrameValid;
}

@end
