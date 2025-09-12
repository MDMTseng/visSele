#pragma once

/**
 * @brief Hardware Abstraction Layer (HAL) interfaces
 * 
 * This file includes all HAL interfaces to provide a unified access point.
 */

#include "IGpio.hpp"
#include "ITimerTickSource.hpp"
#include "IStepperDriver.hpp"
#include "IClock.hpp"
#include "ILogger.hpp"
#include "ILock.hpp"
#include "ITransport.hpp"

/**
 * @brief HAL factory interface for creating platform-specific implementations
 */
class IHAL {
public:
    virtual ~IHAL() = default;
    
    /**
     * @brief Get the GPIO interface
     * @return Reference to the GPIO interface
     */
    virtual IGpio& gpio() = 0;
    
    /**
     * @brief Get the timer tick source interface
     * @return Reference to the timer tick source interface
     */
    virtual ITimerTickSource& timerTickSource() = 0;
    
    /**
     * @brief Get the stepper driver interface
     * @return Reference to the stepper driver interface
     */
    virtual IStepperDriver& stepperDriver() = 0;
    
    /**
     * @brief Get the clock interface
     * @return Reference to the clock interface
     */
    virtual IClock& clock() = 0;
    
    /**
     * @brief Get the logger interface
     * @return Reference to the logger interface
     */
    virtual ILogger& logger() = 0;
    
    /**
     * @brief Create a lock object
     * @return Pointer to a new lock instance (caller takes ownership)
     */
    virtual ILock* createLock() = 0;
    
    /**
     * @brief Create a transport object for the specified type
     * @param transport_type Type of transport to create ("serial", "websocket", etc.)
     * @return Pointer to a new transport instance (caller takes ownership)
     */
    virtual ITransport* createTransport(const char* transport_type) = 0;
};

/**
 * @brief Get the global HAL instance
 * @return Reference to the HAL instance
 */
IHAL& getHAL();