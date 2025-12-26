//
//  ViewController.m
//  VPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "ViewController.h"

#import<LYMVPSocketIO/LYMVPSocketIO.h>

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
@property(nonatomic, strong) UIButton *ackTestButton;
@property(nonatomic, strong) UIView *inputContainerView;

// 新增的下拉选项
@property(nonatomic, strong) UILabel *protocolLabel;
@property(nonatomic, strong) UISegmentedControl *protocolSegment;
@property(nonatomic, strong) UILabel *transportLabel;
@property(nonatomic, strong) UISegmentedControl *transportSegment;

@property(nonatomic, strong)UIColor *connBtnBC;
@property(nonatomic, strong)UIColor *disconnectBtnBC ;
@property(nonatomic, strong)UIColor *sendBtnBC ;
@property(nonatomic, strong)UIColor *inputTFBC;
@property(nonatomic, strong)UIColor *ackTestBtnBC;

// 新增的二进制相关按钮
@property(nonatomic, strong)UIButton *sendBinaryButton;
@property(nonatomic, strong)UIButton *binaryAckTestButton;

@end

@implementation ViewController

// 静态PNG图片数据（16x16像素的透明PNG）
static const uint8_t image_data[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x91, 0x68,
    0x36, 0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xAE, 0xCE, 0x1C, 0xE9, 0x00, 0x00,
    0x00, 0x04, 0x67, 0x41, 0x4D, 0x41, 0x00, 0x00, 0xB1, 0x8F, 0x0B, 0xFC, 0x61, 0x05, 0x00, 0x00,
    0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0E, 0xC3, 0x00, 0x00, 0x0E, 0xC3, 0x01, 0xC7,
    0x6F, 0xA8, 0x64, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54, 0x28, 0x53, 0x63, 0xFC, 0xFF,
    0xFF, 0x3F, 0x03, 0x0D, 0x00, 0x13, 0x03, 0x0D, 0x01, 0x00, 0x04, 0xA0, 0x02, 0xF5, 0xE2, 0xE0,
    0x30, 0x31, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
};

- (void)viewDidLoad {
    [super viewDidLoad];
    _currentEngineProtooQueue = dispatch_queue_create("com.vrv.mediasoupProtoo", DISPATCH_QUEUE_SERIAL);

    // 设置导航栏
    self.title = @"Socket.IO Chat";
    
    // 创建UI
    [self createUI];
    
//    // 初始化Socket客户端
//    [self setupSocket];
    
    // 添加键盘通知
    [self setupKeyboardNotifications];
    
    // 添加点击外部键盘收回手势
    UITapGestureRecognizer *tapGesture = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(dismissKeyboard)];
    tapGesture.cancelsTouchesInView = NO; // 允许子视图的触摸事件
    [self.view addGestureRecognizer:tapGesture];
}

// 点击外部收回键盘
- (void)dismissKeyboard {
    [self.view endEditing:YES];
}

- (void)createUI {
    self.sendBtnBC = [UIColor systemBlueColor];
    self.connBtnBC = [UIColor systemGreenColor];
    self.disconnectBtnBC = [UIColor systemRedColor];
    self.ackTestBtnBC = [UIColor systemPurpleColor];
    self.inputTFBC = [UIColor whiteColor];
    
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
    
    // 协议版本选择
    self.protocolLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    self.protocolLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.protocolLabel.text = @"协议版本:";
    self.protocolLabel.font = [UIFont systemFontOfSize:14.0 weight:UIFontWeightMedium];
    
    self.protocolSegment = [[UISegmentedControl alloc] initWithItems:@[@"v2", @"v3"]];
    self.protocolSegment.translatesAutoresizingMaskIntoConstraints = NO;
    self.protocolSegment.selectedSegmentIndex = 1; // 默认v3
    
    // 传输方式选择
    self.transportLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    self.transportLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.transportLabel.text = @"传输方式:";
    self.transportLabel.font = [UIFont systemFontOfSize:14.0 weight:UIFontWeightMedium];
    
    self.transportSegment = [[UISegmentedControl alloc] initWithItems:@[@"轮询(Polling)", @"WebSocket"]];
    self.transportSegment.translatesAutoresizingMaskIntoConstraints = NO;
    self.transportSegment.selectedSegmentIndex = 1; // 默认WebSocket
    
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
    [self.sendButton setBackgroundColor:self.sendBtnBC];
    [self.sendButton addTarget:self action:@selector(sendButtonTapped:) forControlEvents:UIControlEventTouchUpInside];
    self.sendButton.layer.cornerRadius = 8.0;
    self.sendButton.clipsToBounds = YES;
    self.sendButton.enabled = NO;
    
    // 连接按钮
    self.connectButton = [[UIButton alloc] initWithFrame:CGRectZero];
    self.connectButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.connectButton setTitle:@"Connect" forState:UIControlStateNormal];
    [self.connectButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    [self.connectButton setBackgroundColor:_connBtnBC];
    [self.connectButton addTarget:self action:@selector(connectButtonTapped:) forControlEvents:UIControlEventTouchUpInside];
    self.connectButton.layer.cornerRadius = 8.0;
    self.connectButton.clipsToBounds = YES;
    
    // 断开连接按钮
    self.disconnectButton = [[UIButton alloc] initWithFrame:CGRectZero];
    self.disconnectButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.disconnectButton setTitle:@"Disconnect" forState:UIControlStateNormal];
    [self.disconnectButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    [self.disconnectButton setBackgroundColor:_disconnectBtnBC];
    [self.disconnectButton addTarget:self action:@selector(disconnectButtonTapped:) forControlEvents:UIControlEventTouchUpInside];
    self.disconnectButton.layer.cornerRadius = 8.0;
    self.disconnectButton.clipsToBounds = YES;
    self.disconnectButton.enabled = NO;
    
    // ACK并发测试按钮
    self.ackTestButton = [[UIButton alloc] initWithFrame:CGRectZero];
    self.ackTestButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.ackTestButton setTitle:@"ACK Test" forState:UIControlStateNormal];
    [self.ackTestButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    [self.ackTestButton setBackgroundColor:_ackTestBtnBC];
    [self.ackTestButton addTarget:self action:@selector(ackTestButtonTapped:) forControlEvents:UIControlEventTouchUpInside];
    self.ackTestButton.layer.cornerRadius = 8.0;
    self.ackTestButton.clipsToBounds = YES;
    self.ackTestButton.enabled = NO;
    
    // 发送二进制消息按钮
    self.sendBinaryButton = [[UIButton alloc] initWithFrame:CGRectZero];
    self.sendBinaryButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.sendBinaryButton setTitle:@"Send Binary" forState:UIControlStateNormal];
    [self.sendBinaryButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    [self.sendBinaryButton setBackgroundColor:[UIColor systemTealColor]];
    [self.sendBinaryButton addTarget:self action:@selector(sendBinaryButtonTapped:) forControlEvents:UIControlEventTouchUpInside];
    self.sendBinaryButton.layer.cornerRadius = 8.0;
    self.sendBinaryButton.clipsToBounds = YES;
    self.sendBinaryButton.enabled = NO;
    
    // 二进制ACK测试按钮
    self.binaryAckTestButton = [[UIButton alloc] initWithFrame:CGRectZero];
    self.binaryAckTestButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.binaryAckTestButton setTitle:@"Binary ACK" forState:UIControlStateNormal];
    [self.binaryAckTestButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    [self.binaryAckTestButton setBackgroundColor:[UIColor systemOrangeColor]];
    [self.binaryAckTestButton addTarget:self action:@selector(binaryAckTestButtonTapped:) forControlEvents:UIControlEventTouchUpInside];
    self.binaryAckTestButton.layer.cornerRadius = 8.0;
    self.binaryAckTestButton.clipsToBounds = YES;
    self.binaryAckTestButton.enabled = NO;
    
    // 添加子视图
    [self.inputContainerView addSubview:self.inputTextField];
    [self.inputContainerView addSubview:self.sendButton];
    
    [self.view addSubview:self.statusView];
    [self.view addSubview:self.protocolLabel];
    [self.view addSubview:self.protocolSegment];
    [self.view addSubview:self.transportLabel];
    [self.view addSubview:self.transportSegment];
    [self.view addSubview:self.messageTextView];
    [self.view addSubview:self.inputContainerView];
    [self.view addSubview:self.connectButton];
    [self.view addSubview:self.disconnectButton];
    [self.view addSubview:self.ackTestButton];
    [self.view addSubview:self.sendBinaryButton];
    [self.view addSubview:self.binaryAckTestButton];

    
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
        
        // 协议版本
        [self.protocolLabel.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor constant:10],
        [self.protocolLabel.topAnchor constraintEqualToAnchor:self.statusView.bottomAnchor constant:10],
        [self.protocolLabel.heightAnchor constraintEqualToConstant:20],
        
        [self.protocolSegment.leadingAnchor constraintEqualToAnchor:self.protocolLabel.trailingAnchor constant:10],
        [self.protocolSegment.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor constant:-10],
        [self.protocolSegment.centerYAnchor constraintEqualToAnchor:self.protocolLabel.centerYAnchor],
        [self.protocolSegment.heightAnchor constraintEqualToConstant:24],
        
        // 传输方式
        [self.transportLabel.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor constant:10],
        [self.transportLabel.topAnchor constraintEqualToAnchor:self.protocolLabel.bottomAnchor constant:10],
        [self.transportLabel.heightAnchor constraintEqualToConstant:20],
        
        [self.transportSegment.leadingAnchor constraintEqualToAnchor:self.transportLabel.trailingAnchor constant:10],
        [self.transportSegment.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor constant:-10],
        [self.transportSegment.centerYAnchor constraintEqualToAnchor:self.transportLabel.centerYAnchor],
        [self.transportSegment.heightAnchor constraintEqualToConstant:24],
        
        // 消息文本视图 - 固定高度，位于传输方式下方
        [self.messageTextView.topAnchor constraintEqualToAnchor:self.transportLabel.bottomAnchor constant:10],
        [self.messageTextView.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor constant:10],
        [self.messageTextView.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor constant:-10],
        [self.messageTextView.heightAnchor constraintEqualToConstant:300], // 固定高度，确保按钮可见
        
        // 连接按钮 - 左侧
        [self.connectButton.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor constant:10],
        [self.connectButton.topAnchor constraintEqualToAnchor:self.messageTextView.bottomAnchor constant:10],
        [self.connectButton.widthAnchor constraintEqualToConstant:120],
        [self.connectButton.heightAnchor constraintEqualToConstant:40],
        
        // 断开连接按钮 - 中间
        [self.disconnectButton.leadingAnchor constraintEqualToAnchor:self.connectButton.trailingAnchor constant:10],
        [self.disconnectButton.topAnchor constraintEqualToAnchor:self.messageTextView.bottomAnchor constant:10],
        [self.disconnectButton.widthAnchor constraintEqualToConstant:120],
        [self.disconnectButton.heightAnchor constraintEqualToConstant:40],
        
        // ACK测试按钮 - 右侧
        [self.ackTestButton.leadingAnchor constraintEqualToAnchor:self.disconnectButton.trailingAnchor constant:10],
        [self.ackTestButton.topAnchor constraintEqualToAnchor:self.messageTextView.bottomAnchor constant:10],
        [self.ackTestButton.widthAnchor constraintEqualToConstant:120],
        [self.ackTestButton.heightAnchor constraintEqualToConstant:40],
        
        // 发送二进制消息按钮 - 第二行左侧
        [self.sendBinaryButton.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor constant:10],
        [self.sendBinaryButton.topAnchor constraintEqualToAnchor:self.ackTestButton.bottomAnchor constant:10],
        [self.sendBinaryButton.widthAnchor constraintEqualToConstant:120],
        [self.sendBinaryButton.heightAnchor constraintEqualToConstant:40],
        
        // 二进制ACK测试按钮 - 第二行右侧
        [self.binaryAckTestButton.leadingAnchor constraintEqualToAnchor:self.sendBinaryButton.trailingAnchor constant:10],
        [self.binaryAckTestButton.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor constant:-10],
        [self.binaryAckTestButton.topAnchor constraintEqualToAnchor:self.ackTestButton.bottomAnchor constant:10],
        [self.binaryAckTestButton.widthAnchor constraintEqualToConstant:120],
        [self.binaryAckTestButton.heightAnchor constraintEqualToConstant:40],
        
        // 输入容器视图 - 位于按钮下方，固定位置
        [self.inputContainerView.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor constant:10],
        [self.inputContainerView.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor constant:-10],
        [self.inputContainerView.topAnchor constraintEqualToAnchor:self.connectButton.bottomAnchor constant:10],
        [self.inputContainerView.heightAnchor constraintEqualToConstant:60],
        [self.inputContainerView.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor constant:-10],
        
        // 输入文本框
        [self.inputTextField.leadingAnchor constraintEqualToAnchor:self.inputContainerView.leadingAnchor constant:10],
        [self.inputTextField.topAnchor constraintEqualToAnchor:self.inputContainerView.topAnchor constant:10],
        [self.inputTextField.bottomAnchor constraintEqualToAnchor:self.inputContainerView.bottomAnchor constant:-10],
        [self.inputTextField.trailingAnchor constraintEqualToAnchor:self.sendButton.leadingAnchor constant:-10],
        
        // 发送按钮
        [self.sendButton.trailingAnchor constraintEqualToAnchor:self.inputContainerView.trailingAnchor constant:-10],
        [self.sendButton.centerYAnchor constraintEqualToAnchor:self.inputContainerView.centerYAnchor],
        [self.sendButton.widthAnchor constraintEqualToConstant:80],
        [self.sendButton.heightAnchor constraintEqualToConstant:40]
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
    CGFloat duration = [[info objectForKey:UIKeyboardAnimationDurationUserInfoKey] doubleValue];
    
    // 移除所有与inputContainerView相关的底部约束（约束存在于父视图中）
    NSMutableArray<NSLayoutConstraint *> *constraintsToRemove = [NSMutableArray array];
    for (NSLayoutConstraint *constraint in self.view.constraints) {
        // 查找与inputContainerView相关的底部约束
        BOOL isInputContainerConstraint = 
            (constraint.firstItem == self.inputContainerView || constraint.secondItem == self.inputContainerView) &&
            (constraint.firstAttribute == NSLayoutAttributeBottom || constraint.secondAttribute == NSLayoutAttributeBottom);
        if (isInputContainerConstraint) {
            [constraintsToRemove addObject:constraint];
        }
    }
    [NSLayoutConstraint deactivateConstraints:constraintsToRemove];
    
    [UIView animateWithDuration:duration delay:0 options:UIViewAnimationOptionBeginFromCurrentState animations:^{        
        // 添加新的底部约束，考虑键盘高度
        [NSLayoutConstraint activateConstraints:@[
            [self.inputContainerView.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor constant:-(10 + keyboardHeight)]
        ]];
        [self.view layoutIfNeeded];
    } completion:nil];
}

- (void)keyboardWillHide:(NSNotification *)notification {
    // 键盘隐藏时，恢复输入容器视图的位置
    NSDictionary *info = [notification userInfo];
    CGFloat duration = [[info objectForKey:UIKeyboardAnimationDurationUserInfoKey] doubleValue];
    
    // 移除所有与inputContainerView相关的底部约束（约束存在于父视图中）
    NSMutableArray<NSLayoutConstraint *> *constraintsToRemove = [NSMutableArray array];
    for (NSLayoutConstraint *constraint in self.view.constraints) {
        // 查找与inputContainerView相关的底部约束
        BOOL isInputContainerConstraint = 
            (constraint.firstItem == self.inputContainerView || constraint.secondItem == self.inputContainerView) &&
            (constraint.firstAttribute == NSLayoutAttributeBottom || constraint.secondAttribute == NSLayoutAttributeBottom);
        if (isInputContainerConstraint) {
            [constraintsToRemove addObject:constraint];
        }
    }
    [NSLayoutConstraint deactivateConstraints:constraintsToRemove];
    
    [UIView animateWithDuration:duration delay:0 options:UIViewAnimationOptionBeginFromCurrentState animations:^{        
        // 恢复原始的底部约束，所有按钮会自动回到底部
        [NSLayoutConstraint activateConstraints:@[
            [self.inputContainerView.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor constant:-10]
        ]];
        [self.view layoutIfNeeded];
    } completion:nil];
}

- (void)setupSocket {
    if ((_socket != nil)) {
        [_socket disconnect];
    }
    // 使用电脑的实际IP地址连接（HTTP）
    NSString *urlString = @"https://192.168.141.149:3443";
    
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
    
    // 获取当前选中的协议版本和传输方式
    RTCVPSocketIOProtocolVersion protocolVersion;
    if (self.protocolSegment.selectedSegmentIndex == 0) {
        protocolVersion = RTCVPSocketIOProtocolVersion2;
    } else {
        protocolVersion = RTCVPSocketIOProtocolVersion3;
    }
    
    RTCVPSocketIOTransport transport;
    if (self.transportSegment.selectedSegmentIndex == 0) {
        transport = RTCVPSocketIOTransportPolling;
    } else {
        transport = RTCVPSocketIOTransportWebSocket;
    }
    
    // 创建配置对象
    RTCVPSocketIOConfig *config = [[RTCVPSocketIOConfig alloc ]init];
    config.loggingEnabled = YES;
    config.reconnectionEnabled = YES;
    config.reconnectionAttempts = 3;
    config.secure = YES;
    config.forceNewConnection = YES;
    config.allowSelfSignedCertificates = YES;
    config.ignoreSSLErrors = NO;
    config.reconnectionDelay = 2;
    config.connectTimeout = 15; // 增加连接超时时间
    config.nsp = @"/";
    config.connectParams = connectParams;
    config.logger = logger;
    config.handleQueue = self->_currentEngineProtooQueue;
    
    // 使用当前选中的协议版本和传输方式
    config.protocolVersion = protocolVersion;
    config.transport = transport;
    
    // 打印当前连接配置
    NSLog(@"📋 当前连接配置: 协议版本=%@, 传输方式=%@",
          (protocolVersion == RTCVPSocketIOProtocolVersion2 ? @"v2" : @"v3"),
          (transport == RTCVPSocketIOTransportPolling ? @"轮询" : @"WebSocket"));
    
    // 创建Socket客户端
    self.socket = [[RTCVPSocketIOClient alloc] initWithSocketURL:[NSURL URLWithString:urlString] config:config];
    
   WEAKSELF
    
    // 监听连接事件
    [_socket on:RTCVPSocketEventConnect callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
        STRONGSELF
        [strongSelf updateStatus:YES];
        [strongSelf addMessage:[NSString stringWithFormat:@"✅ 连接成功，协议版本: %@, 传输方式: %@", 
                               (self.protocolSegment.selectedSegmentIndex == 0 ? @"v2" : @"v3"), 
                               (self.transportSegment.selectedSegmentIndex == 0 ? @"轮询" : @"WebSocket")] type:@"system"];
    }];
    
    // 监听断开连接事件
    [_socket on:RTCVPSocketEventDisconnect callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
        STRONGSELF
        [strongSelf updateStatus:NO];
        [strongSelf addMessage:[NSString stringWithFormat:@"❌ 断开连接: %@", array] type:@"system"];
    }];
    
    // 监听错误事件
    [_socket on:RTCVPSocketEventError callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
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
                
                // 发送ACK响应
                if (emitter) {
                    [emitter send:@[@{@"success": @YES, @"message": @"Welcome received is iOS", @"clientId": strongSelf.clientId}]];
                }
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
        if (emitter) {
            [emitter send:@[@{@"success": @YES, @"message": @"hahah", @"clientId": strongSelf.clientId}]];
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
                
                // 打印收到的消息到日志
                NSLog(@"📥 收到消息: %@", messageData);
                
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
        //        NSLog(@"💓 收到心跳消息: %@", array);
    }];
    
    // 监听二进制消息
    [_socket on:@"binaryEvent" callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
        STRONGSELF
        if (array.count > 0) {
            id data = array.firstObject;
            if ([data isKindOfClass:[NSDictionary class]]) {
                NSDictionary *binaryData = (NSDictionary *)data;
                NSString *sender = binaryData[@"sender"];
                NSString *text = binaryData[@"text"];
                if (![binaryData[@"binaryData"] isKindOfClass:[NSData class]]) {
                    return;
                }
                NSData *binary = binaryData[@"binaryData"];
                
                NSString *message = [NSString stringWithFormat:@"📥 二进制消息来自: %@, 文本: %@, 大小: %lu字节", 
                                  sender, text ?: @"无文本", (unsigned long)[binary length]];
                [strongSelf addMessage:message type:@"received"];
                
                // 比较二进制数据
                if ([text isKindOfClass:[NSString class]] && [text containsString:@"testData"]) {
                    // 这是用于比较的测试数据
                    [strongSelf addMessage:@"🔍 开始比较二进制数据..." type:@"system"];
                    
                    // 使用静态PNG数据进行比较
                    NSData *expectedData = [NSData dataWithBytes:image_data length:sizeof(image_data)];
                    
                    // 比较收到的数据与预期数据
                    BOOL isEqual = NO;
                    if (binary.length == expectedData.length) {
                        isEqual = [binary isEqualToData:expectedData];
                    }
                    
                    if (isEqual) {
                        [strongSelf addMessage:@"✅ 二进制数据完全匹配！PNG图片数据传输成功" type:@"system"];
                    } else {
                        [strongSelf addMessage:[NSString stringWithFormat:@"❌ 二进制数据不匹配！预期大小: %lu bytes, 实际大小: %lu bytes", 
                                            (unsigned long)expectedData.length, (unsigned long)binary.length] type:@"system"];
                    }
                }
            }
        }
    }];
    
    // 监听二进制ACK测试响应
    [_socket on:@"binaryAckTest" callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
        STRONGSELF
        if (array.count > 0) {
            id data = array.firstObject;
            if ([data isKindOfClass:[NSDictionary class]]) {
                NSDictionary *ackData = (NSDictionary *)data;
                [strongSelf addMessage:[NSString stringWithFormat:@"📤 二进制ACK测试响应: %@", ackData] type:@"system"];
            }
        }
    }];
}

-(void)updateBtnsBackColor:(BOOL)isConnected{
    self.connectButton.enabled = !isConnected;
    self.disconnectButton.enabled = isConnected;
    self.sendButton.enabled = isConnected;
    self.inputTextField.enabled = isConnected;
    self.ackTestButton.enabled = isConnected;
    self.sendBinaryButton.enabled = isConnected;
    self.binaryAckTestButton.enabled = isConnected;
    
    self.connectButton.backgroundColor = isConnected?[UIColor grayColor] : self.connBtnBC;
    self.disconnectButton.backgroundColor = !isConnected?[UIColor grayColor] : self.disconnectBtnBC;
    self.sendButton.backgroundColor = !isConnected?[UIColor grayColor] : self.sendBtnBC;
    self.inputTextField.backgroundColor = !isConnected?[UIColor grayColor] : self.inputTFBC;
    self.ackTestButton.backgroundColor = !isConnected?[UIColor grayColor] : self.ackTestBtnBC;
    self.sendBinaryButton.backgroundColor = !isConnected?[UIColor grayColor] : [UIColor systemTealColor];
    self.binaryAckTestButton.backgroundColor = !isConnected?[UIColor grayColor] : [UIColor systemOrangeColor];
}

- (void)updateStatus:(BOOL)connected {
    dispatch_async(dispatch_get_main_queue(), ^{        
        // 直接检查socket的实际状态，确保按钮状态与真实连接状态一致
        BOOL isConnected = (self.socket && 
                           (self.socket.status == RTCVPSocketIOClientStatusConnected || 
                            self.socket.status == RTCVPSocketIOClientStatusOpened));
        
        [self updateBtnsBackColor:isConnected];
        
        if (isConnected) {
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
    [self setupSocket    ];
    [self addMessage:@"🔄 正在连接服务器..." type:@"system"];
  
    // 立即更新按钮状态，避免等待事件
    dispatch_async(dispatch_get_main_queue(), ^{        
        self.connectButton.enabled = NO;
        self.disconnectButton.enabled = YES;
        self.sendButton.enabled = NO;
        self.inputTextField.enabled = NO;
        self.ackTestButton.enabled = NO;
        
        self.connectButton.backgroundColor = [UIColor grayColor];
        self.disconnectButton.backgroundColor = [UIColor grayColor];
        self.sendButton.backgroundColor = [UIColor grayColor];
        self.inputTextField.backgroundColor = [UIColor grayColor];
        self.ackTestButton.backgroundColor = [UIColor grayColor];
        
        self.statusLabel.text = @"连接中...";
        self.statusView.backgroundColor = [UIColor systemYellowColor];
    });
    
    // 增加连接超时时间到15秒
    [self.socket connectWithTimeoutAfter:15 withHandler:^{        
        NSLog(@"⏱️ 连接超时回调触发");
        [self addMessage:@"⏱️ 连接超时" type:@"system"];
        
        // 连接超时后更新状态
        dispatch_async(dispatch_get_main_queue(), ^{        
            [self updateBtnsBackColor:NO];
            self.statusLabel.text = @"未连接";
            
            self.connectButton.backgroundColor = self.connBtnBC;
            
            self.statusView.backgroundColor = [UIColor systemRedColor];
        });
    }];
    
    NSLog(@"📞 连接方法调用完成，等待连接结果");
}

- (void)disconnectButtonTapped:(id)sender {
    // 立即更新按钮状态
    dispatch_async(dispatch_get_main_queue(), ^{        
        self.connectButton.enabled = YES;
        self.disconnectButton.enabled = NO;
        self.sendButton.enabled = NO;
        self.inputTextField.enabled = NO;
        self.ackTestButton.enabled = NO;
        
        [self updateBtnsBackColor:NO];
        
        self.statusLabel.text = @"断开中...";
        self.statusView.backgroundColor = [UIColor systemYellowColor];
    });
    [self addMessage:@"🔄 正在断开连接..." type:@"system"];

    // 断开连接
    [self.socket disconnectWithHandler:^{
        [self addMessage:@"❌ 断开连接 blockcb" type:@"system"];
    }];
}

- (void)ackTestButtonTapped:(id)sender {
    // 并发ACK测试
    [self addMessage:@"🔄 开始并发ACK测试..." type:@"system"];
    
    // 测试参数
    const NSInteger testCount = 10; // 测试10个并发ACK
    __block NSInteger completedCount = 0;
    __block NSInteger successCount = 0;
    __block NSInteger failureCount = 0;
    
    // 记录开始时间
    CFAbsoluteTime startTime = CFAbsoluteTimeGetCurrent();
    
    for (NSInteger i = 0; i < testCount; i++) {
        NSInteger testIndex = i;
        
        // 发送带ACK的自定义事件
        [self.socket emitWithAck:@"customEvent" 
                         items:@[@{        
                             @"testIndex": @(testIndex),
                             @"message": [NSString stringWithFormat:@"ACK Test %ld", (long)testIndex],
                             @"timestamp": @([NSDate date].timeIntervalSince1970)
                         }] 
                      ackBlock:^(NSArray * _Nullable data, NSError * _Nullable error) {
            dispatch_async(dispatch_get_main_queue(), ^{                
                completedCount++;
                
                if (error) {
                    failureCount++;
                    [self addMessage:[NSString stringWithFormat:@"❌ ACK %ld 失败: %@", (long)testIndex, error.localizedDescription] type:@"system"];
                } else {
                    successCount++;
                    [self addMessage:[NSString stringWithFormat:@"✅ ACK %ld 成功: %@", (long)testIndex, data] type:@"system"];
                }
                
                // 所有测试完成，显示结果
                if (completedCount == testCount) {
                    CFAbsoluteTime endTime = CFAbsoluteTimeGetCurrent();
                    double duration = endTime - startTime;
                    
                    [self addMessage:[NSString stringWithFormat:@"📊 并发ACK测试完成: 总请求 %ld, 成功 %ld, 失败 %ld, 耗时 %.2fs", 
                                      (long)testCount, (long)successCount, (long)failureCount, duration] 
                               type:@"system"];
                }
            });
        } timeout:10.0];
        
        // 添加小延迟避免请求过于集中
        usleep(5000); // 5ms延迟
    }
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
        NSDictionary *messageData = @{        
            @"message": message,
            @"timestamp": @([NSDate date].timeIntervalSince1970)
        };
        
        // 打印发送的消息到日志
        NSLog(@"📤 发送消息: %@", messageData);
        
        [self.socket emit:@"chatMessage" items:@[messageData]];
        
    } else {
        [self addMessage:@"⚠️ Socket尚未完全连接" type:@"system"];
    }
}

// 发送二进制消息按钮点击事件
- (void)sendBinaryButtonTapped:(id)sender {
    // 确保Socket已连接
    if (self.socket.status == RTCVPSocketIOClientStatusConnected || self.socket.status == RTCVPSocketIOClientStatusOpened) {
        // 使用静态PNG数据
        NSData *binaryData = [NSData dataWithBytes:image_data length:sizeof(image_data)];
        
        NSString *text = @"iOS客户端发送的二进制测试数据";
        //v2 不支持放到 对象里
        
        if (self.protocolSegment.selectedSegmentIndex == 0) {
            // 构造发送数据
            NSDictionary *sendData = @{
                @"text": @"testData: iOS客户端发送的二进制测试数据",
                @"timestamp": @([NSDate date].timeIntervalSince1970)
            };
            [self addMessage:[NSString stringWithFormat:@"v2 📤 发送二进制数据: 大小 %lu 字节, 文本: %@", (unsigned long)binaryData.length, text] type:@"sent"];
            // 发送二进制消息
            [self.socket emitWithAck:@"binaryEvent" items:@[sendData,binaryData] ackBlock:^(NSArray * _Nullable data, NSError * _Nullable error) {
                // UI更新必须在主线程执行
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (error) {
                        [self addMessage:[NSString stringWithFormat:@"❌ 二进制消息发送失败: %@", error.localizedDescription] type:@"system"];
                    } else {
                        [self addMessage:[NSString stringWithFormat:@"✅ 二进制消息发送成功, ACK: %@", data] type:@"system"];
                    }
                });
            } timeout:10.0];
        }else{
            // 构造发送数据
            NSDictionary *sendData = @{
                @"binaryData": binaryData,
                @"text": @"testData: iOS客户端发送的二进制测试数据",
                @"timestamp": @([NSDate date].timeIntervalSince1970)
            };
            [self addMessage:[NSString stringWithFormat:@"v3 📤 发送二进制数据: 大小 %lu 字节, 文本: %@", (unsigned long)binaryData.length, text] type:@"sent"];
            
            // 发送二进制消息
            [self.socket emitWithAck:@"binaryEvent" items:@[sendData] ackBlock:^(NSArray * _Nullable data, NSError * _Nullable error) {
                // UI更新必须在主线程执行
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (error) {
                        [self addMessage:[NSString stringWithFormat:@"❌ 二进制消息发送失败: %@", error.localizedDescription] type:@"system"];
                    } else {
                        [self addMessage:[NSString stringWithFormat:@"✅ 二进制消息发送成功, ACK: %@", data] type:@"system"];
                    }
                });
            } timeout:10.0];
        }
        
        
       
        
    } else {
        [self addMessage:@"⚠️ Socket尚未完全连接" type:@"system"];
    }
}

// 二进制ACK测试按钮点击事件
- (void)binaryAckTestButtonTapped:(id)sender {
    // 确保Socket已连接
        if (self.socket.status == RTCVPSocketIOClientStatusConnected || self.socket.status == RTCVPSocketIOClientStatusOpened) {
           
            
            // 使用静态PNG数据
            NSData *binaryData = [NSData dataWithBytes:image_data length:sizeof(image_data)];
            if (self.protocolSegment.selectedSegmentIndex == 0) {
                [self addMessage:@"🔄 开始V2二进制ACK测试..." type:@"system"];
                // 构造发送数据
                NSDictionary *sendData = @{
                    @"text": @"iOS二进制ACK测试",
                    @"timestamp": @([NSDate date].timeIntervalSince1970)
                };
                
                // 发送带ACK的二进制消息
                [self.socket emitWithAck:@"binaryAckTest" items:@[sendData,binaryData] ackBlock:^(NSArray * _Nullable data, NSError * _Nullable error) {
                    if (error) {
                        [self addMessage:[NSString stringWithFormat:@"❌ 二进制ACK测试失败: %@", error.localizedDescription] type:@"system"];
                    } else {
                        if (data && data.count > 0) {
                            id ackData = data.firstObject;
                            if ([ackData isKindOfClass:[NSDictionary class]]) {
                                NSDictionary *ackDict = (NSDictionary *)ackData;
                                [self addMessage:[NSString stringWithFormat:@"✅ 二进制ACK测试成功, 结果: %@", ackDict] type:@"system"];
                            } else {
                                [self addMessage:[NSString stringWithFormat:@"✅ 二进制ACK测试成功, 结果: %@", data] type:@"system"];
                            }
                        } else {
                            [self addMessage:@"✅ 二进制ACK测试成功, 但未返回数据" type:@"system"];
                        }
                    }
                } timeout:15.0]; // 增加超时时间到15秒
            }else{
                [self addMessage:@"🔄 开始V3二进制ACK测试..." type:@"system"];
                // 构造发送数据
                NSDictionary *sendData = @{
                    @"binaryData": binaryData,
                    @"text": @"iOS二进制ACK测试",
                    @"timestamp": @([NSDate date].timeIntervalSince1970)
                };
                
                // 发送带ACK的二进制消息
                [self.socket emitWithAck:@"binaryAckTest" items:@[sendData] ackBlock:^(NSArray * _Nullable data, NSError * _Nullable error) {
                    if (error) {
                        [self addMessage:[NSString stringWithFormat:@"❌ 二进制ACK测试失败: %@", error.localizedDescription] type:@"system"];
                    } else {
                        if (data && data.count > 0) {
                            id ackData = data.firstObject;
                            if ([ackData isKindOfClass:[NSDictionary class]]) {
                                NSDictionary *ackDict = (NSDictionary *)ackData;
                                [self addMessage:[NSString stringWithFormat:@"✅ 二进制ACK测试成功, 结果: %@", ackDict] type:@"system"];
                            } else {
                                [self addMessage:[NSString stringWithFormat:@"✅ 二进制ACK测试成功, 结果: %@", data] type:@"system"];
                            }
                        } else {
                            [self addMessage:@"✅ 二进制ACK测试成功, 但未返回数据" type:@"system"];
                        }
                    }
                } timeout:15.0]; // 增加超时时间到15秒
            }
            
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
