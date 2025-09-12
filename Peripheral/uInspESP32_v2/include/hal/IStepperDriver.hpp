#pragma once
#include <cstdint>

/**
 * @brief Stepper driver abstraction interface
 * 
 * This interface provides a platform-agnostic way to control stepper motors
 * without direct hardware dependencies.
 */
class IStepperDriver {
public:
    /**
     * @brief Direction enumeration
     */
    enum class Direction {
        CLOCKWISE,
        COUNTER_CLOCKWISE
    };
    
    virtual ~IStepperDriver() = default;
    
    /**
     * @brief Initialize the stepper driver
     * @param pulse_pin Pin connected to the step/pulse input of the driver
     * @param dir_pin Pin connected to the direction input of the driver
     * @param enable_pin Pin connected to the enable input of the driver
     * @param enable_active_state Logic level that enables the driver (true=HIGH, false=LOW)
     * @return true if successful, false otherwise
     */
    virtual bool init(uint8_t pulse_pin, uint8_t dir_pin, uint8_t enable_pin, bool enable_active_state) = 0;
    
    /**
     * @brief Enable or disable the stepper driver
     * @param enabled true to enable, false to disable
     */
    virtual void setEnabled(bool enabled) = 0;
    
    /**
     * @brief Check if the stepper driver is enabled
     * @return true if enabled, false if disabled
     */
    virtual bool isEnabled() const = 0;
    
    /**
     * @brief Set the direction of rotation
     * @param dir Direction (CLOCKWISE or COUNTER_CLOCKWISE)
     */
    virtual void setDirection(Direction dir) = 0;
    
    /**
     * @brief Generate a step pulse
     * 
     * This method should be called from the timer ISR to generate
     * step pulses at the appropriate frequency.
     */
    virtual void step() = 0;
};