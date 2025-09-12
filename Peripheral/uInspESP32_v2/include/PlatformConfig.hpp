#pragma once

/**
 * @brief Platform configuration and conditional compilation
 * 
 * This header provides platform detection and configuration macros
 * for supporting multiple platforms (ESP32, STM32, etc.) with
 * conditional compilation.
 */

// Platform detection
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32) || defined(CONFIG_IDF_TARGET_ESP32)
    #define TARGET_ESP32 1
    #define TARGET_PLATFORM "ESP32"
#elif defined(STM32F4) || defined(STM32F7) || defined(STM32H7) || defined(STM32G4)
    #define TARGET_STM32 1
    #define TARGET_PLATFORM "STM32"
#else
    #define TARGET_UNKNOWN 1
    #define TARGET_PLATFORM "Unknown"
#endif

// Platform-specific feature flags
#if defined(TARGET_ESP32)
    #define PLATFORM_HAS_WIFI 1
    #define PLATFORM_HAS_BLUETOOTH 1
    #define PLATFORM_HAS_DUAL_CORE 1
    #define PLATFORM_HAS_FREERTOS 1
    #define PLATFORM_MAX_PINS 40
    #define PLATFORM_DEFAULT_CPU_FREQ 240000000
#elif defined(TARGET_STM32)
    #define PLATFORM_HAS_WIFI 0
    #define PLATFORM_HAS_BLUETOOTH 0
    #define PLATFORM_HAS_DUAL_CORE 0
    #define PLATFORM_HAS_FREERTOS 1
    #define PLATFORM_MAX_PINS 144
    #define PLATFORM_DEFAULT_CPU_FREQ 168000000
#endif

// Compiler feature detection
#if defined(__GNUC__)
    #define COMPILER_GCC 1
    #if __GNUC__ >= 7
        #define COMPILER_SUPPORTS_CXX17 1
    #endif
#elif defined(_MSC_VER)
    #define COMPILER_MSVC 1
#elif defined(__clang__)
    #define COMPILER_CLANG 1
#endif

// Memory and performance characteristics
#if defined(TARGET_ESP32)
    #define PLATFORM_RAM_SIZE_KB 320
    #define PLATFORM_FLASH_SIZE_KB 4096
    #define PLATFORM_TIMER_RESOLUTION_US 0.125  // 8MHz timer
#elif defined(TARGET_STM32)
    #define PLATFORM_RAM_SIZE_KB 192  // Typical for STM32F4
    #define PLATFORM_FLASH_SIZE_KB 1024  // Typical for STM32F4
    #define PLATFORM_TIMER_RESOLUTION_US 1.0  // 1MHz timer
#endif

// HAL implementation selection
#if defined(TARGET_ESP32)
    #define HAL_IMPLEMENTATION_PATH "hal/esp32/"
    #define HAL_TIMER_IMPLEMENTATION "ESP32TimerTickSource"
    #define HAL_GPIO_IMPLEMENTATION "ESP32Gpio"
    #define HAL_STEPPER_IMPLEMENTATION "ESP32StepperDriver"
    #define HAL_CLOCK_IMPLEMENTATION "ESP32Clock"
    #define HAL_LOGGER_IMPLEMENTATION "ESP32Logger"
    #define HAL_TRANSPORT_IMPLEMENTATION "SerialTransport"
#elif defined(TARGET_STM32)
    #define HAL_IMPLEMENTATION_PATH "hal/stm32/"
    #define HAL_TIMER_IMPLEMENTATION "STM32TimerTickSource"
    #define HAL_GPIO_IMPLEMENTATION "STM32Gpio"
    #define HAL_STEPPER_IMPLEMENTATION "STM32StepperDriver"
    #define HAL_CLOCK_IMPLEMENTATION "STM32Clock"
    #define HAL_LOGGER_IMPLEMENTATION "STM32Logger"
    #define HAL_TRANSPORT_IMPLEMENTATION "UartTransport"
#endif

// Debug and testing support
#if defined(TARGET_ESP32)
    #define PLATFORM_SUPPORTS_SERIAL_DEBUG 1
    #define PLATFORM_SUPPORTS_OTA 1
    #define PLATFORM_SUPPORTS_WEB_SERVER 1
#elif defined(TARGET_STM32)
    #define PLATFORM_SUPPORTS_SERIAL_DEBUG 1
    #define PLATFORM_SUPPORTS_OTA 0
    #define PLATFORM_SUPPORTS_WEB_SERVER 0
#endif

// Compile-time assertions for platform compatibility
#if !defined(TARGET_ESP32) && !defined(TARGET_STM32)
    #error "Unsupported platform. Please define TARGET_ESP32 or TARGET_STM32"
#endif

// Feature availability macros
#define FEATURE_AVAILABLE(feature) \
    (defined(PLATFORM_HAS_##feature) && PLATFORM_HAS_##feature)

// Conditional compilation helpers
#define IF_ESP32(code) \
    do { if constexpr (TARGET_ESP32) { code } } while(0)

#define IF_STM32(code) \
    do { if constexpr (TARGET_STM32) { code } } while(0)

#define IF_FEATURE(feature, code) \
    do { if constexpr (FEATURE_AVAILABLE(feature)) { code } } while(0)
