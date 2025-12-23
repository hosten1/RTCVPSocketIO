#include "sio_packet_impl.h"
#include "sio_packet_parser.h"
#include "sio_ack_manager.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/repeating_task.h"
#include <chrono>
#include <sstream>
#include <iomanip>

namespace sio {

// ============================================================================
// SioPacketBuilder 类实现
// ============================================================================

SioPacketBuilder::SioPacketBuilder(SocketIOVersion version) : version_(version) {}

SioPacket SioPacketBuilder::build_event_packet(
    const std::string& event_name,
    const std::vector<Json::Value>& args,
    int namespace_id,
    int packet_id) {
    
    SioPacket packet;
    packet.type = PacketType::EVENT;
    packet.event_name = event_name;
    packet.args = args;
    packet.namespace_id = namespace_id;
    packet.packet_id = packet_id;
    packet.need_ack = (packet_id > 0);
    packet.version = version_;
    
    // 检查是否有二进制数据
    for (const auto& arg : packet.args) {
        if (binary_helper::is_binary(arg)) {
            packet.type = PacketType::BINARY_EVENT;
            break;
        }
    }
    
    return packet;
}

SioPacket SioPacketBuilder::build_ack_packet(
    const std::vector<Json::Value>& args,
    int namespace_id,
    int packet_id) {
    
    SioPacket packet;
    packet.type = PacketType::ACK;
    packet.args = args;
    packet.namespace_id = namespace_id;
    packet.packet_id = packet_id;
    packet.need_ack = false;
    packet.version = version_;
    
    // 检查是否有二进制数据
    for (const auto& arg : packet.args) {
        if (binary_helper::is_binary(arg)) {
            packet.type = PacketType::BINARY_ACK;
            break;
        }
    }
    
    return packet;
}

SioPacketBuilder::EncodedPacket SioPacketBuilder::encode_packet(const SioPacket& packet) {
    // 始终使用V4版本编码
    return encode_v4_packet(packet);
}

SocketIOVersion SioPacketBuilder::detect_version(const std::string& packet) {
    // 简单版本检测：始终返回V4版本
    return SocketIOVersion::V4;
}

SioPacket SioPacketBuilder::decode_packet(
    const std::string& text_packet,
    const std::vector<SmartBuffer>& binary_parts) {
    
    if (text_packet.empty()) {
        return SioPacket();
    }
    
    // 始终使用V4版本解码
    return decode_v4_packet(text_packet, binary_parts);
}

SioPacketBuilder::EncodedPacket SioPacketBuilder::encode_v4_packet(const SioPacket& packet) {
    EncodedPacket result;
    result.is_binary = packet.is_binary();
    
    // 构建JSON数据
    Json::Value json_data;
    std::vector<SmartBuffer> binary_parts;
    std::map<std::string, int> binary_map;
    
    if (packet.type == PacketType::ACK || packet.type == PacketType::BINARY_ACK) {
        // ACK包：直接是参数数组
        Json::Value args_array(Json::arrayValue);
        for (const auto& arg : packet.args) {
            Json::Value processed_arg;
            extract_binary_data(arg, processed_arg, binary_parts, binary_map);
            args_array.append(processed_arg);
        }
        json_data = args_array;
    } else {
        // 事件包：["event_name", ...args]
        Json::Value event_array(Json::arrayValue);
        event_array.append(Json::Value(packet.event_name));
        
        for (const auto& arg : packet.args) {
            Json::Value processed_arg;
            extract_binary_data(arg, processed_arg, binary_parts, binary_map);
            event_array.append(processed_arg);
        }
        json_data = event_array;
    }
    
    result.binary_parts = binary_parts;
    result.binary_count = static_cast<int>(binary_parts.size());
    
    // 构建文本包
    std::stringstream ss;
    
    // 包类型
    ss << static_cast<int>(packet.type);
    
    // 命名空间
    if (packet.namespace_id > 0) {
        ss << packet.namespace_id << ",";
    }
    
    // 包ID（如果需要ACK）
    if (packet.packet_id > 0) {
        ss << packet.packet_id;
    }
    
    // 二进制计数（如果是二进制包）
    if (packet.is_binary()) {
        ss << binary_parts.size();
    }
    
    // JSON数据
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    ss << Json::writeString(writer, json_data);
    
    result.text_packet = ss.str();
    return result;
}

SioPacket SioPacketBuilder::decode_v4_packet(
    const std::string& text,
    const std::vector<SmartBuffer>& binaries) {
    
    SioPacket packet;
    packet.version = SocketIOVersion::V4;
    
    if (text.empty()) {
        return packet;
    }
    
    // 保存原始配置
    ParserConfig original_config = PacketParser::getInstance().getConfig();
    
    // 临时设置V4配置
    ParserConfig temp_config = original_config;
    temp_config.version = SocketIOVersion::V4;
    PacketParser::getInstance().setConfig(temp_config);
    
    // 解析包
    auto parse_result = PacketParser::getInstance().parsePacket(text);
    
    // 恢复原始配置
    PacketParser::getInstance().setConfig(original_config);
    
    if (!parse_result.success) {
        return packet;
    }
    
    packet.type = parse_result.packet.type;
    
    // 解析命名空间ID（从文本中解析）
    // 假设格式为：4[namespace_id],...
    // 简单实现：查找第一个'['之前的数字
    size_t bracket_pos = text.find('[');
    if (bracket_pos != std::string::npos && bracket_pos > 1) {
        // 包类型已经占用了第一个字符
        std::string after_type = text.substr(1, bracket_pos - 1);
        size_t comma_pos = after_type.find(',');
        if (comma_pos != std::string::npos) {
            // 有命名空间ID
            std::string nsp_str = after_type.substr(0, comma_pos);
            try {
                packet.namespace_id = std::stoi(nsp_str);
            } catch (...) {
                packet.namespace_id = 0;
            }
        }
    }
    
    packet.packet_id = parse_result.packet.id;
    packet.need_ack = (parse_result.packet.id > 0);
    
    // 解析JSON数据
    if (!parse_result.json_data.empty()) {
        Json::CharReaderBuilder reader_builder;
        std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
        Json::Value json_value;
        std::string errors;
        
        if (reader->parse(parse_result.json_data.data(),
                         parse_result.json_data.data() + parse_result.json_data.size(),
                         &json_value, &errors)) {
            
            if (json_value.isArray()) {
                // 事件包：["event_name", ...args]
                if (json_value.size() > 0) {
                    packet.event_name = json_value[0].asString();
                    
                    // 恢复二进制数据到参数中
                    std::map<std::string, int> binary_map;
                    for (Json::ArrayIndex i = 1; i < json_value.size(); i++) {
                        Json::Value restored_arg = json_value[i];
                        restore_binary_data(restored_arg, binaries, binary_map);
                        packet.args.push_back(restored_arg);
                    }
                }
            } else if (json_value.isArray() || json_value.isObject()) {
                // ACK包：直接是参数数组
                Json::Value restored_value = json_value;
                std::map<std::string, int> binary_map;
                restore_binary_data(restored_value, binaries, binary_map);
                
                if (restored_value.isArray()) {
                    for (Json::ArrayIndex i = 0; i < restored_value.size(); i++) {
                        packet.args.push_back(restored_value[i]);
                    }
                } else {
                    packet.args.push_back(restored_value);
                }
            }
        }
    }
    
    packet.binary_parts = binaries;
    return packet;
}

void SioPacketBuilder::extract_binary_data(
    const Json::Value& data,
    Json::Value& json_without_binary,
    std::vector<SmartBuffer>& binary_parts,
    std::map<std::string, int>& binary_map) {
    
    if (data.isNull() || data.isBool() || data.isInt() ||
        data.isUInt() || data.isDouble() || data.isString()) {
        json_without_binary = data;
        return;
    }
    
    if (data.isArray()) {
        Json::Value array(Json::arrayValue);
        for (Json::ArrayIndex i = 0; i < data.size(); i++) {
            Json::Value processed_element;
            extract_binary_data(data[i], processed_element, binary_parts, binary_map);
            array.append(processed_element);
        }
        json_without_binary = array;
        return;
    }
    
    if (data.isObject()) {
        // 检查是否是二进制数据
        if (binary_helper::is_binary(data)) {
            auto buffer_ptr = binary_helper::get_binary_shared_ptr(data);
            if (buffer_ptr) {
                SmartBuffer buffer(buffer_ptr);
                int index = static_cast<int>(binary_parts.size());
                binary_parts.push_back(buffer);
                
                // 创建占位符
                Json::Value placeholder(Json::objectValue);
                placeholder["_placeholder"] = true;
                placeholder["num"] = index;
                json_without_binary = placeholder;
                return;
            }
        }
        
        // 普通对象
        Json::Value obj(Json::objectValue);
        Json::Value::Members members = data.getMemberNames();
        for (const auto& key : members) {
            Json::Value processed_value;
            extract_binary_data(data[key], processed_value, binary_parts, binary_map);
            obj[key] = processed_value;
        }
        json_without_binary = obj;
        return;
    }
    
    json_without_binary = Json::Value(Json::nullValue);
}

void SioPacketBuilder::restore_binary_data(
    Json::Value& data,
    const std::vector<SmartBuffer>& binary_parts,
    const std::map<std::string, int>& binary_map) {
    
    if (data.isNull() || data.isBool() || data.isInt() ||
        data.isUInt() || data.isDouble() || data.isString()) {
        return;
    }
    
    if (data.isArray()) {
        for (Json::ArrayIndex i = 0; i < data.size(); i++) {
            restore_binary_data(data[i], binary_parts, binary_map);
        }
        return;
    }
    
    if (data.isObject()) {
        // 检查是否是占位符
        if (data.isMember("_placeholder") && data["_placeholder"].isBool() &&
            data["_placeholder"].asBool() && data.isMember("num") && data["num"].isInt()) {
            int index = data["num"].asInt();
            if (index >= 0 && index < static_cast<int>(binary_parts.size())) {
                const SmartBuffer& buffer = binary_parts[index];
                if (!buffer.empty()) {
                    data = binary_helper::create_binary_value(buffer.buffer());
                }
            }
            return;
        }
        
        // 普通对象
        Json::Value::Members members = data.getMemberNames();
        for (const auto& key : members) {
            restore_binary_data(data[key], binary_parts, binary_map);
        }
        return;
    }
}

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
        
        if (!ack_manager_) {
            // 创建默认的ACK管理器
            ack_manager_ = SioAckManager::Create(task_queue_factory_.get());
        }
        
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

void PacketSender::set_ack_manager(std::shared_ptr<IAckManager> ack_manager) {
    stop_cleanup_timer();
    ack_manager_ = ack_manager;
    start_cleanup_timer();
}

bool PacketSender::send_event(const std::string& event_name,
                             const std::vector<Json::Value>& args,
                             TextSendCallback text_callback,
                             BinarySendCallback binary_callback,
                             SendResultCallback complete_callback,
                             int namespace_id) {
    if (!packet_builder_ || !text_callback) {
        if (complete_callback) {
            complete_callback(false, "Packet builder or text callback is null");
        }
        return false;
    }
    
    // 构建包
    auto packet = packet_builder_->build_event_packet(
        event_name, args, namespace_id, -1);
    auto encoded = packet_builder_->encode_packet(packet);
    
    if (encoded.text_packet.empty()) {
        if (complete_callback) {
            complete_callback(false, "Failed to encode packet");
        }
        return false;
    }
    
    // 发送文本包
    bool text_sent = text_callback(encoded.text_packet);
    if (!text_sent) {
        if (complete_callback) {
            complete_callback(false, "Failed to send text packet");
        }
        
        webrtc::MutexLock lock(&stats_mutex_);
        stats_.total_failed++;
        return false;
    }
    
    // 发送二进制数据
    bool binary_sent = true;
    if (binary_callback && !encoded.binary_parts.empty()) {
        for (size_t i = 0; i < encoded.binary_parts.size(); i++) {
            if (!binary_callback(encoded.binary_parts[i], static_cast<int>(i))) {
                binary_sent = false;
                break;
            }
        }
    }
    
    bool success = text_sent && binary_sent;
    
    if (complete_callback) {
        complete_callback(success, success ? "" : "Failed to send binary data");
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
    BinarySendCallback binary_callback,
    AckCallback ack_callback,
    AckTimeoutCallback timeout_callback,
    std::chrono::milliseconds timeout,
    int namespace_id) {
    
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
        event_name, args, namespace_id, ack_id);
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
    
    // 发送文本包
    bool text_sent = text_callback(encoded.text_packet);
    if (!text_sent) {
        ack_manager_->cancel_ack(ack_id);
        
        webrtc::MutexLock lock(&pending_mutex_);
        pending_requests_.erase(ack_id);
        
        webrtc::MutexLock stats_lock(&stats_mutex_);
        stats_.total_failed++;
        
        return -1;
    }
    
    // 发送二进制数据
    bool binary_sent = true;
    if (binary_callback && !encoded.binary_parts.empty()) {
        for (size_t i = 0; i < encoded.binary_parts.size(); i++) {
            if (!binary_callback(encoded.binary_parts[i], static_cast<int>(i))) {
                binary_sent = false;
                break;
            }
        }
    }
    
    if (!binary_sent) {
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
    int packet_id,
    const std::vector<Json::Value>& args,
    TextSendCallback text_callback,
    BinarySendCallback binary_callback,
    int namespace_id) {
    
    if (packet_id <= 0 || !packet_builder_ || !text_callback) {
        return false;
    }
    
    // 构建ACK响应包
    auto packet = packet_builder_->build_ack_packet(args, namespace_id, packet_id);
    auto encoded = packet_builder_->encode_packet(packet);
    
    if (encoded.text_packet.empty()) {
        return false;
    }
    
    // 发送文本包
    bool text_sent = text_callback(encoded.text_packet);
    if (!text_sent) {
        webrtc::MutexLock lock(&stats_mutex_);
        stats_.total_failed++;
        return false;
    }
    
    // 发送二进制数据
    bool binary_sent = true;
    if (binary_callback && !encoded.binary_parts.empty()) {
        for (size_t i = 0; i < encoded.binary_parts.size(); i++) {
            if (!binary_callback(encoded.binary_parts[i], static_cast<int>(i))) {
                binary_sent = false;
                break;
            }
        }
    }
    
    bool success = text_sent && binary_sent;
    
    if (!success) {
        webrtc::MutexLock lock(&stats_mutex_);
        stats_.total_failed++;
    }
    
    return success;
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
        
        if (!ack_manager_) {
            // 创建默认的ACK管理器
            ack_manager_ = SioAckManager::Create(task_queue_factory_.get());
        }
    }
}

void PacketReceiver::set_config(const Config& config) {
    config_ = config;
    if (packet_builder_) {
        packet_builder_->set_version(config.default_version);
    }
}

void PacketReceiver::set_ack_manager(std::shared_ptr<IAckManager> ack_manager) {
    ack_manager_ = ack_manager;
}

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
    if (config_.auto_detect_version) {
        version = SioPacketBuilder::detect_version(text_packet);
        // 如果检测失败，使用默认版本
    }
    
    // 解码包
    SioPacket packet = packet_builder_->decode_packet(text_packet);
    
    if (packet.event_name.empty() && packet.args.empty()) {
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
        
        if (packet.is_binary()) {
            state_.expected_binary_count = static_cast<int>(packet.binary_parts.size());
            if (state_.expected_binary_count > 0) {
                state_.state = ReceiveState::WAITING_FOR_BINARY;
            } else {
                state_.state = ReceiveState::COMPLETE;
            }
        } else {
            state_.state = ReceiveState::COMPLETE;
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
        return false;
    }
    
    // 检查二进制数据大小
    if (binary_data.size() > config_.max_binary_size) {
        return false;
    }
    
    state_.received_binaries.push_back(binary_data);
    
    // 检查是否接收完成
    if (static_cast<int>(state_.received_binaries.size()) >= state_.expected_binary_count) {
        state_.state = ReceiveState::COMPLETE;
        
        // 重新解码包，包含二进制数据
        if (packet_builder_) {
            auto encoded = packet_builder_->encode_packet(state_.current_packet);
            SioPacket complete_packet = packet_builder_->decode_packet(
                encoded.text_packet, state_.received_binaries);
            complete_packet.version = state_.packet_version;
            
            process_complete_packet(complete_packet);
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
    
    // 处理事件包
    if (!packet.event_name.empty() && event_callback_) {
        // 在任务队列中执行回调
        if (task_queue_) {
            task_queue_->PostTask([this, packet]() {
                event_callback_(packet.event_name, packet.args);
            });
        } else {
            event_callback_(packet.event_name, packet.args);
        }
    }
}

void PacketReceiver::handle_ack_packet(const SioPacket& packet) {
    if (!ack_manager_ || packet.packet_id <= 0) {
        return;
    }
    
    // 处理ACK响应
    ack_manager_->handle_ack_response(packet.packet_id, packet.args);
}

} // namespace sio
