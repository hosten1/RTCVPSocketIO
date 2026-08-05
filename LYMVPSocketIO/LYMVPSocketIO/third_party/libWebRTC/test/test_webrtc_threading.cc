#include <iostream>
#include <string>
#include <atomic>
#include <chrono>
#include <cassert>

#include "rtc_base/thread.h"
#include "rtc_base/event.h"
#include "rtc_base/message_handler.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/thread_checker.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/physical_socket_server.h"

// =============================================================================
// 全局原子变量，用于跨线程结果收集
// =============================================================================
static std::atomic<int> g_test_counter{0};
static std::atomic<int> g_post_task_count{0};
static std::atomic<int> g_delayed_count{0};
static std::atomic<int> g_message_count{0};
static std::atomic<int> g_invoke_result{0};
static std::atomic<bool> g_runnable_ran{false};
static std::atomic<int> g_platform_thread_val{0};
static int g_shared_value = 0;
static rtc::CriticalSection g_crit;

// =============================================================================
// 自定义 Runnable
// =============================================================================
class MyRunnable : public rtc::Runnable {
public:
    void Run(rtc::Thread* thread) override {
        std::cout << "  [MyRunnable::Run] thread=" << thread->name()
                  << ", is_current=" << thread->IsCurrent() << std::endl;
        g_runnable_ran.store(true);
    }
};

// =============================================================================
// 自定义 MessageHandler
// =============================================================================
class MyMessageHandler : public rtc::MessageHandler {
public:
    explicit MyMessageHandler(const std::string& name) : name_(name) {}

    void OnMessage(rtc::Message* msg) override {
        g_message_count.fetch_add(1);
        std::cout << "  [MyMessageHandler::OnMessage] name=" << name_
                  << ", id=" << msg->message_id
                  << ", posted_from=" << msg->posted_from.ToString() << std::endl;
    }

private:
    std::string name_;
};

// =============================================================================
// 测试1: Thread::Create / Start / Stop / IsCurrent / name / SetName
// =============================================================================
bool TestBasicThreadLifecycle() {
    std::cout << "--- Test 1: Basic Thread Lifecycle ---" << std::endl;

    auto thread = rtc::Thread::Create();
    thread->SetName("TestThread1", nullptr);
    std::cout << "  Thread name: " << thread->name() << std::endl;

    thread->Start();
    std::cout << "  IsOwned: " << thread->IsOwned() << std::endl;
    std::cout << "  RunningForTest: " << thread->RunningForTest() << std::endl;

    // 用 Invoke 验证线程正在运行
    bool is_current = thread->Invoke<bool>(RTC_FROM_HERE, [&thread]() {
        return thread->IsCurrent();
    });
    std::cout << "  IsCurrent from worker: " << is_current << std::endl;

    thread->Stop();
    std::cout << "  RunningForTest after Stop: " << thread->RunningForTest() << std::endl;
    std::cout << "  PASS" << std::endl;
    return true;
}

// =============================================================================
// 测试2: Thread::Invoke - 同步跨线程调用
// =============================================================================
bool TestInvoke() {
    std::cout << "\n--- Test 2: Thread::Invoke (sync cross-thread call) ---" << std::endl;

    auto thread = rtc::Thread::Create();
    thread->SetName("InvokeThread", nullptr);
    thread->Start();

    // Invoke 返回 int
    int result = thread->Invoke<int>(RTC_FROM_HERE, []() {
        return 42;
    });
    std::cout << "  Invoke<int> result: " << result << std::endl;

    // Invoke 返回 string
    std::string str_result = thread->Invoke<std::string>(RTC_FROM_HERE, []() {
        return std::string("hello from worker");
    });
    std::cout << "  Invoke<string> result: " << str_result << std::endl;

    // Invoke 修改原子变量
    thread->Invoke<void>(RTC_FROM_HERE, []() {
        g_invoke_result.store(100);
    });
    std::cout << "  Invoke<void> set g_invoke_result: " << g_invoke_result.load() << std::endl;

    thread->Stop();
    std::cout << "  PASS" << std::endl;
    return true;
}

// =============================================================================
// 测试3: Thread::PostTask - 异步投递任务
// =============================================================================
bool TestPostTask() {
    std::cout << "\n--- Test 3: Thread::PostTask (async) ---" << std::endl;

    g_post_task_count.store(0);
    rtc::Event all_done;
    auto thread = rtc::Thread::Create();
    thread->SetName("PostTaskThread", nullptr);
    thread->Start();

    // 投递多个任务，最后一个设置事件
    for (int i = 0; i < 5; ++i) {
        thread->PostTask(RTC_FROM_HERE, [i, &all_done]() {
            g_post_task_count.fetch_add(1);
            std::cout << "    PostTask #" << i << " executed" << std::endl;
            if (i == 4) all_done.Set();
        });
    }

    // 等待所有任务完成
    all_done.Wait(rtc::Event::kForever);

    int count = g_post_task_count.load();
    std::cout << "  PostTask count: " << count << std::endl;

    thread->Stop();
    std::cout << "  PASS" << std::endl;
    return (count == 5);
}

// =============================================================================
// 测试4: Thread::Post / Send / PostDelayed - 消息传递
// =============================================================================
bool TestMessagePassing() {
    std::cout << "\n--- Test 4: Message Post/Send/PostDelayed ---" << std::endl;

    g_message_count.store(0);
    auto thread = rtc::Thread::Create();
    thread->SetName("MessageThread", nullptr);
    thread->Start();

    MyMessageHandler handler("Handler1");

    // Post 消息
    thread->Post(RTC_FROM_HERE, &handler, 1);
    thread->Post(RTC_FROM_HERE, &handler, 2);

    // Send 消息（同步等待）
    thread->Send(RTC_FROM_HERE, &handler, 3);

    // PostDelayed 消息
    thread->PostDelayed(RTC_FROM_HERE, 100, &handler, 4);

    // 等待延迟消息
    rtc::Thread::SleepMs(200);

    // 用 Invoke 确保所有消息处理完毕
    thread->Invoke<void>(RTC_FROM_HERE, []() {});

    std::cout << "  Message count: " << g_message_count.load() << std::endl;
    std::cout << "  PASS" << std::endl;

    thread->Stop();
    return (g_message_count.load() >= 4);
}

// =============================================================================
// 测试5: Thread::Current / ThreadManager / WrapCurrent / UnwrapCurrent
// =============================================================================
bool TestThreadManager() {
    std::cout << "\n--- Test 5: ThreadManager / WrapCurrent / UnwrapCurrent ---" << std::endl;

    // 主线程没有 wrap，所以 CurrentThread 可能为 nullptr
    rtc::Thread* current = rtc::ThreadManager::Instance()->CurrentThread();
    std::cout << "  CurrentThread before wrap: " << (current ? current->name() : "nullptr") << std::endl;

    // Wrap 当前线程
    rtc::Thread* wrapped = rtc::ThreadManager::Instance()->WrapCurrentThread();
    std::cout << "  Wrapped thread name: " << wrapped->name() << std::endl;
    std::cout << "  IsOwned: " << wrapped->IsOwned() << std::endl;
    std::cout << "  IsCurrent: " << wrapped->IsCurrent() << std::endl;

    // 在 wrapped 线程上 PostTask
    wrapped->PostTask(RTC_FROM_HERE, []() {
        std::cout << "    PostTask on wrapped thread executed" << std::endl;
        g_test_counter.fetch_add(1);
    });

    // 处理消息
    wrapped->ProcessMessages(100);

    // Unwrap
    rtc::ThreadManager::Instance()->UnwrapCurrentThread();
    rtc::Thread* after = rtc::ThreadManager::Instance()->CurrentThread();
    std::cout << "  CurrentThread after unwrap: " << (after ? after->name() : "nullptr") << std::endl;

    std::cout << "  PASS" << std::endl;
    return true;
}

// =============================================================================
// 测试6: Runnable 自定义线程体
// =============================================================================
bool TestRunnable() {
    std::cout << "\n--- Test 6: Runnable ---" << std::endl;

    g_runnable_ran.store(false);
    auto thread = rtc::Thread::Create();
    thread->SetName("RunnableThread", nullptr);

    MyRunnable runnable;
    thread->Start(&runnable);

    // 等 Runnable 执行
    rtc::Thread::SleepMs(100);

    bool ran = g_runnable_ran.load();
    std::cout << "  Runnable ran: " << ran << std::endl;

    thread->Stop();
    std::cout << "  PASS" << std::endl;
    return ran;
}

// =============================================================================
// 测试7: Event 同步原语
// =============================================================================
bool TestEvent() {
    std::cout << "\n--- Test 7: Event synchronization ---" << std::endl;

    rtc::Event event;
    std::atomic<bool> done{false};

    auto thread = rtc::Thread::Create();
    thread->SetName("EventThread", nullptr);
    thread->Start();

    thread->PostTask(RTC_FROM_HERE, [&event, &done]() {
        std::cout << "    Worker: doing work..." << std::endl;
        rtc::Thread::SleepMs(50);
        std::cout << "    Worker: signaling event" << std::endl;
        done.store(true);
        event.Set();
    });

    std::cout << "  Main: waiting for event..." << std::endl;
    bool signaled = event.Wait(1000);
    std::cout << "  Main: event signaled=" << signaled << ", done=" << done.load() << std::endl;

    thread->Stop();
    std::cout << "  PASS" << std::endl;
    return signaled && done.load();
}

// =============================================================================
// 测试8: DisallowBlockingCalls / ScopedDisallowBlockingCalls
// =============================================================================
bool TestDisallowBlockingCalls() {
    std::cout << "\n--- Test 8: DisallowBlockingCalls / ScopedDisallowBlockingCalls ---" << std::endl;

    auto thread = rtc::Thread::Create();
    thread->SetName("NoBlockThread", nullptr);
    thread->Start();

    // 在 worker 线程上禁止阻塞调用
    thread->PostTask(RTC_FROM_HERE, [&thread]() {
        thread->DisallowBlockingCalls();
        std::cout << "    Worker: blocking calls disallowed" << std::endl;
    });

    rtc::Thread::SleepMs(50);

    // 在作用域内禁止阻塞调用
    {
        rtc::Thread::ScopedDisallowBlockingCalls no_block;
        std::cout << "  Main: ScopedDisallowBlockingCalls active" << std::endl;
        // 注意：此作用域内不能调用 Invoke（会 assert）
    }
    std::cout << "  Main: ScopedDisallowBlockingCalls released" << std::endl;

    thread->Stop();
    std::cout << "  PASS" << std::endl;
    return true;
}

// =============================================================================
// 测试9: PlatformThread
// =============================================================================
static void PlatformThreadFunc(void* obj) {
    int* val = static_cast<int*>(obj);
    *val = 999;
    g_platform_thread_val.store(999);
    std::cout << "  [PlatformThread] running, value=" << *val << std::endl;
}

bool TestPlatformThread() {
    std::cout << "\n--- Test 9: PlatformThread ---" << std::endl;

    int value = 0;
    rtc::PlatformThread pt(PlatformThreadFunc, &value, "PlatformTestThread");
    std::cout << "  PlatformThread name: " << pt.name() << std::endl;

    pt.Start();
    std::cout << "  IsRunning: " << pt.IsRunning() << std::endl;

    // 等待线程完成
    rtc::Thread::SleepMs(100);
    pt.Stop();

    std::cout << "  IsRunning after Stop: " << pt.IsRunning() << std::endl;
    std::cout << "  value: " << value << ", atomic: " << g_platform_thread_val.load() << std::endl;
    std::cout << "  PASS" << std::endl;
    return (value == 999);
}

// =============================================================================
// 测试10: Thread::SleepMs / ThreadChecker
// =============================================================================
bool TestSleepAndThreadChecker() {
    std::cout << "\n--- Test 10: SleepMs / ThreadChecker ---" << std::endl;

    auto start = std::chrono::steady_clock::now();
    rtc::Thread::SleepMs(100);
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "  SleepMs(100) actually slept: " << elapsed << "ms" << std::endl;

    // ThreadChecker 验证
    rtc::ThreadChecker checker;
    std::cout << "  ThreadChecker created on current thread" << std::endl;

    auto thread = rtc::Thread::Create();
    thread->SetName("CheckerThread", nullptr);
    thread->Start();

    // 在 worker 线程上创建一个新的 checker
    bool checker_ok = thread->Invoke<bool>(RTC_FROM_HERE, []() {
        rtc::ThreadChecker worker_checker;
        return true;  // 构造成功即可
    });
    std::cout << "  ThreadChecker on worker: " << (checker_ok ? "OK" : "FAIL") << std::endl;

    thread->Stop();
    std::cout << "  PASS" << std::endl;
    return checker_ok;
}

// =============================================================================
// 测试11: CriticalSection 线程安全
// =============================================================================
bool TestCriticalSection() {
    std::cout << "\n--- Test 11: CriticalSection ---" << std::endl;

    g_shared_value = 0;
    auto thread1 = rtc::Thread::Create();
    auto thread2 = rtc::Thread::Create();
    thread1->SetName("CritThread1", nullptr);
    thread2->SetName("CritThread2", nullptr);
    thread1->Start();
    thread2->Start();

    auto increment = []() {
        for (int i = 0; i < 1000; ++i) {
            rtc::CritScope cs(&g_crit);
            ++g_shared_value;
        }
    };

    thread1->PostTask(RTC_FROM_HERE, increment);
    thread2->PostTask(RTC_FROM_HERE, increment);

    thread1->Invoke<void>(RTC_FROM_HERE, []() {});
    thread2->Invoke<void>(RTC_FROM_HERE, []() {});

    std::cout << "  g_shared_value after 2x1000 increments: " << g_shared_value << std::endl;

    thread1->Stop();
    thread2->Stop();
    std::cout << "  PASS" << std::endl;
    return (g_shared_value == 2000);
}

// =============================================================================
// 测试12: 多线程 PostTask 交叉调用
// =============================================================================
bool TestMultiThreadPostTaskInterleaving() {
    std::cout << "\n--- Test 12: MultiThread PostTask interleaving ---" << std::endl;

    g_test_counter.store(0);
    rtc::Event all_done;
    auto thread_a = rtc::Thread::Create();
    auto thread_b = rtc::Thread::Create();
    thread_a->SetName("ThreadA", nullptr);
    thread_b->SetName("ThreadB", nullptr);
    thread_a->Start();
    thread_b->Start();

    // ThreadA 向 ThreadB 投递任务，完成后设置事件
    thread_a->PostTask(RTC_FROM_HERE, [&thread_b, &all_done]() {
        std::cout << "    ThreadA: posting task to ThreadB" << std::endl;
        g_test_counter.fetch_add(1);
        thread_b->PostTask(RTC_FROM_HERE, [&all_done]() {
            g_test_counter.fetch_add(1);
            std::cout << "    ThreadB: received task from ThreadA" << std::endl;
            all_done.Set();
        });
    });

    // 等待交叉任务完成
    all_done.Wait(rtc::Event::kForever);

    int count = g_test_counter.load();
    std::cout << "  Cross-thread tasks: " << count << std::endl;

    thread_a->Stop();
    thread_b->Stop();
    std::cout << "  PASS" << std::endl;
    return (count >= 2);
}

// =============================================================================
// 测试13: ProcessMessages / IsProcessingMessagesForTesting
// =============================================================================
bool TestProcessMessages() {
    std::cout << "\n--- Test 13: ProcessMessages ---" << std::endl;

    auto thread = rtc::Thread::Create();
    thread->SetName("ProcessMsgThread", nullptr);
    thread->Start();

    g_test_counter.store(0);

    // 投递 3 个任务
    for (int i = 0; i < 3; ++i) {
        thread->PostTask(RTC_FROM_HERE, [i]() {
            std::cout << "    ProcessMessages task #" << i << std::endl;
            g_test_counter.fetch_add(1);
        });
    }

    // 等待消息处理
    rtc::Thread::SleepMs(100);

    std::cout << "  IsProcessingMessagesForTesting: " << thread->IsProcessingMessagesForTesting() << std::endl;
    std::cout << "  Tasks processed: " << g_test_counter.load() << std::endl;

    // 用 Invoke 确保所有任务完成
    thread->Invoke<void>(RTC_FROM_HERE, []() {});
    std::cout << "  After Invoke sync: " << g_test_counter.load() << std::endl;

    thread->Stop();
    std::cout << "  PASS" << std::endl;
    return (g_test_counter.load() >= 3);
}

// =============================================================================
// main
// =============================================================================
int main() {
    std::cout << "=== WebRTC Threading API Test ===\n" << std::endl;

    bool all_pass = true;
    all_pass = TestBasicThreadLifecycle() && all_pass;
    all_pass = TestInvoke() && all_pass;
    all_pass = TestPostTask() && all_pass;
    all_pass = TestMessagePassing() && all_pass;
    all_pass = TestThreadManager() && all_pass;
    all_pass = TestRunnable() && all_pass;
    all_pass = TestEvent() && all_pass;
    all_pass = TestDisallowBlockingCalls() && all_pass;
    all_pass = TestPlatformThread() && all_pass;
    all_pass = TestSleepAndThreadChecker() && all_pass;
    all_pass = TestCriticalSection() && all_pass;
    all_pass = TestMultiThreadPostTaskInterleaving() && all_pass;
    all_pass = TestProcessMessages() && all_pass;

    std::cout << "\n========================================" << std::endl;
    std::cout << "Overall result: " << (all_pass ? "ALL PASS" : "FAIL") << std::endl;
    std::cout << "========================================" << std::endl;

    return all_pass ? 0 : 1;
}