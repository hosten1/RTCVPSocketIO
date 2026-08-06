#ifndef WEBSOCKET_LOGGER_H
#define WEBSOCKET_LOGGER_H

#include <string>
#include <memory>

#include "rtc_base/logging.h"
#include "rtc_base/log_sinks.h"

namespace ws {

enum class LogLevel {
    Verbose = rtc::LS_VERBOSE,
    Info = rtc::LS_INFO,
    Warning = rtc::LS_WARNING,
    Error = rtc::LS_ERROR,
    None = rtc::LS_NONE
};

class WebSocketLogger {
public:
    static WebSocketLogger& Instance();

    bool InitFileLog(const std::string& log_dir,
                     const std::string& file_prefix = "websocket",
                     size_t max_file_size = 10 * 1024 * 1024,
                     size_t num_files = 5);

    void SetLogLevel(LogLevel level);
    LogLevel GetLogLevel() const;

    void EnableConsoleLog(bool enable);
    void EnableLogThreads(bool enable);
    void EnableLogTimestamps(bool enable);

    void Shutdown();

private:
    WebSocketLogger();
    ~WebSocketLogger();

    WebSocketLogger(const WebSocketLogger&) = delete;
    WebSocketLogger& operator=(const WebSocketLogger&) = delete;

    std::unique_ptr<rtc::FileRotatingLogSink> file_sink_;
    bool console_enabled_ = true;
    LogLevel current_level_ = LogLevel::Info;
};

}

#endif
