#include "test_sio_packet.h"
#include "test_client_core.h"
#include <iostream>
#include <cstdlib>
#include "rtc_base/logging.h"

int main() {
    RTC_LOG(LS_INFO) << "================================================================";
    RTC_LOG(LS_INFO) << "          Socket.IO 异步包处理库 - 详细测试套件";
    RTC_LOG(LS_INFO) << "================================================================";
    
    int tests_passed = 0;
    int tests_failed = 0;
    
    auto run_test = [&](const std::string& test_name, void (*test_func)()) {
        RTC_LOG(LS_INFO) << "\n\n" << std::string(80, '=');
        RTC_LOG(LS_INFO) << "开始测试: " << test_name;
        RTC_LOG(LS_INFO) << std::string(80, '=');
        
        try {
            test_func();
            tests_passed++;
            RTC_LOG(LS_INFO) << "\n✅ 测试通过: " << test_name;
        } catch (const std::exception& e) {
            tests_failed++;
            RTC_LOG(LS_ERROR) << "\n❌ 测试失败: " << test_name;
            RTC_LOG(LS_ERROR) << "   错误: " << e.what();
        } catch (...) {
            tests_failed++;
            RTC_LOG(LS_ERROR) << "\n❌ 测试失败: " << test_name;
            RTC_LOG(LS_ERROR) << "   未知错误";
        }
    };
    
    // 运行所有测试
    run_test("嵌套结构测试", sio_test::test_nested_structures);
    run_test("PacketSender和PacketReceiver测试", sio_test::test_packet_sender_receiver);
    run_test("版本兼容性测试", sio_test::test_version_compatibility);
    
    // 运行客户端核心测试
    run_test("客户端核心基本测试", sio_test::test_client_core_basic);
    run_test("客户端核心数据发送测试", sio_test::test_client_core_emit_data);
    run_test("客户端核心带ACK发送测试", sio_test::test_client_core_emit_with_ack);
    run_test("客户端核心超时测试", sio_test::test_client_core_timeout);
    run_test("客户端核心状态变化测试", sio_test::test_client_core_status_changes);
    run_test("客户端核心版本特定事件测试", sio_test::test_client_core_version_specific_events);
    run_test("客户端核心版本切换测试", sio_test::test_client_core_version_switching);
    run_test("客户端核心事件监听器测试", sio_test::test_client_core_event_listeners);
    
    // 打印测试总结
    RTC_LOG(LS_INFO) << "\n\n" << std::string(80, '=');
    RTC_LOG(LS_INFO) << "测试总结";
    RTC_LOG(LS_INFO) << std::string(80, '=');
    
    RTC_LOG(LS_INFO) << "总测试数: " << (tests_passed + tests_failed);
    RTC_LOG(LS_INFO) << "通过测试: " << tests_passed;
    RTC_LOG(LS_INFO) << "失败测试: " << tests_failed;
    
    if (tests_failed == 0) {
        RTC_LOG(LS_INFO) << "\n🎉 所有测试通过！";
    } else {
        RTC_LOG(LS_ERROR) << "\n❌ 有 " << tests_failed << " 个测试失败";
    }
    
    return tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}