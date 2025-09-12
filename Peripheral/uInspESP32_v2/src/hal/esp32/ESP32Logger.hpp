#pragma once
#include "hal/ILogger.hpp"
#include <Arduino.h>
#include <stdarg.h>

/**
 * @brief ESP32-specific implementation of the logger interface
 */
class ESP32Logger : public ILogger {
private:
    LogLevel current_level_ = LogLevel::INFO;
    static constexpr size_t MAX_LOG_LENGTH = 256;
    
public:
    void setLogLevel(LogLevel level) override {
        current_level_ = level;
    }
    
    LogLevel getLogLevel() const override {
        return current_level_;
    }
    
    int log(LogLevel level, const char* format, ...) override {
        if (level < current_level_) {
            return 0;
        }
        
        char buffer[MAX_LOG_LENGTH];
        va_list args;
        va_start(args, format);
        int result = vsnprintf(buffer, MAX_LOG_LENGTH, format, args);
        va_end(args);
        
        if (result < 0) {
            return result;
        }
        
        // Add log level prefix
        const char* level_str = "";
        switch (level) {
            case LogLevel::DEBUG: level_str = "[DEBUG] "; break;
            case LogLevel::INFO:  level_str = "[INFO] "; break;
            case LogLevel::WARNING:  level_str = "[WARN] "; break;
            case LogLevel::ERROR: level_str = "[ERROR] "; break;
            case LogLevel::FATAL: level_str = "[FATAL] "; break;
        }
        
        Serial.print(level_str);
        Serial.println(buffer);
        
        return result;
    }
    
    int vlog(LogLevel level, const char* format, va_list args) override {
        if (level < current_level_) {
            return 0;
        }
        
        char buffer[MAX_LOG_LENGTH];
        int result = vsnprintf(buffer, MAX_LOG_LENGTH, format, args);
        
        if (result < 0) {
            return result;
        }
        
        // Add log level prefix
        const char* level_str = "";
        switch (level) {
            case LogLevel::DEBUG: level_str = "[DEBUG] "; break;
            case LogLevel::INFO:  level_str = "[INFO] "; break;
            case LogLevel::WARNING:  level_str = "[WARN] "; break;
            case LogLevel::ERROR: level_str = "[ERROR] "; break;
            case LogLevel::FATAL: level_str = "[FATAL] "; break;
        }
        
        Serial.print(level_str);
        Serial.println(buffer);
        
        return result;
    }
    
    void debug(const char* format, ...) {
        if (LogLevel::DEBUG < current_level_) {
            return;
        }
        
        char buffer[MAX_LOG_LENGTH];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, MAX_LOG_LENGTH, format, args);
        va_end(args);
        
        Serial.print("[DEBUG] ");
        Serial.println(buffer);
    }
    
    void info(const char* format, ...) {
        if (LogLevel::INFO < current_level_) {
            return;
        }
        
        char buffer[MAX_LOG_LENGTH];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, MAX_LOG_LENGTH, format, args);
        va_end(args);
        
        Serial.print("[INFO] ");
        Serial.println(buffer);
    }
    
    void warn(const char* format, ...) {
        if (LogLevel::WARNING < current_level_) {
            return;
        }
        
        char buffer[MAX_LOG_LENGTH];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, MAX_LOG_LENGTH, format, args);
        va_end(args);
        
        Serial.print("[WARN] ");
        Serial.println(buffer);
    }
    
    void error(const char* format, ...) {
        if (LogLevel::ERROR < current_level_) {
            return;
        }
        
        char buffer[MAX_LOG_LENGTH];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, MAX_LOG_LENGTH, format, args);
        va_end(args);
        
        Serial.print("[ERROR] ");
        Serial.println(buffer);
    }
    
    void fatal(const char* format, ...) {
        if (LogLevel::FATAL < current_level_) {
            return;
        }
        
        char buffer[MAX_LOG_LENGTH];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, MAX_LOG_LENGTH, format, args);
        va_end(args);
        
        Serial.print("[FATAL] ");
        Serial.println(buffer);
    }
};