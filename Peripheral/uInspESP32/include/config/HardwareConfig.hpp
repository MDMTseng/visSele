#pragma once

#include <cstdint>

// Motion timing derived from hardware characteristics
constexpr uint32_t kSubPulseSkipCount = 16;                                // Skip N sub pulses to reduce ISR workload
constexpr uint32_t kPerRevPulseCountHw = static_cast<uint32_t>(2400) * 16; // Raw hardware pulse count per revolution
constexpr uint32_t kPerRevPulseCount = kPerRevPulseCountHw / kSubPulseSkipCount;

// Legacy aliases maintained for existing code paths
constexpr uint32_t subPulseSkipCount = kSubPulseSkipCount;
constexpr uint32_t perRevPulseCount_HW = kPerRevPulseCountHw;
constexpr uint32_t perRevPulseCount = kPerRevPulseCount;

// Stepper driver IO
#define STEPPER_PLS_PIN 22
#define STEPPER_DIR_PIN 23
#define STEPPER_EN_ACTIVATION 0
#define STEPPER_EN_PIN 13

// Lighting and camera IO
#define PIN_O_L1A 16
#define PIN_O_CAM1 17
#define PIN_O_L2A 18
#define PIN_O_CAM2 19

// Selector IO
#define PIN_O_SEL1 25
#define PIN_O_SEL2 26
#define PIN_O_SEL3 32

// Auxiliary IO
#define FEEDER_PIN 21
#define PIN_I_GATE 27
