#pragma once
#include "hal/IClock.hpp"
#include <Arduino.h>

/**
 * @brief ESP32-specific implementation of the clock interface
 */
class ESP32Clock : public IClock {
public:
    uint32_t millis() override {
        return ::millis();
    }
    
    uint64_t micros() override {
        return static_cast<uint64_t>(::micros());
    }
    
    void delayMs(uint32_t ms) override {
        ::delay(ms);
    }
    
    void delayUs(uint32_t us) override {
        ::delayMicroseconds(us);
    }
};