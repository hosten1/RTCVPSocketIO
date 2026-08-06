#include "core/engineio/http_client.h"
#include "rtc_base/logging.h"

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/keyvalq_struct.h>
#include <event2/util.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <cstring>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <atomic>

namespace engineio {

struct HttpClient::Impl {
    std::atomic<bool> self_signed_ssl_{false};
};

namespace {

struct RequestData {
    HttpResponse* response;
    bool done;
};

static void on_request_done(struct evhttp_request* req, void* ctx) {
    RequestData* data = static_cast<RequestData*>(ctx);
    data->done = true;
    
    if (!req || evhttp_request_get_response_code(req) == 0) {
        data->response->success = false;
        data->response->error_message = "request failed";
        return;
    }
    
    data->response->status_code = evhttp_request_get_response_code(req);
    data->response->success = (data->response->status_code >= 200 && 
                               data->response->status_code < 300);
    
    struct evbuffer* input = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(input);
    if (len > 0) {
        std::vector<char> buf(len);
        evbuffer_remove(input, buf.data(), len);
        data->response->body.assign(buf.data(), len);
    }
    
    struct evkeyvalq* headers = evhttp_request_get_input_headers(req);
    if (headers) {
        struct evkeyval* header;
        for (header = headers->tqh_first; header;
             header = header->next.tqe_next) {
            data->response->headers[header->key] = header->value;
        }
    }
}

} // namespace

std::shared_ptr<HttpClient> HttpClient::Create() {
    return std::shared_ptr<HttpClient>(new HttpClient());
}

HttpClient::HttpClient() : impl_(new Impl()) {
}

HttpClient::~HttpClient() = default;

void HttpClient::set_self_signed_ssl(bool enabled) {
    impl_->self_signed_ssl_.store(enabled);
}

HttpResponse HttpClient::get(const std::string& url,
                             const std::map<std::string, std::string>& headers,
                             int timeout_sec) {
    return do_request(HttpMethod::GET, url, headers, "", "", timeout_sec);
}

HttpResponse HttpClient::post(const std::string& url,
                              const std::map<std::string, std::string>& headers,
                              const std::string& body,
                              const std::string& content_type,
                              int timeout_sec) {
    return do_request(HttpMethod::POST, url, headers, body, content_type, timeout_sec);
}

HttpResponse HttpClient::do_request(HttpMethod method,
                                    const std::string& url,
                                    const std::map<std::string, std::string>& headers,
                                    const std::string& body,
                                    const std::string& content_type,
                                    int timeout_sec) {
    HttpResponse response;
    
    RTC_LOG(LS_INFO) << "[HttpClient] " << (method == HttpMethod::GET ? "GET" : "POST") 
                    << " request to: " << url
                    << ", timeout=" << timeout_sec << "s";
    
    struct evhttp_uri* http_uri = evhttp_uri_parse(url.c_str());
    if (!http_uri) {
        RTC_LOG(LS_ERROR) << "[HttpClient] Malformed URL: " << url;
        response.success = false;
        response.error_message = "malformed url: " + url;
        return response;
    }
    
    const char* scheme = evhttp_uri_get_scheme(http_uri);
    const char* host = evhttp_uri_get_host(http_uri);
    int port = evhttp_uri_get_port(http_uri);
    const char* path = evhttp_uri_get_path(http_uri);
    const char* query = evhttp_uri_get_query(http_uri);
    
    bool is_https = false;
    if (scheme) {
        is_https = (strcasecmp(scheme, "https") == 0);
    }
    
    if (port == -1) {
        port = is_https ? 443 : 80;
    }
    
    std::string uri_str;
    if (path && strlen(path) > 0) {
        uri_str = path;
    } else {
        uri_str = "/";
    }
    if (query && strlen(query) > 0) {
        uri_str += "?";
        uri_str += query;
    }
    
    RTC_LOG(LS_VERBOSE) << "[HttpClient] Host=" << host << ", port=" << port 
                       << ", https=" << (is_https ? "yes" : "no")
                       << ", uri=" << uri_str;
    
    struct event_base* base = event_base_new();
    if (!base) {
        RTC_LOG(LS_ERROR) << "[HttpClient] Failed to create event_base";
        response.success = false;
        response.error_message = "failed to create event_base";
        evhttp_uri_free(http_uri);
        return response;
    }
    
    SSL_CTX* ssl_ctx = nullptr;
    SSL* ssl = nullptr;
    struct bufferevent* bev = nullptr;
    
    if (is_https) {
        ssl_ctx = SSL_CTX_new(TLS_client_method());
        if (!ssl_ctx) {
            RTC_LOG(LS_ERROR) << "[HttpClient] Failed to create SSL_CTX";
            response.success = false;
            response.error_message = "failed to create SSL_CTX";
            event_base_free(base);
            evhttp_uri_free(http_uri);
            return response;
        }
        
        SSL_CTX_set_options(ssl_ctx,
            SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION);
        
        ssl = SSL_new(ssl_ctx);
        if (!ssl) {
            RTC_LOG(LS_ERROR) << "[HttpClient] Failed to create SSL";
            response.success = false;
            response.error_message = "failed to create SSL";
            SSL_CTX_free(ssl_ctx);
            event_base_free(base);
            evhttp_uri_free(http_uri);
            return response;
        }
        
        if (impl_->self_signed_ssl_.load()) {
            RTC_LOG(LS_INFO) << "[HttpClient] Self-signed SSL enabled, skipping verification";
            SSL_set_verify(ssl, SSL_VERIFY_NONE, nullptr);
        }
        
        SSL_set_tlsext_host_name(ssl, host);
        
        bev = bufferevent_openssl_socket_new(
            base, -1, ssl,
            BUFFEREVENT_SSL_CONNECTING,
            BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
    } else {
        bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
    }
    
    if (!bev) {
        RTC_LOG(LS_ERROR) << "[HttpClient] Failed to create bufferevent";
        response.success = false;
        response.error_message = "failed to create bufferevent";
        if (ssl) SSL_free(ssl);
        if (ssl_ctx) SSL_CTX_free(ssl_ctx);
        event_base_free(base);
        evhttp_uri_free(http_uri);
        return response;
    }
    
    if (is_https) {
        bufferevent_openssl_set_allow_dirty_shutdown(bev, 1);
    }
    
    struct evhttp_connection* evcon = evhttp_connection_base_bufferevent_new(
        base, nullptr, bev, host, port);
    
    if (!evcon) {
        RTC_LOG(LS_ERROR) << "[HttpClient] Failed to create http connection";
        response.success = false;
        response.error_message = "failed to create http connection";
        if (ssl) SSL_free(ssl);
        if (ssl_ctx) SSL_CTX_free(ssl_ctx);
        event_base_free(base);
        evhttp_uri_free(http_uri);
        return response;
    }
    
    if (timeout_sec > 0) {
        evhttp_connection_set_timeout(evcon, timeout_sec);
    }
    
    RequestData req_data;
    req_data.response = &response;
    req_data.done = false;
    
    struct evhttp_request* req = evhttp_request_new(on_request_done, &req_data);
    if (!req) {
        RTC_LOG(LS_ERROR) << "[HttpClient] Failed to create http request";
        response.success = false;
        response.error_message = "failed to create http request";
        evhttp_connection_free(evcon);
        if (ssl_ctx) SSL_CTX_free(ssl_ctx);
        event_base_free(base);
        evhttp_uri_free(http_uri);
        return response;
    }
    
    struct evkeyvalq* output_headers = evhttp_request_get_output_headers(req);
    evhttp_add_header(output_headers, "Host", host);
    
    for (const auto& kv : headers) {
        evhttp_add_header(output_headers, kv.first.c_str(), kv.second.c_str());
    }
    
    enum evhttp_cmd_type cmd_type = (method == HttpMethod::POST)
        ? EVHTTP_REQ_POST : EVHTTP_REQ_GET;
    
    if (method == HttpMethod::POST && !body.empty()) {
        struct evbuffer* output = evhttp_request_get_output_buffer(req);
        evbuffer_add(output, body.data(), body.size());
        
        if (!content_type.empty()) {
            evhttp_add_header(output_headers, "Content-Type", content_type.c_str());
        }
        
        char cl_buf[32];
        snprintf(cl_buf, sizeof(cl_buf), "%zu", body.size());
        evhttp_add_header(output_headers, "Content-Length", cl_buf);
        
        RTC_LOG(LS_VERBOSE) << "[HttpClient] POST body size=" << body.size()
                           << ", content_type=" << content_type;
    }
    
    int ret = evhttp_make_request(evcon, req, cmd_type, uri_str.c_str());
    
    if (ret != 0) {
        RTC_LOG(LS_ERROR) << "[HttpClient] evhttp_make_request failed, ret=" << ret;
        response.success = false;
        response.error_message = "evhttp_make_request failed";
        evhttp_connection_free(evcon);
        if (ssl_ctx) SSL_CTX_free(ssl_ctx);
        event_base_free(base);
        evhttp_uri_free(http_uri);
        return response;
    }
    
    while (!req_data.done) {
        if (event_base_loop(base, EVLOOP_ONCE) < 0) {
            break;
        }
    }
    
    evhttp_connection_free(evcon);
    if (ssl_ctx) SSL_CTX_free(ssl_ctx);
    event_base_free(base);
    evhttp_uri_free(http_uri);
    
    RTC_LOG(LS_INFO) << "[HttpClient] Request completed: success=" << response.success
                    << ", status_code=" << response.status_code
                    << ", body_length=" << response.body.length()
                    << ", error=" << response.error_message;
    
    return response;
}

} // namespace engineio
