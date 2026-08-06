#import "ViewController.h"
#import <LYMVPSocketIO/RTCVPSocketIOClient.h>
#import <LYMVPSocketIO/RTCVPSocketIOConfig.h>

@interface ViewController ()

@property (nonatomic, strong) UITextView *logTextView;
@property (nonatomic, strong) RTCVPSocketIOClient *client;

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor whiteColor];
    self.title = @"Socket.IO iOS Demo";
    
    [self setupUI];
    [self log:@"iOS Demo 启动成功!"];
    [self testSocketIO];
}

- (void)setupUI {
    CGFloat padding = 20.0;
    CGRect logFrame = CGRectMake(padding, 100, self.view.bounds.size.width - padding * 2, self.view.bounds.size.height - 120);
    
    self.logTextView = [[UITextView alloc] initWithFrame:logFrame];
    self.logTextView.editable = NO;
    self.logTextView.font = [UIFont systemFontOfSize:14];
    self.logTextView.backgroundColor = [UIColor colorWithWhite:0.95 alpha:1.0];
    [self.view addSubview:self.logTextView];
}

- (void)log:(NSString *)message {
    NSLog(@"[iOSDemo] %@", message);
    NSString *timestamp = [NSDateFormatter localizedStringFromDate:[NSDate date] dateStyle:NSDateFormatterNoStyle timeStyle:NSDateFormatterMediumStyle];
    NSString *logMessage = [NSString stringWithFormat:@"[%@] %@\n", timestamp, message];
    self.logTextView.text = [self.logTextView.text stringByAppendingString:logMessage];
}

- (void)testSocketIO {
    [self log:@"正在初始化 Socket.IO 客户端..."];
    
    RTCVPSocketIOConfig *config = [RTCVPSocketIOConfig defaultConfig];
    config.protocolVersion = RTCVPSocketIOProtocolVersion3;
    config.transport = RTCVPSocketIOTransportWebSocket;
    config.connectTimeout = 10;
    config.reconnectionEnabled = NO;
    config.loggingEnabled = YES;
    config.logLevel = 1;
    
    NSURL *url = [NSURL URLWithString:@"http://localhost:3003"];
    self.client = [[RTCVPSocketIOClient alloc] initWithSocketURL:url config:config];
    
    __weak typeof(self) weakSelf = self;
    
    [self.client on:@"connect" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
        [weakSelf log:@"✅ 连接成功!"];
    }];
    
    [self.client on:@"disconnect" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
        [weakSelf log:[NSString stringWithFormat:@"❌ 断开连接: %@", data.firstObject ?: @"未知原因"]];
    }];
    
    [self.client on:@"error" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
        [weakSelf log:[NSString stringWithFormat:@"⚠️  错误: %@", data]];
    }];
    
    [self log:@"Socket.IO 客户端初始化完成 ✅"];
    [self log:@"Framework 链接验证通过!"];
}

@end
