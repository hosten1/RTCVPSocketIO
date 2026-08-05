#include "websocket_handshake.h"
#include <sstream>
#include <algorithm>
#include <cstring>
#include <random>
#include <cctype>

#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#endif

namespace ws {

static const char* kMagicGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static const std::string base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string WebSocketHandshake::base64_encode(const uint8_t* data, size_t len) {
    std::string result;
    result.reserve(((len + 2) / 3) * 4);
    
    size_t i = 0;
    while (i < len) {
        uint32_t octet_a = i < len ? data[i++] : 0;
        uint32_t octet_b = i < len ? data[i++] : 0;
        uint32_t octet_c = i < len ? data[i++] : 0;
        
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
        
        result.push_back(base64_chars[(triple >> 18) & 0x3F]);
        result.push_back(base64_chars[(triple >> 12) & 0x3F]);
        result.push_back(base64_chars[(triple >> 6) & 0x3F]);
        result.push_back(base64_chars[triple & 0x3F]);
    }
    
    if (len % 3 == 1) {
        result[result.size() - 1] = '=';
        result[result.size() - 2] = '=';
    } else if (len % 3 == 2) {
        result[result.size() - 1] = '=';
    }
    
    return result;
}

std::string WebSocketHandshake::sha1(const std::string& input) {
    uint8_t digest[20];
    
#ifdef __APPLE__
    CC_SHA1(input.data(), static_cast<CC_LONG>(input.size()), digest);
#else
    SHA1(reinterpret_cast<const uint8_t*>(input.data()), input.size(), digest);
#endif
    
    return std::string(reinterpret_cast<char*>(digest), 20);
}

std::string WebSocketHandshake::trim(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(*start)) start++;
    auto end = s.end();
    do { end--; } while (std::distance(start, end) > 0 && std::isspace(*end));
    return std::string(start, end + 1);
}

std::string WebSocketHandshake::to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string WebSocketHandshake::generateKey() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);
    
    uint8_t key_bytes[16];
    for (int i = 0; i < 16; i++) {
        key_bytes[i] = dis(gen);
    }
    return base64_encode(key_bytes, 16);
}

std::string WebSocketHandshake::buildRequest(const HandshakeRequest& req) {
    std::ostringstream ss;
    
    ss << "GET " << req.path << " HTTP/1.1\r\n";
    ss << "Host: " << req.host;
    if (req.port != 80 && req.port != 443) {
        ss << ":" << req.port;
    }
    ss << "\r\n";
    ss << "Upgrade: websocket\r\n";
    ss << "Connection: Upgrade\r\n";
    ss << "Sec-WebSocket-Key: " << req.key << "\r\n";
    ss << "Sec-WebSocket-Version: 13\r\n";
    
    if (!req.protocols.empty()) {
        ss << "Sec-WebSocket-Protocol: ";
        for (size_t i = 0; i < req.protocols.size(); i++) {
            if (i > 0) ss << ", ";
            ss << req.protocols[i];
        }
        ss << "\r\n";
    }
    
    for (const auto& header : req.extra_headers) {
        ss << header.first << ": " << header.second << "\r\n";
    }
    
    ss << "\r\n";
    return ss.str();
}

HandshakeResponse WebSocketHandshake::parseResponse(const std::string& response) {
    HandshakeResponse resp;
    
    size_t pos = response.find("\r\n\r\n");
    if (pos == std::string::npos) {
        resp.success = false;
        resp.error = "Incomplete response";
        return resp;
    }
    
    std::string header_section = response.substr(0, pos);
    
    size_t line_end = header_section.find("\r\n");
    if (line_end == std::string::npos) {
        resp.success = false;
        resp.error = "Invalid status line";
        return resp;
    }
    
    resp.status_line = header_section.substr(0, line_end);
    
    size_t first_space = resp.status_line.find(' ');
    size_t second_space = resp.status_line.find(' ', first_space + 1);
    if (first_space != std::string::npos) {
        std::string code_str = resp.status_line.substr(first_space + 1, 
            second_space != std::string::npos ? second_space - first_space - 1 : std::string::npos);
        resp.status_code = std::stoi(code_str);
    }
    
    size_t header_start = line_end + 2;
    while (header_start < header_section.size()) {
        size_t header_end = header_section.find("\r\n", header_start);
        if (header_end == std::string::npos) break;
        
        std::string line = header_section.substr(header_start, header_end - header_start);
        size_t colon_pos = line.find(':');
        
        if (colon_pos != std::string::npos) {
            std::string key = to_lower(trim(line.substr(0, colon_pos)));
            std::string value = trim(line.substr(colon_pos + 1));
            resp.headers[key] = value;
        }
        
        header_start = header_end + 2;
    }
    
    if (resp.status_code != 101) {
        resp.success = false;
        resp.error = "Status code is not 101 Switching Protocols";
        return resp;
    }
    
    auto it_upgrade = resp.headers.find("upgrade");
    auto it_connection = resp.headers.find("connection");
    auto it_accept = resp.headers.find("sec-websocket-accept");
    
    if (it_upgrade == resp.headers.end() || to_lower(it_upgrade->second) != "websocket") {
        resp.success = false;
        resp.error = "Missing or invalid Upgrade header";
        return resp;
    }
    
    if (it_connection == resp.headers.end() || 
        to_lower(it_connection->second).find("upgrade") == std::string::npos) {
        resp.success = false;
        resp.error = "Missing or invalid Connection header";
        return resp;
    }
    
    if (it_accept != resp.headers.end()) {
        resp.accept_key = it_accept->second;
    }
    
    resp.success = true;
    return resp;
}

bool WebSocketHandshake::verifyAccept(const std::string& client_key, const std::string& server_accept) {
    std::string expected = client_key + kMagicGuid;
    std::string sha1_hash = sha1(expected);
    std::string expected_accept = base64_encode(
        reinterpret_cast<const uint8_t*>(sha1_hash.data()), sha1_hash.size());
    return expected_accept == server_accept;
}

}
