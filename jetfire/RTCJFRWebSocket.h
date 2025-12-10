//////////////////////////////////////////////////////////////////////////////////////////////////
//
//  JFRWebSocket.h
//
//  Created by Austin and Dalton Cherry on on 5/13/14.
//  Copyright (c) 2014-2025 Austin Cherry.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
//////////////////////////////////////////////////////////////////////////////////////////////////

#import <Foundation/Foundation.h>
#import "RTCJFRSecurity.h"

@class RTCJFRWebSocket;

NS_ASSUME_NONNULL_BEGIN

/**
 It is important to note that all the delegate methods are put back on the main thread.
 This means if you want to do some major process of the data, you need to create a background thread.
 */
@protocol RTCJFRWebSocketDelegate <NSObject>

@optional
/**
 The websocket connected to its host.
 @param socket is the current socket object.
 */
-(void)websocketDidConnect:(RTCJFRWebSocket*)socket;

/**
 The websocket was disconnected from its host.
 @param socket is the current socket object.
 @param error  is return an error occured to trigger the disconnect.
 */
-(void)websocketDidDisconnect:(RTCJFRWebSocket*)socket error:(nullable NSError*)error;

/**
 The websocket got a text based message.
 @param socket is the current socket object.
 @param string is the text based data that has been returned.
 */
-(void)websocket:(RTCJFRWebSocket*)socket didReceiveMessage:(NSString*)string;

/**
 The websocket got a binary based message.
 @param socket is the current socket object.
 @param data   is the binary based data that has been returned.
 */
-(void)websocket:(RTCJFRWebSocket*)socket didReceiveData:(NSData*)data;

@end

@interface RTCJFRWebSocket : NSObject

@property(nonatomic, weak, nullable) id<RTCJFRWebSocketDelegate> delegate;
@property(nonatomic, strong, readonly) NSURL *url;
@property(nonatomic, assign, readonly) BOOL isConnected;
@property(nonatomic, assign) BOOL voipEnabled;
@property(nonatomic, assign) BOOL selfSignedSSL;
@property(nonatomic, strong, nullable) RTCJFRSecurity *security;
@property(nonatomic, strong, nullable) dispatch_queue_t queue;

/**
 Block property to use on connect.
 */
@property(nonatomic, copy, nullable) void (^onConnect)(void);

/**
 Block property to use on disconnect.
 */
@property(nonatomic, copy, nullable) void (^onDisconnect)(NSError* _Nullable);

/**
 Block property to use on receiving data.
 */
@property(nonatomic, copy, nullable) void (^onData)(NSData* _Nullable);

/**
 Block property to use on receiving text.
 */
@property(nonatomic, copy, nullable) void (^onText)(NSString* _Nullable);

/**
 constructor to create a new websocket.
 @param url       the host you want to connect to.
 @param protocols the websocket protocols you want to use (e.g. chat,superchat).
 @return a newly initalized websocket.
 */
- (instancetype)initWithURL:(NSURL *)url protocols:(nullable NSArray<NSString*>*)protocols;

/**
 connect to the host.
 */
- (void)connect;

/**
 disconnect to the host. This sends the close Connection opcode to terminate cleanly.
 */
- (void)disconnect;

/**
 write binary based data to the socket.
 @param data the binary data to write.
 */
- (void)writeData:(NSData*)data;

/**
 write text based data to the socket.
 @param string the string to write.
 */
- (void)writeString:(NSString*)string;

/**
 write ping to the socket.
 @param data the binary data to write (if desired).
 */
- (void)writePing:(nullable NSData*)data;

/**
 Add a header to send along on the the HTTP connect.
 @param value the string to send
 @param key   the HTTP key name to send
 */
- (void)addHeader:(NSString*)value forKey:(NSString*)key;

@end

NS_ASSUME_NONNULL_END
