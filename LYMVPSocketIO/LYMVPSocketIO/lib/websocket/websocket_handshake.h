#ifndef WEBSOCKET_HANDSHAKE_H
#define WEBSOCKET_HANDSHAKE_H

#include <string>
#include <map>
#include <vector>

namespace ws {

struct HandshakeRequest {
    std::string host;
    int port;
    std::string path;
    std::string key;
    std::vector<std::string> protocols;
    std::map<std::string, std::string> extra_headers;
    bool use_tls = false;
};

struct HandshakeResponse {
    int status_code = 0;
    std::string status_line;
    std::map<std::string, std::string> headers;
    std::string accept_key;
    bool success = false;
    std::string error;
};

class WebSocketHandshake {
public:
    static std::string generateKey();
    
    static std::string buildRequest(const HandshakeRequest& req);
    
    static HandshakeResponse parseResponse(const std::string& response);
    
    static bool verifyAccept(const std::string& client_key, const std::string& server_accept);
    
    static std::string computeAccept(const std::string& client_key);
    
private:
    static std::string base64_encode(const uint8_t* data, size_t len);
    static std::string sha1(const std::string& input);
    static std::string trim(const std::string& s);
    static std::string to_lower(const std::string& s);
};

}

#endif
