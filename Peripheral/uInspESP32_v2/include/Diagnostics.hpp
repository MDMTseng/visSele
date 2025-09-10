// Diagnostics.hpp - Error logging and system diagnostics (Stage S4.2)
#pragma once
#include <stdint.h>
#include "RingBuf.hpp"
#include <ArduinoJson.h>

// Forward declaration - GEN_ERROR_CODE will be defined in main.hpp
enum class GEN_ERROR_CODE;

// Error history management
class Diagnostics {
public:
    static constexpr int ERROR_HISTORY_CAPACITY = 20;
    
    // Initialize diagnostics system
    static void init();
    
    // Push an error code to the history
    static void pushError(GEN_ERROR_CODE code);
    
    // Get the current error history size
    static int getErrorCount();
    
    // Get error at specific index (0 = oldest, size-1 = newest)
    static GEN_ERROR_CODE getError(int index);
    
    // Clear all error history
    static void clearErrors();
    
    // Export error history to JSON array
    static void exportErrorsToJson(JsonArray jsonArray);
    
    // Check if there are any errors
    static bool hasErrors();
    
    // Get the most recent error
    static GEN_ERROR_CODE getLatestError();
    
private:
    // Error history ring buffer
    static GEN_ERROR_CODE errorBuffer_[ERROR_HISTORY_CAPACITY];
    static RingBuf<GEN_ERROR_CODE, uint8_t> errorHistory_;
};

// Legacy compatibility function (will be removed in later stages)
extern void ERROR_LOG_PUSH(GEN_ERROR_CODE code);
