//
//  ViewController.m
//  VPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "ViewController.h"

#import<VPSocketIO/RTCVPSocketIO.h>

#ifndef WEAKSELF
#define WEAKSELF __weak __typeof(&*self)weakSelf = self;
#endif
#ifndef STRONGSELF
#define STRONGSELF __strong __typeof(&*weakSelf)strongSelf = weakSelf;
#endif

@interface ViewController ()

@property(nonatomic, strong) RTCVPSocketIOClient *socket;
@property(nonatomic, strong) dispatch_queue_t currentEngineProtooQueue;
@property(nonatomic, strong) NSString *clientId;

// UI Elements
@property(nonatomic, strong) UIView *statusView;
@property(nonatomic, strong) UILabel *statusLabel;
@property(nonatomic, strong) UITextView *messageTextView;
@property(nonatomic, strong) UITextField *inputTextField;
@property(nonatomic, strong) UIButton *sendButton;
@property(nonatomic, strong) UIButton *connectButton;
@property(nonatomic, strong) UIButton *disconnectButton;
@property(nonatomic, strong) UIView *inputContainerView;

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    _currentEngineProtooQueue = dispatch_queue_create("com.vrv.mediasoupProtoo", DISPATCH_QUEUE_SERIAL);

    // 设置导航栏
    self.title = @"Socket.IO Chat";
    
    // 创建UI
    [self createUI];
    
    // 初始化Socket客户端
    [self setupSocket];
    
    // 添加键盘通知
    [self setupKeyboardNotifications];
}

- (void)createUI {
    // 状态视图
    self.statusView = [[UIView alloc] initWithFrame:CGRectZero];
    self.statusView.translatesAutoresizingMaskIntoConstraints = NO;
    self.statusView.backgroundColor = [UIColor systemRedColor];
    self.statusView.layer.cornerRadius = 8.0;
    self.statusView.clipsToBounds = YES;
    
    self.statusLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    self.statusLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.statusLabel.text = @"未连接";
    self.statusLabel.textColor = [UIColor whiteColor];
    self.statusLabel.font = [UIFont systemFontOfSize:14.0 weight:UIFontWeightSemibold];
    [self.statusView addSubview:self.statusLabel];
    
    // 消息文本视图
    self.messageTextView = [[UITextView alloc] initWithFrame:CGRectZero];
    self.messageTextView.translatesAutoresizingMaskIntoConstraints = NO;
    self.messageTextView.editable = NO;
    self.messageTextView.font = [UIFont systemFontOfSize:16.0];
    self.messageTextView.backgroundColor = [UIColor groupTableViewBackgroundColor];
    self.messageTextView.textContainerInset = UIEdgeInsetsMake(10, 10, 10, 10);
    self.messageTextView.layer.cornerRadius = 8.0;
    self.messageTextView.clipsToBounds = YES;
    
    // 输入容器视图
    self.inputContainerView = [[UIView alloc] initWithFrame:CGRectZero];
    self.inputContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    self.inputContainerView.backgroundColor = [UIColor whiteColor];
    self.inputContainerView.layer.cornerRadius = 8.0;
    self.inputContainerView.clipsToBounds = YES;
    
    // 输入文本框
    self.inputTextField = [[UITextField alloc] initWithFrame:CGRectZero];
    self.inputTextField.translatesAutoresizingMaskIntoConstraints = NO;
    self.inputTextField.placeholder = @"Type a message...";
    self.inputTextField.font = [UIFont systemFontOfSize:16.0];
    self.inputTextField.enabled = NO;
    [self.inputTextField addTarget:self action:@selector(inputTextFieldReturn:) forControlEvents:UIControlEventEditingDidEndOnExit];
    
    // 发送按钮
    self.sendButton = [[UIButton alloc] initWithFrame:CGRectZero];
    self.sendButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.sendButton setTitle:@"Send" forState:UIControlStateNormal];
    [self.sendButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    [self.sendButton setBackgroundColor:[UIColor systemBlueColor]];
    [self.sendButton addTarget:self action:@selector(sendButtonTapped:) forControlEvents:UIControlEventTouchUpInside];
    self.sendButton.layer.cornerRadius = 8.0;
    self.sendButton.clipsToBounds = YES;
    self.sendButton.enabled = NO;
    
    // 连接按钮
    self.connectButton = [[UIButton alloc] initWithFrame:CGRectZero];
    self.connectButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.connectButton setTitle:@"Connect" forState:UIControlStateNormal];
    [self.connectButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    [self.connectButton setBackgroundColor:[UIColor systemGreenColor]];
    [self.connectButton addTarget:self action:@selector(connectButtonTapped:) forControlEvents:UIControlEventTouchUpInside];
    self.connectButton.layer.cornerRadius = 8.0;
    self.connectButton.clipsToBounds = YES;
    
    // 断开连接按钮
    self.disconnectButton = [[UIButton alloc] initWithFrame:CGRectZero];
    self.disconnectButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.disconnectButton setTitle:@"Disconnect" forState:UIControlStateNormal];
    [self.disconnectButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    [self.disconnectButton setBackgroundColor:[UIColor systemRedColor]];
    [self.disconnectButton addTarget:self action:@selector(disconnectButtonTapped:) forControlEvents:UIControlEventTouchUpInside];
    self.disconnectButton.layer.cornerRadius = 8.0;
    self.disconnectButton.clipsToBounds = YES;
    self.disconnectButton.enabled = NO;
    
    // 添加子视图
    [self.inputContainerView addSubview:self.inputTextField];
    [self.inputContainerView addSubview:self.sendButton];
    
    [self.view addSubview:self.statusView];
    [self.view addSubview:self.messageTextView];
    [self.view addSubview:self.inputContainerView];
    [self.view addSubview:self.connectButton];
    [self.view addSubview:self.disconnectButton];
    
    // 设置约束
    [NSLayoutConstraint activateConstraints:@[
        // 状态视图
        [self.statusView.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor constant:10],
        [self.statusView.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor constant:10],
        [self.statusView.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor constant:-10],
        [self.statusView.heightAnchor constraintEqualToConstant:40],
        
        // 状态标签
        [self.statusLabel.centerXAnchor constraintEqualToAnchor:self.statusView.centerXAnchor],
        [self.statusLabel.centerYAnchor constraintEqualToAnchor:self.statusView.centerYAnchor],
        
        // 消息文本视图
        [self.messageTextView.topAnchor constraintEqualToAnchor:self.statusView.bottomAnchor constant:10],
        [self.messageTextView.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor constant:10],
        [self.messageTextView.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor constant:-10],
        
        // 输入容器视图
        [self.inputContainerView.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor constant:10],
        [self.inputContainerView.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor constant:-10],
        [self.inputContainerView.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor constant:-10],
        [self.inputContainerView.heightAnchor constraintEqualToConstant:60],
        
        // 输入文本框
        [self.inputTextField.leadingAnchor constraintEqualToAnchor:self.inputContainerView.leadingAnchor constant:10],
        [self.inputTextField.topAnchor constraintEqualToAnchor:self.inputContainerView.topAnchor constant:10],
        [self.inputTextField.bottomAnchor constraintEqualToAnchor:self.inputContainerView.bottomAnchor constant:-10],
        [self.inputTextField.trailingAnchor constraintEqualToAnchor:self.sendButton.leadingAnchor constant:-10],
        
        // 发送按钮
        [self.sendButton.trailingAnchor constraintEqualToAnchor:self.inputContainerView.trailingAnchor constant:-10],
        [self.sendButton.centerYAnchor constraintEqualToAnchor:self.inputContainerView.centerYAnchor],
        [self.sendButton.widthAnchor constraintEqualToConstant:80],
        [self.sendButton.heightAnchor constraintEqualToConstant:40],
        
        // 连接按钮
        [self.connectButton.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor constant:10],
        [self.connectButton.topAnchor constraintEqualToAnchor:self.messageTextView.bottomAnchor constant:10],
        [self.connectButton.widthAnchor constraintEqualToConstant:120],
        [self.connectButton.heightAnchor constraintEqualToConstant:40],
        [self.connectButton.bottomAnchor constraintEqualToAnchor:self.inputContainerView.topAnchor constant:-10],
        
        // 断开连接按钮
        [self.disconnectButton.leadingAnchor constraintEqualToAnchor:self.connectButton.trailingAnchor constant:10],
        [self.disconnectButton.topAnchor constraintEqualToAnchor:self.messageTextView.bottomAnchor constant:10],
        [self.disconnectButton.widthAnchor constraintEqualToConstant:120],
        [self.disconnectButton.heightAnchor constraintEqualToConstant:40],
        [self.disconnectButton.bottomAnchor constraintEqualToAnchor:self.inputContainerView.topAnchor constant:-10],
    ]];
}

- (void)setupKeyboardNotifications {
    // 监听键盘显示和隐藏通知
    [[NSNotificationCenter defaultCenter] addObserver:self 
                                             selector:@selector(keyboardWillShow:) 
                                                 name:UIKeyboardWillShowNotification 
                                               object:nil];
    
    [[NSNotificationCenter defaultCenter] addObserver:self 
                                             selector:@selector(keyboardWillHide:) 
                                                 name:UIKeyboardWillHideNotification 
                                               object:nil];
}

- (void)keyboardWillShow:(NSNotification *)notification {
    // 键盘显示时，调整输入容器视图的位置
    NSDictionary *info = [notification userInfo];
    CGRect keyboardFrame = [[info objectForKey:UIKeyboardFrameEndUserInfoKey] CGRectValue];
    CGFloat keyboardHeight = keyboardFrame.size.height;
    
    // 移除所有现有的底部约束
    for (NSLayoutConstraint *constraint in self.inputContainerView.constraints) {
        if (constraint.firstAttribute == NSLayoutAttributeBottom) {
            constraint.active = NO;
        }
    }
    
    [UIView animateWithDuration:0.3 animations:^{        
        // 添加新的底部约束
        [NSLayoutConstraint activateConstraints:@[
            [self.inputContainerView.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor constant:-(10 + keyboardHeight)]
        ]];
        [self.view layoutIfNeeded];
    }];
}

- (void)keyboardWillHide:(NSNotification *)notification {
    // 键盘隐藏时，恢复输入容器视图的位置
    
    // 移除所有现有的底部约束
    for (NSLayoutConstraint *constraint in self.inputContainerView.constraints) {
        if (constraint.firstAttribute == NSLayoutAttributeBottom) {
            constraint.active = NO;
        }
    }
    
    [UIView animateWithDuration:0.3 animations:^{        
        // 添加新的底部约束
        [NSLayoutConstraint activateConstraints:@[
            [self.inputContainerView.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor constant:-10]
        ]];
        [self.view layoutIfNeeded];
    }];
}

- (void)setupSocket {
    // 使用电脑的实际IP地址连接（HTTP）
    NSString *urlString = @"http://192.168.141.149:3000";
    
    // 连接参数
    NSDictionary *connectParams = @{
                                    @"version_name":@"3.2.1",
                                    @"version_code":@"43234",
                                    @"platform":@"iOS",
                                    @"mac":@"ff:44:55:dd:88",
                                    @"resolution":@"1820*1080"
    };
    
    // 日志配置
    RTCVPSocketLogger *logger = [[RTCVPSocketLogger alloc]init];
    [logger onLogMsgWithCB:^(NSString *message, NSString *type) {
        NSLog(@"[%@] %@", type, message);
    }];
    
    // 创建配置对象
    RTCVPSocketIOConfig *config = [RTCVPSocketIOConfig configWithBlock:^(RTCVPSocketIOConfig *config) {
        config.loggingEnabled = YES;
        config.reconnectionEnabled = YES;
        config.reconnectionAttempts = 3;
        config.secure = NO;
        config.forceNewConnection = YES;
        config.allowSelfSignedCertificates = YES;
        config.ignoreSSLErrors = NO;
        config.reconnectionDelay = 2;
        config.connectTimeout = 15; // 增加连接超时时间
        config.namespace = @"/";
        config.connectParams = connectParams;
        config.logger = logger;
        config.handleQueue = self->_currentEngineProtooQueue;
        // 使用轮询传输，避免WebSocket控制帧碎片问题
        config.protocolVersion = RTCVPSocketIOProtocolVersion2; // Socket.IO 2.x
        config.transport = RTCVPSocketIOTransportPolling; // 直接指定轮询传输，无需额外配置
    }];
    
    // 创建Socket客户端
    self.socket = [[RTCVPSocketIOClient alloc] initWithSocketURL:[NSURL URLWithString:urlString] config:config];
    
   WEAKSELF
    
    // 监听连接事件
    [_socket on:kSocketEventConnect callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
        STRONGSELF
        [strongSelf updateStatus:YES];
        [strongSelf addMessage:@"✅ 连接成功" type:@"system"];
    }];
    
    // 监听断开连接事件
    [_socket on:kSocketEventDisconnect callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
        STRONGSELF
        [strongSelf updateStatus:NO];
        [strongSelf addMessage:[NSString stringWithFormat:@"❌ 断开连接: %@", array] type:@"system"];
    }];
    
    // 监听错误事件
    [_socket on:kSocketEventError callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
        STRONGSELF
        [strongSelf addMessage:[NSString stringWithFormat:@"⚠️ 连接出错: %@", array] type:@"system"];
    }];
    
    // 监听欢迎消息
    [_socket on:@"welcome" callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
        STRONGSELF
        if (array.count > 0) {
            id data = array.firstObject;
            if ([data isKindOfClass:[NSDictionary class]]) {
                NSDictionary *welcomeData = (NSDictionary *)data;
                strongSelf.clientId = welcomeData[@"socketId"];
                [strongSelf addMessage:[NSString stringWithFormat:@"📩 欢迎: %@", welcomeData[@"message"]] type:@"received"];
            }
        }
    }];
    
    // 监听用户连接事件
    [_socket on:@"userConnected" callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
        STRONGSELF
        if (array.count > 0) {
            id data = array.firstObject;
            if ([data isKindOfClass:[NSDictionary class]]) {
                NSDictionary *userData = (NSDictionary *)data;
                [strongSelf addMessage:[NSString stringWithFormat:@"👤 用户加入: %@", userData[@"socketId"]] type:@"system"];
            }
        }
    }];
    
    // 监听用户断开连接事件
    [_socket on:@"userDisconnected" callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
        STRONGSELF
        if (array.count > 0) {
            id data = array.firstObject;
            if ([data isKindOfClass:[NSDictionary class]]) {
                NSDictionary *userData = (NSDictionary *)data;
                [strongSelf addMessage:[NSString stringWithFormat:@"👤 用户离开: %@", userData[@"socketId"]] type:@"system"];
            }
        }
    }];
    
    // 监听聊天消息
    [_socket on:@"chatMessage" callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
        STRONGSELF
        if (array.count > 0) {
            id data = array.firstObject;
            if ([data isKindOfClass:[NSDictionary class]]) {
                NSDictionary *messageData = (NSDictionary *)data;
                NSString *sender = messageData[@"sender"];
                NSString *message = messageData[@"message"];
                
                if ([sender isEqualToString:strongSelf.clientId]) {
                    [strongSelf addMessage:message type:@"sent"];
                } else {
                    [strongSelf addMessage:[NSString stringWithFormat:@"%@: %@", sender, message] type:@"received"];
                }
            }
        }
    }];
    
    // 监听心跳消息
    [_socket on:@"heartbeat" callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
        // 心跳消息不显示在UI上
        NSLog(@"💓 收到心跳消息: %@", array);
    }];
}

- (void)updateStatus:(BOOL)connected {
    dispatch_async(dispatch_get_main_queue(), ^{        
        self.connectButton.enabled = !connected;
        self.disconnectButton.enabled = connected;
        self.sendButton.enabled = connected;
        self.inputTextField.enabled = connected;
        
        if (connected) {
            self.statusLabel.text = @"已连接";
            self.statusView.backgroundColor = [UIColor systemGreenColor];
        } else {
            self.statusLabel.text = @"未连接";
            self.statusView.backgroundColor = [UIColor systemRedColor];
        }
    });
}

- (void)addMessage:(NSString *)message type:(NSString *)type {
    dispatch_async(dispatch_get_main_queue(), ^{        
        // 格式化为带时间的消息
        NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
        [dateFormatter setDateFormat:@"HH:mm:ss"];
        NSString *timeString = [dateFormatter stringFromDate:[NSDate date]];
        NSString *formattedMessage = [NSString stringWithFormat:@"[%@] %@\n", timeString, message];
        
        // 追加到消息文本视图
        NSMutableString *currentText = [NSMutableString stringWithString:self.messageTextView.text];
        [currentText appendString:formattedMessage];
        self.messageTextView.text = currentText;
        
        // 滚动到底部
        [self.messageTextView scrollRangeToVisible:NSMakeRange([self.messageTextView.text length], 0)];
    });
}

- (void)sendButtonTapped:(id)sender {
    [self sendMessage:self.inputTextField.text];
}

- (void)connectButtonTapped:(id)sender {
    // 连接到服务器
    NSLog(@"📞 连接按钮点击，开始连接到服务器");
    [self addMessage:@"🔄 正在连接服务器..." type:@"system"];
    
    // 增加连接超时时间到15秒
    [self.socket connectWithTimeoutAfter:15 withHandler:^{        
        NSLog(@"⏱️ 连接超时回调触发");
        [self addMessage:@"⏱️ 连接超时" type:@"system"];
    }];
    
    NSLog(@"� 连接方法调用完成，等待连接结果");
}

- (void)disconnectButtonTapped:(id)sender {
    // 断开连接
    [self.socket disconnect];
    [self addMessage:@"🔄 正在断开连接..." type:@"system"];
}

- (void)inputTextFieldReturn:(id)sender {
    [self sendMessage:self.inputTextField.text];
}

- (void)sendMessage:(NSString *)message {
    if (!message || message.length == 0) {
        return;
    }
    
    // 清空输入框
    self.inputTextField.text = @"";
    
    // 确保Socket已连接
    if (self.socket.status == RTCVPSocketIOClientStatusConnected || self.socket.status == RTCVPSocketIOClientStatusOpened) {
        // 发送消息
        [self.socket emit:@"chatMessage" items:@[@{        
            @"message": message,
            @"timestamp": @([NSDate date].timeIntervalSince1970)
        }]];
        
    } else {
        [self addMessage:@"⚠️ Socket尚未完全连接" type:@"system"];
    }
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)dealloc {
    // 移除键盘通知
    [[NSNotificationCenter defaultCenter] removeObserver:self name:UIKeyboardWillShowNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:UIKeyboardWillHideNotification object:nil];
}

@end
