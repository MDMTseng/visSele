#pragma once
#include <cstdint>

/**
 * @brief Clock abstraction interface
 * 
 * This interface provides a platform-agnostic way to handle time-related
 * functions (micros, millis, delay) without direct Arduino dependencies.
 */
class IClock {
public:
    virtual ~IClock() = default;
    
    /**
     * @brief Get the number of microseconds since the device started
     * @return Microseconds since start
     */
    virtual uint64_t micros() = 0;
    
    /**
     * @brief Get the number of milliseconds since the device started
     * @return Milliseconds since start
     */
    virtual uint32_t millis() = 0;
    
    /**
     * @brief Delay execution for a specified number of milliseconds
     * @param ms Number of milliseconds to delay
     */
    virtual void delayMs(uint32_t ms) = 0;
    
    /**
     * @brief Delay execution for a specified number of microseconds
     * @param us Number of microseconds to delay
     */
    virtual void delayUs(uint32_t us) = 0;
};