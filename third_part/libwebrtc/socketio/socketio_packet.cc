#include "socketio_packet.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <mutex>

namespace socketio {

// Simple JSON serializer for basic SocketIO needs
class SimpleJSONSerializer {
public:
    static std::string Serialize(const std::vector<std::string>& data) {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < data.size(); ++i) {
            if (i > 0) oss << ",";
            oss << '"' << EscapeString(data[i]) << '"';
        }
        oss << "]";
        return oss.str();
    }
    
    static std::vector<std::string> Deserialize(const std::string& json) {
        std::vector<std::string> result;
        if (json.empty() || json[0] != '[' || json.back() != ']') {
            return result;
        }
        
        // Simple parser for basic JSON arrays
        std::string current;
        bool in_string = false;
        bool escaped = false;
        
        for (size_t i = 1; i < json.size() - 1; ++i) {
            char c = json[i];
            
            if (escaped) {
                current += c;
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = !in_string;
            } else if (c == ',' && !in_string) {
                if (!current.empty()) {
                    result.push_back(std::move(current));
                    current.clear();
                }
            } else if (in_string) {
                current += c;
            }
        }
        
        if (!current.empty()) {
            result.push_back(std::move(current));
        }
        
        return result;
    }
    
private:
    static std::string EscapeString(const std::string& str) {
        std::string result;
        for (char c : str) {
            switch (c) {
                case '"': result += "\\"";
                case '\\': result += "\\\\";
                case '\b': result += "\\b";
                case '\f': result += "\\f";
                case '\n': result += "\\n";
                case '\r': result += "\\r";
                case '\t': result += "\\t";
                default: result += c;
            }
        }
        return result;
    }
};

// Thread-safe implementation for timeout management
class ThreadSafeTimeout {
public:
    ThreadSafeTimeout(std::function<void()> callback, double timeout) 
        : callback_(std::move(callback)), timeout_(timeout) {
        // Note: In a real implementation, this would start a timer
        // For simplicity, we'll just handle it synchronously
    }
    
    void Cancel() {
        std::lock_guard<std::mutex> lock(mutex_);
        cancelled_ = true;
    }
    
    bool IsCancelled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cancelled_;
    }
    
private:
    std::function<void()> callback_;
    double timeout_;
    mutable std::mutex mutex_;
    bool cancelled_ = false;
};

// Constructors
SocketIOPacket::SocketIOPacket() 
    : type_(SocketIOPacketType::kConnect),
      packet_id_(-1),
      nsp_("/"),
      placeholders_(0),
      requires_ack_(false),
      state_(SocketIOPacketState::kPending),
      timeout_interval_(0.0),
      creation_time_(std::chrono::steady_clock::now()),
      timeout_timer_running_(false) {
}

SocketIOPacket::SocketIOPacket(SocketIOPacketType type,
                              const std::string& nsp,
                              int placeholders)
    : type_(type),
      packet_id_(-1),
      nsp_(nsp.empty() ? "/" : nsp),
      placeholders_(placeholders),
      requires_ack_(false),
      state_(SocketIOPacketState::kPending),
      timeout_interval_(0.0),
      creation_time_(std::chrono::steady_clock::now()),
      timeout_timer_running_(false) {
}

SocketIOPacket::SocketIOPacket(SocketIOPacketType type,
                              const std::vector<std::string>& data,
                              int packet_id,
                              const std::string& nsp,
                              int placeholders,
                              const std::vector<std::vector<uint8_t>>& binary)
    : type_(type),
      data_(data),
      packet_id_(packet_id),
      nsp_(nsp.empty() ? "/" : nsp),
      placeholders_(placeholders),
      binary_(binary),
      requires_ack_(false),
      state_(SocketIOPacketState::kPending),
      timeout_interval_(0.0),
      creation_time_(std::chrono::steady_clock::now()),
      timeout_timer_running_(false) {
}

// Factory methods
std::unique_ptr<SocketIOPacket> SocketIOPacket::CreateEventPacket(
    const std::string& event,
    const std::vector<std::string>& items,
    int packet_id,
    const std::string& nsp,
    bool requires_ack) {
    
    std::vector<std::string> data;
    data.push_back(event);
    data.insert(data.end(), items.begin(), items.end());
    
    SocketIOPacketType packet_type = SocketIOPacketType::kEvent;
    if (requires_ack) {
        // Binary events would be handled differently
    }
    
    auto packet = std::make_unique<SocketIOPacket>(
        packet_type,
        data,
        packet_id,
        nsp,
        0, // binary count
        {} // binary data
    );
    
    packet->requires_ack_ = requires_ack;
    return packet;
}

std::unique_ptr<SocketIOPacket> SocketIOPacket::CreateAckPacket(
    int ack_id,
    const std::vector<std::string>& items,
    const std::string& nsp) {
    
    return std::make_unique<SocketIOPacket>(
        SocketIOPacketType::kAck,
        items,
        ack_id,
        nsp,
        0,
        {}
    );
}

std::unique_ptr<SocketIOPacket> SocketIOPacket::CreateFromMessage(
    const std::string& message) {
    
    if (message.empty()) {
        return nullptr;
    }
    
    size_t cursor = 0;
    
    // Parse packet type
    SocketIOPacketType type = SocketIOProtocol::CharToPacketType(message[cursor++]);
    
    // Parse binary count (if binary packet)
    int binary_count = 0;
    if (SocketIOProtocol::IsBinaryPacket(type) && cursor < message.length() && message[cursor] != '[') {
        std::string count_str;
        while (cursor < message.length() && std::isdigit(message[cursor])) {
            count_str += message[cursor++];
        }
        if (cursor < message.length() && message[cursor] == '-') {
            cursor++;
            binary_count = std::stoi(count_str);
        }
    }
    
    // Parse namespace
    std::string nsp = "/";
    if (cursor < message.length() && message[cursor] == '/') {
        size_t start = cursor;
        while (cursor < message.length() && message[cursor] != ',') {
            cursor++;
        }
        nsp = message.substr(start, cursor - start);
        if (cursor < message.length() && message[cursor] == ',') {
            cursor++;
        }
    }
    
    // Parse packet ID
    int packet_id = -1;
    if (cursor < message.length() && std::isdigit(message[cursor])) {
        std::string id_str;
        while (cursor < message.length() && std::isdigit(message[cursor])) {
            id_str += message[cursor++];
        }
        packet_id = std::stoi(id_str);
    }
    
    // Parse JSON data
    std::vector<std::string> data;
    if (cursor < message.length()) {
        std::string json_part = message.substr(cursor);
        data = SimpleJSONSerializer::Deserialize(json_part);
    }
    
    return std::make_unique<SocketIOPacket>(
        type,
        data,
        packet_id,
        nsp,
        binary_count,
        {}
    );
}

// Setup ACK callbacks
void SocketIOPacket::SetupAckCallbacks(SuccessCallback success,
                                      ErrorCallback error,
                                      double timeout) {
    success_callback_ = std::move(success);
    error_callback_ = std::move(error);
    timeout_interval_ = timeout;
    
    if (timeout > 0.0) {
        StartTimeoutTimer();
    }
}

// ACK handling
void SocketIOPacket::Acknowledge(const std::vector<std::string>& data) {
    if (state_ != SocketIOPacketState::kPending) {
        return;
    }
    
    state_ = SocketIOPacketState::kAcknowledged;
    
    if (success_callback_) {
        success_callback_(data);
    }
}

void SocketIOPacket::Fail(const std::string& error) {
    if (state_ != SocketIOPacketState::kPending) {
        return;
    }
    
    state_ = SocketIOPacketState::kCancelled;
    
    if (error_callback_) {
        error_callback_(error);
    }
}

void SocketIOPacket::Cancel() {
    if (state_ != SocketIOPacketState::kPending) {
        return;
    }
    
    state_ = SocketIOPacketState::kCancelled;
}

// Binary data handling
bool SocketIOPacket::AddBinaryData(const std::vector<uint8_t>& data) {
    binary_.push_back(data);
    
    if (binary_.size() == static_cast<size_t>(placeholders_)) {
        FillInPlaceholders();
        return true;
    }
    return false;
}

// Packet string generation
std::string SocketIOPacket::ToString() const {
    return CreatePacketString();
}

// Helper methods
std::string SocketIOPacket::GetEventName() const {
    if ((type_ == SocketIOPacketType::kEvent || type_ == SocketIOPacketType::kBinaryEvent) && !data_.empty()) {
        return data_[0];
    }
    return "";
}

std::vector<std::string> SocketIOPacket::GetEventArgs() const {
    if ((type_ == SocketIOPacketType::kEvent || type_ == SocketIOPacketType::kBinaryEvent) && data_.size() > 1) {
        return std::vector<std::string>(data_.begin() + 1, data_.end());
    }
    return {};
}

// Debug info
std::string SocketIOPacket::DebugDescription() const {
    std::ostringstream oss;
    oss << "SocketIOPacket{" 
        << "type=" << SocketIOProtocol::PacketTypeToString(type_) << ", "
        << "packet_id=" << packet_id_ << ", "
        << "event=" << GetEventName() << ", "
        << "nsp=" << nsp_ << ", "
        << "requires_ack=" << (requires_ack_ ? "YES" : "NO") << ", "
        << "state=" << static_cast<int>(state_) << ", "
        << "timeout=" << timeout_interval_ << "}";
    return oss.str();
}

// Private methods
void SocketIOPacket::StartTimeoutTimer() {
    // In a real implementation, this would start a timer
    // For simplicity, we'll just mark it as running
    timeout_timer_running_ = true;
}

void SocketIOPacket::HandleTimeout() {
    if (state_ == SocketIOPacketState::kPending) {
        state_ = SocketIOPacketState::kTimeout;
        if (error_callback_) {
            error_callback_("ACK timeout");
        }
    }
}

void SocketIOPacket::FillInPlaceholders() {
    // Replace placeholders with actual binary data
    // This is simplified for demonstration purposes
}

std::string SocketIOPacket::CreatePacketString() const {
    std::ostringstream oss;
    
    // Add packet type
    oss << SocketIOProtocol::PacketTypeToChar(type_);
    
    // Add binary count (for binary packets)
    if (SocketIOProtocol::IsBinaryPacket(type_)) {
        oss << binary_.size() << "-";
    }
    
    // Add namespace (if not root)
    if (nsp_ != "/") {
        oss << nsp_ << ",";
    }
    
    // Add packet ID (if any)
    if (packet_id_ >= 0) {
        oss << packet_id_;
    }
    
    // Add data
    oss << CompleteMessage("");
    
    return oss.str();
}

std::string SocketIOPacket::CompleteMessage(const std::string& message) const {
    std::ostringstream oss;
    oss << message;
    
    if (!data_.empty()) {
        oss << SimpleJSONSerializer::Serialize(data_);
    } else {
        oss << "[]";
    }
    
    return oss.str();
}

} // namespace socketio
