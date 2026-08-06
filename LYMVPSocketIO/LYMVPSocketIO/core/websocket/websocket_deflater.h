#ifndef WEBSOCKET_DEFLATER_H
#define WEBSOCKET_DEFLATER_H

#include <vector>
#include <string>
#include <cstdint>

struct z_stream_s;

namespace ws {

struct DeflateConfig {
    int window_bits = 15;
    int mem_level = 8;
    int compression_level = 6; // 0-9, default 6
    bool server_no_context_takeover = false;
    bool client_no_context_takeover = false;
    int server_max_window_bits = 15;
    int client_max_window_bits = 15;
};

class WebSocketDeflater {
public:
    WebSocketDeflater();
    ~WebSocketDeflater();
    
    bool init(const DeflateConfig& config);
    
    bool compress(const uint8_t* data, size_t len, std::vector<uint8_t>& out);
    
    bool decompress(const uint8_t* data, size_t len, std::vector<uint8_t>& out);
    
    void reset();
    
    bool isEnabled() const { return enabled_; }
    
    const DeflateConfig& config() const { return config_; }
    
    static std::string buildExtensionHeader(const DeflateConfig& config);
    
    static bool parseExtensionHeader(const std::string& header, DeflateConfig& out_config);
    
private:
    bool enabled_ = false;
    DeflateConfig config_;
    z_stream_s* deflate_stream_ = nullptr;
    z_stream_s* inflate_stream_ = nullptr;
};

}

#endif
