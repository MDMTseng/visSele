#pragma once
#include <cstdint>

/**
 * @brief GPIO abstraction interface
 * 
 * This interface provides a platform-agnostic way to handle GPIO operations
 * (pinMode, digitalWrite, digitalRead) without direct Arduino dependencies.
 */
class IGpio {
public:
    /**
     * @brief Pin mode definitions
     */
    enum class PinMode {
        PIN_INPUT,
        PIN_OUTPUT,
        PIN_INPUT_PULLUP,
        PIN_INPUT_PULLDOWN
    };
    
    /**
     * @brief Pin state definitions
     */
    enum class PinState {
        PIN_LOW,
        PIN_HIGH
    };
    
    virtual ~IGpio() = default;
    
    /**
     * @brief Configure a pin's mode
     * @param pin Pin number
     * @param mode Mode to set (INPUT, OUTPUT, etc)
     */
    virtual void setPinMode(uint8_t pin, PinMode mode) = 0;
    
    /**
     * @brief Write a digital value to a pin
     * @param pin Pin number
     * @param state State to set (HIGH or LOW)
     */
    virtual void writePin(uint8_t pin, PinState state) = 0;
    
    /**
     * @brief Read a digital value from a pin
     * @param pin Pin number
     * @return Current state of the pin (HIGH or LOW)
     */
    virtual PinState readPin(uint8_t pin) = 0;
};