//
//  test_client_core.h
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/20.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#ifndef TEST_CLIENT_CORE_H
#define TEST_CLIENT_CORE_H

#include "sio_client_core.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <map>
#include <atomic>
#include <chrono>
#include <memory>

namespace sio_test {

// 测试函数声明
void test_client_core_basic();
void test_client_core_emit_data();
void test_client_core_emit_with_ack();
void test_client_core_timeout();
void test_client_core_status_changes();



} // namespace sio_test

#endif // TEST_CLIENT_CORE_H
