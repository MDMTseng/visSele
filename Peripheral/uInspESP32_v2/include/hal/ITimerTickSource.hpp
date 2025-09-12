#pragma once
#include <cstdint>
#include <functional>

/**
 * @brief Timer tick source abstraction interface
 * 
 * This interface provides a platform-agnostic way to handle timer interrupts
 * for generating precise timing pulses, particularly for stepper motor control.
 */
class ITimerTickSource {
public:
    /**
     * @brief Callback type for timer tick ISR
     */
    using TickCallback = std::function<void()>;
    
    virtual ~ITimerTickSource() = default;
    
    /**
     * @brief Initialize the timer
     * @param frequency_hz Initial frequency in Hz
     * @return true if successful, false otherwise
     */
    virtual bool init(float frequency_hz) = 0;
    
    /**
     * @brief Start the timer
     * @return true if successful, false otherwise
     */
    virtual bool start() = 0;
    
    /**
     * @brief Stop the timer
     * @return true if successful, false otherwise
     */
    virtual bool stop() = 0;
    
    /**
     * @brief Set the timer frequency
     * @param frequency_hz Frequency in Hz
     * @return true if successful, false otherwise
     */
    virtual bool setFrequencyHz(float frequency_hz) = 0;
    
    /**
     * @brief Get the current timer frequency
     * @return Current frequency in Hz
     */
    virtual float getFrequencyHz() const = 0;
    
    /**
     * @brief Register a callback function to be called on each timer tick
     * @param callback Function to call on each timer tick
     */
    virtual void registerTickCallback(TickCallback callback) = 0;
};