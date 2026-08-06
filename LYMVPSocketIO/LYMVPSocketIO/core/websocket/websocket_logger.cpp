#include "websocket_logger.h"

#include "rtc_base/synchronization/mutex.h"

namespace ws {

namespace {
webrtc::Mutex g_logger_mutex;
WebSocketLogger* g_logger_instance = nullptr;
}

WebSocketLogger& WebSocketLogger::Instance() {
    webrtc::MutexLock lock(&g_logger_mutex);
    if (!g_logger_instance) {
        g_logger_instance = new WebSocketLogger();
    }
    return *g_logger_instance;
}

WebSocketLogger::WebSocketLogger() {
    rtc::LogMessage::LogTimestamps(true);
    rtc::LogMessage::LogThreads(true);
}

WebSocketLogger::~WebSocketLogger() {
    Shutdown();
}

bool WebSocketLogger::InitFileLog(const std::string& log_dir,
                                  const std::string& file_prefix,
                                  size_t max_file_size,
                                  size_t num_files) {
    if (file_sink_) {
        rtc::LogMessage::RemoveLogToStream(file_sink_.get());
        file_sink_.reset();
    }

    file_sink_ = std::make_unique<rtc::FileRotatingLogSink>(
        log_dir, file_prefix, max_file_size, num_files);

    if (!file_sink_->Init()) {
        file_sink_.reset();
        return false;
    }

    rtc::LogMessage::AddLogToStream(
        file_sink_.get(),
        static_cast<rtc::LoggingSeverity>(current_level_));

    return true;
}

void WebSocketLogger::SetLogLevel(LogLevel level) {
    current_level_ = level;
    auto sev = static_cast<rtc::LoggingSeverity>(level);

    if (console_enabled_) {
        rtc::LogMessage::LogToDebug(sev);
    }

    if (file_sink_) {
        rtc::LogMessage::RemoveLogToStream(file_sink_.get());
        rtc::LogMessage::AddLogToStream(file_sink_.get(), sev);
    }
}

LogLevel WebSocketLogger::GetLogLevel() const {
    return current_level_;
}

void WebSocketLogger::EnableConsoleLog(bool enable) {
    console_enabled_ = enable;
    if (enable) {
        rtc::LogMessage::LogToDebug(
            static_cast<rtc::LoggingSeverity>(current_level_));
    } else {
        rtc::LogMessage::LogToDebug(rtc::LS_NONE);
    }
}

void WebSocketLogger::EnableLogThreads(bool enable) {
    rtc::LogMessage::LogThreads(enable);
}

void WebSocketLogger::EnableLogTimestamps(bool enable) {
    rtc::LogMessage::LogTimestamps(enable);
}

void WebSocketLogger::Shutdown() {
    if (file_sink_) {
        rtc::LogMessage::RemoveLogToStream(file_sink_.get());
        file_sink_.reset();
    }
}

}
