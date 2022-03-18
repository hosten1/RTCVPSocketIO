//
//  ViewController.m
//  VPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "ViewController.h"
#import "RTCVPSocketIO.h"
#import "RTCVPSocketAckEmitter.h"
#ifndef WEAKSELF
#define WEAKSELF __weak __typeof(&*self)weakSelf = self;
#endif
#ifndef STRONGSELF
#define STRONGSELF __strong __typeof(&*weakSelf)strongSelf = weakSelf;
#endif
#ifndef dispatch_queue_async_safe
#define dispatch_queue_async_safe(queue, block)\
if (dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL) == dispatch_queue_get_label(queue)) {\
block();\
} else {\
dispatch_async(queue, block);\
}
#endif

#ifndef dispatch_main_async_safe
#define dispatch_main_async_safe(block) dispatch_queue_async_safe(dispatch_get_main_queue(), block)
#endif

@interface ViewController ()

@property(nonatomic, strong) RTCVPSocketIOClient *socket;
@property(nonatomic,strong) dispatch_queue_t currentEngineProtooQueue;

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    _currentEngineProtooQueue = dispatch_queue_create("com.vrv.mediasoupProtoo", DISPATCH_QUEUE_SERIAL);

    // Do any additional setup after loading the view, typically from a nib.
   
}
- (IBAction)connectServer:(id)sender {
    [self socketExample];
}

- (void)sendMessage:(NSDictionary*)message withMethod:(NSString*)method{
    
    WEAKSELF
    dispatch_async(_currentEngineProtooQueue, ^{
        @autoreleasepool {
            __strong typeof(weakSelf) blockSelf = weakSelf;
            if (!blockSelf.socket) {
               
                return;
            }
            RTCVPSocketOnAckCallback *callback = [blockSelf.socket emitWithAck:method items:@[message]];
            [callback timingOutAfter:10 callback:^(NSArray *array) {
                if ([array[0] isKindOfClass:[NSNull class]]) {
                    NSLog(@"");
                }
            }];
        }
        
    });
    
}
- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}
- (IBAction)sendMsg:(id)sender {
    
    [self sendMessage:@{} withMethod:@"online"];
    
}
- (IBAction)disconnect:(id)sender {
    
    [self.socket removeAllHandlers];
    [self.socket disconnect];
    self.socket = nil;
    
}

-(void)socketExample
{
    NSString *urlString = @"http://192.168.140.184:8000";
    NSDictionary *connectParams = @{@"key":@"value"};
    RTCVPSocketLogger *logger = [[RTCVPSocketLogger alloc]init];
    [logger onLogMsgWithCB:^(NSString *message, NSString *type) {
        if ([type isEqualToString:@"RTCVPSocketIOClient"]) {
            //忽略消息
            return;
        }else if ([type isEqualToString:@"SocketParser"] && [message containsString:@"Decoded packet as"]){
            return;
        }
        
    }];
    self.socket = [[RTCVPSocketIOClient alloc] init:[NSURL URLWithString:urlString]
                                    withConfig:@{@"log": @YES,
                                                 @"reconnects":@YES,
                                                 @"reconnectAttempts":@(20),
                                                 @"forcePolling": @NO,
                                                 @"secure": @NO,
                                                 @"forceNew":@YES,
                                                 @"forceWebsockets":@YES,
                                                 @"selfSigned":@NO,
                                                 @"reconnectWait":@3,
                                                 @"nsp":@"/",
                                                 @"connectParams":connectParams,
                                                 @"logger":logger
                                    }];
   WEAKSELF
    [_socket on:kSocketEventConnect callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
        STRONGSELF
        NSLog(@"====> connect msg:%@",array);
    }];
    [_socket on:kSocketEventDisconnect callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
        STRONGSELF
//        network change
        dispatch_main_async_safe(^{
            if (array.count > 1) {
                if ([array containsObject:@"network change"]) {
                    return;
                }
            }else{
                if ([array containsObject:@"network change"]) {
                    return;
                }
            }
        });
    }];
    [_socket on:kSocketEventError callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
        STRONGSELF
       
    }];
    
    
    [_socket connectWithTimeoutAfter:10 withHandler:^{
        STRONGSELF
       
       
    }];
    
}


@end
