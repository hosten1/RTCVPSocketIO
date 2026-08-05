#import <Foundation/Foundation.h>
#import "RTCVPSocketIOClient.h"
#import "RTCVPSocketIOConfig.h"

@interface SocketIODemo : NSObject

@property (nonatomic, strong) RTCVPSocketIOClient *client;
@property (nonatomic, strong) NSString *currentServer;
@property (nonatomic, assign) BOOL running;

- (void)start;
- (void)printMenu;
- (void)connectToServer:(NSString *)url version:(RTCVPSocketIOProtocolVersion)version;
- (void)disconnect;
- (void)sendTextMessage;
- (void)sendBinaryMessage;
- (void)sendAckMessage;
- (void)joinRoom;
- (void)sendRoomMessage;

@end

@implementation SocketIODemo

- (instancetype)init {
    self = [super init];
    if (self) {
        _running = YES;
    }
    return self;
}

- (void)start {
    NSLog(@"\n========================================");
    NSLog(@"  Socket.IO Mac Demo");
    NSLog(@"========================================");
    
    [self setupEventHandlers];
    [self printMenu];
    
    char buf[256];
    while (_running) {
        printf("\n请选择: ");
        fgets(buf, sizeof(buf), stdin);
        int choice = atoi(buf);
        
        switch (choice) {
            case 1:
                [self connectToServer:@"http://localhost:3002" version:RTCVPSocketIOProtocolVersion2];
                break;
            case 2:
                [self connectToServer:@"http://localhost:3003" version:RTCVPSocketIOProtocolVersion3];
                break;
            case 3:
                [self sendTextMessage];
                break;
            case 4:
                [self sendBinaryMessage];
                break;
            case 5:
                [self sendAckMessage];
                break;
            case 6:
                [self joinRoom];
                break;
            case 7:
                [self sendRoomMessage];
                break;
            case 8:
                [self disconnect];
                break;
            case 9:
                _running = NO;
                [self disconnect];
                NSLog(@"再见!");
                break;
            default:
                [self printMenu];
                break;
        }
    }
}

- (void)printMenu {
    printf("\n=== 菜单 ===\n");
    printf("1. 连接 V2 服务器 (localhost:3002)\n");
    printf("2. 连接 V3 服务器 (localhost:3003)\n");
    printf("3. 发送文本消息 (chat message)\n");
    printf("4. 发送二进制消息\n");
    printf("5. 发送带 ACK 的消息\n");
    printf("6. 加入房间\n");
    printf("7. 发送房间消息\n");
    printf("8. 断开连接\n");
    printf("9. 退出\n");
}

- (void)setupEventHandlers {
    // 由 connectToServer 方法设置
}

- (void)connectToServer:(NSString *)url version:(RTCVPSocketIOProtocolVersion)version {
    if (self.client) {
        [self.client disconnect];
        self.client = nil;
    }
    
    NSString *versionStr = (version == RTCVPSocketIOProtocolVersion2) ? @"V2" : @"V3";
    NSLog(@"\n[连接] 正在连接 %@ (%@)...", url, versionStr);
    
    RTCVPSocketIOConfig *config = [RTCVPSocketIOConfig defaultConfig];
    config.protocolVersion = version;
    config.transport = RTCVPSocketIOTransportWebSocket;
    config.connectTimeout = 10;
    config.reconnectionEnabled = NO;
    config.loggingEnabled = YES;
    config.logLevel = 1;
    
    self.client = [[RTCVPSocketIOClient alloc] initWithSocketURL:[NSURL URLWithString:url] config:config];
    
    __weak typeof(self) weakSelf = self;
    
    [self.client on:@"connect" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
        NSLog(@"\n✅ 连接成功!");
    }];
    
    [self.client on:@"disconnect" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
        NSLog(@"\n❌ 断开连接: %@", data.firstObject ?: @"未知原因");
    }];
    
    [self.client on:@"error" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
        NSLog(@"\n⚠️  错误: %@", data);
    }];
    
    [self.client on:@"welcome" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
        if (data.count > 0 && [data[0] isKindOfClass:[NSDictionary class]]) {
            NSDictionary *welcome = data[0];
            NSLog(@"\n👋 欢迎消息: %@", welcome[@"message"]);
            NSLog(@"   客户端ID: %@", welcome[@"clientId"]);
        }
    }];
    
    [self.client on:@"chat message" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
        if (data.count > 0 && [data[0] isKindOfClass:[NSDictionary class]]) {
            NSDictionary *msg = data[0];
            NSLog(@"\n💬 聊天消息 - 来自: %@\n   内容: %@", 
                  msg[@"from"] ?: @"未知", 
                  msg[@"content"] ?: @"");
        }
    }];
    
    [self.client on:@"user joined" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
        if (data.count > 0 && [data[0] isKindOfClass:[NSDictionary class]]) {
            NSDictionary *info = data[0];
            NSLog(@"\n🚪 用户 %@ 加入了房间 %@", info[@"user"], info[@"room"]);
        }
    }];
    
    [self.client on:@"room message" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
        if (data.count > 0 && [data[0] isKindOfClass:[NSDictionary class]]) {
            NSDictionary *msg = data[0];
            NSLog(@"\n🏠 房间消息 [%@] - 来自: %@\n   内容: %@", 
                  msg[@"room"], msg[@"from"], msg[@"content"]);
        }
    }];
    
    [self.client connect];
    _currentServer = url;
}

- (void)disconnect {
    if (self.client) {
        NSLog(@"\n[断开] 正在断开连接...");
        [self.client disconnect];
        self.client = nil;
        _currentServer = nil;
    } else {
        NSLog(@"\n⚠️  当前没有连接");
    }
}

- (void)sendTextMessage {
    if (!self.client) {
        NSLog(@"\n⚠️  请先连接服务器");
        return;
    }
    
    char buf[512];
    printf("输入消息内容: ");
    fgets(buf, sizeof(buf), stdin);
    NSString *message = [NSString stringWithUTF8String:buf];
    message = [message stringByTrimmingCharactersInSet:[NSCharacterSet newlineCharacterSet]];
    
    if (message.length == 0) {
        message = @"Hello from Mac Demo!";
    }
    
    NSLog(@"\n[发送] 文本消息: %@", message);
    [self.client emit:@"chat message" items:@[message]];
}

- (void)sendBinaryMessage {
    if (!self.client) {
        NSLog(@"\n⚠️  请先连接服务器");
        return;
    }
    
    NSLog(@"\n[发送] 二进制消息 (1KB)...");
    
    NSMutableData *binaryData = [NSMutableData dataWithLength:1024];
    uint8_t *bytes = (uint8_t *)binaryData.mutableBytes;
    for (int i = 0; i < 1024; i++) {
        bytes[i] = i % 256;
    }
    
    [self.client emitWithAck:@"binary test" items:@[binaryData] ackBlock:^(NSArray *data, NSError *error) {
        if (error) {
            NSLog(@"\n❌ 二进制消息发送失败: %@", error.localizedDescription);
        } else if (data.count > 0 && [data[0] isKindOfClass:[NSDictionary class]]) {
            NSDictionary *result = data[0];
            NSLog(@"\n✅ 二进制消息发送成功!");
            NSLog(@"   大小: %@ bytes", result[@"size"]);
            NSLog(@"   前5字节: %@", result[@"firstBytes"]);
        }
    } timeout:5];
}

- (void)sendAckMessage {
    if (!self.client) {
        NSLog(@"\n⚠️  请先连接服务器");
        return;
    }
    
    NSDictionary *testData = @{
        @"foo": @"bar",
        @"num": @42,
        @"array": @[@1, @2, @3],
        @"nested": @{@"key": @"value"}
    };
    
    NSLog(@"\n[发送] 带 ACK 的 echo 消息...");
    
    [self.client emitWithAck:@"echo" items:@[testData] ackBlock:^(NSArray *data, NSError *error) {
        if (error) {
            NSLog(@"\n❌ ACK 失败: %@", error.localizedDescription);
        } else if (data.count > 0 && [data[0] isKindOfClass:[NSDictionary class]]) {
            NSDictionary *result = data[0];
            NSLog(@"\n✅ ACK 成功!");
            NSLog(@"   服务器: %@", result[@"server"]);
            NSLog(@"   回显数据: %@", result[@"echoed"]);
        }
    } timeout:5];
}

- (void)joinRoom {
    if (!self.client) {
        NSLog(@"\n⚠️  请先连接服务器");
        return;
    }
    
    char buf[256];
    printf("输入房间名 (默认: demo-room): ");
    fgets(buf, sizeof(buf), stdin);
    NSString *room = [NSString stringWithUTF8String:buf];
    room = [room stringByTrimmingCharactersInSet:[NSCharacterSet newlineCharacterSet]];
    
    if (room.length == 0) {
        room = @"demo-room";
    }
    
    NSLog(@"\n[房间] 加入房间: %@", room);
    [self.client emit:@"join room" items:@[room]];
}

- (void)sendRoomMessage {
    if (!self.client) {
        NSLog(@"\n⚠️  请先连接服务器");
        return;
    }
    
    char roomBuf[256];
    char msgBuf[512];
    
    printf("输入房间名: ");
    fgets(roomBuf, sizeof(roomBuf), stdin);
    NSString *room = [NSString stringWithUTF8String:roomBuf];
    room = [room stringByTrimmingCharactersInSet:[NSCharacterSet newlineCharacterSet]];
    
    if (room.length == 0) {
        room = @"demo-room";
    }
    
    printf("输入消息: ");
    fgets(msgBuf, sizeof(msgBuf), stdin);
    NSString *message = [NSString stringWithUTF8String:msgBuf];
    message = [message stringByTrimmingCharactersInSet:[NSCharacterSet newlineCharacterSet]];
    
    if (message.length == 0) {
        message = @"Hello from room!";
    }
    
    NSDictionary *data = @{@"room": room, @"message": message};
    NSLog(@"\n[房间] 向房间 %@ 发送: %@", room, message);
    [self.client emit:@"room message" items:@[data]];
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        SocketIODemo *demo = [[SocketIODemo alloc] init];
        [demo start];
    }
    return 0;
}
