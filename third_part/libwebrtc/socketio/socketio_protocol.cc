#include "socketio_protocol.h"

namespace socketio {

std::string SocketIOProtocol::PacketTypeToString(SocketIOPacketType type) {
    switch (type) {
        case SocketIOPacketType::kConnect: return "connect";
        case SocketIOPacketType::kDisconnect: return "disconnect";
        case SocketIOPacketType::kEvent: return "event";
        case SocketIOPacketType::kAck: return "ack";
        case SocketIOPacketType::kError: return "error";
        case SocketIOPacketType::kBinaryEvent: return "binaryEvent";
        case SocketIOPacketType::kBinaryAck: return "binaryAck";
        default: return "unknown";
    }
}

SocketIOPacketType SocketIOProtocol::StringToPacketType(const std::string& type_str) {
    if (type_str == "connect") return SocketIOPacketType::kConnect;
    if (type_str == "disconnect") return SocketIOPacketType::kDisconnect;
    if (type_str == "event") return SocketIOPacketType::kEvent;
    if (type_str == "ack") return SocketIOPacketType::kAck;
    if (type_str == "error") return SocketIOPacketType::kError;
    if (type_str == "binaryEvent") return SocketIOPacketType::kBinaryEvent;
    if (type_str == "binaryAck") return SocketIOPacketType::kBinaryAck;
    return SocketIOPacketType::kConnect; // Default
}

char SocketIOProtocol::PacketTypeToChar(SocketIOPacketType type) {
    return static_cast<char>('0' + static_cast<int>(type));
}

SocketIOPacketType SocketIOProtocol::CharToPacketType(char type_char) {
    int type_int = type_char - '0';
    if (type_int >= 0 && type_int <= 6) {
        return static_cast<SocketIOPacketType>(type_int);
    }
    return SocketIOPacketType::kConnect; // Default
}

std::string SocketIOProtocol::StatusToString(SocketIOClientStatus status) {
    switch (status) {
        case SocketIOClientStatus::kNotConnected: return kSocketIOStatusNotConnected;
        case SocketIOClientStatus::kDisconnected: return kSocketIOStatusDisconnected;
        case SocketIOClientStatus::kConnecting: return kSocketIOStatusConnecting;
        case SocketIOClientStatus::kOpened: return kSocketIOStatusOpened;
        case SocketIOClientStatus::kConnected: return kSocketIOStatusConnected;
        default: return kSocketIOStatusNotConnected;
    }
}

SocketIOClientStatus SocketIOProtocol::StringToStatus(const std::string& status_str) {
    if (status_str == kSocketIOStatusNotConnected) return SocketIOClientStatus::kNotConnected;
    if (status_str == kSocketIOStatusDisconnected) return SocketIOClientStatus::kDisconnected;
    if (status_str == kSocketIOStatusConnecting) return SocketIOClientStatus::kConnecting;
    if (status_str == kSocketIOStatusOpened) return SocketIOClientStatus::kOpened;
    if (status_str == kSocketIOStatusConnected) return SocketIOClientStatus::kConnected;
    return SocketIOClientStatus::kNotConnected; // Default
}

std::string SocketIOProtocol::ClientEventToString(SocketIOClientEvent event) {
    switch (event) {
        case SocketIOClientEvent::kConnect: return kSocketIOEventConnect;
        case SocketIOClientEvent::kDisconnect: return kSocketIOEventDisconnect;
        case SocketIOClientEvent::kError: return kSocketIOEventError;
        case SocketIOClientEvent::kReconnect: return kSocketIOEventReconnect;
        case SocketIOClientEvent::kReconnectAttempt: return kSocketIOEventReconnectAttempt;
        case SocketIOClientEvent::kStatusChange: return kSocketIOEventStatusChange;
        default: return "unknown";
    }
}

SocketIOClientEvent SocketIOProtocol::StringToClientEvent(const std::string& event_str) {
    if (event_str == kSocketIOEventConnect) return SocketIOClientEvent::kConnect;
    if (event_str == kSocketIOEventDisconnect) return SocketIOClientEvent::kDisconnect;
    if (event_str == kSocketIOEventError) return SocketIOClientEvent::kError;
    if (event_str == kSocketIOEventReconnect) return SocketIOClientEvent::kReconnect;
    if (event_str == kSocketIOEventReconnectAttempt) return SocketIOClientEvent::kReconnectAttempt;
    if (event_str == kSocketIOEventStatusChange) return SocketIOClientEvent::kStatusChange;
    return SocketIOClientEvent::kConnect; // Default
}

bool SocketIOProtocol::IsBinaryPacket(SocketIOPacketType type) {
    return type == SocketIOPacketType::kBinaryEvent || 
           type == SocketIOPacketType::kBinaryAck;
}

bool SocketIOProtocol::RequiresAck(SocketIOPacketType type, int packet_id) {
    return packet_id >= 0 && 
           (type == SocketIOPacketType::kEvent || 
            type == SocketIOPacketType::kBinaryEvent);
}

} // namespace socketio
