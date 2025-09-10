#pragma once
#include <cstdint>
#include <cstddef>

/**
 * @brief Transport abstraction interface for communication protocols
 * 
 * This interface provides a platform-agnostic way to handle different
 * communication transports (Serial, WebSocket, Network, etc.)
 */
class ITransport {
public:
    virtual ~ITransport() = default;
    
    /**
     * @brief Write data to the transport
     * @param data Pointer to data buffer
     * @param length Number of bytes to write
     * @return Number of bytes written, or -1 on error
     */
    virtual int write(const uint8_t* data, size_t length) = 0;
    
    /**
     * @brief Read data from the transport
     * @param buffer Buffer to store read data
     * @param maxLength Maximum number of bytes to read
     * @return Number of bytes read, or -1 on error
     */
    virtual int read(uint8_t* buffer, size_t maxLength) = 0;
    
    /**
     * @brief Check if data is available for reading
     * @return Number of bytes available, or 0 if none
     */
    virtual int available() = 0;
    
    /**
     * @brief Flush any pending output
     */
    virtual void flush() = 0;
    
    /**
     * @brief Check if transport is connected/ready
     * @return true if connected, false otherwise
     */
    virtual bool isConnected() = 0;
};
