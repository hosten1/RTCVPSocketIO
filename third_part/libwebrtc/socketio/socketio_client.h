#ifndef SOCKETIO_CLIENT_H
#define SOCKETIO_CLIENT_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>

#include "rtc_base/thread.h"
#include "socketio_delegate.h"
#include "socketio_packet.h"
#include "socketio_thread_safe_queue.h"
#include "socketio_protocol.h"

namespace socketio {

// Forward declarations
class SocketIOPacket;
class SocketIOThreadSafeQueue;

// SocketIO client class
class SocketIOClient {
public:
    // Constructor
    SocketIOClient();
    
    // Destructor
    ~SocketIOClient();
    
    // Set delegate
    void SetDelegate(SocketIODelegate* delegate);
    
    // Connection management
    void Connect(const std::string& url, const std::map<std::string, std::string>& config);
    void Disconnect();
    void Reconnect();
    
    // Event emission
    void Emit(const std::string& event);
    void Emit(const std::string& event, const std::vector<std::string>& items);
    void EmitWithAck(const std::string& event, 
                    const std::vector<std::string>& items, 
                    std::function<void(const std::vector<std::string>& data, const std::string& error)> ack_callback,
                    double timeout = 10.0);
    
    // Event listening (placeholder for now, will be implemented later)
    void On(const std::string& event, std::function<void(const std::vector<std::string>& data)> callback);
    void Off(const std::string& event);
    void OffAll();
    
    // Accessors
    SocketIOClientStatus status() const { return status_; }
    const std::string& namespace_() const { return namespace_; }
    void set_namespace(const std::string& namespace_) { namespace_ = namespace_; }
    
    // Thread-safe methods for external thread communication
    void PostMessage(const std::string& message);
    void PostBinaryData(const std::vector<uint8_t>& data);
    void PostAck(int ack_id, const std::vector<std::string>& data);
    
private:
    // Internal methods
    void SetupThread();
    void CleanupThread();
    void ProcessMessage(const std::string& message);
    void ProcessPacket(const SocketIOPacket& packet);
    void HandleEvent(const std::string& event_name, 
                    const std::vector<std::string>& args,
                    int ack_id);
    void HandleAck(int ack_id, const std::vector<std::string>& args);
    
    // Connection callbacks
    void DidConnect(const std::string& namespace_);
    void DidDisconnect(const std::string& reason);
    void DidError(const std::string& reason);
    void DidReconnectAttempt(int attempt);
    void DidReconnect(int attempt);
    void DidStatusChange(SocketIOClientStatus status);
    
    // Status update
    void SetStatus(SocketIOClientStatus status);
    
    // ACK management
    int GenerateAckId();
    
    // Data members
    SocketIODelegate* delegate_ = nullptr;
    
    // Thread management using libwebrtc's Thread class
    std::unique_ptr<rtc::Thread> client_thread_;
    
    // Thread-safe queues for inter-thread communication
    SocketIOThreadSafeQueue<std::string> message_queue_;
    SocketIOThreadSafeQueue<std::vector<uint8_t>> binary_queue_;
    SocketIOThreadSafeQueue<std::pair<int, std::vector<std::string>>> ack_queue_;
    
    // Client state
    SocketIOClientStatus status_ = SocketIOClientStatus::kNotConnected;
    std::string url_;
    std::string namespace_ = "/";
    
    // ACK management
    int current_ack_id_ = -1;
    std::map<int, std::function<void(const std::vector<std::string>&, const std::string&)>> ack_callbacks_;
    
    // Event handlers
    std::map<std::string, std::vector<std::function<void(const std::vector<std::string>&)>>> event_handlers_;
    
    // Reconnection state
    bool reconnecting_ = false;
    int reconnect_attempts_ = 0;
    int max_reconnect_attempts_ = -1; // -1 for unlimited
    double reconnect_wait_time_ = 10.0;
    
    // Packet queue for pending messages
    std::vector<std::unique_ptr<SocketIOPacket>> waiting_packets_;
    
    // Binary data handling
    std::vector<std::vector<uint8_t>> pending_binary_data_;
    
    // Thread-safe flags
    bool is_running_ = false;
    mutable std::mutex mutex_;
};

} // namespace socketio

#endif // SOCKETIO_CLIENT_H
