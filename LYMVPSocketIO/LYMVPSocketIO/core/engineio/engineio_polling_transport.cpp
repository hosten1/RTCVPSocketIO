#include "core/engineio/engineio_polling_transport.h"
#include "core/engineio/http_client.h"
#include "api/task_queue/default_task_queue_factory.h"

#include "json/json.h"

#include <chrono>
#include <random>
#include <sstream>

namespace engineio {

struct PollingTransport::Impl {
    std::shared_ptr<HttpClient> http_client;
    int protocol_version;
    
    std::string base_url;
    std::string sid;
    int ping_interval;
    int ping_timeout;
    
    std::atomic<bool> connected;
    std::atomic<bool> connecting;
    std::atomic<bool> should_stop;
    
    std::thread poll_thread;
    std::mutex send_mutex;
    
    std::unique_ptr<webrtc::TaskQueueFactory> owned_task_queue_factory;
    std::shared_ptr<rtc::TaskQueue> send_task_queue;
    
    Impl(std::shared_ptr<HttpClient> client,
         int ver,
         webrtc::TaskQueueFactory* task_queue_factory)
        : http_client(std::move(client))
        , protocol_version(ver)
        , ping_interval(25000)
        , ping_timeout(20000)
        , connected(false)
        , connecting(false)
        , should_stop(false) {
        if (!task_queue_factory) {
            owned_task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();
            task_queue_factory = owned_task_queue_factory.get();
        }
        if (task_queue_factory) {
            send_task_queue = std::make_shared<rtc::TaskQueue>(
                task_queue_factory->CreateTaskQueue(
                    "polling_send",
                    webrtc::TaskQueueFactory::Priority::NORMAL));
        }
    }
};

std::shared_ptr<PollingTransport> PollingTransport::Create(
    std::shared_ptr<HttpClient> http_client,
    int protocol_version,
    webrtc::TaskQueueFactory* task_queue_factory) {
    return std::shared_ptr<PollingTransport>(
        new PollingTransport(std::move(http_client), protocol_version, task_queue_factory));
}

PollingTransport::PollingTransport(
    std::shared_ptr<HttpClient> http_client,
    int protocol_version,
    webrtc::TaskQueueFactory* task_queue_factory)
    : impl_(new Impl(std::move(http_client), protocol_version, task_queue_factory)) {
}

PollingTransport::~PollingTransport() {
    disconnect();
}

bool PollingTransport::is_connected() const {
    return impl_->connected.load();
}

void PollingTransport::set_self_signed_ssl(bool enabled) {
    impl_->http_client->set_self_signed_ssl(enabled);
}

void PollingTransport::connect(const std::string& url) {
    if (impl_->connecting.exchange(true) || impl_->connected.load()) {
        return;
    }
    
    impl_->base_url = url;
    impl_->should_stop.store(false);
    
    impl_->poll_thread = std::thread([this]() {
        poll_thread_func();
    });
}

void PollingTransport::disconnect() {
    impl_->should_stop.store(true);
    
    if (impl_->poll_thread.joinable()) {
        impl_->poll_thread.join();
    }
    
    impl_->connected.store(false);
    impl_->connecting.store(false);
}

void PollingTransport::send(const std::string& message) {
    if (!impl_->connected.load()) {
        return;
    }
    
    std::string packet;
    packet += static_cast<char>(EnginePacketType::MESSAGE);
    packet += message;
    
    auto self = shared_from_this();
    if (impl_->send_task_queue) {
        impl_->send_task_queue->PostTask([self, packet]() {
            self->do_post(packet);
        });
    }
}

void PollingTransport::send_ping() {
    if (!impl_->connected.load()) {
        return;
    }
    
    std::string ping_packet;
    ping_packet += static_cast<char>(EnginePacketType::PING);
    
    auto self = shared_from_this();
    if (impl_->send_task_queue) {
        impl_->send_task_queue->PostTask([self, ping_packet]() {
            self->do_post(ping_packet);
        });
    }
}

void PollingTransport::poll_thread_func() {
    do_handshake();
    
    if (!impl_->connected.load()) {
        impl_->connecting.store(false);
        return;
    }
    
    while (!impl_->should_stop.load()) {
        do_poll();
    }
    
    impl_->connected.store(false);
    impl_->connecting.store(false);
}

void PollingTransport::do_handshake() {
    std::string handshake_url = build_url(false, "polling");
    
    HttpResponse response = impl_->http_client->get(
        handshake_url,
        {{"Connection", "keep-alive"}},
        30
    );
    
    if (!response.success || response.body.empty()) {
        if (on_error_) {
            on_error_(response.error_message.empty()
                ? "handshake failed"
                : response.error_message);
        }
        return;
    }
    
    handle_engine_packet(response.body);
}

void PollingTransport::do_poll() {
    std::string poll_url = build_url(true, "polling");
    
    HttpResponse response = impl_->http_client->get(
        poll_url,
        {{"Connection", "keep-alive"}},
        impl_->ping_timeout / 1000 + 5
    );
    
    if (!response.success) {
        if (!impl_->should_stop.load() && on_error_) {
            on_error_("poll failed: " + response.error_message);
        }
        return;
    }
    
    if (response.body.empty()) {
        return;
    }
    
    handle_engine_packet(response.body);
}

void PollingTransport::do_post(const std::string& payload) {
    std::string post_url = build_url(true, "polling");
    
    HttpResponse response = impl_->http_client->post(
        post_url,
        {
            {"Connection", "keep-alive"},
        },
        payload,
        "text/plain;charset=UTF-8",
        30
    );
    
    if (!response.success) {
        if (on_error_) on_error_("post failed: " + response.error_message);
    }
}

void PollingTransport::handle_engine_packet(const std::string& packet_data) {
    auto packets = decode_packets(packet_data);
    
    for (const auto& packet : packets) {
        if (packet.empty()) continue;
        
        char type_char = packet[0];
        std::string data = packet.substr(1);
        
        switch (type_char) {
            case '0':
                handle_open(data);
                break;
            case '1':
                if (on_close_) on_close_("transport close");
                impl_->connected.store(false);
                impl_->should_stop.store(true);
                break;
            case '2': {
                std::string pong_packet;
                pong_packet += static_cast<char>(EnginePacketType::PONG);
                auto self = shared_from_this();
                if (impl_->send_task_queue) {
                    impl_->send_task_queue->PostTask([self, pong_packet]() {
                        self->do_post(pong_packet);
                    });
                }
                break;
            }
            case '3':
                if (on_pong_) on_pong_();
                break;
            case '4':
                handle_message(data);
                break;
            default:
                break;
        }
    }
}

void PollingTransport::handle_open(const std::string& data) {
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value root;
    std::string errors;
    
    if (!reader->parse(data.data(), data.data() + data.size(), &root, &errors)) {
        if (on_error_) on_error_("failed to parse open packet: " + errors);
        return;
    }
    
    if (root.isMember("sid") && root["sid"].isString()) {
        impl_->sid = root["sid"].asString();
    }
    if (root.isMember("pingInterval") && root["pingInterval"].isInt()) {
        impl_->ping_interval = root["pingInterval"].asInt();
    }
    if (root.isMember("pingTimeout") && root["pingTimeout"].isInt()) {
        impl_->ping_timeout = root["pingTimeout"].asInt();
    }
    
    impl_->connected.store(true);
    impl_->connecting.store(false);
    
    if (on_open_) on_open_(impl_->sid, impl_->ping_interval, impl_->ping_timeout);
}

void PollingTransport::handle_message(const std::string& data) {
    if (on_message_) on_message_(data);
}

std::string PollingTransport::build_url(bool with_sid, const std::string& transport) {
    std::string url = impl_->base_url;
    
    bool has_query = (url.find('?') != std::string::npos);
    
    if (!has_query) {
        url += "?";
    } else {
        url += "&";
    }
    
    int eio_version = (impl_->protocol_version == 2) ? 3 : 4;
    url += "EIO=" + std::to_string(eio_version);
    url += "&transport=" + transport;
    
    if (with_sid && !impl_->sid.empty()) {
        url += "&sid=" + impl_->sid;
    }
    
    url += "&t=" + generate_t_param();
    
    return url;
}

std::string PollingTransport::generate_t_param() {
    static const char chars[] = 
        "abcdefghijklmnopqrstuvwxyz0123456789";
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
    
    std::string result;
    result.reserve(12);
    for (int i = 0; i < 12; ++i) {
        result += chars[dis(gen)];
    }
    return result;
}

std::string PollingTransport::encode_packets(const std::vector<std::string>& packets) {
    if (impl_->protocol_version == 2) {
        std::string result;
        for (const auto& packet : packets) {
            result += std::to_string(packet.size()) + ":" + packet;
        }
        return result;
    } else {
        const char delim = '\x1e';
        std::string result;
        for (size_t i = 0; i < packets.size(); ++i) {
            if (i > 0) result += delim;
            result += packets[i];
        }
        return result;
    }
}

std::vector<std::string> PollingTransport::decode_packets(const std::string& data) {
    std::vector<std::string> packets;
    
    if (data.empty()) return packets;
    
    if (impl_->protocol_version == 2) {
        size_t pos = 0;
        while (pos < data.size()) {
            size_t colon_pos = data.find(':', pos);
            if (colon_pos == std::string::npos) break;
            
            std::string len_str = data.substr(pos, colon_pos - pos);
            int len = std::stoi(len_str);
            size_t start = colon_pos + 1;
            
            if (start + len > data.size()) break;
            
            packets.push_back(data.substr(start, len));
            pos = start + len;
        }
    } else {
        const char delim = '\x1e';
        size_t pos = 0;
        while (pos < data.size()) {
            size_t next = data.find(delim, pos);
            if (next == std::string::npos) {
                packets.push_back(data.substr(pos));
                break;
            }
            packets.push_back(data.substr(pos, next - pos));
            pos = next + 1;
        }
    }
    
    return packets;
}

} // namespace engineio
