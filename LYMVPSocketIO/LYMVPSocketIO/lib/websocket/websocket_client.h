#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <map>
#include <deque>

#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread.h"

namespace ws {

enum class WebSocketState {
    Disconnected,
    Connecting,
    Handshaking,
    Connected,
    Closing,
    Closed
};

enum class CloseCode : uint16_t {
    Normal = 1000,
    GoingAway = 1001,
    ProtocolError = 1002,
    UnsupportedData = 1003,
    NoStatus = 1005,
    AbnormalClosure = 1006,
    InvalidPayloadData = 1007,
    PolicyViolation = 1008,
    MessageTooBig = 1009,
    MissingExtension = 1010,
    InternalError = 1011,
    ServiceRestart = 1012,
    TryAgainLater = 1013,
    TlsHandshake = 1015
};

class WebSocketClient {
public:
    using ConnectCallback = std::function<void()>;
    using DisconnectCallback = std::function<void(const std::string& reason, CloseCode code)>;
    using TextCallback = std::function<void(const std::string& text)>;
    using BinaryCallback = std::function<void(const std::vector<uint8_t>& data)>;
    using PingCallback = std::function<void(const std::vector<uint8_t>& data)>;
    using PongCallback = std::function<void(const std::vector<uint8_t>& data)>;
    using ErrorCallback = std::function<void(const std::string& error)>;

    WebSocketClient();
    ~WebSocketClient();

    void setURL(const std::string& url);
    void setProtocols(const std::vector<std::string>& protocols);
    void addHeader(const std::string& key, const std::string& value);

    void connect();
    void disconnect(CloseCode code = CloseCode::Normal, const std::string& reason = "");

    void sendText(const std::string& text);
    void sendBinary(const std::vector<uint8_t>& data);
    void sendPing(const std::vector<uint8_t>& data = {});
    void sendPong(const std::vector<uint8_t>& data = {});

    WebSocketState getState() const { return state_; }
    bool isConnected() const { return state_ == WebSocketState::Connected; }

    void setOnConnect(ConnectCallback cb) { on_connect_ = std::move(cb); }
    void setOnDisconnect(DisconnectCallback cb) { on_disconnect_ = std::move(cb); }
    void setOnText(TextCallback cb) { on_text_ = std::move(cb); }
    void setOnBinary(BinaryCallback cb) { on_binary_ = std::move(cb); }
    void setOnPing(PingCallback cb) { on_ping_ = std::move(cb); }
    void setOnPong(PongCallback cb) { on_pong_ = std::move(cb); }
    void setOnError(ErrorCallback cb) { on_error_ = std::move(cb); }

    void setSelfSignedSSL(bool enabled) { self_signed_ssl_ = enabled; }
    void setPingInterval(int seconds) { ping_interval_ = seconds; }

    void setExternalEventLoop(void* event_base);

    void* getEventBase() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    std::string url_;
    std::vector<std::string> protocols_;
    std::map<std::string, std::string> extra_headers_;
    std::atomic<WebSocketState> state_{WebSocketState::Disconnected};
    bool self_signed_ssl_ = false;
    int ping_interval_ = 30;

    ConnectCallback on_connect_;
    DisconnectCallback on_disconnect_;
    TextCallback on_text_;
    BinaryCallback on_binary_;
    PingCallback on_ping_;
    PongCallback on_pong_;
    ErrorCallback on_error_;

    void setState(WebSocketState state);
    void handleConnect();
    void handleDisconnect(const std::string& reason, CloseCode code);
    void handleText(const std::string& text);
    void handleBinary(const std::vector<uint8_t>& data);
    void handleError(const std::string& error);

    friend class WebSocketClientImpl;
};

}

#endif
