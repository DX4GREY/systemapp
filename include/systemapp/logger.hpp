#pragma once
// SystemApp - core/Logger
// Simple thread-safe leveled logger with optional file sink.

#include <string>
#include <mutex>
#include <memory>
#include <fstream>
#include <sstream>
#include <cstdio>

namespace systemapp::core {

enum class LogLevel { VERBOSE = 0, DEBUG = 1, INFO = 2, WARN = 3, ERROR = 4, SILENT = 5 };

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void set_level(LogLevel lvl) { level_ = lvl; }
    void set_log_file(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        file_ = std::make_unique<std::ofstream>(path, std::ios::app);
    }
    void set_color_enabled(bool enabled) { color_enabled_ = enabled; }

    template <typename... Args>
    void log(LogLevel lvl, const std::string& fmt) {
        if (lvl < level_) return;
        std::lock_guard<std::mutex> lock(mutex_);
        std::string prefix = level_prefix(lvl);
        std::string line = prefix + fmt;
        FILE* out = (lvl == LogLevel::ERROR || lvl == LogLevel::WARN) ? stderr : stdout;
        std::fprintf(out, "%s\n", line.c_str());
        if (file_ && file_->is_open()) {
            (*file_) << plain_prefix(lvl) << fmt << "\n";
            file_->flush();
        }
    }

    void verbose(const std::string& m) { log(LogLevel::VERBOSE, m); }
    void debug(const std::string& m)   { log(LogLevel::DEBUG, m); }
    void info(const std::string& m)    { log(LogLevel::INFO, m); }
    void warn(const std::string& m)    { log(LogLevel::WARN, m); }
    void error(const std::string& m)   { log(LogLevel::ERROR, m); }

private:
    Logger() = default;
    std::string level_prefix(LogLevel lvl) const {
        if (!color_enabled_) return "[" + level_name(lvl) + "] ";
        switch (lvl) {
            case LogLevel::VERBOSE: return "\x1b[90m[VERBOSE]\x1b[0m ";
            case LogLevel::DEBUG:   return "\x1b[36m[DEBUG]\x1b[0m ";
            case LogLevel::INFO:    return "\x1b[32m[INFO]\x1b[0m ";
            case LogLevel::WARN:    return "\x1b[33m[WARN]\x1b[0m ";
            case LogLevel::ERROR:   return "\x1b[31m[ERROR]\x1b[0m ";
            default: return "";
        }
    }
    std::string plain_prefix(LogLevel lvl) const { return "[" + level_name(lvl) + "] "; }
    std::string level_name(LogLevel lvl) const {
        switch (lvl) {
            case LogLevel::VERBOSE: return "VERBOSE";
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARN: return "WARN";
            case LogLevel::ERROR: return "ERROR";
            default: return "SILENT";
        }
    }

    LogLevel level_ = LogLevel::INFO;
    bool color_enabled_ = true;
    std::unique_ptr<std::ofstream> file_;
    std::mutex mutex_;
};

} // namespace systemapp::core
