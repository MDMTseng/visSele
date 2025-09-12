#include "StepperController.hpp"

StepperController::StepperController(IStepperDriver& stepper_driver, 
                                     ITimerTickSource& timer_tick_source,
                                     IClock& clock)
    : stepper_driver_(stepper_driver),
      timer_tick_source_(timer_tick_source),
      clock_(clock),
      last_update_time_ms_(0) {
}

bool StepperController::init(uint8_t pulse_pin, uint8_t dir_pin, uint8_t enable_pin, 
                            bool enable_active_state, float initial_freq_hz,
                            float max_accel_hz_per_sec) {
    // Initialize the stepper driver
    if (!stepper_driver_.init(pulse_pin, dir_pin, enable_pin, enable_active_state)) {
        return false;
    }
    
    // Set initial frequency and acceleration
    current_freq_hz_ = initial_freq_hz;
    target_freq_hz_ = initial_freq_hz;
    max_accel_hz_per_sec_ = max_accel_hz_per_sec;
    
    // Initialize the timer tick source with the initial frequency
    if (!timer_tick_source_.init(initial_freq_hz)) {
        return false;
    }
    
    // Register the step function as the tick callback
    timer_tick_source_.registerTickCallback([this]() {
        this->stepper_driver_.step();
    });
    
    // Start the timer if the initial frequency is greater than 0
    if (initial_freq_hz > 0) {
        timer_tick_source_.start();
    }
    
    // Initialize the last update time
    last_update_time_ms_ = clock_.millis();
    
    return true;
}

void StepperController::setTargetFrequency(float target_freq_hz) {
    target_freq_hz_ = target_freq_hz;
    is_stable_ = (current_freq_hz_ == target_freq_hz_);
}

float StepperController::getCurrentFrequency() const {
    return current_freq_hz_;
}

bool StepperController::isStable() const {
    return is_stable_;
}

void StepperController::enableStepper(bool enabled) {
    stepper_driver_.setEnabled(enabled);
    
    // If disabling, also stop the timer
    if (!enabled) {
        timer_tick_source_.stop();
    } 
    // If enabling and we have a non-zero frequency, start the timer
    else if (current_freq_hz_ > 0) {
        timer_tick_source_.start();
    }
}

bool StepperController::isStepperEnabled() const {
    return stepper_driver_.isEnabled();
}

void StepperController::setDirection(IStepperDriver::Direction direction) {
    stepper_driver_.setDirection(direction);
}

void StepperController::update() {
    // Get the current time
    uint32_t current_time_ms = clock_.millis();
    
    // Calculate the time delta in seconds
    float delta_time_sec = (current_time_ms - last_update_time_ms_) / 1000.0f;
    last_update_time_ms_ = current_time_ms;
    
    // If the delta time is too large (e.g., after a long delay), cap it
    if (delta_time_sec > 0.1f) {
        delta_time_sec = 0.1f;
    }
    
    // Calculate the maximum frequency change for this update
    float max_freq_change = max_accel_hz_per_sec_ * delta_time_sec;
    
    // Update the current frequency based on the target
    if (current_freq_hz_ < target_freq_hz_) {
        // Accelerate
        current_freq_hz_ += max_freq_change;
        if (current_freq_hz_ > target_freq_hz_) {
            current_freq_hz_ = target_freq_hz_;
            is_stable_ = true;
        } else {
            is_stable_ = false;
        }
    } else if (current_freq_hz_ > target_freq_hz_) {
        // Decelerate
        current_freq_hz_ -= max_freq_change;
        if (current_freq_hz_ < target_freq_hz_) {
            current_freq_hz_ = target_freq_hz_;
            is_stable_ = true;
        } else {
            is_stable_ = false;
        }
    } else {
        // Already at target
        is_stable_ = true;
    }
    
    // Update the timer frequency
    if (current_freq_hz_ > 0) {
        timer_tick_source_.setFrequencyHz(current_freq_hz_);
        
        // Ensure the timer is running if the stepper is enabled
        if (stepper_driver_.isEnabled() && !timer_tick_source_.getFrequencyHz()) {
            timer_tick_source_.start();
        }
    } else {
        // Stop the timer if the frequency is 0
        timer_tick_source_.stop();
    }
}

void StepperController::setMaxAcceleration(float max_accel_hz_per_sec) {
    max_accel_hz_per_sec_ = max_accel_hz_per_sec;
}