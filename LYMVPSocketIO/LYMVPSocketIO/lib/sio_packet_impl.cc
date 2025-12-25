#include "sio_packet_impl.h"
#include "sio_packet_parser.h"
#include "sio_ack_manager.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/logging.h"
#include <chrono>
#include <sstream>
#include <iomanip>

namespace sio {

// ============================================================================
// PacketSender 类实现
// ============================================================================

PacketSender::PacketSender(std::shared_ptr<IAckManager> ack_manager,
                          webrtc::TaskQueueFactory* task_queue_factory,
                          const Config& config)
    : ack_manager_(ack_manager),
      config_(config),
      initialized_(false),
      running_(false) {
    
    initialize_task_queue();
}

PacketSender::~PacketSender() {
    stop_cleanup_timer();
    reset();
}

void PacketSender::initialize_task_queue() {
    if (initialized_) {
        return;
    }
    
    if (!task_queue_factory_) {
        task_queue_factory_ = webrtc::CreateDefaultTaskQueueFactory();
    }
    
    if (task_queue_factory_) {
        task_queue_ = std::make_shared<rtc::TaskQueue>(
            task_queue_factory_->CreateTaskQueue(
                "packet_sender",
                webrtc::TaskQueueFactory::Priority::NORMAL));
        
        packet_builder_.reset(new SioPacketBuilder(config_.version));
        
        start_cleanup_timer();
        initialized_ = true;
        running_ = true;
    }
}

void PacketSender::set_config(const Config& config) {
    config_ = config;
    if (packet_builder_) {
        packet_builder_->set_version(config.version);
    }
}

//void PacketSender::set_ack_manager(std::shared_ptr<IAckManager> ack_manager) {
//    stop_cleanup_timer();
//    ack_manager_ = ack_manager;
//    start_cleanup_timer();
//}

bool PacketSender::send_event(const std::string& event_name,
                             const std::vector<Json::Value>& args,
                             TextSendCallback text_callback,
                             SendResultCallback complete_callback,
                              const std::string& namespace_s) {
    if (!packet_builder_ || !text_callback) {
        if (complete_callback) {
            complete_callback(false, "Packet builder or text callback is null");
        }
        return false;
    }
    
    // 构建包
    auto packet = packet_builder_->build_event_packet(
        event_name, args, "/", -1);
    auto encoded = packet_builder_->encode_packet(packet);
    
    if (encoded.text_packet.empty()) {
        if (complete_callback) {
            complete_callback(false, "Failed to encode packet");
        }
        return false;
    }
    
    // 发送文本包和二进制数据
    bool text_sent = text_callback(encoded.text_packet, encoded.binary_parts);
    if (!text_sent) {
        if (complete_callback) {
            complete_callback(false, "Failed to send text packet");
        }
        
        webrtc::MutexLock lock(&stats_mutex_);
        stats_.total_sent++;
        stats_.total_failed++;
        return false;
    }
    
    bool success = text_sent;
    
    if (complete_callback) {
        complete_callback(success, success ? "" : "Failed to send data");
    }
    
    webrtc::MutexLock lock(&stats_mutex_);
    stats_.total_sent++;
    if (!success) {
        stats_.total_failed++;
    }
    
    return success;
}

int PacketSender::send_event_with_ack(
    const std::string& event_name,
    const std::vector<Json::Value>& args,
    TextSendCallback text_callback,
    AckCallback ack_callback,
    AckTimeoutCallback timeout_callback,
    std::chrono::milliseconds timeout,
    const std::string& namespace_s) {
    
    if (!packet_builder_ || !text_callback || !ack_manager_) {
        return -1;
    }
    
    // 生成ACK ID
    int ack_id = ack_manager_->generate_ack_id();
    if (ack_id <= 0) {
        return -1;
    }
    
    // 使用配置的超时时间
    if (timeout.count() <= 0) {
        timeout = config_.default_ack_timeout;
    }
    
    // 注册ACK回调
    bool registered = ack_manager_->register_ack_callback(
                                                          ack_id,
                                                          [this, ack_callback](const std::vector<Json::Value>& data_array) {
                                                              // 成功回调
                                                              webrtc::MutexLock lock(&stats_mutex_);
                                                              stats_.total_acked++;
                                                              
                                                              if (ack_callback) {
                                                                  ack_callback(data_array);
                                                              }
                                                          },
                                                          timeout,
                                                          [timeout_callback](int ack_id) {
                                                              if (timeout_callback) {
                                                                  timeout_callback(ack_id);
                                                              }
                                                          });
    
    if (!registered) {
        return -1;
    }
    
    // 构建包
    auto packet = packet_builder_->build_event_packet(
                                                      event_name, args, namespace_s, ack_id);
    auto encoded = packet_builder_->encode_packet(packet);
    
    if (encoded.text_packet.empty()) {
        ack_manager_->cancel_ack(ack_id);
        return -1;
    }
    
    // 记录待处理请求
    {
        webrtc::MutexLock lock(&pending_mutex_);
        PendingRequest request;
        request.ack_id = ack_id;
        request.send_time = std::chrono::steady_clock::now();
        request.timeout = timeout;
        request.waiting_for_ack = true;
        request.event_name = event_name;
        pending_requests_[ack_id] = request;
    }
    
    // 发送文本包和二进制数据
    bool text_sent = text_callback(encoded.text_packet, encoded.binary_parts);
    if (!text_sent) {
        ack_manager_->cancel_ack(ack_id);
        
        webrtc::MutexLock lock(&pending_mutex_);
        pending_requests_.erase(ack_id);
        
        webrtc::MutexLock stats_lock(&stats_mutex_);
        stats_.total_failed++;
        
        return -1;
    }
    
    webrtc::MutexLock lock(&stats_mutex_);
    stats_.total_sent++;
    
    return ack_id;
}

bool PacketSender::send_ack_response(
    int ack_id,
    const std::vector<Json::Value>& args,
    TextSendCallback text_callback,
    const std::string& namespace_s) {
    
    if (ack_id <= 0 || !packet_builder_ || !text_callback) {
        return false;
    }
    
    // 构建ACK响应包
    auto packet = packet_builder_->build_ack_packet(args, namespace_s, ack_id);
    auto encoded = packet_builder_->encode_packet(packet);
    
    if (encoded.text_packet.empty()) {
        return false;
    }
    
    // 发送文本包和二进制数据
    bool text_sent = text_callback(encoded.text_packet, encoded.binary_parts);
    if (!text_sent) {
        webrtc::MutexLock lock(&stats_mutex_);
        stats_.total_failed++;
        return false;
    }
    
    return text_sent;
}

void PacketSender::reset() {
    stop_cleanup_timer();
    
    if (ack_manager_) {
        ack_manager_->clear_all_acks();
    }
    
    {
        webrtc::MutexLock lock(&pending_mutex_);
        pending_requests_.clear();
    }
    
    {
        webrtc::MutexLock lock(&stats_mutex_);
        stats_ = Stats();
    }
}

PacketSender::Stats PacketSender::get_stats() const {
    webrtc::MutexLock lock(&stats_mutex_);
    return stats_;
}

void PacketSender::cleanup_expired_requests() {
    if (!running_) {
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    std::vector<int> expired_ids;
    
    {
        webrtc::MutexLock lock(&pending_mutex_);
        
        for (const auto& kv : pending_requests_) {
            int ack_id = kv.first;
            const PendingRequest& request = kv.second;
            
            if (request.waiting_for_ack && request.is_expired()) {
                expired_ids.push_back(ack_id);
            }
        }
    }
    
    // 取消超时的ACK
    for (int ack_id : expired_ids) {
        if (ack_manager_) {
            ack_manager_->cancel_ack(ack_id);
        }
        
        webrtc::MutexLock lock(&pending_mutex_);
        pending_requests_.erase(ack_id);
        
        webrtc::MutexLock stats_lock(&stats_mutex_);
        stats_.total_timeout++;
    }
}

void PacketSender::start_cleanup_timer() {
    if (!task_queue_ || !running_) {
        return;
    }
    
    std::weak_ptr<PacketSender> weak_this = shared_from_this();
    
    cleanup_handle_ = webrtc::RepeatingTaskHandle::Start(
        task_queue_->Get(),
        [weak_this]() {
            auto self = weak_this.lock();
            if (!self || !self->running_) {
                return webrtc::TimeDelta::PlusInfinity();
            }
            
            self->cleanup_expired_requests();
            
            // 每1秒清理一次
            return webrtc::TimeDelta::ms(1000);
        });
}

void PacketSender::stop_cleanup_timer() {
    running_ = false;
    if (cleanup_handle_.Running()) {
        cleanup_handle_.Stop();
    }
}

// ============================================================================
// PacketReceiver 类实现
// ============================================================================

PacketReceiver::PacketReceiver(std::shared_ptr<IAckManager> ack_manager,
                              webrtc::TaskQueueFactory* task_queue_factory,
                              const Config& config)
    : ack_manager_(ack_manager),
      config_(config),
      state_() {
    
    initialize_task_queue();
}

PacketReceiver::~PacketReceiver() {
    reset();
}

void PacketReceiver::initialize_task_queue() {
    if (!task_queue_factory_) {
        task_queue_factory_ = webrtc::CreateDefaultTaskQueueFactory();
    }
    
    if (task_queue_factory_) {
        task_queue_ = std::make_shared<rtc::TaskQueue>(
            task_queue_factory_->CreateTaskQueue(
                "packet_receiver",
                webrtc::TaskQueueFactory::Priority::NORMAL));
        
        packet_builder_.reset(new SioPacketBuilder(config_.default_version));
        
    }
}

void PacketReceiver::set_config(const Config& config) {
    config_ = config;
    if (packet_builder_) {
        packet_builder_->set_version(config.default_version);
    }
}

//void PacketReceiver::set_ack_manager(std::shared_ptr<IAckManager> ack_manager) {
//    ack_manager_ = ack_manager;
//}

void PacketReceiver::set_event_callback(EventCallback callback) {
    event_callback_ = callback;
}

bool PacketReceiver::process_text_packet(const std::string& text_packet) {
    if (text_packet.empty()) {
        webrtc::MutexLock lock(&stats_mutex_);
        stats_.parse_errors++;
        return false;
    }
    
    // 检测协议版本
    SocketIOVersion version = config_.default_version;
    
    // 解码包
    SioPacket packet = packet_builder_->decode_packet(text_packet);
    
    if (packet.event_name.empty() && packet.args.empty()) {
        RTC_LOG(LS_ERROR)<<"Empty packet or packet with only binary data packet:"<<packet.to_string();
        webrtc::MutexLock lock(&stats_mutex_);
        stats_.parse_errors++;
        return false;
    }
    
    // 更新状态
    {
        webrtc::MutexLock lock(&state_mutex_);
        state_.reset();
        state_.current_packet = packet;
        state_.packet_version = version;
        state_.original_text_packet = text_packet;  // 保存原始文本包
        
        // 重要：设置正确的二进制计数
        // 这里需要设置期望的二进制数据数量
        // 对于V3，packet.binary_count已经包含了二进制计数
        // 对于V2，可能需要从JSON数据中提取
        
        state_.expected_binary_count = 0;
        
        // 检查是否是二进制包
        if (packet.is_binary()) {
            // 如果是二进制包，我们需要等待二进制数据
            // 设置期望的二进制数量
            state_.expected_binary_count = packet.binary_count;
            if (state_.expected_binary_count > 0) {
                state_.state = ReceiveState::WAITING_FOR_BINARY;
                // 重要：不要在这里处理包，等待二进制数据
            } else {
                state_.state = ReceiveState::COMPLETE;
                // 如果没有二进制数据，直接处理
            }
        } else {
            state_.state = ReceiveState::COMPLETE;
            // 非二进制包直接处理
        }
    }
    
    // 更新统计
    {
        webrtc::MutexLock lock(&stats_mutex_);
        stats_.total_received++;
        if (packet.is_binary()) {
            stats_.binary_packets++;
        } else {
            stats_.text_packets++;
        }
    }
    
    // 检查是否可以直接处理
    {
        webrtc::MutexLock lock(&state_mutex_);
        if (state_.state == ReceiveState::COMPLETE) {
            process_complete_packet(packet);
        }
    }
    
    return true;
}

bool PacketReceiver::process_binary_data(const SmartBuffer& binary_data) {
    webrtc::MutexLock lock(&state_mutex_);
    
    if (state_.state != ReceiveState::WAITING_FOR_BINARY) {
        // 如果不在等待二进制数据状态，可能是重复的二进制数据或者状态错误
        // 记录日志但不返回false，继续处理
//        std::cout << "Warning: Received binary data but not in WAITING_FOR_BINARY state" << std::endl;
        RTC_LOG(LS_WARNING) << "Warning: Received binary data but not in WAITING_FOR_BINARY state";
    }
    
    // 检查二进制数据大小
    if (binary_data.size() > config_.max_binary_size) {
        RTC_LOG(LS_WARNING) <<  "Error: Binary data too large: " << binary_data.size()
                  << " > " << config_.max_binary_size;
        return false;
    }
    
    // 添加二进制数据
    state_.received_binaries.push_back(binary_data);
    
    // 检查是否接收完成
    bool complete = false;
    if (state_.state == ReceiveState::WAITING_FOR_BINARY) {
        if (static_cast<int>(state_.received_binaries.size()) >= state_.expected_binary_count) {
            state_.state = ReceiveState::COMPLETE;
            complete = true;
        }
    }
    
    // 如果完成，处理完整的包
    if (complete && !state_.original_text_packet.empty()) {
        // 重新解码包，包含二进制数据
        if (packet_builder_) {
            SioPacket complete_packet = packet_builder_->decode_packet(
                state_.original_text_packet, state_.received_binaries);
            complete_packet.version = state_.packet_version;
            
            // 在任务队列中处理完整的包
            if (task_queue_) {
                task_queue_->PostTask([this, complete_packet]() {
                    process_complete_packet(complete_packet);
                });
            } else {
                process_complete_packet(complete_packet);
            }
            
            // 重置状态
            state_.reset();
            return true;
        }
    }
    
    return true;
}

void PacketReceiver::reset() {
    webrtc::MutexLock lock(&state_mutex_);
    state_.reset();
    
    webrtc::MutexLock stats_lock(&stats_mutex_);
    stats_ = Stats();
}

bool PacketReceiver::is_waiting_for_binary() const {
    webrtc::MutexLock lock(&state_mutex_);
    return state_.state == ReceiveState::WAITING_FOR_BINARY;
}

int PacketReceiver::get_expected_binary_count() const {
    webrtc::MutexLock lock(&state_mutex_);
    return state_.expected_binary_count;
}

int PacketReceiver::get_received_binary_count() const {
    webrtc::MutexLock lock(&state_mutex_);
    return static_cast<int>(state_.received_binaries.size());
}

PacketReceiver::Stats PacketReceiver::get_stats() const {
    webrtc::MutexLock lock(&stats_mutex_);
    return stats_;
}

void PacketReceiver::process_complete_packet(const SioPacket& packet) {
    // 处理ACK包
    if (packet.type == PacketType::ACK || packet.type == PacketType::BINARY_ACK) {
        handle_ack_packet(packet);
        
        webrtc::MutexLock lock(&stats_mutex_);
        stats_.ack_processed++;
    }
    
    if (task_queue_) {
        task_queue_->PostTask([this, packet]() {
            event_callback_(packet);
        });
    } else {
        event_callback_(packet);
    }
}

void PacketReceiver::handle_ack_packet(const SioPacket& packet) {
    if (!ack_manager_ || packet.ack_id <= 0) {
        return;
    }
    
    // 处理ACK响应
    ack_manager_->handle_ack_response(packet.ack_id, packet.args);
}

} // namespace sio
