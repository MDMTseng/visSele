#pragma once
#include "hal/ITimerTickSource.hpp"
#include <Arduino.h>

/**
 * @brief ESP32-specific implementation of the timer tick source interface
 */
class ESP32TimerTickSource : public ITimerTickSource {
private:
    hw_timer_t* timer_ = nullptr;
    float frequency_hz_ = 0.0f;
    TickCallback callback_ = nullptr;
    static ESP32TimerTickSource* instance_;
    
    // Static C-style ISR wrapper that calls the instance method
    static void IRAM_ATTR timerISR();

public:
    ESP32TimerTickSource() {
        instance_ = this;
    }
    
    ~ESP32TimerTickSource() {
        if (timer_) {
            timerAlarmDisable(timer_);
            timerDetachInterrupt(timer_);
            timerEnd(timer_);
        }
        if (instance_ == this) {
            instance_ = nullptr;
        }
    }
    
    bool init(float frequency_hz) override {
        // Use timer 0 with 80MHz/10 = 8MHz base clock (divider 10)
        // This gives a resolution of 0.125 µs
        timer_ = timerBegin(0, 10, true);
        if (!timer_) {
            return false;
        }
        
        timerAttachInterrupt(timer_, &timerISR, true);
        frequency_hz_ = frequency_hz;
        
        // Calculate timer period based on frequency
        uint64_t period_us = frequency_hz > 0 ? 1000000 / frequency_hz : 1000000;
        timerAlarmWrite(timer_, period_us * 8, true); // 8MHz = 8 ticks per µs
        
        return true;
    }
    
    bool start() override {
        if (!timer_) {
            return false;
        }
        timerAlarmEnable(timer_);
        return true;
    }
    
    bool stop() override {
        if (!timer_) {
            return false;
        }
        timerAlarmDisable(timer_);
        return true;
    }
    
    bool setFrequencyHz(float frequency_hz) override {
        if (!timer_ || frequency_hz <= 0) {
            return false;
        }
        
        frequency_hz_ = frequency_hz;
        uint64_t period_us = 1000000 / frequency_hz;
        timerAlarmWrite(timer_, period_us * 8, true); // 8MHz = 8 ticks per µs
        
        return true;
    }
    
    float getFrequencyHz() const override {
        return frequency_hz_;
    }
    
    void registerTickCallback(TickCallback callback) override {
        callback_ = callback;
    }
};