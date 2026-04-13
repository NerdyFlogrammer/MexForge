#ifndef MEXFORGE_CORE_LOGGER_HPP
#define MEXFORGE_CORE_LOGGER_HPP

#include "mex.hpp"
#include "MatlabDataArray.hpp"

#include <string>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>
#include <memory>
#include <mutex>

namespace mexforge {

// ============================================================================
// Log levels
// ============================================================================

enum class LogLevel : int {
    Trace = 0,
    Debug = 1,
    Info  = 2,
    Warn  = 3,
    Error = 4,
    Fatal = 5,
    Off   = 6
};

inline const char* log_level_str(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
        default:              return "OFF";
    }
}

inline LogLevel log_level_from_str(const std::string& s) {
    if (s == "trace") return LogLevel::Trace;
    if (s == "debug") return LogLevel::Debug;
    if (s == "info")  return LogLevel::Info;
    if (s == "warn")  return LogLevel::Warn;
    if (s == "error") return LogLevel::Error;
    if (s == "fatal") return LogLevel::Fatal;
    if (s == "off")   return LogLevel::Off;
    return LogLevel::Info;
}

// ============================================================================
// Compile-time minimum log level (strip calls below this from binary)
// Define MEXFORGE_MIN_LOG_LEVEL before including to override.
// Default: Trace (all levels active)
// ============================================================================

#ifndef MEXFORGE_MIN_LOG_LEVEL
#define MEXFORGE_MIN_LOG_LEVEL 0
#endif

// ============================================================================
// Logger
// ============================================================================

class Logger {
public:
    using EnginePtr = std::shared_ptr<matlab::engine::MATLABEngine>;

    explicit Logger(EnginePtr engine)
        : engine_(std::move(engine))
        , runtimeLevel_(LogLevel::Info)
        , matlabEnabled_(true)
        , fileEnabled_(false)
        , buffered_(false)
    {}

    // ---- Configuration (callable from MATLAB via control commands) ----

    void setLevel(LogLevel level) { runtimeLevel_ = level; }
    void setLevel(const std::string& level) { runtimeLevel_ = log_level_from_str(level); }
    LogLevel getLevel() const { return runtimeLevel_; }

    void enableMatlab(bool enable) { matlabEnabled_ = enable; }
    bool isMatlabEnabled() const { return matlabEnabled_; }

    void enableBuffered(bool enable) {
        buffered_ = enable;
        if (!enable) flush();
    }

    void setLogFile(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (fileStream_.is_open()) fileStream_.close();
        if (!path.empty()) {
            fileStream_.open(path, std::ios::out | std::ios::app);
            fileEnabled_ = fileStream_.is_open();
        } else {
            fileEnabled_ = false;
        }
    }

    void closeLogFile() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (fileStream_.is_open()) fileStream_.close();
        fileEnabled_ = false;
    }

    // ---- Logging methods ----

    template<LogLevel Level>
    void log(const std::string& msg) {
        // Compile-time elimination
        if constexpr (static_cast<int>(Level) < MEXFORGE_MIN_LOG_LEVEL) {
            return;
        }

        // Runtime level check (~1ns)
        if (Level < runtimeLevel_) return;

        auto formatted = formatMessage(Level, msg);

        std::lock_guard<std::mutex> lock(mutex_);

        // File output (fast, no MATLAB roundtrip)
        if (fileEnabled_ && fileStream_.is_open()) {
            fileStream_ << formatted << '\n';
            fileStream_.flush();
        }

        // MATLAB output
        if (matlabEnabled_) {
            if (buffered_) {
                buffer_.push_back(std::move(formatted));
            } else {
                writeToMatlab(formatted);
            }
        }
    }

    // Variadic log with simple string concatenation (no fmt dependency)
    template<LogLevel Level, typename... Args>
    void log(const std::string& msg, const Args&... args) {
        if constexpr (static_cast<int>(Level) < MEXFORGE_MIN_LOG_LEVEL) return;
        if (Level < runtimeLevel_) return;

        std::ostringstream oss;
        oss << msg;
        ((oss << args), ...);
        log<Level>(oss.str());
    }

    // Convenience methods
    template<typename... Args>
    void trace(const std::string& msg, const Args&... args) { log<LogLevel::Trace>(msg, args...); }

    template<typename... Args>
    void debug(const std::string& msg, const Args&... args) { log<LogLevel::Debug>(msg, args...); }

    template<typename... Args>
    void info(const std::string& msg, const Args&... args) { log<LogLevel::Info>(msg, args...); }

    template<typename... Args>
    void warn(const std::string& msg, const Args&... args) { log<LogLevel::Warn>(msg, args...); }

    template<typename... Args>
    void error(const std::string& msg, const Args&... args) { log<LogLevel::Error>(msg, args...); }

    template<typename... Args>
    void fatal(const std::string& msg, const Args&... args) { log<LogLevel::Fatal>(msg, args...); }

    // ---- Buffer management ----

    void flush() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (buffer_.empty()) return;

        std::ostringstream combined;
        for (const auto& line : buffer_) {
            combined << line << '\n';
        }
        buffer_.clear();

        writeToMatlab(combined.str());
    }

    size_t bufferSize() const { return buffer_.size(); }

private:
    std::string formatMessage(LogLevel level, const std::string& msg) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time), "%H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count()
            << " [" << log_level_str(level) << "] "
            << msg;
        return oss.str();
    }

    void writeToMatlab(const std::string& msg) {
        if (!engine_) return;
        try {
            matlab::data::ArrayFactory factory;
            engine_->feval(u"fprintf",
                {factory.createScalar(msg + "\n")},
                {});
        } catch (...) {
            // Suppress errors in logging to avoid cascading failures
        }
    }

    EnginePtr engine_;
    LogLevel runtimeLevel_;
    bool matlabEnabled_;
    bool fileEnabled_;
    bool buffered_;
    std::ofstream fileStream_;
    std::vector<std::string> buffer_;
    std::mutex mutex_;
};

// ============================================================================
// Convenience macros with compile-time elimination
// ============================================================================

#define MEXFORGE_LOG_TRACE(logger, ...) (logger).trace(__VA_ARGS__)
#define MEXFORGE_LOG_DEBUG(logger, ...) (logger).debug(__VA_ARGS__)
#define MEXFORGE_LOG_INFO(logger, ...)  (logger).info(__VA_ARGS__)
#define MEXFORGE_LOG_WARN(logger, ...)  (logger).warn(__VA_ARGS__)
#define MEXFORGE_LOG_ERROR(logger, ...) (logger).error(__VA_ARGS__)
#define MEXFORGE_LOG_FATAL(logger, ...) (logger).fatal(__VA_ARGS__)

} // namespace mexforge

#endif // MEXFORGE_CORE_LOGGER_HPP
