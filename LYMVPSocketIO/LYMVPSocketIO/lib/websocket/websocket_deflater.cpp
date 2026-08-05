#include "websocket_deflater.h"

#include <cstring>
#include <sstream>
#include <vector>

#include <zlib.h>

#include "rtc_base/logging.h"

namespace ws {

namespace {

constexpr size_t kDefaultBufferSize = 4096;

void* zlibAlloc(void* opaque, unsigned int items, unsigned int size) {
    (void)opaque;
    return std::malloc(items * size);
}

void zlibFree(void* opaque, void* address) {
    (void)opaque;
    std::free(address);
}

}

WebSocketDeflater::WebSocketDeflater() = default;

WebSocketDeflater::~WebSocketDeflater() {
    reset();
}

bool WebSocketDeflater::init(const DeflateConfig& config) {
    if (enabled_) {
        reset();
    }
    
    config_ = config;
    
    deflate_stream_ = static_cast<z_stream_s*>(std::calloc(1, sizeof(z_stream_s)));
    if (!deflate_stream_) {
        RTC_LOG(LS_ERROR) << "Failed to allocate deflate stream";
        return false;
    }
    deflate_stream_->zalloc = zlibAlloc;
    deflate_stream_->zfree = zlibFree;
    deflate_stream_->opaque = nullptr;
    
    int ret = deflateInit2(deflate_stream_,
                           config.compression_level,
                           Z_DEFLATED,
                           -config.window_bits, // negative = raw deflate
                           config.mem_level,
                           Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) {
        RTC_LOG(LS_ERROR) << "deflateInit2 failed: " << ret;
        std::free(deflate_stream_);
        deflate_stream_ = nullptr;
        return false;
    }
    
    inflate_stream_ = static_cast<z_stream_s*>(std::calloc(1, sizeof(z_stream_s)));
    if (!inflate_stream_) {
        RTC_LOG(LS_ERROR) << "Failed to allocate inflate stream";
        deflateEnd(deflate_stream_);
        std::free(deflate_stream_);
        deflate_stream_ = nullptr;
        return false;
    }
    inflate_stream_->zalloc = zlibAlloc;
    inflate_stream_->zfree = zlibFree;
    inflate_stream_->opaque = nullptr;
    
    ret = inflateInit2(inflate_stream_, -config.window_bits);
    if (ret != Z_OK) {
        RTC_LOG(LS_ERROR) << "inflateInit2 failed: " << ret;
        deflateEnd(deflate_stream_);
        std::free(deflate_stream_);
        deflate_stream_ = nullptr;
        std::free(inflate_stream_);
        inflate_stream_ = nullptr;
        return false;
    }
    
    enabled_ = true;
    RTC_LOG(LS_INFO) << "WebSocket permessage-deflate enabled, window_bits=" << config.window_bits
                     << " compression_level=" << config.compression_level;
    return true;
}

bool WebSocketDeflater::compress(const uint8_t* data, size_t len, std::vector<uint8_t>& out) {
    if (!enabled_ || !deflate_stream_) return false;
    
    out.clear();
    
    if (len == 0) {
        return true;
    }
    
    deflate_stream_->next_in = const_cast<uint8_t*>(data);
    deflate_stream_->avail_in = static_cast<uInt>(len);
    
    size_t total_out = 0;
    
    do {
        out.resize(total_out + kDefaultBufferSize);
        deflate_stream_->next_out = out.data() + total_out;
        deflate_stream_->avail_out = kDefaultBufferSize;
        
        int ret = deflate(deflate_stream_, Z_SYNC_FLUSH);
        if (ret == Z_STREAM_ERROR) {
            RTC_LOG(LS_ERROR) << "deflate failed: Z_STREAM_ERROR";
            return false;
        }
        
        size_t produced = kDefaultBufferSize - deflate_stream_->avail_out;
        total_out += produced;
        
    } while (deflate_stream_->avail_out == 0);
    
    out.resize(total_out);
    
    if (out.size() >= 4) {
        const uint8_t tail[4] = {0x00, 0x00, 0xFF, 0xFF};
        if (std::memcmp(out.data() + out.size() - 4, tail, 4) == 0) {
            out.resize(out.size() - 4);
        }
    }
    
    return true;
}

bool WebSocketDeflater::decompress(const uint8_t* data, size_t len, std::vector<uint8_t>& out) {
    if (!enabled_ || !inflate_stream_) return false;
    
    out.clear();
    
    if (len == 0) {
        return true;
    }
    
    std::vector<uint8_t> input_buf(len + 4);
    std::memcpy(input_buf.data(), data, len);
    input_buf[len] = 0x00;
    input_buf[len + 1] = 0x00;
    input_buf[len + 2] = 0xFF;
    input_buf[len + 3] = 0xFF;
    
    inflate_stream_->next_in = input_buf.data();
    inflate_stream_->avail_in = static_cast<uInt>(len + 4);
    
    size_t total_out = 0;
    
    do {
        out.resize(total_out + kDefaultBufferSize);
        inflate_stream_->next_out = out.data() + total_out;
        inflate_stream_->avail_out = kDefaultBufferSize;
        
        int ret = inflate(inflate_stream_, Z_SYNC_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            RTC_LOG(LS_ERROR) << "inflate failed: " << ret;
            return false;
        }
        
        size_t produced = kDefaultBufferSize - inflate_stream_->avail_out;
        total_out += produced;
        
    } while (inflate_stream_->avail_out == 0);
    
    out.resize(total_out);
    return true;
}

void WebSocketDeflater::reset() {
    if (deflate_stream_) {
        deflateEnd(deflate_stream_);
        std::free(deflate_stream_);
        deflate_stream_ = nullptr;
    }
    if (inflate_stream_) {
        inflateEnd(inflate_stream_);
        std::free(inflate_stream_);
        inflate_stream_ = nullptr;
    }
    enabled_ = false;
}

std::string WebSocketDeflater::buildExtensionHeader(const DeflateConfig& config) {
    std::ostringstream oss;
    oss << "permessage-deflate";
    
    if (config.client_no_context_takeover) {
        oss << "; client_no_context_takeover";
    }
    if (config.server_no_context_takeover) {
        oss << "; server_no_context_takeover";
    }
    if (config.client_max_window_bits < 15) {
        oss << "; client_max_window_bits=" << config.client_max_window_bits;
    }
    if (config.server_max_window_bits < 15) {
        oss << "; server_max_window_bits=" << config.server_max_window_bits;
    }
    
    return oss.str();
}

static void trim(std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        s.clear();
        return;
    }
    size_t end = s.find_last_not_of(" \t\r\n");
    s = s.substr(start, end - start + 1);
}

static std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream iss(s);
    while (std::getline(iss, token, delim)) {
        trim(token);
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

bool WebSocketDeflater::parseExtensionHeader(const std::string& header, DeflateConfig& out_config) {
    auto extensions = split(header, ',');
    
    bool found = false;
    
    for (const auto& ext : extensions) {
        auto params = split(ext, ';');
        if (params.empty()) continue;
        
        if (params[0] != "permessage-deflate") continue;
        
        found = true;
        
        for (size_t i = 1; i < params.size(); i++) {
            const auto& param = params[i];
            size_t eq_pos = param.find('=');
            
            if (eq_pos == std::string::npos) {
                if (param == "client_no_context_takeover") {
                    out_config.client_no_context_takeover = true;
                } else if (param == "server_no_context_takeover") {
                    out_config.server_no_context_takeover = true;
                }
            } else {
                std::string key = param.substr(0, eq_pos);
                std::string val = param.substr(eq_pos + 1);
                trim(key);
                trim(val);
                
                if (key == "client_max_window_bits") {
                    int bits = std::atoi(val.c_str());
                    if (bits >= 8 && bits <= 15) {
                        out_config.client_max_window_bits = bits;
                    }
                } else if (key == "server_max_window_bits") {
                    int bits = std::atoi(val.c_str());
                    if (bits >= 8 && bits <= 15) {
                        out_config.server_max_window_bits = bits;
                    }
                }
            }
        }
        
        break;
    }
    
    return found;
}

}
