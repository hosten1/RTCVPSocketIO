#ifndef ENGINEIO_HTTP_CLIENT_H
#define ENGINEIO_HTTP_CLIENT_H

#include <string>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace engineio {

enum class HttpMethod {
    GET,
    POST
};

struct HttpResponse {
    int status_code;
    std::string body;
    std::map<std::string, std::string> headers;
    bool success;
    std::string error_message;
    
    HttpResponse() : status_code(0), success(false) {}
};

class HttpClient {
public:
    static std::shared_ptr<HttpClient> Create();
    
    ~HttpClient();
    
    void set_self_signed_ssl(bool enabled);
    
    HttpResponse get(const std::string& url,
                     const std::map<std::string, std::string>& headers,
                     int timeout_sec = 30);
    
    HttpResponse post(const std::string& url,
                      const std::map<std::string, std::string>& headers,
                      const std::string& body,
                      const std::string& content_type,
                      int timeout_sec = 30);
    
private:
    HttpClient();
    
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    HttpResponse do_request(HttpMethod method,
                            const std::string& url,
                            const std::map<std::string, std::string>& headers,
                            const std::string& body,
                            const std::string& content_type,
                            int timeout_sec);
};

} // namespace engineio

#endif // ENGINEIO_HTTP_CLIENT_H
