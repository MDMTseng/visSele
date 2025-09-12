#pragma once
#include "hal/IGpio.hpp"
#include <cstdint>
#include <functional>

/**
 * @brief Gate sensor class using HAL interfaces
 * 
 * This class detects objects passing through a gate sensor
 * using the HAL GPIO interface instead of direct Arduino calls.
 */
class GateSensor_HAL {
public:
    // Callback type for object detection
    using ObjectDetectedCallback = std::function<void(uint32_t, uint32_t, uint32_t)>;
    
private:
    IGpio& gpio_;
    uint8_t gate_pin_;
    bool sense_inverted_;
    uint32_t min_width_;
    uint32_t max_width_;
    uint8_t debounce_threshold_;
    uint8_t debounce_counter_;
    uint8_t cur_sense_;
    uint32_t start_pulse_;
    uint32_t end_pulse_;
    ObjectDetectedCallback object_detected_callback_;
    
public:
    /**
     * @brief Constructor
     * 
     * @param gpio GPIO interface to use
     */
    explicit GateSensor_HAL(IGpio& gpio) : gpio_(gpio) {}
    
    /**
     * @brief Initialize the gate sensor
     * 
     * @param gate_pin Pin number for the gate sensor
     * @param sense_inverted Whether the sensor logic is inverted
     * @param min_width Minimum width of a valid object (in pulses)
     * @param max_width Maximum width of a valid object (in pulses)
     * @param debounce_threshold Number of consecutive readings needed for debounce
     */
    void init(uint8_t gate_pin, bool sense_inverted, 
              uint32_t min_width, uint32_t max_width, 
              uint8_t debounce_threshold) {
        gate_pin_ = gate_pin;
        sense_inverted_ = sense_inverted;
        min_width_ = min_width;
        max_width_ = max_width;
        debounce_threshold_ = debounce_threshold;
        
        // Initialize pin using HAL
        gpio_.setPinMode(gate_pin_, IGpio::PinMode::PIN_INPUT_PULLUP);
        
        // Reset state
        reset();
    }
    
    /**
     * @brief Reset the sensor state
     */
    void reset() {
        cur_sense_ = 0;
        debounce_counter_ = debounce_threshold_;
        start_pulse_ = ~0U;  // Use max value to indicate uninitialized
        end_pulse_ = ~0U;
    }
    
    /**
     * @brief Process a timer tick
     * 
     * @param current_pulse_count Current pulse count
     */
    void tick(uint32_t current_pulse_count) {
        uint8_t new_sense = readSensor();
        
        // Process edge detection with debouncing
        if (processEdge(new_sense, current_pulse_count)) {
            // Edge was detected and processed
            cur_sense_ = new_sense;
        }
    }
    
    /**
     * @brief Set the callback for object detection
     * 
     * @param callback Callback function
     */
    void setObjectDetectedCallback(ObjectDetectedCallback callback) {
        object_detected_callback_ = callback;
    }
    
private:
    /**
     * @brief Read the sensor state
     * 
     * @return Sensor state (0 or 1)
     */
    uint8_t readSensor() const {
        // Use HAL GPIO interface instead of direct digitalRead
        IGpio::PinState state = gpio_.readPin(gate_pin_);
        uint8_t raw_sense = (state == IGpio::PinState::PIN_HIGH) ? 1 : 0;
        return sense_inverted_ ? !raw_sense : raw_sense;
    }
    
    /**
     * @brief Process an edge detection
     * 
     * @param new_sense New sensor state
     * @param current_pulse_count Current pulse count
     * @return true if edge was detected and processed, false otherwise
     */
    bool processEdge(uint8_t new_sense, uint32_t current_pulse_count) {
        bool on_sense_edge = false;
        
        if (cur_sense_) {
            // Currently HIGH
            if (!new_sense) {
                // Transition to LOW
                if (debounce_counter_ > 0) {
                    debounce_counter_--;
                    return false;
                }
                
                // Debounced falling edge
                end_pulse_ = current_pulse_count;
                debounce_counter_ = debounce_threshold_;
                on_sense_edge = true;
                
                // Check if we have a valid object
                if (start_pulse_ != ~0U) {
                    uint32_t width = end_pulse_ - start_pulse_;
                    
                    if (width >= min_width_ && width <= max_width_) {
                        // Valid object detected
                        if (object_detected_callback_) {
                            object_detected_callback_(start_pulse_, end_pulse_, width);
                        }
                    }
                }
            }
        } else {
            // Currently LOW
            if (new_sense) {
                // Transition to HIGH
                if (debounce_counter_ > 0) {
                    debounce_counter_--;
                    return false;
                }
                
                // Debounced rising edge
                start_pulse_ = current_pulse_count;
                debounce_counter_ = debounce_threshold_;
                on_sense_edge = true;
            }
        }
        
        return on_sense_edge;
    }
};