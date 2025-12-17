#include "socketio_client.h"
#include <iostream>

namespace socketio {

SocketIOClient::SocketIOClient() 
    : status_(SocketIOClientStatus::kNotConnected),
      namespace_("/"),
      current_ack_id_(-1),
      reconnecting_(false),
      reconnect_attempts_(0),
      max_reconnect_attempts_(-1),
      reconnect_wait_time_(10.0),
      is_running_(false) {
    SetupThread();
}

SocketIOClient::~SocketIOClient() {
    Disconnect();
    CleanupThread();
}

void SocketIOClient::SetDelegate(SocketIODelegate* delegate) {
    std::lock_guard<std::mutex> lock(mutex_);
    delegate_ = delegate;
}

void SocketIOClient::Connect(const std::string& url, const std::map<std::string, std::string>& config) {
    url_ = url;
    
    // Setup namespace from config if present
    auto it = config.find("namespace");
    if (it != config.end()) {
        namespace_ = it->second;
    }
    
    // Setup reconnection settings
    it = config.find("reconnectionAttempts");
    if (it != config.end()) {
        max_reconnect_attempts_ = std::stoi(it->second);
    }
    
    it = config.find("reconnectionDelay");
    if (it != config.end()) {
        reconnect_wait_time_ = std::stod(it->second);
    }
    
    SetStatus(SocketIOClientStatus::kConnecting);
    
    // In a real implementation, this would connect to the server
    // For now, we'll just simulate a successful connection
    DidConnect(namespace_);
}

void SocketIOClient::Disconnect() {
    SetStatus(SocketIOClientStatus::kDisconnected);
    DidDisconnect("User disconnected");
}

void SocketIOClient::Reconnect() {
    if (!reconnecting_) {
        reconnecting_ = true;
        reconnect_attempts_ = 0;
        DidReconnectAttempt(reconnect_attempts_ + 1);
        Connect(url_, {});
    }
}

void SocketIOClient::Emit(const std::string& event) {
    Emit(event, {});
}

void SocketIOClient::Emit(const std::string& event, const std::vector<std::string>& items) {
    // Generate packet ID if needed
    int packet_id = -1;
    
    // Create event packet
    auto packet = SocketIOPacket::CreateEventPacket(
        event,
        items,
        packet_id,
        namespace_,
        false // no ack required
    );
    
    // Send packet (in a real implementation, this would send to the server)
    if (delegate_) {
        delegate_->OnPacket(*packet);
    }
    
    // In a real implementation, we would queue this packet for sending
    std::string packet_str = packet->ToString();
    std::cout << "Emitting packet: " << packet_str << std::endl;
}

void SocketIOClient::EmitWithAck(const std::string& event, 
                                const std::vector<std::string>& items, 
                                std::function<void(const std::vector<std::string>& data, const std::string& error)> ack_callback,
                                double timeout) {
    // Generate unique ack ID
    int packet_id = GenerateAckId();
    
    // Store ack callback
    { 
        std::lock_guard<std::mutex> lock(mutex_);
        ack_callbacks_[packet_id] = ack_callback;
    }
    
    // Create event packet with ack
    auto packet = SocketIOPacket::CreateEventPacket(
        event,
        items,
        packet_id,
        namespace_,
        true // requires ack
    );
    
    // Setup ack callbacks
    packet->SetupAckCallbacks(
        [this, packet_id](const std::vector<std::string>& data) {
            // Handle success
            std::function<void(const std::vector<std::string>&, const std::string&)> callback;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = ack_callbacks_.find(packet_id);
                if (it != ack_callbacks_.end()) {
                    callback = std::move(it->second);
                    ack_callbacks_.erase(it);
                }
            }
            
            if (callback) {
                callback(data, "");
            }
        },
        [this, packet_id](const std::string& error) {
            // Handle error
            std::function<void(const std::vector<std::string>&, const std::string&)> callback;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = ack_callbacks_.find(packet_id);
                if (it != ack_callbacks_.end()) {
                    callback = std::move(it->second);
                    ack_callbacks_.erase(it);
                }
            }
            
            if (callback) {
                std::vector<std::string> empty;
                callback(empty, error);
            }
        },
        timeout
    );
    
    // Send packet
    if (delegate_) {
        delegate_->OnPacket(*packet);
    }
    
    std::string packet_str = packet->ToString();
    std::cout << "Emitting packet with ack: " << packet_str << std::endl;
}

void SocketIOClient::On(const std::string& event, std::function<void(const std::vector<std::string>& data)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    event_handlers_[event].push_back(callback);
}

void SocketIOClient::Off(const std::string& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    event_handlers_.erase(event);
}

void SocketIOClient::OffAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    event_handlers_.clear();
}

void SocketIOClient::PostMessage(const std::string& message) {
    message_queue_.Push(message);
}

void SocketIOClient::PostBinaryData(const std::vector<uint8_t>& data) {
    binary_queue_.Push(data);
}

void SocketIOClient::PostAck(int ack_id, const std::vector<std::string>& data) {
    ack_queue_.Push(std::make_pair(ack_id, data));
}

// Internal methods
void SocketIOClient::SetupThread() {
    // Create and start the client thread using libwebrtc's Thread class
    client_thread_ = std::unique_ptr<rtc::Thread>(rtc::Thread::Create());
    if (client_thread_) {
        client_thread_->Start();
        is_running_ = true;
        
        // Post initial task to start message processing loop
        client_thread_->PostTask([this]() {
            // Message processing loop would run here in a real implementation
            // For now, we'll just process messages as they come
        });
    }
}

void SocketIOClient::CleanupThread() {
    if (client_thread_) {
        client_thread_->Stop();
        client_thread_.reset();
    }
    is_running_ = false;
}

void SocketIOClient::ProcessMessage(const std::string& message) {
    // Parse message into packet
    auto packet = SocketIOPacket::CreateFromMessage(message);
    if (packet) {
        ProcessPacket(*packet);
    }
}

void SocketIOClient::ProcessPacket(const SocketIOPacket& packet) {
    std::cout << "Processing packet: " << packet.DebugDescription() << std::endl;
    
    switch (packet.type()) {
        case SocketIOPacketType::kConnect:
            DidConnect(packet.nsp());
            break;
        
        case SocketIOPacketType::kDisconnect:
            DidDisconnect("Server disconnected");
            break;
        
        case SocketIOPacketType::kEvent:
        case SocketIOPacketType::kBinaryEvent:
            HandleEvent(packet.GetEventName(), packet.GetEventArgs(), packet.packet_id());
            break;
        
        case SocketIOPacketType::kAck:
        case SocketIOPacketType::kBinaryAck:
            HandleAck(packet.packet_id(), packet.GetEventArgs());
            break;
        
        case SocketIOPacketType::kError:
            DidError(packet.data().empty() ? "Unknown error" : packet.data()[0]);
            break;
        
        default:
            std::cout << "Unknown packet type: " << static_cast<int>(packet.type()) << std::endl;
            break;
    }
}

void SocketIOClient::HandleEvent(const std::string& event_name, 
                               const std::vector<std::string>& args,
                               int ack_id) {
    std::cout << "Handling event: " << event_name << " with " << args.size() << " arguments" << std::endl;
    
    // Notify delegate
    SocketIODelegate* delegate = nullptr;
    { 
        std::lock_guard<std::mutex> lock(mutex_);
        delegate = delegate_;
    }
    
    if (delegate) {
        delegate->OnEvent(event_name, args, ack_id);
    }
    
    // Call registered event handlers
    std::vector<std::function<void(const std::vector<std::string>&)>> callbacks;
    { 
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = event_handlers_.find(event_name);
        if (it != event_handlers_.end()) {
            callbacks = it->second;
        }
    }
    
    for (const auto& callback : callbacks) {
        callback(args);
    }
}

void SocketIOClient::HandleAck(int ack_id, const std::vector<std::string>& args) {
    std::cout << "Handling ack: " << ack_id << " with " << args.size() << " arguments" << std::endl;
    
    // Notify delegate
    SocketIODelegate* delegate = nullptr;
    { 
        std::lock_guard<std::mutex> lock(mutex_);
        delegate = delegate_;
    }
    
    if (delegate) {
        delegate->OnAck(ack_id, args);
    }
}

// Connection callbacks
void SocketIOClient::DidConnect(const std::string& namespace_) {
    SetStatus(SocketIOClientStatus::kConnected);
    
    SocketIODelegate* delegate = nullptr;
    { 
        std::lock_guard<std::mutex> lock(mutex_);
        delegate = delegate_;
    }
    
    if (delegate) {
        delegate->OnConnect(namespace_);
    }
    
    // Reset reconnection attempts
    reconnecting_ = false;
    reconnect_attempts_ = 0;
}

void SocketIOClient::DidDisconnect(const std::string& reason) {
    SetStatus(SocketIOClientStatus::kDisconnected);
    
    SocketIODelegate* delegate = nullptr;
    { 
        std::lock_guard<std::mutex> lock(mutex_);
        delegate = delegate_;
    }
    
    if (delegate) {
        delegate->OnDisconnect(reason);
    }
    
    // Cleanup
    ack_callbacks_.clear();
}

void SocketIOClient::DidError(const std::string& reason) {
    std::cout << "SocketIO error: " << reason << std::endl;
    
    SocketIODelegate* delegate = nullptr;
    { 
        std::lock_guard<std::mutex> lock(mutex_);
        delegate = delegate_;
    }
    
    if (delegate) {
        delegate->OnError(reason);
    }
}

void SocketIOClient::DidReconnectAttempt(int attempt) {
    std::cout << "Reconnect attempt: " << attempt << std::endl;
    
    SocketIODelegate* delegate = nullptr;
    { 
        std::lock_guard<std::mutex> lock(mutex_);
        delegate = delegate_;
    }
    
    if (delegate) {
        delegate->OnReconnectAttempt(attempt);
    }
}

void SocketIOClient::DidReconnect(int attempt) {
    std::cout << "Reconnected after " << attempt << " attempts" << std::endl;
    
    SocketIODelegate* delegate = nullptr;
    { 
        std::lock_guard<std::mutex> lock(mutex_);
        delegate = delegate_;
    }
    
    if (delegate) {
        delegate->OnReconnect(attempt);
    }
}

void SocketIOClient::DidStatusChange(SocketIOClientStatus status) {
    SocketIODelegate* delegate = nullptr;
    { 
        std::lock_guard<std::mutex> lock(mutex_);
        delegate = delegate_;
    }
    
    if (delegate) {
        delegate->OnStatusChange(SocketIOProtocol::StatusToString(status));
    }
}

void SocketIOClient::SetStatus(SocketIOClientStatus status) {
    if (status_ != status) {
        status_ = status;
        DidStatusChange(status);
    }
}

int SocketIOClient::GenerateAckId() {
    std::lock_guard<std::mutex> lock(mutex_);
    current_ack_id_ += 1;
    if (current_ack_id_ >= 1000) {
        current_ack_id_ = 0;
    }
    return current_ack_id_;
}

} // namespace socketio
