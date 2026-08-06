#include "sio_packet_impl.h"
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

std::shared_ptr<PacketSender> PacketSender::Create(std::shared_ptr<IAckManager> ack_manager,
                                                   webrtc::TaskQueueFactory* task_queue_factory,
                                                   const Config& config) {
    auto sender = std::shared_ptr<PacketSender>(
        new PacketSender(ack_manager, task_queue_factory, config));
    sender->Start();
    return sender;
}

PacketSender::PacketSender(std::shared_ptr<IAckManager> ack_manager,
                          webrtc::TaskQueueFactory* task_queue_factory,
                          const Config& config)
    : ack_manager_(ack_manager),
      config_(config),
      initialized_(false),
      running_(false) {
}

PacketSender::~PacketSender() {
    stop_cleanup_timer();
    reset();
}

void PacketSender::Start() {
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
        
        initialized_ = true;
        running_ = true;
        
        start_cleanup_timer();
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
    if (!initialized_) {
        Start();
    }
    
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
    
    if (!initialized_) {
        Start();
    }
    
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
        request.args = args;
        request.namespace_s = namespace_s;
        request.retry_count = 0;
        request.ack_callback = ack_callback;
        request.timeout_callback = timeout_callback;
        request.send_callback = text_callback;
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
    std::vector<int> retry_ids;
    
    {
        webrtc::MutexLock lock(&pending_mutex_);
        for (const auto& kv : pending_requests_) {
            int ack_id = kv.first;
            const PendingRequest& request = kv.second;
            
            if (request.waiting_for_ack && request.is_expired()) {
                if (request.retry_count < config_.max_retries && config_.max_retries > 0) {
                    retry_ids.push_back(ack_id);
                } else {
                    expired_ids.push_back(ack_id);
                }
            }
        }
    }
    
    // 先处理重发
    for (int ack_id : retry_ids) {
        PendingRequest request;
        {
            webrtc::MutexLock lock(&pending_mutex_);
            auto it = pending_requests_.find(ack_id);
            if (it == pending_requests_.end()) continue;
            request = it->second;
        }
        
        if (retry_request(request)) {
            webrtc::MutexLock lock(&pending_mutex_);
            auto it = pending_requests_.find(ack_id);
            if (it != pending_requests_.end()) {
                pending_requests_.erase(it);
            }
            pending_requests_[request.ack_id] = request;
            
            RTC_LOG(LS_INFO) << "[PacketSender] Retry event '" << request.event_name 
                             << "', retry_count=" << request.retry_count
                             << ", new_ack_id=" << request.ack_id;
        }
    }
    
    // 处理真正超时的（重发次数用完的）
    for (int ack_id : expired_ids) {
        PendingRequest request;
        {
            webrtc::MutexLock lock(&pending_mutex_);
            auto it = pending_requests_.find(ack_id);
            if (it != pending_requests_.end()) {
                request = it->second;
                pending_requests_.erase(it);
            }
        }
        
        if (ack_manager_) {
            ack_manager_->cancel_ack(ack_id);
        }
        
        if (request.timeout_callback) {
            request.timeout_callback(ack_id);
        }
        
        webrtc::MutexLock stats_lock(&stats_mutex_);
        stats_.total_timeout++;
        
        RTC_LOG(LS_WARNING) << "[PacketSender] ACK timeout after " 
                            << config_.max_retries << " retries, event=" 
                            << request.event_name << ", ack_id=" << ack_id;
    }
}

bool PacketSender::retry_request(PendingRequest& request) {
    if (!ack_manager_ || !packet_builder_ || !request.send_callback) {
        return false;
    }
    
    int old_ack_id = request.ack_id;
    
    ack_manager_->cancel_ack(old_ack_id);
    
    int new_ack_id = ack_manager_->generate_ack_id();
    if (new_ack_id <= 0) {
        return false;
    }
    
    bool registered = ack_manager_->register_ack_callback(
        new_ack_id,
        [this, request](const std::vector<Json::Value>& data_array) {
            webrtc::MutexLock lock(&stats_mutex_);
            stats_.total_acked++;
            if (request.ack_callback) {
                request.ack_callback(data_array);
            }
        },
        request.timeout,
        [request](int aid) {
            if (request.timeout_callback) {
                request.timeout_callback(aid);
            }
        });
    
    if (!registered) {
        return false;
    }
    
    auto packet = packet_builder_->build_event_packet(
        request.event_name, request.args, request.namespace_s, new_ack_id);
    auto encoded = packet_builder_->encode_packet(packet);
    
    if (encoded.text_packet.empty()) {
        ack_manager_->cancel_ack(new_ack_id);
        return false;
    }
    
    bool text_sent = request.send_callback(encoded.text_packet, encoded.binary_parts);
    if (!text_sent) {
        ack_manager_->cancel_ack(new_ack_id);
        return false;
    }
    
    request.ack_id = new_ack_id;
    request.send_time = std::chrono::steady_clock::now();
    request.retry_count++;
    
    webrtc::MutexLock stats_lock(&stats_mutex_);
    stats_.total_sent++;
    
    return true;
}

void PacketSender::start_cleanup_timer() {
    if (!task_queue_ || !running_) {
        RTC_LOG(LS_WARNING) << "[PacketSender] start_cleanup_timer skipped: task_queue_=" 
                            << (task_queue_ ? "yes" : "no") 
                            << ", running_=" << running_.load();
        return;
    }
    
    std::weak_ptr<PacketSender> weak_this = shared_from_this();
    
    RTC_LOG(LS_INFO) << "[PacketSender] Starting cleanup timer";
    
    cleanup_handle_ = webrtc::RepeatingTaskHandle::Start(
        task_queue_->Get(),
        [weak_this]() {
            auto self = weak_this.lock();
            if (!self || !self->running_) {
                return webrtc::TimeDelta::PlusInfinity();
            }
            
            self->cleanup_expired_requests();
            
            return webrtc::TimeDelta::ms(100);
        });
    
    RTC_LOG(LS_INFO) << "[PacketSender] Cleanup timer started: " << cleanup_handle_.Running();
}

void PacketSender::stop_cleanup_timer() {
    running_ = false;
    if (cleanup_handle_.Running()) {
        cleanup_handle_.Stop();
    }
}

// ─── 简洁 API 实现 ──────────────────────────────────────

void PacketSender::set_send_callback(TextSendCallback callback) {
    webrtc::MutexLock lock(&send_callback_mutex_);
    send_callback_ = std::move(callback);
}

void PacketSender::emit(const std::string& event_name,
                       std::initializer_list<Json::Value> args,
                       const std::string& namespace_s) {
    TextSendCallback cb;
    {
        webrtc::MutexLock lock(&send_callback_mutex_);
        cb = send_callback_;
    }
    if (cb) {
        send_event(event_name, std::vector<Json::Value>(args), cb, nullptr, namespace_s);
    }
}

void PacketSender::emit(const std::string& event_name,
                       std::initializer_list<Json::Value> args,
                       AckCallback ack_callback,
                       const std::string& namespace_s) {
    TextSendCallback cb;
    {
        webrtc::MutexLock lock(&send_callback_mutex_);
        cb = send_callback_;
    }
    if (cb) {
        send_event_with_ack(event_name, std::vector<Json::Value>(args), cb,
                            ack_callback, nullptr, std::chrono::milliseconds(0), namespace_s);
    }
}

void PacketSender::emit(const std::string& event_name,
                       std::initializer_list<Json::Value> args,
                       AckCallback ack_callback,
                       AckTimeoutCallback timeout_callback,
                       std::chrono::milliseconds timeout,
                       const std::string& namespace_s) {
    TextSendCallback cb;
    {
        webrtc::MutexLock lock(&send_callback_mutex_);
        cb = send_callback_;
    }
    if (cb) {
        send_event_with_ack(event_name, std::vector<Json::Value>(args), cb,
                            ack_callback, timeout_callback, timeout, namespace_s);
    }
}

void PacketSender::emit(const std::string& event_name,
                       const std::vector<Json::Value>& args,
                       const std::string& namespace_s) {
    TextSendCallback cb;
    {
        webrtc::MutexLock lock(&send_callback_mutex_);
        cb = send_callback_;
    }
    if (cb) {
        send_event(event_name, args, cb, nullptr, namespace_s);
    }
}

void PacketSender::emit(const std::string& event_name,
                       const std::vector<Json::Value>& args,
                       AckCallback ack_callback,
                       const std::string& namespace_s) {
    TextSendCallback cb;
    {
        webrtc::MutexLock lock(&send_callback_mutex_);
        cb = send_callback_;
    }
    if (cb) {
        send_event_with_ack(event_name, args, cb,
                            ack_callback, nullptr, std::chrono::milliseconds(0), namespace_s);
    }
}

void PacketSender::emit(const std::string& event_name,
                       const std::vector<Json::Value>& args,
                       AckCallback ack_callback,
                       AckTimeoutCallback timeout_callback,
                       std::chrono::milliseconds timeout,
                       const std::string& namespace_s) {
    TextSendCallback cb;
    {
        webrtc::MutexLock lock(&send_callback_mutex_);
        cb = send_callback_;
    }
    if (cb) {
        send_event_with_ack(event_name, args, cb,
                            ack_callback,
                            std::move(timeout_callback),
                            timeout,
                            namespace_s);
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
    
    if ((version != SocketIOVersion::V2 && packet.type != PacketType::CONNECT) && (packet.event_name.empty() && packet.args.empty())) {
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
    
    // 老的全局回调（保持兼容）
    if (event_callback_) {
        if (task_queue_) {
            task_queue_->PostTask([this, packet]() {
                event_callback_(packet);
            });
        } else {
            event_callback_(packet);
        }
    }
    
    // 新的事件处理器分发
    if (packet.type == PacketType::EVENT || packet.type == PacketType::BINARY_EVENT) {
        dispatch_to_event_handler(packet);
    }
}

void PacketReceiver::handle_ack_packet(const SioPacket& packet) {
    if (!ack_manager_ || packet.ack_id <= 0) {
        return;
    }
    
    // 处理ACK响应
    ack_manager_->handle_ack_response(packet.ack_id, packet.args);
}

// ─── 简洁 API 实现 ──────────────────────────────────────

void PacketReceiver::on(const std::string& event_name, EventHandler handler) {
    webrtc::MutexLock lock(&handlers_mutex_);
    event_handlers_[event_name] = std::move(handler);
}

void PacketReceiver::off(const std::string& event_name) {
    webrtc::MutexLock lock(&handlers_mutex_);
    event_handlers_.erase(event_name);
}

void PacketReceiver::remove_all_listeners() {
    webrtc::MutexLock lock(&handlers_mutex_);
    event_handlers_.clear();
}

void PacketReceiver::set_send_callback(TextSendCallback send_callback) {
    webrtc::MutexLock lock(&handlers_mutex_);
    send_callback_ = std::move(send_callback);
}

void PacketReceiver::dispatch_to_event_handler(const SioPacket& packet) {
    EventHandler handler;
    TextSendCallback send_cb;
    
    {
        webrtc::MutexLock lock(&handlers_mutex_);
        auto it = event_handlers_.find(packet.event_name);
        if (it != event_handlers_.end()) {
            handler = it->second;
        }
        send_cb = send_callback_;
    }
    
    if (!handler) {
        return;
    }
    
    // 如果事件带 ack_id，需要提供 ack 响应函数
    if (packet.ack_id > 0 && send_cb) {
        int ack_id = packet.ack_id;
        std::string ns = packet.namespace_s;
        SioPacketBuilder* builder = packet_builder_.get();
        
        AckResponder ack_responder = [ack_id, ns, builder, send_cb]
            (const std::vector<Json::Value>& args) {
            if (!builder) return;
            
            auto ack_packet = builder->build_ack_packet(args, ns, ack_id);
            auto encoded = builder->encode_packet(ack_packet);
            send_cb(encoded.text_packet, encoded.binary_parts);
        };
        
        if (task_queue_) {
            task_queue_->PostTask([handler, packet, ack_responder]() {
                handler(packet.args, ack_responder);
            });
        } else {
            handler(packet.args, ack_responder);
        }
    } else {
        // 没有 ack，直接调用
        AckResponder null_ack = [](const std::vector<Json::Value>&) {};
        
        if (task_queue_) {
            task_queue_->PostTask([handler, packet, null_ack]() {
                handler(packet.args, null_ack);
            });
        } else {
            handler(packet.args, null_ack);
        }
    }
}

} // namespace sio
