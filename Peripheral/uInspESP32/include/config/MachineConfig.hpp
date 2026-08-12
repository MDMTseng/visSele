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

// How long each station stays ON, in MICROSECONDS.
//
// The offsets above are positions -- "how many stage ticks after the gate saw
// this part" -- and position is the right unit for them: a tick is 0.0126mm of
// plate, so changing speed does not move where anything fires. Width is a
// different question with a different answer. Nothing a station drives cares
// about distance:
//
//   camera   trigger floor ~100us, and its exposure is set in us on the camera
//   backlight ~300us to reach full brightness
//   SEL      solenoid open time + air transit
//
// Storing width as ticks made every one of those swing with plate speed. The
// SEL blow, tuned to 50ms at 30rpm, is 500ms at 3rpm and 38ms at 40rpm -- a 13x
// spread from one stored number, wrong at both ends.
//
// 0 means "not set": the corresponding *_off above stays authoritative, so an
// existing NVS config keeps behaving exactly as it did. Set a width and it
// takes over.
typedef struct stagePulseWidthUs
{
  uint32_t CAM1;
  uint32_t L1A;
  uint32_t CAM2;
  uint32_t L2A;
  uint32_t SEL1;
  uint32_t SEL2;
  uint32_t SEL3;
} stagePulseWidthUs;

extern stagePulseWidthUs STAGE_PULSE_WIDTH_US;

// Where the window sits relative to the position, in stage ticks.
//
// The *_on offsets above are the START of a window and the width runs FORWARD
// from them. That is right for the camera and the light -- the trigger edge IS
// the event, and everything after it is exposure -- but it is the wrong shape
// for the air blow.
//
// A blow has to be open before the part arrives (solenoid travel + air
// transit) and stay open after it (the part needs pushing, not touching). With
// a forward-only window the only way to buy the leading half is to move the
// position earlier, which puts two different quantities into one number:
// WHERE the part is, which is geometry and fixed, and HOW SLOW THE VALVE IS,
// which is pneumatics and changes with air pressure and nozzle. Retuning the
// second then silently moves the first -- and the first shares its coordinate
// frame with the camera trigger.
//
// So: a non-zero value here is the CENTRE of the window, and both edges are
// derived from it --
//
//     on  = centre - width/2      off = on + width
//
// 0 means "not set", exactly as a 0 width does, so an existing NVS config is
// untouched and keeps its forward-only behaviour. This is opt-in per station
// because reinterpreting the stored offsets would move every deployed blow by
// half its width, which on this machine is 750 ticks.
typedef struct stagePulseCenter
{
  uint32_t CAM1;
  uint32_t L1A;
  uint32_t CAM2;
  uint32_t L2A;
  uint32_t SEL1;
  uint32_t SEL2;
  uint32_t SEL3;
} stagePulseCenter;

extern stagePulseCenter STAGE_PULSE_CENTER;
extern float PLATE_FREQ_SETPOINT;
// Config is stored as the same JSON the wire uses, so these are shared.
void genMachineSetup(JsonDocument &jdoc);
void setMachineSetup(JsonDocument &jdoc, bool apply_hw);
// Names every key in a setup document that the schema does not contain.
// Defined beside setMachineSetup; used at boot to report what a stored
// config carries that this firmware no longer understands.
int cfgUnknownKeys(JsonObject in, char *out, size_t outN);

extern uint32_t SYS_MIN_PULSE_TIME_SEP_us;
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
// false = the eight actuator pins are still inputs (high impedance) because no
// config has said what ON means on this machine. See IO_ARMED in
// LegacyFirmware.cpp for why there is no default.
extern volatile bool IO_ARMED;
extern char IO_SAFE_WHY[96];
// Does this setup document define ON for every output, unambiguously?
// 1 = yes; 0 = no, and `why` says which key and how.
int ioConfigCheck(JsonObject in, char *why, size_t whyN);
// Rest the actuator pins at OFF and configure them as outputs.
void ioArm();
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
  // Whether begin() found an io_on_level it could believe. Read once, by
  // firmwareSetup, to decide whether to arm the outputs at all.
  bool ioConfigValid();

  // Commits the globals' current values to NVS. Returns false if the write
  // failed; the in-RAM values are untouched either way.
  bool save();

  // Drops the stored config -- both the JSON and any legacy packed blob. The
  // globals keep their current values; the next boot comes up on compiled
  // defaults.
  bool clear();

  // True if begin() found a usable stored config.
  bool isLoadedFromNVS();
  // True when begin() loaded the legacy packed struct rather than the JSON.
  // Reported as cfg_legacy_blob so the fleet can be audited before the struct
  // and its append-only discipline are deleted.
  bool isLegacyBlob();
  // Stored keys this firmware does not recognise, with their old values.
  // Never migrated and never re-saved: only an explicit save_setup or
  // set_setup persist:true may write NVS, so an upgrade cannot rewrite an
  // operator's calibration behind their back. The UI shows these, shows
  // the old values, and offers the conversion.
  // Call after anything changes the live config, so the next cfg_crc is
  // recomputed. hash() is otherwise cached: it builds a 3KB document and a
  // heap String, and poll() -- the cheap one -- asks for it every call.
  void invalidateHash();
  int staleKeyCount();
  const char* staleKeyNames();
  const char* staleKeyValues();

  // Identifies which physical machine this board is bolted to. Empty until
  // set. Reported in get_setup so the host cannot cross-wire two machines'
  // settings.
  // FNV-1a over the canonical config image -- the config-drift fingerprint
  // reported as cfg_crc in get_setup.
  uint32_t hash();

  const char *machineId();
  void setMachineId(const char *id);
} // namespace MachineConfig
