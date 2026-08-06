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
//
// WIRING (2026-08-06): the two lines are SEPARATE again.
//   GPIO16 (L1A)  -> backlight
//   GPIO17 (CAM1) -> camera 1 trigger
// The names now mean what they say.
//
// HISTORY, because it changes what the timestamps mean and the old arrangement
// is still described in commits and docs: from 2026-08-05 until 2026-08-06 the
// camera trigger was spliced onto the light line, GPIO16 did both jobs, and
// GPIO17 went nowhere. Everything written in that window that says "L1A is the
// actual trigger" or "CAM1 is a no-op" was true then and is false now.
//
// Consequence for timestamps: the instant that matters is the CAM1 rising edge,
// because that is what the camera actually sees. It coincides with the L1A edge
// only while the two offsets are equal -- ACT_L1A and ACT_CAM1 are scheduled at
// the same step offset and run in the same ISR pass off one fetched time_us, and
// calFireNow drives both back to back. If the two are ever given DIFFERENT
// stage_pulse_offsets (to lead the light, say), cam_us must follow CAM1, not
// L1A, or every pairing residual acquires that difference as a bias.
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
