#pragma once
#include "hal/IStepperDriver.hpp"
#include "ESP32Gpio.hpp"

/**
 * @brief ESP32-specific implementation of the stepper driver interface
 */
class ESP32StepperDriver : public IStepperDriver {
private:
    ESP32Gpio& gpio_;
    uint8_t pulse_pin_ = 0;
    uint8_t dir_pin_ = 0;
    uint8_t enable_pin_ = 0;
    bool enable_active_state_ = true;
    bool enabled_ = false;
    Direction direction_ = Direction::CLOCKWISE;
    bool pulse_state_ = false;
    
public:
    explicit ESP32StepperDriver(ESP32Gpio& gpio) : gpio_(gpio) {}
    
    bool init(uint8_t pulse_pin, uint8_t dir_pin, uint8_t enable_pin, bool enable_active_state) override {
        pulse_pin_ = pulse_pin;
        dir_pin_ = dir_pin;
        enable_pin_ = enable_pin;
        enable_active_state_ = enable_active_state;
        
        // Configure pins
        gpio_.setPinMode(pulse_pin_, IGpio::PinMode::PIN_OUTPUT);
        gpio_.setPinMode(dir_pin_, IGpio::PinMode::PIN_OUTPUT);
        gpio_.setPinMode(enable_pin_, IGpio::PinMode::PIN_OUTPUT);
        
        // Initialize outputs
        gpio_.writePin(pulse_pin_, IGpio::PinState::PIN_LOW);
        gpio_.writePin(dir_pin_, IGpio::PinState::PIN_LOW);
        gpio_.writePin(enable_pin_, enable_active_state_ ? IGpio::PinState::PIN_LOW : IGpio::PinState::PIN_HIGH);
        
        enabled_ = false;
        pulse_state_ = false;
        
        return true;
    }
    
    void setEnabled(bool enabled) override {
        enabled_ = enabled;
        gpio_.writePin(enable_pin_, 
                     enabled_ ? 
                     (enable_active_state_ ? IGpio::PinState::PIN_HIGH : IGpio::PinState::PIN_LOW) : 
                     (enable_active_state_ ? IGpio::PinState::PIN_LOW : IGpio::PinState::PIN_HIGH));
    }
    
    bool isEnabled() const override {
        return enabled_;
    }
    
    void setDirection(Direction dir) override {
        direction_ = dir;
        gpio_.writePin(dir_pin_, 
                     direction_ == Direction::CLOCKWISE ? 
                     IGpio::PinState::PIN_LOW : IGpio::PinState::PIN_HIGH);
    }
    
    void step() override {
        // Toggle pulse pin to create a step
        pulse_state_ = !pulse_state_;
        gpio_.writePin(pulse_pin_, 
                     pulse_state_ ? IGpio::PinState::PIN_HIGH : IGpio::PinState::PIN_LOW);
    }
};