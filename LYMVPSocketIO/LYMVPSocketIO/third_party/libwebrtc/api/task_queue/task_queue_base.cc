/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/task_queue/task_queue_base.h"

#include "rtc_base/checks.h"

// 检测编译器是否支持 thread_local
#if (defined(__cplusplus) && __cplusplus >= 201103L) || \
    (defined(_MSC_VER) && _MSC_VER >= 1900) || \
    (defined(__GNUC__) && __GNUC__ >= 4 && __GNUC_MINOR__ >= 8) || \
    defined(__clang__)
#define WEBRTC_HAVE_THREAD_LOCAL 1
#else
#define WEBRTC_HAVE_THREAD_LOCAL 0
#endif

#if WEBRTC_HAVE_THREAD_LOCAL

namespace webrtc {
namespace {

// 使用编译器特定的 constinit 属性（如果可用）
#if defined(__has_cpp_attribute)
#  if __has_cpp_attribute(clang::require_constant_initialization)
#    define WEBRTC_CONST_INIT [[clang::require_constant_initialization]]
#  elif __has_cpp_attribute(gnu::require_constant_initialization)
#    define WEBRTC_CONST_INIT [[gnu::require_constant_initialization]]
#  else
#    define WEBRTC_CONST_INIT
#  endif
#else
#  define WEBRTC_CONST_INIT
#endif

WEBRTC_CONST_INIT thread_local TaskQueueBase* current = nullptr;

}  // namespace

TaskQueueBase* TaskQueueBase::Current() {
  return current;
}

TaskQueueBase::CurrentTaskQueueSetter::CurrentTaskQueueSetter(
    TaskQueueBase* task_queue)
    : previous_(current) {
  current = task_queue;
}

TaskQueueBase::CurrentTaskQueueSetter::~CurrentTaskQueueSetter() {
  current = previous_;
}
}  // namespace webrtc

#elif defined(WEBRTC_POSIX)

#include <pthread.h>

namespace webrtc {
namespace {

pthread_key_t g_queue_ptr_tls = 0;
pthread_once_t g_init_once = PTHREAD_ONCE_INIT;

void InitializeTls() {
  RTC_CHECK(pthread_key_create(&g_queue_ptr_tls, nullptr) == 0);
}

pthread_key_t GetQueuePtrTls() {
  RTC_CHECK(pthread_once(&g_init_once, &InitializeTls) == 0);
  return g_queue_ptr_tls;
}

}  // namespace

TaskQueueBase* TaskQueueBase::Current() {
  return static_cast<TaskQueueBase*>(pthread_getspecific(GetQueuePtrTls()));
}

TaskQueueBase::CurrentTaskQueueSetter::CurrentTaskQueueSetter(
    TaskQueueBase* task_queue)
    : previous_(TaskQueueBase::Current()) {
  pthread_setspecific(GetQueuePtrTls(), task_queue);
}

TaskQueueBase::CurrentTaskQueueSetter::~CurrentTaskQueueSetter() {
  pthread_setspecific(GetQueuePtrTls(), previous_);
}

}  // namespace webrtc

#else
#error Unsupported platform
#endif
