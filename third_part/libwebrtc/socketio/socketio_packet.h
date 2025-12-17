#ifndef SOCKETIO_PACKET_H
#define SOCKETIO_PACKET_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>

#include "socketio_protocol.h"

namespace socketio {

// SocketIO packet class for handling SocketIO protocol packets
class SocketIOPacket {
public:
    // Constructors
    SocketIOPacket();
    SocketIOPacket(SocketIOPacketType type,
                  const std::string& nsp,
                  int placeholders);
    
    SocketIOPacket(SocketIOPacketType type,
                  const std::vector<std::string>& data,
                  int packet_id,
                  const std::string& nsp,
                  int placeholders,
                  const std::vector<std::vector<uint8_t>>& binary);
    
    // Factory methods
    static std::unique_ptr<SocketIOPacket> CreateEventPacket(
        const std::string& event,
        const std::vector<std::string>& items,
        int packet_id,
        const std::string& nsp,
        bool requires_ack);
    
    static std::unique_ptr<SocketIOPacket> CreateAckPacket(
        int ack_id,
        const std::vector<std::string>& items,
        const std::string& nsp);
    
    static std::unique_ptr<SocketIOPacket> CreateFromMessage(
        const std::string& message);
    
    // ACK callback typedefs
    using SuccessCallback = std::function<void(const std::vector<std::string>& data)>;
    using ErrorCallback = std::function<void(const std::string& error)>;
    
    // Setup ACK callbacks
    void SetupAckCallbacks(SuccessCallback success,
                          ErrorCallback error,
                          double timeout);
    
    // ACK handling
    void Acknowledge(const std::vector<std::string>& data);
    void Fail(const std::string& error);
    void Cancel();
    
    // Binary data handling
    bool AddBinaryData(const std::vector<uint8_t>& data);
    
    // Packet string generation
    std::string ToString() const;
    
    // Accessors
    SocketIOPacketType type() const { return type_; }
    void set_type(SocketIOPacketType type) { type_ = type; }
    
    const std::vector<std::string>& data() const { return data_; }
    void set_data(const std::vector<std::string>& data) { data_ = data; }
    
    int packet_id() const { return packet_id_; }
    void set_packet_id(int packet_id) { packet_id_ = packet_id; }
    
    const std::string& nsp() const { return nsp_; }
    void set_nsp(const std::string& nsp) { nsp_ = nsp; }
    
    int placeholders() const { return placeholders_; }
    void set_placeholders(int placeholders) { placeholders_ = placeholders; }
    
    const std::vector<std::vector<uint8_t>>& binary() const { return binary_; }
    void set_binary(const std::vector<std::vector<uint8_t>>& binary) { binary_ = binary; }
    
    bool requires_ack() const { return requires_ack_; }
    void set_requires_ack(bool requires_ack) { requires_ack_ = requires_ack; }
    
    SocketIOPacketState state() const { return state_; }
    
    // Helper methods
    std::string GetEventName() const;
    std::vector<std::string> GetEventArgs() const;
    
    // Debug info
    std::string DebugDescription() const;
    
private:
    // Private methods
    void StartTimeoutTimer();
    void HandleTimeout();
    void FillInPlaceholders();
    std::string CreatePacketString() const;
    std::string CompleteMessage(const std::string& message) const;
    
    // Private data members
    SocketIOPacketType type_ = SocketIOPacketType::kConnect;
    std::vector<std::string> data_;
    int packet_id_ = -1;
    std::string nsp_ = "/";
    int placeholders_ = 0;
    std::vector<std::vector<uint8_t>> binary_;
    bool requires_ack_ = false;
    
    SocketIOPacketState state_ = SocketIOPacketState::kPending;
    
    SuccessCallback success_callback_;
    ErrorCallback error_callback_;
    double timeout_interval_ = 0.0;
    
    // Timeout management
    std::chrono::steady_clock::time_point creation_time_;
    bool timeout_timer_running_ = false;
};

} // namespace socketio

#endif // SOCKETIO_PACKET_H
