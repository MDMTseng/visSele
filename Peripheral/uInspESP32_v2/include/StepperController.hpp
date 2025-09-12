#pragma once
#include "hal/IStepperDriver.hpp"
#include "hal/ITimerTickSource.hpp"
#include "hal/IClock.hpp"

/**
 * @brief Controller for stepper motor with frequency ramping capabilities
 * 
 * This class handles the frequency ramping logic for the stepper motor,
 * providing smooth acceleration and deceleration to target frequencies.
 */
class StepperController {
public:
    /**
     * @brief Construct a new Stepper Controller object
     * 
     * @param stepper_driver Reference to the stepper driver interface
     * @param timer_tick_source Reference to the timer tick source interface
     * @param clock Reference to the clock interface
     */
    StepperController(IStepperDriver& stepper_driver, 
                      ITimerTickSource& timer_tick_source,
                      IClock& clock);
    
    /**
     * @brief Initialize the stepper controller
     * 
     * @param pulse_pin Pin connected to the step/pulse input of the driver
     * @param dir_pin Pin connected to the direction input of the driver
     * @param enable_pin Pin connected to the enable input of the driver
     * @param enable_active_state Logic level that enables the driver (true=HIGH, false=LOW)
     * @param initial_freq_hz Initial frequency in Hz
     * @param max_accel_hz_per_sec Maximum acceleration in Hz per second
     * @return true if successful, false otherwise
     */
    bool init(uint8_t pulse_pin, uint8_t dir_pin, uint8_t enable_pin, 
              bool enable_active_state, float initial_freq_hz = 0.0f,
              float max_accel_hz_per_sec = 1000.0f);
    
    /**
     * @brief Set the target frequency
     * 
     * The controller will ramp up or down to this frequency at the
     * configured acceleration rate.
     * 
     * @param target_freq_hz Target frequency in Hz
     */
    void setTargetFrequency(float target_freq_hz);
    
    /**
     * @brief Get the current frequency
     * 
     * @return Current frequency in Hz
     */
    float getCurrentFrequency() const;
    
    /**
     * @brief Check if the frequency is stable (at target)
     * 
     * @return true if at target frequency, false otherwise
     */
    bool isStable() const;
    
    /**
     * @brief Enable or disable the stepper driver
     * 
     * @param enabled true to enable, false to disable
     */
    void enableStepper(bool enabled);
    
    /**
     * @brief Check if the stepper is enabled
     * 
     * @return true if enabled, false if disabled
     */
    bool isStepperEnabled() const;
    
    /**
     * @brief Set the direction of rotation
     * 
     * @param direction Direction (CLOCKWISE or COUNTER_CLOCKWISE)
     */
    void setDirection(IStepperDriver::Direction direction);
    
    /**
     * @brief Update the frequency ramp
     * 
     * This method should be called regularly from the main loop to
     * update the frequency ramping logic.
     */
    void update();
    
    /**
     * @brief Set the maximum acceleration rate
     * 
     * @param max_accel_hz_per_sec Maximum acceleration in Hz per second
     */
    void setMaxAcceleration(float max_accel_hz_per_sec);
    
private:
    IStepperDriver& stepper_driver_;
    ITimerTickSource& timer_tick_source_;
    IClock& clock_;
    
    float current_freq_hz_ = 0.0f;
    float target_freq_hz_ = 0.0f;
    float max_accel_hz_per_sec_ = 1000.0f;
    uint32_t last_update_time_ms_ = 0;
    bool is_stable_ = true;
};