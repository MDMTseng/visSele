#pragma once
#include "hal/IGpio.hpp"
#include <Arduino.h>

/**
 * @brief ESP32-specific implementation of the GPIO interface
 */
class ESP32Gpio : public IGpio {
public:
    void setPinMode(uint8_t pin, PinMode mode) override {
        switch (mode) {
            case PinMode::PIN_INPUT:
                pinMode(pin, INPUT);
                break;
            case PinMode::PIN_OUTPUT:
                pinMode(pin, OUTPUT);
                break;
            case PinMode::PIN_INPUT_PULLUP:
                pinMode(pin, INPUT_PULLUP);
                break;
            case PinMode::PIN_INPUT_PULLDOWN:
                pinMode(pin, INPUT_PULLDOWN);
                break;
        }
    }
    
    void writePin(uint8_t pin, PinState state) override {
        digitalWrite(pin, state == PinState::PIN_HIGH ? HIGH : LOW);
    }
    
    PinState readPin(uint8_t pin) override {
        return digitalRead(pin) == HIGH ? PinState::PIN_HIGH : PinState::PIN_LOW;
    }
};