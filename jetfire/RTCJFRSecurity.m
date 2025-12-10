//////////////////////////////////////////////////////////////////////////////////////////////////
//
//  RTCJFRSecurity.m
//
//  Created by Austin and Dalton Cherry on on 9/3/15.
//  Copyright (c) 2014-2017 Austin Cherry.
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

#import "RTCJFRSecurity.h"

@interface RTCJFRSSLCert ()

@property(nonatomic, strong) NSData *certData;
@property(nonatomic, assign) SecKeyRef key;
@property(nonatomic, assign) BOOL keyOwned;

@end

@implementation RTCJFRSSLCert

/////////////////////////////////////////////////////////////////////////////
- (instancetype)initWithData:(NSData *)data {
    if(self = [super init]) {
        self.certData = data;
        self.keyOwned = NO;
    }
    return self;
}
/////////////////////////////////////////////////////////////////////////////
- (instancetype)initWithKey:(SecKeyRef)key {
    if(self = [super init]) {
        if (key) {
            CFRetain(key);
            self.key = key;
            self.keyOwned = YES;
        }
    }
    return self;
}
/////////////////////////////////////////////////////////////////////////////
- (void)dealloc {
    if(self.key && self.keyOwned) {
        CFRelease(self.key);
    }
}
/////////////////////////////////////////////////////////////////////////////

@end

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

@interface RTCJFRSecurity ()

@property(nonatomic, assign) BOOL isReady; //is the key processing done?
@property(nonatomic, strong) NSMutableArray *certificates;
@property(nonatomic, strong) NSMutableArray *publicKeys;
@property(nonatomic, assign) BOOL usePublicKeys;
@property(nonatomic, strong) dispatch_semaphore_t readySemaphore;

@end

@implementation RTCJFRSecurity

/////////////////////////////////////////////////////////////////////////////
- (instancetype)init {
    if (self = [super init]) {
        _validatedDN = YES;
        _readySemaphore = dispatch_semaphore_create(0);
    }
    return self;
}
/////////////////////////////////////////////////////////////////////////////
- (instancetype)initUsingPublicKeys:(BOOL)publicKeys {
    NSArray *paths = [[NSBundle mainBundle] pathsForResourcesOfType:@"cer" inDirectory:@"."];
    NSMutableArray<RTCJFRSSLCert*> *collect = [NSMutableArray array];
    for(NSString *path in paths) {
        NSData *data = [NSData dataWithContentsOfFile:path];
        if(data) {
            [collect addObject:[[RTCJFRSSLCert alloc] initWithData:data]];
        }
    }
    return [self initWithCerts:collect publicKeys:publicKeys];
}
/////////////////////////////////////////////////////////////////////////////
- (instancetype)initWithCerts:(NSArray<RTCJFRSSLCert*>*)certs publicKeys:(BOOL)publicKeys {
    if(self = [self init]) {
        self.usePublicKeys = publicKeys;
        
        if(self.usePublicKeys) {
            dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                @autoreleasepool {
                    NSMutableArray *collect = [NSMutableArray array];
                    for(RTCJFRSSLCert *cert in certs) {
                        if(cert.certData && !cert.key) {
                            SecKeyRef key = [self extractPublicKey:cert.certData];
                            if (key) {
                                // 创建一个新的SSLCert对象来持有key
                                RTCJFRSSLCert *keyCert = [[RTCJFRSSLCert alloc] initWithKey:key];
                                CFRelease(key); // initWithKey已经retain了
                                [collect addObject:keyCert];
                            }
                        } else if (cert.key) {
                            // 已经有关键字的证书
                            [collect addObject:cert];
                        }
                    }
                    
                    // 提取公钥到数组
                    NSMutableArray *pubKeyArray = [NSMutableArray array];
                    for (RTCJFRSSLCert *cert in collect) {
                        if (cert.key) {
                            [pubKeyArray addObject:(__bridge id)cert.key];
                        }
                    }
                    
                    self.publicKeys = pubKeyArray;
                    self.isReady = YES;
                    dispatch_semaphore_signal(self.readySemaphore);
                }
            });
        } else {
            NSMutableArray<NSData*> *collect = [NSMutableArray array];
            for(RTCJFRSSLCert *cert in certs) {
                if(cert.certData) {
                    [collect addObject:cert.certData];
                }
            }
            self.certificates = collect;
            self.isReady = YES;
            dispatch_semaphore_signal(self.readySemaphore);
        }
    }
    return self;
}
/////////////////////////////////////////////////////////////////////////////
- (BOOL)isValid:(SecTrustRef)trust domain:(NSString*)domain {
    // 等待准备完成，最多5秒
    dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(5 * NSEC_PER_SEC));
    if (dispatch_semaphore_wait(self.readySemaphore, timeout) != 0) {
        return NO; // 超时
    }
    
    BOOL status = NO;
    SecPolicyRef policy = NULL;
    
    if(self.validatedDN && domain) {
        policy = SecPolicyCreateSSL(true, (__bridge CFStringRef)domain);
    } else {
        policy = SecPolicyCreateBasicX509();
    }
    
    if (!policy) {
        return NO;
    }
    
    SecTrustSetPolicies(trust, policy);
    
    if(self.usePublicKeys) {
        NSArray *serverKeys = [self publicKeyChainForTrust:trust];
        for(id serverKey in serverKeys) {
            for(id keyObj in self.publicKeys) {
                if([serverKey isEqual:keyObj]) {
                    status = YES;
                    break;
                }
            }
            if (status) break;
        }
    } else {
        NSArray *serverCerts = [self certificateChainForTrust:trust];
        NSMutableArray *collect = [NSMutableArray arrayWithCapacity:self.certificates.count];
        for(NSData *data in self.certificates) {
            SecCertificateRef cert = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)data);
            if (cert) {
                [collect addObject:CFBridgingRelease(cert)];
            }
        }
        
        SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)collect);
        
        // 评估信任
        if (@available(iOS 12.0, *)) {
            CFErrorRef error = NULL;
            status = SecTrustEvaluateWithError(trust, &error);
            if (error) {
                CFRelease(error);
                status = NO;
            }
        } else {
            SecTrustResultType result = kSecTrustResultInvalid;
            SecTrustEvaluate(trust, &result);
            status = (result == kSecTrustResultUnspecified || result == kSecTrustResultProceed);
        }
        
        if (status) {
            // 确保证书匹配
            NSUInteger trustedCount = 0;
            for(NSData *serverData in serverCerts) {
                for(NSData *certData in self.certificates) {
                    if([certData isEqualToData:serverData]) {
                        trustedCount++;
                        break;
                    }
                }
            }
            
            // 至少需要有一个证书匹配
            if (trustedCount == 0) {
                status = NO;
            }
        }
    }
    
    if (policy) {
        CFRelease(policy);
    }
    
    return status;
}
/////////////////////////////////////////////////////////////////////////////
- (SecKeyRef)extractPublicKey:(NSData*)data {
    SecCertificateRef cert = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)data);
    if (!cert) {
        return NULL;
    }
    
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    if (!policy) {
        CFRelease(cert);
        return NULL;
    }
    
    SecTrustRef trust = NULL;
    OSStatus status = SecTrustCreateWithCertificates(cert, policy, &trust);
    
    SecKeyRef key = NULL;
    if (status == errSecSuccess && trust) {
        if (@available(iOS 14.0, *)) {
            key = SecTrustCopyKey(trust);
        } else {
            key = SecTrustCopyPublicKey(trust);
        }
    }
    
    if (trust) CFRelease(trust);
    if (policy) CFRelease(policy);
    if (cert) CFRelease(cert);
    
    return key;
}
/////////////////////////////////////////////////////////////////////////////
- (SecKeyRef)extractPublicKeyFromCert:(SecCertificateRef)cert policy:(SecPolicyRef)policy {
    if (!cert || !policy) {
        return NULL;
    }
    
    SecTrustRef trust = NULL;
    OSStatus status = SecTrustCreateWithCertificates(cert, policy, &trust);
    
    SecKeyRef key = NULL;
    if (status == errSecSuccess && trust) {
        if (@available(iOS 14.0, *)) {
            key = SecTrustCopyKey(trust);
        } else {
            key = SecTrustCopyPublicKey(trust);
        }
    }
    
    if (trust) CFRelease(trust);
    return key;
}
/////////////////////////////////////////////////////////////////////////////
- (NSArray*)certificateChainForTrust:(SecTrustRef)trust {
    if (!trust) {
        return @[];
    }
    
    CFIndex certCount = SecTrustGetCertificateCount(trust);
    NSMutableArray *collect = [NSMutableArray arrayWithCapacity:certCount];
    
    for(CFIndex i = 0; i < certCount; i++) {
        SecCertificateRef cert = SecTrustGetCertificateAtIndex(trust, i);
        if(cert) {
            CFDataRef certData = SecCertificateCopyData(cert);
            if (certData) {
                [collect addObject:(__bridge_transfer NSData*)certData];
            }
        }
    }
    return collect;
}
/////////////////////////////////////////////////////////////////////////////
- (NSArray*)publicKeyChainForTrust:(SecTrustRef)trust {
    if (!trust) {
        return @[];
    }
    
    CFIndex certCount = SecTrustGetCertificateCount(trust);
    NSMutableArray *collect = [NSMutableArray arrayWithCapacity:certCount];
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    
    if (policy) {
        for(CFIndex i = 0; i < certCount; i++) {
            SecCertificateRef cert = SecTrustGetCertificateAtIndex(trust, i);
            if (cert) {
                SecKeyRef key = [self extractPublicKeyFromCert:cert policy:policy];
                if (key) {
                    [collect addObject:CFBridgingRelease(key)];
                }
            }
        }
        CFRelease(policy);
    }
    
    return collect;
}
/////////////////////////////////////////////////////////////////////////////

@end
