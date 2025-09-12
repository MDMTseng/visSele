#pragma once
#include <cstdint>
#include <cstdarg>

/**
 * @brief Logger abstraction interface
 * 
 * This interface provides a platform-agnostic way to handle logging
 * without direct dependencies on specific output mechanisms.
 */
class ILogger {
public:
    /**
     * @brief Log level enumeration
     */
    enum class LogLevel {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        FATAL
    };
    
    virtual ~ILogger() = default;
    
    /**
     * @brief Log a formatted message with variable arguments
     * @param level Log level
     * @param format Format string (printf style)
     * @param ... Variable arguments
     * @return Number of characters written, or negative value on error
     */
    virtual int log(LogLevel level, const char* format, ...) = 0;
    
    /**
     * @brief Log a formatted message with va_list
     * @param level Log level
     * @param format Format string (printf style)
     * @param args Variable argument list
     * @return Number of characters written, or negative value on error
     */
    virtual int vlog(LogLevel level, const char* format, va_list args) = 0;
    
    /**
     * @brief Set the minimum log level to output
     * @param level Minimum level to log
     */
    virtual void setLogLevel(LogLevel level) = 0;
    
    /**
     * @brief Get the current minimum log level
     * @return Current minimum log level
     */
    virtual LogLevel getLogLevel() const = 0;
};