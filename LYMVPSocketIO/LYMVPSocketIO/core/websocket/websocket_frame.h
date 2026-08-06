#ifndef WEBSOCKET_FRAME_H
#define WEBSOCKET_FRAME_H

#include <cstdint>
#include <vector>
#include <string>

namespace ws {

enum class OpCode : uint8_t {
    Continuation = 0x0,
    Text = 0x1,
    Binary = 0x2,
    Close = 0x8,
    Ping = 0x9,
    Pong = 0xA
};

enum class FrameParseResult {
    Ok,
    Incomplete,
    Error
};

struct WebSocketFrame {
    bool fin = true;
    bool rsv1 = false;
    bool rsv2 = false;
    bool rsv3 = false;
    OpCode opcode = OpCode::Text;
    bool masked = false;
    uint64_t payload_length = 0;
    uint32_t masking_key = 0;
    std::vector<uint8_t> payload;
    
    bool isControlFrame() const {
        return static_cast<uint8_t>(opcode) & 0x8;
    }
};

class WebSocketFrameCoder {
public:
    static std::vector<uint8_t> encode(const WebSocketFrame& frame, bool mask = true);
    
    static std::vector<uint8_t> encodeText(const std::string& text, bool mask = true);
    
    static std::vector<uint8_t> encodeBinary(const std::vector<uint8_t>& data, bool mask = true);
    
    static std::vector<uint8_t> encodePing(const std::vector<uint8_t>& data = {}, bool mask = true);
    
    static std::vector<uint8_t> encodePong(const std::vector<uint8_t>& data = {}, bool mask = true);
    
    static std::vector<uint8_t> encodeClose(uint16_t code = 1000, const std::string& reason = "", bool mask = true);
    
    static FrameParseResult parse(const uint8_t* data, size_t len, WebSocketFrame& out_frame, size_t& out_consumed);
    
private:
    static uint32_t generateMaskingKey();
    static void maskData(uint8_t* data, size_t len, uint32_t key);
};

}

#endif
