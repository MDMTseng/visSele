#pragma once

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
extern float SETUP_TAR_FREQ;
extern uint32_t SYS_MIN_PULSE_TIME_SEP_us;
extern int minWidth;
extern int maxWidth;

#define MACHINE_ID_MAX_LEN 24

namespace MachineConfig
{
  // Bump when the stored layout changes; a mismatch is treated as "no config"
  // so an old board silently falls back to compiled defaults instead of
  // loading garbage into the pulse offsets.
  constexpr uint32_t kConfigVersion = 1;

  // Reads NVS into the globals above. Call once, early in firmwareSetup(),
  // before anything derives timing from them.
  void begin();

  // Commits the globals' current values to NVS. Returns false if the write
  // failed; the in-RAM values are untouched either way.
  bool save();

  // Drops the stored blob. The globals keep their current values; the next
  // boot comes up on compiled defaults.
  bool clear();

  // True if begin() found a usable stored config.
  bool isLoadedFromNVS();

  // Identifies which physical machine this board is bolted to. Empty until
  // set. Reported in get_setup so the host cannot cross-wire two machines'
  // settings.
  const char *machineId();
  void setMachineId(const char *id);
} // namespace MachineConfig
