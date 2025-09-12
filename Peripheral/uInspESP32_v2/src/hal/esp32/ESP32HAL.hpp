#pragma once
#include "hal/HAL.hpp"
#include "ESP32Gpio.hpp"
#include "ESP32TimerTickSource.hpp"
#include "ESP32StepperDriver.hpp"
#include "ESP32Clock.hpp"
#include "ESP32Logger.hpp"
#include "ESP32Lock.hpp"
#include "SerialTransport.hpp"

/**
 * @brief ESP32-specific implementation of the HAL interface
 */
class ESP32HAL : public IHAL {
private:
    ESP32Gpio gpio_;
    ESP32TimerTickSource timer_tick_source_;
    ESP32StepperDriver stepper_driver_;
    ESP32Clock clock_;
    ESP32Logger logger_;
    
public:
    ESP32HAL() : stepper_driver_(gpio_) {}
    
    IGpio& gpio() override {
        return gpio_;
    }
    
    ITimerTickSource& timerTickSource() override {
        return timer_tick_source_;
    }
    
    IStepperDriver& stepperDriver() override {
        return stepper_driver_;
    }
    
    IClock& clock() override {
        return clock_;
    }
    
    ILogger& logger() override {
        return logger_;
    }
    
    ILock* createLock() override {
        return new ESP32Lock();
    }
    
    ITransport* createTransport(const char* transport_type) override {
        if (strcmp(transport_type, "serial") == 0) {
            return new SerialTransport(Serial);
        }
        return nullptr;
    }
};

// Global HAL instance
static ESP32HAL g_hal;

// Implementation of the global HAL getter function
IHAL& getHAL() {
    return g_hal;
}