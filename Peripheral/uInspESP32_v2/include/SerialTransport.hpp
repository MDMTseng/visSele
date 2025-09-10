#pragma once
#include "ITransport.hpp"
#include <HardwareSerial.h>

/**
 * @brief Serial transport implementation using Arduino HardwareSerial
 */
class SerialTransport : public ITransport {
private:
    HardwareSerial& serial;
    
public:
    explicit SerialTransport(HardwareSerial& serialPort);
    
    int write(const uint8_t* data, size_t length) override;
    int read(uint8_t* buffer, size_t maxLength) override;
    int available() override;
    void flush() override;
    bool isConnected() override;
};
