#include "GateSensor.hpp"
#include "main.hpp"
#include <Arduino.h>

void GateSensor::init(uint8_t gate_pin, bool sense_inverted, 
                      uint32_t min_width, uint32_t max_width, 
                      uint8_t debounce_threshold) {
    gate_pin_ = gate_pin;
    sense_inverted_ = sense_inverted;
    min_width_ = min_width;
    max_width_ = max_width;
    debounce_threshold_ = debounce_threshold;
    
    // Initialize pin
    pinMode(gate_pin_, INPUT_PULLUP);
    
    // Reset state
    reset();
}

void GateSensor::reset() {
    cur_sense_ = 0;
    debounce_counter_ = debounce_threshold_;
    start_pulse_ = ~0U;  // Use max value to indicate uninitialized
    end_pulse_ = ~0U;
}

void GateSensor::tick(uint32_t current_pulse_count) {
    uint8_t new_sense = readSensor();
    
    // Process edge detection with debouncing
    if (processEdge(new_sense, current_pulse_count)) {
        // Edge was detected and processed
        cur_sense_ = new_sense;
    }
}

void GateSensor::setObjectDetectedCallback(ObjectDetectedCallback callback) {
    object_detected_callback_ = callback;
}

uint8_t GateSensor::readSensor() const {
    uint8_t raw_sense = digitalRead(gate_pin_);
    return sense_inverted_ ? !raw_sense : raw_sense;
}

bool GateSensor::processEdge(uint8_t new_sense, uint32_t current_pulse_count) {
    bool on_sense_edge = false;
    
    if (cur_sense_) {
        // Currently HIGH
        if (!new_sense) {
            // Transition to LOW
            debounce_counter_--;
            if (debounce_counter_ == 0) {
                on_sense_edge = true;
                debounce_counter_ = debounce_threshold_;
            }
        } else {
            // Still HIGH, reset debounce counter
            debounce_counter_ = debounce_threshold_;
            end_pulse_ = current_pulse_count;
        }
    } else {
        // Currently LOW
        if (new_sense) {
            // Transition to HIGH
            debounce_counter_--;
            if (debounce_counter_ == 0) {
                on_sense_edge = true;
                debounce_counter_ = debounce_threshold_;
            }
        } else {
            // Still LOW, reset debounce counter
            debounce_counter_ = debounce_threshold_;
            start_pulse_ = current_pulse_count;
        }
    }
    
    if (on_sense_edge) {
        if (!new_sense) {
            // Pulse completed (HIGH to LOW transition)
            handlePulseComplete(current_pulse_count);
        } else {
            // Pulse started (LOW to HIGH transition)
            end_pulse_ = current_pulse_count;
        }
    }
    
    return on_sense_edge;
}

void GateSensor::handlePulseComplete(uint32_t current_pulse_count) {
    end_pulse_ = current_pulse_count;
    
    // Check if we have valid start and end pulses
    if (start_pulse_ != ~0U && end_pulse_ != ~0U) {
        uint32_t diff = end_pulse_ - start_pulse_;
        
        // Check if pulse width is within valid range
        if (diff > min_width_ && diff < max_width_) {
            uint32_t middle_pulse = start_pulse_ + (diff >> 1);
            
            // Call the object detected callback if registered
            if (object_detected_callback_) {
                object_detected_callback_(start_pulse_, end_pulse_, middle_pulse, diff);
            }
        }
        // Note: Skipped pulses (outside width range) are silently ignored
        // This matches the original behavior
    }
    
    // Reset for next pulse
    start_pulse_ = current_pulse_count;
}
