#include "SerialTransport.hpp"

SerialTransport::SerialTransport(HardwareSerial& serialPort) : serial(serialPort) {}

int SerialTransport::write(const uint8_t* data, size_t length) {
    return serial.write(data, length);
}

int SerialTransport::read(uint8_t* buffer, size_t maxLength) {
    size_t bytesRead = 0;
    while (serial.available() && bytesRead < maxLength) {
        buffer[bytesRead++] = serial.read();
    }
    return bytesRead;
}

int SerialTransport::available() {
    return serial.available();
}

void SerialTransport::flush() {
    serial.flush();
}

bool SerialTransport::isConnected() {
    return true; // Serial is always "connected" if initialized
}
