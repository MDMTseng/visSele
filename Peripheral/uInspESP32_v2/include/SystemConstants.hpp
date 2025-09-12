#pragma once

/**
 * @brief System constants and configuration values
 * 
 * This header contains constexpr constants that replace numeric macros
 * for better type safety, compile-time evaluation, and modern C++ practices.
 */

namespace SystemConstants {

// Platform mechanical constants
constexpr double PLATFORM_DIAMETER_MM = 350.0;
constexpr double SYSTEM_PI = 3.14159;
constexpr int PLATFORM_CIRCUMFERENCE_UM = static_cast<int>(PLATFORM_DIAMETER_MM * SYSTEM_PI * 1000);
constexpr int PLATFORM_SUB_STEP = 800;
constexpr int PLATFORM_PULSE_PER_TURN = PLATFORM_SUB_STEP * 18 * 2;
constexpr int PLATFORM_DISTANCE_UM_PER_STEP = PLATFORM_CIRCUMFERENCE_UM / PLATFORM_PULSE_PER_TURN;

// Stepper motor constants
constexpr int STEPPER_SUBDIVISION = 3200;
constexpr int STEPPER_MM_PER_REVOLUTION = 95;

// Timing constants
constexpr int TICK_TO_SEC_BASE = 10 * 1000 * 1000;  // 10 million ticks per second
constexpr int PULSE_TIME_SYNC_US_SHIFT = 25;

// System limits and counts
constexpr int AUX_TASK_COUNT = 5;
constexpr int PRT_FUNC_LEN = 20;  // Function name length for logging

// Pin definitions (these should eventually move to BoardConfig.hpp)
constexpr int PIN_O1 = 5;
constexpr int PIN_LED = 2;
constexpr int PIN_TRIG_595 = 5;

// Stepper motor pins
constexpr int STEPPER_PLS_PIN = 22;
constexpr int STEPPER_DIR_PIN = 23;
constexpr int STEPPER_EN_PIN = 13;
constexpr int STEPPER_EN_ACTIVATION = 0;

// Output pins
constexpr int PIN_O_L1A = 16;
constexpr int PIN_O_CAM1 = 17;
constexpr int PIN_O_L2A = 18;
constexpr int PIN_O_CAM2 = 19;
constexpr int PIN_O_SEL1 = 25;
constexpr int PIN_O_SEL2 = 26;
constexpr int PIN_O_SEL3 = 32;
constexpr int FEEDER_PIN = 21;

// Input pins
constexpr int PIN_I_GATE = 27;

// Inspection status constants
constexpr int INSP_STATUS_SKIP = -2100;   // Mark inspection result not yet arrived
constexpr int INSP_STATUS_UNSET = -2000;  // Mark inspection result not yet arrived
constexpr int INSP_STATUS_DEL = -1000;    // Mark object can be deleted

// Bit manipulation constants
constexpr int UNSIGNED_NUM_HIGHEST_BIT_MASK = 0x80;

// Array size helper (replaces S_ARR_LEN macro)
template<typename T, size_t N>
constexpr size_t array_size(const T (&)[N]) {
    return N;
}

// Bit pattern constants for logging
constexpr const char* BIT_PATTERN_FORMAT = "%c%c%c%c%c%c%c%c";

// Helper function to get bit pattern (replaces PRTF_B2b macro)
constexpr char get_bit_char(uint8_t byte, int bit_position) {
    return (byte & (1 << bit_position)) ? '1' : '0';
}

} // namespace SystemConstants
