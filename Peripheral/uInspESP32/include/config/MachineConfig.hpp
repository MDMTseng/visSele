#pragma once

#include <ArduinoJson.h>
#include <cstdint>

// Per-machine tunables that must survive a power cycle.
//
// The compiled-in values in LegacyFirmware.cpp stay as the fallback: they are
// what a freshly flashed board runs with until a host pushes a set_setup and
// the result is committed to NVS.  A board with no stored config therefore
// behaves exactly as it did before this module existed.

typedef struct stagePulseOffset
{
  uint32_t CAM1_on;
  uint32_t CAM1_off;
  uint32_t L1A_on;
  uint32_t L1A_off;

  uint32_t CAM2_on;
  uint32_t CAM2_off;
  uint32_t L2A_on;
  uint32_t L2A_off;

  uint32_t SWITCH;

  uint32_t SEL1_on;
  uint32_t SEL1_off;

  uint32_t SEL2_on;
  uint32_t SEL2_off;

  uint32_t SEL3_on;
  uint32_t SEL3_off;
} stagePulseOffset;

// Machine tunables owned by LegacyFirmware.cpp, persisted by this module.
extern stagePulseOffset STAGE_PULSE_OFFSET;
extern float PLATE_FREQ_SETPOINT;
// Config is stored as the same JSON the wire uses, so these are shared.
void genMachineSetup(JsonDocument &jdoc);
void setMachineSetup(JsonDocument &jdoc, bool apply_hw);

extern uint32_t SYS_MIN_PULSE_TIME_SEP_us;
extern volatile bool AUTO_RATE;
extern uint32_t AUTO_RATE_FLOOR_us;
extern uint32_t AUTO_RATE_RECOVER_N;
// volatile: shared between the main loop (set_setup) and the step ISR
// (GateSensing) -- see LegacyFirmware.cpp.
extern volatile int minWidth;
extern volatile int maxWidth;
// Stepper electrical polarity. A driver with common-anode (common +5V) opto
// inputs sees every signal inverted relative to direct wiring, so the level
// that means "enabled" -- and the DIR level for the wanted rotation -- are
// per-machine wiring facts, not compile-time ones.
extern int stepper_en_active;   // level on STEPPER_EN_PIN that enables the driver
extern int stepper_dir_level;   // level driven on STEPPER_DIR_PIN
// Passive machine metadata: the firmware computes purely in the pulse domain
// and never uses these; they let a host read the board and convert
// mm/rpm <-> pulses without a side channel (pulse rate == plate_freq, so
// rpm = plate_freq / pulses_per_rev * 60).
extern uint32_t pulses_per_rev;    // full stepper pulses per plate revolution
extern float plate_diameter_mm;   // 0 = not configured
// Per-output ON polarity mask, bit=IO_IDX in LegacyFirmware.cpp (set = ON is
// LOW, for common-anode driver inputs). volatile: read by the step ISR.
extern volatile uint32_t IO_INV_MASK;
// Plate ramp acceleration, Hz of plate_freq per second (<=0 = instant).
extern float SYS_FREQ_ACCEL;
// Gate edge debounce thresholds (samples) -- see GateSensing().
extern int DEBOUNCE_H_THRES;
extern int DEBOUNCE_L_THRES;
// Fail-to-reject policy for unanswered parts (see LegacyFirmware.cpp).
extern volatile int UNANSWERED_POLICY;
extern volatile int UNANSWERED_STOP_AFTER;
// Host-link watchdog (ms, 0 = off) -- see firmwareLoop().
extern volatile int host_timeout_ms;

#define MACHINE_ID_MAX_LEN 24

namespace MachineConfig
{
  // Bump when the stored layout changes; a mismatch is treated as "no config"
  // so an old board silently falls back to compiled defaults instead of
  // loading garbage into the pulse offsets.
  // v2: + stepper_en_active / stepper_dir_level
  // v3: + pulses_per_rev / plate_diameter_mm
  // v4: + io_inv_mask (per-output ON polarity)
  // v5: + plate_accel (Hz/s ramp)
  // v6: + gate debounce rise/fall
  // v7: + unanswered_policy / unanswered_stop_after
  // v8: + host_timeout_ms
  // v9: + auto_rate / auto_rate_floor_us / auto_rate_recover_n
  //     (WITHDRAWN -- inserted mid-struct, see trustedPrefix())
  // v10: same fields, appended after machine_id where they belong
  // (an older blob falls back to compiled defaults -- re-push and save_setup
  // once after upgrading).
  constexpr uint32_t kConfigVersion = 10;

  // Reads NVS into the globals above. Call once, early in firmwareSetup(),
  // before anything derives timing from them.
  void begin();

  // Commits the globals' current values to NVS. Returns false if the write
  // failed; the in-RAM values are untouched either way.
  bool save();

  // Drops the stored config -- both the JSON and any legacy packed blob. The
  // globals keep their current values; the next boot comes up on compiled
  // defaults.
  bool clear();

  // True if begin() found a usable stored config.
  bool isLoadedFromNVS();

  // Identifies which physical machine this board is bolted to. Empty until
  // set. Reported in get_setup so the host cannot cross-wire two machines'
  // settings.
  // FNV-1a over the canonical config image -- the config-drift fingerprint
  // reported as cfg_crc in get_setup.
  uint32_t hash();

  const char *machineId();
  void setMachineId(const char *id);
} // namespace MachineConfig
