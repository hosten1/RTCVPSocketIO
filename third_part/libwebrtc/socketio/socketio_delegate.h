#ifndef SOCKETIO_DELEGATE_H
#define SOCKETIO_DELEGATE_H

#include <string>
#include <vector>
#include <memory>

namespace socketio {

// Forward declarations
class SocketIOPacket;
class SocketIOEvent;

// Delegate interface for SocketIO client events
class SocketIODelegate {
public:
    virtual ~SocketIODelegate() = default;
    
    // Connection events
    virtual void OnConnect(const std::string& namespace_) = 0;
    virtual void OnDisconnect(const std::string& reason) = 0;
    virtual void OnError(const std::string& reason) = 0;
    virtual void OnReconnectAttempt(int attempt) = 0;
    virtual void OnReconnect(int attempt) = 0;
    virtual void OnStatusChange(const std::string& status) = 0;
    
    // Packet events
    virtual void OnPacket(const SocketIOPacket& packet) = 0;
    
    // Event handling
    virtual void OnEvent(const std::string& event_name, 
                        const std::vector<std::string>& args,
                        int ack_id) = 0;
    
    // ACK response
    virtual void OnAck(int ack_id, const std::vector<std::string>& args) = 0;
    
    // Binary data handling
    virtual void OnBinaryData(const std::vector<uint8_t>& data) = 0;
    
    // Message parsing callback
    virtual void ParseSocketMessage(const std::string& message) = 0;
    
    // Engine events
    virtual void EngineDidError(const std::string& reason) = 0;
    virtual void EngineDidOpen(const std::string& reason) = 0;
    virtual void EngineDidClose(const std::string& reason) = 0;
};

} // namespace socketio

#endif // SOCKETIO_DELEGATE_H
