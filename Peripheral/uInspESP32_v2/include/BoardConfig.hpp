#pragma once

/**
 * @brief Board configuration and pin mappings
 * 
 * This header provides platform-specific pin mappings and board configuration
 * for different hardware platforms (ESP32, STM32, etc.).
 */

#include "PlatformConfig.hpp"

namespace BoardConfig {

// Platform-specific pin mappings
#if defined(TARGET_ESP32)
    // ESP32 Pin Mappings
    namespace ESP32Config {
        // Stepper motor pins
        constexpr int STEPPER_PLS_PIN = 22;
        constexpr int STEPPER_DIR_PIN = 23;
        constexpr int STEPPER_EN_PIN = 13;
        constexpr int STEPPER_EN_ACTIVATION = 0;  // 0 = active low
        
        // Output pins (lights, cameras, selectors)
        constexpr int PIN_O_L1A = 16;      // Light for Camera 1
        constexpr int PIN_O_CAM1 = 17;     // Camera 1 trigger
        constexpr int PIN_O_L2A = 18;      // Light for Camera 2
        constexpr int PIN_O_CAM2 = 19;     // Camera 2 trigger
        constexpr int PIN_O_SEL1 = 25;     // Selector 1
        constexpr int PIN_O_SEL2 = 26;     // Selector 2
        constexpr int PIN_O_SEL3 = 32;     // Selector 3
        
        // Input pins
        constexpr int PIN_I_GATE = 27;     // Gate sensor
        constexpr int FEEDER_PIN = 21;     // Feeder control
        
        // System pins
        constexpr int PIN_LED = 2;         // Status LED
        constexpr int PIN_TRIG_595 = 5;    // 595 shift register trigger
    }

#elif defined(TARGET_STM32)
    // STM32 Pin Mappings (placeholder - would need actual hardware definition)
    namespace STM32Config {
        // Stepper motor pins (example mapping for STM32F4)
        constexpr int STEPPER_PLS_PIN = 8;   // PA8
        constexpr int STEPPER_DIR_PIN = 9;   // PA9
        constexpr int STEPPER_EN_PIN = 10;   // PA10
        constexpr int STEPPER_EN_ACTIVATION = 0;  // 0 = active low
        
        // Output pins (example mapping)
        constexpr int PIN_O_L1A = 0;       // PA0 - Light for Camera 1
        constexpr int PIN_O_CAM1 = 1;      // PA1 - Camera 1 trigger
        constexpr int PIN_O_L2A = 2;       // PA2 - Light for Camera 2
        constexpr int PIN_O_CAM2 = 3;      // PA3 - Camera 2 trigger
        constexpr int PIN_O_SEL1 = 4;      // PA4 - Selector 1
        constexpr int PIN_O_SEL2 = 5;      // PA5 - Selector 2
        constexpr int PIN_O_SEL3 = 6;      // PA6 - Selector 3
        
        // Input pins
        constexpr int PIN_I_GATE = 7;      // PA7 - Gate sensor
        constexpr int FEEDER_PIN = 15;     // PB15 - Feeder control
        
        // System pins
        constexpr int PIN_LED = 13;        // PA13 - Status LED (built-in)
        constexpr int PIN_TRIG_595 = 12;   // PA12 - 595 shift register trigger
        
        // UART pins for communication
        constexpr int UART_TX_PIN = 2;     // PA2 - UART TX
        constexpr int UART_RX_PIN = 3;     // PA3 - UART RX
    }

#endif

// Platform-agnostic pin access
#if defined(TARGET_ESP32)
    // Use ESP32 pin mappings
    using namespace ESP32Config;
#elif defined(TARGET_STM32)
    // Use STM32 pin mappings
    using namespace STM32Config;
#endif

// Mechanical configuration (platform-independent)
namespace Mechanical {
    // Platform dimensions
    constexpr double PLATFORM_DIAMETER_MM = 350.0;
    constexpr double PLATFORM_CIRCUMFERENCE_MM = PLATFORM_DIAMETER_MM * 3.14159;
    
    // Stepper motor configuration
    constexpr int STEPPER_STEPS_PER_REVOLUTION = 800;
    constexpr int STEPPER_MICROSTEPS = 18;
    constexpr int STEPPER_PULSE_PER_TURN = STEPPER_STEPS_PER_REVOLUTION * STEPPER_MICROSTEPS * 2;
    
    // Distance calculations
    constexpr double MM_PER_STEP = PLATFORM_CIRCUMFERENCE_MM / STEPPER_PULSE_PER_TURN;
    constexpr double STEPS_PER_MM = STEPPER_PULSE_PER_TURN / PLATFORM_CIRCUMFERENCE_MM;
}

// Timing configuration
namespace Timing {
    // Default frequencies
    constexpr float DEFAULT_PLATE_FREQUENCY_HZ = 15.0f;
    constexpr float MAX_PLATE_FREQUENCY_HZ = 50.0f;
    constexpr float MIN_PLATE_FREQUENCY_HZ = 1.0f;
    
    // Timing constraints
    constexpr uint32_t MIN_PULSE_SEPARATION_US = 1000000 / 15;  // 15 Hz minimum
    constexpr uint32_t GATE_DEBOUNCE_TIME_US = 1000;           // 1ms debounce
    constexpr uint32_t CAMERA_TRIGGER_WIDTH_US = 100;          // 100µs trigger pulse
}

// Communication configuration
namespace Communication {
    // Serial communication
    constexpr uint32_t SERIAL_BAUD_RATE = 115200;
    constexpr uint8_t SERIAL_DATA_BITS = 8;
    constexpr uint8_t SERIAL_STOP_BITS = 1;
    constexpr uint8_t SERIAL_PARITY = 0;  // None
    
    // Protocol settings
    constexpr size_t MAX_JSON_SIZE = 2048;
    constexpr size_t MAX_MESSAGE_SIZE = 1024;
    constexpr uint32_t COMMAND_TIMEOUT_MS = 5000;
}

// Inspection configuration
namespace Inspection {
    // Object detection
    constexpr int GATE_MIN_WIDTH_US = 500;   // Minimum object width
    constexpr int GATE_MAX_WIDTH_US = 5000;  // Maximum object width
    
    // Inspection status values
    constexpr int STATUS_SKIP = -2100;    // Skip inspection
    constexpr int STATUS_UNSET = -2000;   // Not yet inspected
    constexpr int STATUS_DELETE = -1000;  // Mark for deletion
    
    // Selector countdown
    constexpr int DEFAULT_SEL1_COUNTDOWN = 1000;
    constexpr int MAX_SEL1_COUNTDOWN = 10000;
}

// System limits
namespace Limits {
    // Pipeline capacity
    constexpr size_t MAX_PIPELINE_OBJECTS = 50;
    constexpr size_t MAX_SCHEDULED_ACTIONS = 100;
    
    // Error handling
    constexpr size_t MAX_ERROR_HISTORY = 100;
    constexpr size_t MAX_LOG_ENTRIES = 1000;
    
    // Memory limits
    constexpr size_t MAX_JSON_BUFFER_SIZE = 4096;
    constexpr size_t MAX_MESSAGE_QUEUE_SIZE = 50;
}

} // namespace BoardConfig
