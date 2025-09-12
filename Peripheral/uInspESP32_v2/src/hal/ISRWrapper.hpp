#pragma once
#include "hal/HAL.hpp"
#include "hal/ITimerTickSource.hpp"
#include "GateSensor.hpp"
#include "Scheduler.hpp"

/**
 * @brief Platform-neutral ISR wrapper
 * 
 * This class provides a thin wrapper around the ISR functionality,
 * ensuring that the ISR code is minimal and platform-neutral.
 */
class ISRWrapper {
private:
    static ISRWrapper* instance_;
    ITimerTickSource& timer_tick_source_;
    GateSensor& gate_sensor_;
    uint32_t step_count_;
    
    // Callback function to be registered with the timer tick source
    void onTimerTick() {
        // Increment step count
        step_count_++;
        
        // Process gate sensor
        gate_sensor_.tick(step_count_);
        
        // Run scheduled actions
        Run_ACTS(step_count_);
    }
    
public:
    ISRWrapper(ITimerTickSource& timer_tick_source, GateSensor& gate_sensor)
        : timer_tick_source_(timer_tick_source),
          gate_sensor_(gate_sensor),
          step_count_(0) {
        instance_ = this;
    }
    
    ~ISRWrapper() {
        if (instance_ == this) {
            instance_ = nullptr;
        }
    }
    
    /**
     * @brief Initialize the ISR wrapper
     * 
     * Registers the callback with the timer tick source
     */
    void init() {
        // Register the callback with the timer tick source
        timer_tick_source_.registerTickCallback([this]() {
            this->onTimerTick();
        });
    }
    
    /**
     * @brief Get the current step count
     * 
     * @return Current step count
     */
    uint32_t getStepCount() const {
        return step_count_;
    }
    
    /**
     * @brief Get the singleton instance
     * 
     * @return Reference to the singleton instance
     */
    static ISRWrapper* getInstance() {
        return instance_;
    }
};

// Initialize static member
ISRWrapper* ISRWrapper::instance_ = nullptr;