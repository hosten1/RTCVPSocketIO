#include "websocket_frame.h"
#include <cstring>
#include <arpa/inet.h>

#include "rtc_base/logging.h"
#include "rtc_base/synchronization/mutex.h"

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#define htobe64(x) OSSwapHostToBigInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#endif

namespace ws {

namespace {
webrtc::Mutex g_random_mutex;
bool g_random_initialized = false;
unsigned int g_random_seed = 0;
}

uint32_t WebSocketFrameCoder::generateMaskingKey() {
    webrtc::MutexLock lock(&g_random_mutex);
    if (!g_random_initialized) {
        g_random_seed = static_cast<unsigned int>(time(nullptr));
        g_random_initialized = true;
    }
    g_random_seed = g_random_seed * 1103515245 + 12345;
    return static_cast<uint32_t>(g_random_seed);
}

void WebSocketFrameCoder::maskData(uint8_t* data, size_t len, uint32_t key) {
    uint8_t key_bytes[4];
    key_bytes[0] = key & 0xFF;
    key_bytes[1] = (key >> 8) & 0xFF;
    key_bytes[2] = (key >> 16) & 0xFF;
    key_bytes[3] = (key >> 24) & 0xFF;
    
    for (size_t i = 0; i < len; i++) {
        data[i] ^= key_bytes[i % 4];
    }
}

std::vector<uint8_t> WebSocketFrameCoder::encode(const WebSocketFrame& frame, bool mask) {
    RTC_LOG(LS_VERBOSE) << "Encoding frame: opcode=" << static_cast<int>(frame.opcode)
                        << " fin=" << (frame.fin ? "yes" : "no")
                        << " payload_size=" << frame.payload.size()
                        << " masked=" << (mask ? "yes" : "no");
    
    std::vector<uint8_t> result;
    result.reserve(14 + frame.payload.size());
    
    uint8_t byte0 = 0;
    if (frame.fin) byte0 |= 0x80;
    if (frame.rsv1) byte0 |= 0x40;
    if (frame.rsv2) byte0 |= 0x20;
    if (frame.rsv3) byte0 |= 0x10;
    byte0 |= static_cast<uint8_t>(frame.opcode) & 0x0F;
    result.push_back(byte0);
    
    uint64_t payload_len = frame.payload.size();
    uint8_t byte1 = mask ? 0x80 : 0x00;
    
    if (payload_len < 126) {
        byte1 |= static_cast<uint8_t>(payload_len);
        result.push_back(byte1);
    } else if (payload_len < 65536) {
        byte1 |= 126;
        result.push_back(byte1);
        uint16_t len16 = htons(static_cast<uint16_t>(payload_len));
        result.insert(result.end(), 
                      reinterpret_cast<uint8_t*>(&len16),
                      reinterpret_cast<uint8_t*>(&len16) + 2);
    } else {
        byte1 |= 127;
        result.push_back(byte1);
        uint64_t len64 = htobe64(payload_len);
        result.insert(result.end(),
                      reinterpret_cast<uint8_t*>(&len64),
                      reinterpret_cast<uint8_t*>(&len64) + 8);
    }
    
    if (mask) {
        uint32_t key = generateMaskingKey();
        uint8_t key_bytes[4];
        key_bytes[0] = key & 0xFF;
        key_bytes[1] = (key >> 8) & 0xFF;
        key_bytes[2] = (key >> 16) & 0xFF;
        key_bytes[3] = (key >> 24) & 0xFF;
        result.insert(result.end(), key_bytes, key_bytes + 4);
        
        size_t payload_start = result.size();
        result.insert(result.end(), frame.payload.begin(), frame.payload.end());
        maskData(result.data() + payload_start, frame.payload.size(), key);
    } else {
        result.insert(result.end(), frame.payload.begin(), frame.payload.end());
    }
    
    RTC_LOG(LS_VERBOSE) << "Frame encoded, total size: " << result.size();
    return result;
}

std::vector<uint8_t> WebSocketFrameCoder::encodeText(const std::string& text, bool mask) {
    WebSocketFrame frame;
    frame.opcode = OpCode::Text;
    frame.payload.assign(text.begin(), text.end());
    return encode(frame, mask);
}

std::vector<uint8_t> WebSocketFrameCoder::encodeBinary(const std::vector<uint8_t>& data, bool mask) {
    WebSocketFrame frame;
    frame.opcode = OpCode::Binary;
    frame.payload = data;
    return encode(frame, mask);
}

std::vector<uint8_t> WebSocketFrameCoder::encodePing(const std::vector<uint8_t>& data, bool mask) {
    WebSocketFrame frame;
    frame.opcode = OpCode::Ping;
    frame.payload = data;
    return encode(frame, mask);
}

std::vector<uint8_t> WebSocketFrameCoder::encodePong(const std::vector<uint8_t>& data, bool mask) {
    WebSocketFrame frame;
    frame.opcode = OpCode::Pong;
    frame.payload = data;
    return encode(frame, mask);
}

std::vector<uint8_t> WebSocketFrameCoder::encodeClose(uint16_t code, const std::string& reason, bool mask) {
    WebSocketFrame frame;
    frame.opcode = OpCode::Close;
    
    uint16_t code_net = htons(code);
    frame.payload.resize(2 + reason.size());
    std::memcpy(frame.payload.data(), &code_net, 2);
    if (!reason.empty()) {
        std::memcpy(frame.payload.data() + 2, reason.data(), reason.size());
    }
    return encode(frame, mask);
}

FrameParseResult WebSocketFrameCoder::parse(const uint8_t* data, size_t len, 
                                             WebSocketFrame& out_frame, size_t& out_consumed) {
    if (len < 2) {
        RTC_LOG(LS_VERBOSE) << "Frame parse: incomplete (need at least 2 bytes, have " << len << ")";
        return FrameParseResult::Incomplete;
    }
    
    size_t offset = 0;
    
    uint8_t byte0 = data[offset++];
    out_frame.fin = (byte0 & 0x80) != 0;
    out_frame.rsv1 = (byte0 & 0x40) != 0;
    out_frame.rsv2 = (byte0 & 0x20) != 0;
    out_frame.rsv3 = (byte0 & 0x10) != 0;
    out_frame.opcode = static_cast<OpCode>(byte0 & 0x0F);
    
    uint8_t byte1 = data[offset++];
    out_frame.masked = (byte1 & 0x80) != 0;
    uint8_t payload_len_byte = byte1 & 0x7F;
    
    uint64_t payload_len = 0;
    if (payload_len_byte < 126) {
        payload_len = payload_len_byte;
    } else if (payload_len_byte == 126) {
        if (len < offset + 2) {
            RTC_LOG(LS_VERBOSE) << "Frame parse: incomplete (need extended payload length)";
            return FrameParseResult::Incomplete;
        }
        uint16_t len16;
        std::memcpy(&len16, data + offset, 2);
        payload_len = ntohs(len16);
        offset += 2;
    } else {
        if (len < offset + 8) {
            RTC_LOG(LS_VERBOSE) << "Frame parse: incomplete (need extended payload length)";
            return FrameParseResult::Incomplete;
        }
        uint64_t len64;
        std::memcpy(&len64, data + offset, 8);
        payload_len = be64toh(len64);
        offset += 8;
    }
    
    out_frame.payload_length = payload_len;
    
    if (out_frame.masked) {
        if (len < offset + 4) {
            RTC_LOG(LS_VERBOSE) << "Frame parse: incomplete (need masking key)";
            return FrameParseResult::Incomplete;
        }
        std::memcpy(&out_frame.masking_key, data + offset, 4);
        offset += 4;
    }
    
    if (len < offset + payload_len) {
        RTC_LOG(LS_VERBOSE) << "Frame parse: incomplete (need " << (offset + payload_len) 
                            << " bytes, have " << len << ")";
        return FrameParseResult::Incomplete;
    }
    
    out_frame.payload.resize(payload_len);
    if (payload_len > 0) {
        std::memcpy(out_frame.payload.data(), data + offset, payload_len);
        if (out_frame.masked) {
            maskData(out_frame.payload.data(), payload_len, out_frame.masking_key);
        }
    }
    
    out_consumed = offset + payload_len;
    
    RTC_LOG(LS_VERBOSE) << "Frame parsed: opcode=" << static_cast<int>(out_frame.opcode)
                        << " fin=" << (out_frame.fin ? "yes" : "no")
                        << " payload_size=" << payload_len
                        << " masked=" << (out_frame.masked ? "yes" : "no")
                        << " consumed=" << out_consumed;
    
    return FrameParseResult::Ok;
}

}
