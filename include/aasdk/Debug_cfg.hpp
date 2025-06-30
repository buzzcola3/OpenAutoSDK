#ifndef OPENAUTO_SDK_DEBUG_CFG_HPP
#define OPENAUTO_SDK_DEBUG_CFG_HPP

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

// 1. Define Log Severity Levels
enum class LogSeverity {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    NONE // Special level to disable all logging
};

// 2. Set the global minimum log level.
// Only logs with severity >= SDK_LOG_LEVEL will be compiled.
// Example: Set to LogSeverity::INFO to see INFO, WARNING, and ERROR messages.
#ifndef SDK_LOG_LEVEL
#define SDK_LOG_LEVEL LogSeverity::DEBUG
#endif

// Helper to get the string representation of a severity level
inline const char* toString(LogSeverity severity) {
    switch (severity) {
        case LogSeverity::DEBUG:   return "DEBUG";
        case LogSeverity::INFO:    return "INFO";
        case LogSeverity::WARNING: return "WARN";
        case LogSeverity::ERROR:   return "ERROR";
        default:                   return "";
    }
}

// The core logging function
template<typename... Args>
void logMessage(LogSeverity severity, const char* file, int line, Args&&... args) {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::stringstream ss;
    // C++11 compatible way to build the message string
    using expander = int[];
    (void)expander{0, (void(ss << std::forward<Args>(args)), 0)...};

    std::cerr << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S")
              << '.' << std::setw(3) << std::setfill('0') << ms.count()
              << " [" << std::setw(5) << std::setfill(' ') << toString(severity) << "]"
              << " [" << file << ":" << line << "] "
              << ss.str() << std::endl;
}

// 3. The main logging macro
// Checks for per-file disable flag `SDK_LOG_DISABLED` first.
// Then checks if the message severity is high enough to be logged.
#ifdef SDK_LOG_DISABLED
    #define SDK_LOG(severity, ...) (void)0
#else
    #define SDK_LOG(severity, ...) \
        do { \
            if constexpr (static_cast<LogSeverity>(severity) >= SDK_LOG_LEVEL && SDK_LOG_LEVEL != LogSeverity::NONE) { \
                logMessage(static_cast<LogSeverity>(severity), __FILE__, __LINE__, __VA_ARGS__); \
            } \
        } while (false)
#endif

// 4. Convenience macros for each level
#define SDK_LOG_DEBUG(...)   SDK_LOG(LogSeverity::DEBUG, __VA_ARGS__)
#define SDK_LOG_INFO(...)    SDK_LOG(LogSeverity::INFO, __VA_ARGS__)
#define SDK_LOG_WARNING(...) SDK_LOG(LogSeverity::WARNING, __VA_ARGS__)
#define SDK_LOG_ERROR(...)   SDK_LOG(LogSeverity::ERROR, __VA_ARGS__)

#endif // OPENAUTO_SDK_DEBUG_CFG_HPP