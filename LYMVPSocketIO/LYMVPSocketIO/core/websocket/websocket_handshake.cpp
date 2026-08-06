#include "websocket_handshake.h"
#include <algorithm>
#include <cstring>
#include <cctype>

#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/string_utils.h"
#include "rtc_base/third_party/base64/base64.h"

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
    rtc::Base64::EncodeFromArray(data, len, &result);
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
    return rtc::string_trim(s);
}

std::string WebSocketHandshake::to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string WebSocketHandshake::generateKey() {
    RTC_LOG(LS_VERBOSE) << "Generating WebSocket key";
    
    uint8_t key_bytes[16];
    for (int i = 0; i < 16; i++) {
        key_bytes[i] = static_cast<uint8_t>(rand() & 0xFF);
    }
    std::string key = base64_encode(key_bytes, 16);
    RTC_LOG(LS_VERBOSE) << "Generated key: " << key;
    return key;
}

std::string WebSocketHandshake::buildRequest(const HandshakeRequest& req) {
    RTC_LOG(LS_INFO) << "Building handshake request for " << req.host << ":" << req.port << req.path;
    
    rtc::StringBuilder ss;
    
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
    
    std::string request = ss.str();
    RTC_LOG(LS_VERBOSE) << "Handshake request built, size: " << request.size();
    return request;
}

HandshakeResponse WebSocketHandshake::parseResponse(const std::string& response) {
    RTC_LOG(LS_INFO) << "Parsing handshake response, size: " << response.size();
    
    HandshakeResponse resp;
    
    size_t pos = response.find("\r\n\r\n");
    if (pos == std::string::npos) {
        resp.success = false;
        resp.error = "Incomplete response";
        RTC_LOG(LS_WARNING) << "Handshake parse failed: " << resp.error;
        return resp;
    }
    
    std::string header_section = response.substr(0, pos);
    if (!header_section.empty() && 
        (header_section.size() < 2 || 
         header_section[header_section.size()-2] != '\r' || 
         header_section[header_section.size()-1] != '\n')) {
        header_section += "\r\n";
    }
    
    size_t line_end = header_section.find("\r\n");
    if (line_end == std::string::npos) {
        resp.success = false;
        resp.error = "Invalid status line";
        RTC_LOG(LS_WARNING) << "Handshake parse failed: " << resp.error;
        return resp;
    }
    
    resp.status_line = header_section.substr(0, line_end);
    RTC_LOG(LS_VERBOSE) << "Status line: " << resp.status_line;
    
    size_t first_space = resp.status_line.find(' ');
    size_t second_space = resp.status_line.find(' ', first_space + 1);
    if (first_space != std::string::npos) {
        std::string code_str = resp.status_line.substr(first_space + 1, 
            second_space != std::string::npos ? second_space - first_space - 1 : std::string::npos);
        resp.status_code = std::stoi(code_str);
    }
    
    RTC_LOG(LS_VERBOSE) << "Status code: " << resp.status_code;
    
    size_t header_start = line_end + 2;
    int header_count = 0;
    while (header_start < header_section.size()) {
        size_t header_end = header_section.find("\r\n", header_start);
        if (header_end == std::string::npos) break;
        
        std::string line = header_section.substr(header_start, header_end - header_start);
        size_t colon_pos = line.find(':');
        
        if (colon_pos != std::string::npos) {
            std::string key = to_lower(trim(line.substr(0, colon_pos)));
            std::string value = trim(line.substr(colon_pos + 1));
            resp.headers[key] = value;
            header_count++;
            RTC_LOG(LS_VERBOSE) << "Header: " << key << "=" << value;
        }
        
        header_start = header_end + 2;
    }
    
    RTC_LOG(LS_VERBOSE) << "Parsed " << header_count << " headers";
    
    if (resp.status_code != 101) {
        resp.success = false;
        resp.error = "Status code is not 101 Switching Protocols";
        RTC_LOG(LS_ERROR) << "Handshake failed: " << resp.error 
                          << ", got " << resp.status_code;
        return resp;
    }
    
    auto it_upgrade = resp.headers.find("upgrade");
    auto it_connection = resp.headers.find("connection");
    auto it_accept = resp.headers.find("sec-websocket-accept");
    
    if (it_upgrade == resp.headers.end() || to_lower(it_upgrade->second) != "websocket") {
        resp.success = false;
        resp.error = "Missing or invalid Upgrade header";
        RTC_LOG(LS_ERROR) << "Handshake failed: " << resp.error;
        return resp;
    }
    
    if (it_connection == resp.headers.end() || 
        to_lower(it_connection->second).find("upgrade") == std::string::npos) {
        resp.success = false;
        resp.error = "Missing or invalid Connection header";
        RTC_LOG(LS_ERROR) << "Handshake failed: " << resp.error;
        return resp;
    }
    
    if (it_accept != resp.headers.end()) {
        resp.accept_key = it_accept->second;
    }
    
    resp.success = true;
    RTC_LOG(LS_INFO) << "Handshake response parsed successfully";
    return resp;
}

bool WebSocketHandshake::verifyAccept(const std::string& client_key, const std::string& server_accept) {
    RTC_LOG(LS_VERBOSE) << "Verifying accept key";
    std::string expected = computeAccept(client_key);
    bool match = (expected == server_accept);
    RTC_LOG(LS_VERBOSE) << "Accept key verification: " << (match ? "PASS" : "FAIL");
    return match;
}

std::string WebSocketHandshake::computeAccept(const std::string& client_key) {
    std::string input = client_key + kMagicGuid;
    std::string sha1_hash = sha1(input);
    return base64_encode(
        reinterpret_cast<const uint8_t*>(sha1_hash.data()), sha1_hash.size());
}

}
