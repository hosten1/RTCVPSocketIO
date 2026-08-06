//
//  RTCVPWebSocketAdapter.h
//  RTCVPSocketIO
//
//  WebSocket 适配器 - 通过宏切换使用 ObjC jetfire 或 C++ libevent 实现
//

#import <Foundation/Foundation.h>

#ifdef USE_CPP_WEBSOCKET
    #import "RTCCPPCWebSocket.h"
    #define RTCVPWebSocket RTCCPPCWebSocket
    #define RTCVPWebSocketDelegate RTCCPPCWebSocketDelegate
#else
    #import "RTCJFRWebSocket.h"
    #define RTCVPWebSocket RTCJFRWebSocket
    #define RTCVPWebSocketDelegate RTCJFRWebSocketDelegate
#endif
