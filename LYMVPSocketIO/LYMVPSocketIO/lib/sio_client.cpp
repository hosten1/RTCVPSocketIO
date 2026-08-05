#include "lib/sio_client.h"
#include "lib/sio_packet_impl.h"
#include "lib/sio_ack_manager.h"
#include "api/task_queue/default_task_queue_factory.h"

namespace sio {

std::shared_ptr<SioClient> SioClient::Create(const Config& config) {
    auto client = std::shared_ptr<SioClient>(new SioClient(config));
    client->initialize();
    return client;
}

SioClient::SioClient(const Config& config) : config_(config) {
}

SioClient::~SioClient() {
    reset();
}

void SioClient::initialize() {
    task_queue_factory_ = webrtc::CreateDefaultTaskQueueFactory();
    
    ack_manager_ = SioAckManager::Create(task_queue_factory_.get());
    ack_manager_->set_default_timeout(config_.default_ack_timeout);
    
    PacketSender::Config sender_config;
    sender_config.version = config_.version;
    sender_config.default_ack_timeout = config_.default_ack_timeout;
    sender_config.max_retries = config_.max_retries;
    sender_config.enable_logging = config_.enable_logging;
    sender_ = std::make_shared<PacketSender>(ack_manager_, task_queue_factory_.get(), sender_config);
    
    PacketReceiver::Config receiver_config;
    receiver_config.default_version = config_.version;
    receiver_config.auto_detect_version = true;
    receiver_config.enable_logging = config_.enable_logging;
    receiver_ = std::make_shared<PacketReceiver>(ack_manager_, task_queue_factory_.get(), receiver_config);
}

void SioClient::set_version(SocketIOVersion version) {
    config_.version = version;
    
    PacketSender::Config sender_config = sender_->get_config();
    sender_config.version = version;
    sender_->set_config(sender_config);
    
    PacketReceiver::Config receiver_config = receiver_->get_config();
    receiver_config.default_version = version;
    receiver_->set_config(receiver_config);
}

SocketIOVersion SioClient::get_version() const {
    return config_.version;
}

void SioClient::set_send_callback(TextSendCallback callback) {
    sender_->set_send_callback(callback);
    receiver_->set_send_callback(callback);
}

void SioClient::on(const std::string& event_name, EventHandler handler) {
    receiver_->on(event_name, std::move(handler));
}

void SioClient::off(const std::string& event_name) {
    receiver_->off(event_name);
}

void SioClient::remove_all_listeners() {
    receiver_->remove_all_listeners();
}

void SioClient::emit(const std::string& event_name,
                     std::initializer_list<Json::Value> args,
                     const std::string& namespace_s) {
    sender_->emit(event_name, args, namespace_s);
}

void SioClient::emit(const std::string& event_name,
                     std::initializer_list<Json::Value> args,
                     AckCallback ack_callback,
                     const std::string& namespace_s) {
    sender_->emit(event_name, args, std::move(ack_callback), namespace_s);
}

void SioClient::emit(const std::string& event_name,
                     std::initializer_list<Json::Value> args,
                     AckCallback ack_callback,
                     AckTimeoutCallback timeout_callback,
                     std::chrono::milliseconds timeout,
                     const std::string& namespace_s) {
    sender_->emit(event_name, args, std::move(ack_callback), std::move(timeout_callback), timeout, namespace_s);
}

void SioClient::emit(const std::string& event_name,
                     const std::vector<Json::Value>& args,
                     const std::string& namespace_s) {
    sender_->emit(event_name, args, namespace_s);
}

void SioClient::emit(const std::string& event_name,
                     const std::vector<Json::Value>& args,
                     AckCallback ack_callback,
                     const std::string& namespace_s) {
    sender_->emit(event_name, args, std::move(ack_callback), namespace_s);
}

bool SioClient::process_text_packet(const std::string& text_packet) {
    return receiver_->process_text_packet(text_packet);
}

bool SioClient::process_binary_data(const SmartBuffer& binary_data) {
    return receiver_->process_binary_data(binary_data);
}

void SioClient::reset() {
    if (sender_) sender_->reset();
    if (receiver_) receiver_->reset();
    if (ack_manager_) ack_manager_->clear_all_acks();
}

SioClient::Stats SioClient::get_stats() const {
    Stats stats;
    if (sender_) {
        auto s = sender_->get_stats();
        stats.total_sent = s.total_sent;
        stats.total_acked = s.total_acked;
    }
    if (receiver_) {
        auto r = receiver_->get_stats();
        stats.total_received = r.total_received;
    }
    if (ack_manager_) {
        auto a = ack_manager_->get_stats();
        stats.pending_acks = a.pending_requests;
    }
    return stats;
}

} // namespace sio
