#include "test_sio_packet.h"
#include <iostream>
#include <cstdlib>

int main() {
    std::cout << "================================================================" << std::endl;
    std::cout << "          Socket.IO 异步包处理库 - 详细测试套件" << std::endl;
    std::cout << "================================================================" << std::endl;
    
    int tests_passed = 0;
    int tests_failed = 0;
    
    auto run_test = [&](const std::string& test_name, void (*test_func)()) {
        std::cout << "\n\n" << std::string(80, '=') << std::endl;
        std::cout << "开始测试: " << test_name << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        try {
            test_func();
            tests_passed++;
            std::cout << "\n✅ 测试通过: " << test_name << std::endl;
        } catch (const std::exception& e) {
            tests_failed++;
            std::cerr << "\n❌ 测试失败: " << test_name << std::endl;
            std::cerr << "   错误: " << e.what() << std::endl;
        } catch (...) {
            tests_failed++;
            std::cerr << "\n❌ 测试失败: " << test_name << std::endl;
            std::cerr << "   未知错误" << std::endl;
        }
    };
    
    // 运行所有测试
    run_test("嵌套结构测试", sio_test::test_nested_structures);
    run_test("PacketSender和PacketReceiver测试", sio_test::test_packet_sender_receiver);
    run_test("版本兼容性测试", sio_test::test_version_compatibility);
    
    // 打印测试总结
    std::cout << "\n\n" << std::string(80, '=') << std::endl;
    std::cout << "测试总结" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    std::cout << "总测试数: " << (tests_passed + tests_failed) << std::endl;
    std::cout << "通过测试: " << tests_passed << std::endl;
    std::cout << "失败测试: " << tests_failed << std::endl;
    
    if (tests_failed == 0) {
        std::cout << "\n🎉 所有测试通过！" << std::endl;
    } else {
        std::cout << "\n❌ 有 " << tests_failed << " 个测试失败" << std::endl;
    }
    
    return tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}