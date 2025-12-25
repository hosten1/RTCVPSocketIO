// Socket.IO 二进制包测试程序
// 测试目标: 验证Socket.IO二进制事件的完整发送和接收流程
// Socket.IO 协议参考: https://socket.io/docs/v4/protocol/
// Socket.IO 二进制事件参考: https://socket.io/docs/v4/binary-events/
//
// 测试流程:
// 1. 创建TaskQueueFactory用于异步操作
// 2. 初始化PacketSender和PacketReceiver
// 3. 设置事件回调处理
// 4. 创建包含二进制数据的测试事件
// 5. 使用PacketSender发送事件
// 6. 验证事件接收和二进制数据完整性
// 7. 清理资源

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>

#include "sio_packet_impl.h"
#include "sio_smart_buffer.hpp"
#include "json/json.h"
#include "api/task_queue/default_task_queue_factory.h"

using namespace sio;
using namespace std;

/**
 * Socket.IO 二进制包测试主函数
 * 演示了从事件创建、发送到接收、验证的完整流程
 */
int main(int argc, char* argv[]) {
    cout << "=== Socket.IO 二进制包测试程序 ===\n";
    
    // 创建任务队列工厂
    std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();
    if (!task_queue_factory) {
        cout << "创建任务队列工厂失败\n";
        return -1;
    }
    
    // 初始化 PacketSender
    PacketSender::Config sender_config;
    sender_config.version = SocketIOVersion::V4;
    auto packet_sender = make_shared<PacketSender>(nullptr, task_queue_factory.get(), sender_config);
    
    // 初始化 PacketReceiver
    PacketReceiver::Config receiver_config;
    receiver_config.default_version = SocketIOVersion::V4;
    receiver_config.auto_detect_version = false; // 禁用版本检测，避免调用未实现的detect_version方法
    auto packet_receiver = make_shared<PacketReceiver>(nullptr, task_queue_factory.get(), receiver_config);
    
    // 事件处理标志
    bool event_received = false;
    string received_event;
    vector<Json::Value> received_args;
    
    // 设置事件回调
    packet_receiver->set_event_callback([&](const string& event, const vector<Json::Value>& args) {
        cout << "收到事件: " << event << endl;
        cout << "参数数量: " << args.size() << endl;
        
        received_event = event;
        received_args = args;
        event_received = true;
        
        // 打印每个参数
        for (size_t i = 0; i < args.size(); ++i) {
            const Json::Value& arg = args[i];
            if (arg.isString()) {
                cout << "参数 " << i << " (字符串): " << arg.asString() << endl;
            } else if (arg.isInt()) {
                cout << "参数 " << i << " (整数): " << arg.asInt() << endl;
            } else if (arg.isDouble()) {
                cout << "参数 " << i << " (浮点数): " << arg.asDouble() << endl;
            } else if (arg.isBool()) {
                cout << "参数 " << i << " (布尔值): " << (arg.asBool() ? "true" : "false") << endl;
            } else if (arg.isArray()) {
                cout << "参数 " << i << " (数组): " << arg.size() << " 个元素" << endl;
            } else if (arg.isObject()) {
                cout << "参数 " << i << " (对象): " << arg.getMemberNames().size() << " 个属性" << endl;
            } else if (binary_helper::is_binary(arg)) {
                auto binary_ptr = binary_helper::get_binary_shared_ptr(arg);
                cout << "参数 " << i << " (二进制): " << binary_ptr->size() << " 字节" << endl;
            } else {
                cout << "参数 " << i << " (其他类型): " << arg.toStyledString() << endl;
            }
        }
    });
    
    // 创建二进制数据
    string binary_data_str = "这是一个测试二进制数据，包含各种字符：1234567890!@#$%^&*()_+";
    SmartBuffer binary_data((const uint8_t*)binary_data_str.data(), binary_data_str.size());
    cout << "创建二进制数据，长度: " << binary_data.size() << " 字节\n";
    
    // 创建带有二进制数据的JSON参数
    vector<Json::Value> args;
    args.push_back(Json::Value("test_binary_event"));  // 事件名称
    args.push_back(Json::Value(12345));               // 整数参数
    args.push_back(Json::Value(3.1415926));           // 浮点数参数
    args.push_back(Json::Value(true));                // 布尔参数
    
    // 添加二进制数据
    Json::Value binary_json = binary_helper::create_binary_value(binary_data.buffer());
    args.push_back(binary_json);                      // 二进制参数
    
    // 发送事件
    cout << "\n发送带二进制数据的事件...\n";
    string event_name = "test_event";
    
    bool send_success = packet_sender->send_event(event_name, args, 
        [&](const string& text_packet) {
            cout << "生成的文本包: " << text_packet << endl;
            
            // 直接将文本包传递给接收器处理
            bool parse_success = packet_receiver->process_text_packet(text_packet);
            if (!parse_success) {
                cout << "解析文本包失败\n";
            }
            return true;
        },
        [&](const SmartBuffer& smart_buffer, int index) {
            cout << "生成的二进制数据 (索引 " << index << "): " << smart_buffer.size() << " 字节\n";
            
            // 直接将二进制数据传递给接收器处理
            bool parse_success = packet_receiver->process_binary_data(smart_buffer);
            if (!parse_success) {
                cout << "解析二进制数据失败\n";
            }
            return true;
        },
        [&](bool success, const string& error) {
            if (success) {
                cout << "事件发送成功\n";
            } else {
                cout << "事件发送失败: " << error << endl;
            }
        },
        0);
    
    if (!send_success) {
        cout << "send_event 调用失败\n";
        return -1;
    }
    
    // 等待事件处理完成
    cout << "\n等待事件处理完成...\n";
    this_thread::sleep_for(chrono::seconds(2));
    
    // 验证结果
    cout << "\n=== 测试结果 ===\n";
    if (event_received) {
        cout << "✓ 事件接收成功\n";
        cout << "✓ 事件名称: " << received_event << endl;
        cout << "✓ 参数数量: " << received_args.size() << endl;
        
        // 检查是否收到二进制数据
            bool binary_received = false;
            for (const auto& arg : received_args) {
                if (binary_helper::is_binary(arg)) {
                    // 验证二进制数据内容
                    auto binary_ptr = binary_helper::get_binary_shared_ptr(arg);
                    cout << "✓ 收到二进制数据，长度: " << binary_ptr->size() << " 字节\n";
                    
                    if (binary_ptr && binary_ptr->size() == binary_data.size()) {
                        bool content_match = memcmp(binary_ptr->data(), binary_data.data(), binary_data.size()) == 0;
                        if (content_match) {
                            cout << "✓ 二进制数据内容匹配\n";
                        } else {
                            cout << "✗ 二进制数据内容不匹配\n";
                        }
                    } else {
                        cout << "✗ 二进制数据长度不匹配\n";
                    }
                    binary_received = true;
                    break;
                }
            }
        
        if (!binary_received) {
            cout << "✗ 未收到二进制数据\n";
        }
        
        cout << "\n=== 测试通过！===\n";
    } else {
        cout << "✗ 未收到事件\n";
        cout << "\n=== 测试失败！===\n";
        return -1;
    }
    
    // 清理资源
    this_thread::sleep_for(chrono::seconds(1));
    
    return 0;
}