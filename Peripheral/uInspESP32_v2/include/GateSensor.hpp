#ifndef GATE_SENSOR_HPP
#define GATE_SENSOR_HPP

#include <cstdint>
#include <functional>

/**
 * @brief Gate sensor abstraction for object detection
 * 
 * This module handles gate sensing logic including debouncing, pulse width filtering,
 * and object detection callbacks. It provides a clean interface that can be called
 * from ISR context and supports callback-based object detection.
 */
class GateSensor {
public:
    /**
     * @brief Callback function type for object detection
     * @param start_pulse Start pulse count when object entered gate
     * @param end_pulse End pulse count when object left gate  
     * @param middle_pulse Middle pulse count (center of object)
     * @param pulse_width Width of the pulse in step counts
     */
    using ObjectDetectedCallback = std::function<void(uint32_t start_pulse, uint32_t end_pulse, uint32_t middle_pulse, uint32_t pulse_width)>;

    /**
     * @brief Initialize the gate sensor
     * @param gate_pin GPIO pin for gate sensor input
     * @param sense_inverted Whether to invert the sensor reading (true for active low)
     * @param min_width Minimum pulse width to consider valid (step counts)
     * @param max_width Maximum pulse width to consider valid (step counts)
     * @param debounce_threshold Debounce threshold for edge detection
     */
    void init(uint8_t gate_pin, bool sense_inverted = true, 
              uint32_t min_width = 0, uint32_t max_width = 1000, 
              uint8_t debounce_threshold = 1);

    /**
     * @brief Reset the gate sensor state
     */
    void reset();

    /**
     * @brief Process gate sensor tick (call from ISR)
     * @param current_pulse_count Current system pulse count
     */
    void tick(uint32_t current_pulse_count);

    /**
     * @brief Register callback for object detection
     * @param callback Function to call when object is detected
     */
    void setObjectDetectedCallback(ObjectDetectedCallback callback);

    /**
     * @brief Get current sensor state
     * @return Current sensor reading (after inversion if configured)
     */
    uint8_t getCurrentSense() const { return cur_sense_; }

    /**
     * @brief Get current debounce counter
     * @return Current debounce counter value
     */
    uint8_t getDebounceCounter() const { return debounce_counter_; }

    /**
     * @brief Check if sensor is currently detecting an object
     * @return true if object is currently being detected
     */
    bool isObjectDetected() const { return cur_sense_ == 1; }

private:
    // Configuration
    uint8_t gate_pin_;
    bool sense_inverted_;
    uint32_t min_width_;
    uint32_t max_width_;
    uint8_t debounce_threshold_;

    // State
    uint8_t cur_sense_;
    uint8_t debounce_counter_;
    uint32_t start_pulse_;
    uint32_t end_pulse_;

    // Callback
    ObjectDetectedCallback object_detected_callback_;

    /**
     * @brief Read the physical sensor pin
     * @return Raw sensor reading
     */
    uint8_t readSensor() const;

    /**
     * @brief Process edge detection with debouncing
     * @param new_sense New sensor reading
     * @param current_pulse_count Current system pulse count
     * @return true if edge was detected and processed
     */
    bool processEdge(uint8_t new_sense, uint32_t current_pulse_count);

    /**
     * @brief Handle completed pulse detection
     * @param current_pulse_count Current system pulse count
     */
    void handlePulseComplete(uint32_t current_pulse_count);
};

#endif // GATE_SENSOR_HPP
