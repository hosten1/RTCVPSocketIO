#ifndef SOCKETIO_PROTOCOL_H
#define SOCKETIO_PROTOCOL_H

#include <string>

namespace socketio {

// SocketIO packet types
enum class SocketIOPacketType {
    kConnect = 0,
    kDisconnect = 1,
    kEvent = 2,
    kAck = 3,
    kError = 4,
    kBinaryEvent = 5,
    kBinaryAck = 6
};

// SocketIO client status enum
enum class SocketIOClientStatus {
    kNotConnected,
    kDisconnected,
    kConnecting,
    kOpened,
    kConnected
};

// SocketIO client events
enum class SocketIOClientEvent {
    kConnect,
    kDisconnect,
    kError,
    kReconnect,
    kReconnectAttempt,
    kStatusChange
};

// SocketIO protocol constants
constexpr const char* kSocketIOEventConnect = "connect";
constexpr const char* kSocketIOEventDisconnect = "disconnect";
constexpr const char* kSocketIOEventError = "error";
constexpr const char* kSocketIOEventReconnect = "reconnect";
constexpr const char* kSocketIOEventReconnectAttempt = "reconnectAttempt";
constexpr const char* kSocketIOEventStatusChange = "statusChange";

constexpr const char* kSocketIOStatusNotConnected = "notconnected";
constexpr const char* kSocketIOStatusDisconnected = "disconnected";
constexpr const char* kSocketIOStatusConnecting = "connecting";
constexpr const char* kSocketIOStatusOpened = "opened";
constexpr const char* kSocketIOStatusConnected = "connected";

// SocketIO packet state
enum class SocketIOPacketState {
    kPending,
    kAcknowledged,
    kTimeout,
    kCancelled
};

// SocketIO protocol helper functions
class SocketIOProtocol {
public:
    // Convert packet type to string
    static std::string PacketTypeToString(SocketIOPacketType type);
    
    // Convert string to packet type
    static SocketIOPacketType StringToPacketType(const std::string& type_str);
    
    // Convert packet type to char
    static char PacketTypeToChar(SocketIOPacketType type);
    
    // Convert char to packet type
    static SocketIOPacketType CharToPacketType(char type_char);
    
    // Convert status to string
    static std::string StatusToString(SocketIOClientStatus status);
    
    // Convert string to status
    static SocketIOClientStatus StringToStatus(const std::string& status_str);
    
    // Convert client event to string
    static std::string ClientEventToString(SocketIOClientEvent event);
    
    // Convert string to client event
    static SocketIOClientEvent StringToClientEvent(const std::string& event_str);
    
    // Check if a packet type is binary
    static bool IsBinaryPacket(SocketIOPacketType type);
    
    // Check if a packet requires ACK
    static bool RequiresAck(SocketIOPacketType type, int packet_id);
};

} // namespace socketio

#endif // SOCKETIO_PROTOCOL_H
