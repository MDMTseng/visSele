#include "config/MachineConfig.hpp"

#include <Preferences.h>
#include <cstring>

namespace
{
  constexpr char kNamespace[] = "uinsp";
  constexpr char kBlobKey[] = "cfg";
  constexpr uint32_t kMagic = 0x75494E53; // 'uINS'

  // One blob rather than a key per field: a partial write can then never leave
  // half the offsets from one machine and half from another.
  struct StoredConfig
  {
    uint32_t magic;
    uint32_t version;
    stagePulseOffset spo;
    float plate_freq;
    uint32_t min_detect_sep_us;
    int32_t pulse_min_width;
    int32_t pulse_max_width;
    int32_t stepper_en_active;
    int32_t stepper_dir_level;
    uint32_t pulses_per_rev;
    float plate_diameter_mm;
    uint32_t io_inv_mask;
    float plate_accel;
    int32_t gate_debounce_rise;
    int32_t gate_debounce_fall;
    int32_t unanswered_policy;
    int32_t unanswered_stop_after;
    int32_t host_timeout_ms;
    char machine_id[MACHINE_ID_MAX_LEN];
  };

  Preferences prefs;
  bool loadedFromNVS = false;
  char machine_id[MACHINE_ID_MAX_LEN] = {0};

  void fillFromGlobals(StoredConfig &cfg)
  {
    cfg.magic = kMagic;
    cfg.version = MachineConfig::kConfigVersion;
    cfg.spo = STAGE_PULSE_OFFSET;
    cfg.plate_freq = PLATE_FREQ_SETPOINT;
    cfg.min_detect_sep_us = SYS_MIN_PULSE_TIME_SEP_us;
    cfg.pulse_min_width = minWidth;
    cfg.pulse_max_width = maxWidth;
    cfg.stepper_en_active = stepper_en_active;
    cfg.stepper_dir_level = stepper_dir_level;
    cfg.pulses_per_rev = pulses_per_rev;
    cfg.plate_diameter_mm = plate_diameter_mm;
    cfg.io_inv_mask = IO_INV_MASK;
    cfg.plate_accel = SYS_FREQ_ACCEL;
    cfg.gate_debounce_rise = DEBOUNCE_H_THRES;
    cfg.gate_debounce_fall = DEBOUNCE_L_THRES;
    cfg.unanswered_policy = UNANSWERED_POLICY;
    cfg.unanswered_stop_after = UNANSWERED_STOP_AFTER;
    cfg.host_timeout_ms = host_timeout_ms;
    memcpy(cfg.machine_id, machine_id, MACHINE_ID_MAX_LEN);
    cfg.machine_id[MACHINE_ID_MAX_LEN - 1] = '\0';
  }

  void applyToGlobals(const StoredConfig &cfg)
  {
    STAGE_PULSE_OFFSET = cfg.spo;
    PLATE_FREQ_SETPOINT = cfg.plate_freq;
    SYS_MIN_PULSE_TIME_SEP_us = cfg.min_detect_sep_us;
    minWidth = cfg.pulse_min_width;
    maxWidth = cfg.pulse_max_width;
    stepper_en_active = cfg.stepper_en_active ? 1 : 0;
    stepper_dir_level = cfg.stepper_dir_level ? 1 : 0;
    pulses_per_rev = cfg.pulses_per_rev;
    plate_diameter_mm = cfg.plate_diameter_mm;
    IO_INV_MASK = cfg.io_inv_mask;
    SYS_FREQ_ACCEL = cfg.plate_accel;
    DEBOUNCE_H_THRES = cfg.gate_debounce_rise < 1 ? 1 : cfg.gate_debounce_rise;
    DEBOUNCE_L_THRES = cfg.gate_debounce_fall < 1 ? 1 : cfg.gate_debounce_fall;
    UNANSWERED_POLICY = cfg.unanswered_policy == 1 ? 1 : 0;
    UNANSWERED_STOP_AFTER = cfg.unanswered_stop_after < 1 ? 1 : cfg.unanswered_stop_after;
    host_timeout_ms = cfg.host_timeout_ms < 0 ? 0 : cfg.host_timeout_ms;
    memcpy(machine_id, cfg.machine_id, MACHINE_ID_MAX_LEN);
    machine_id[MACHINE_ID_MAX_LEN - 1] = '\0';
  }
} // namespace

namespace MachineConfig
{

  void begin()
  {
    loadedFromNVS = false;

    if (!prefs.begin(kNamespace, /*readOnly=*/true))
    {
      // Namespace has never been written. Compiled defaults stand.
      return;
    }

    StoredConfig cfg;
    size_t got = prefs.getBytes(kBlobKey, &cfg, sizeof(cfg));
    prefs.end();

    if (got != sizeof(cfg))
      return; // absent or written by a different layout
    if (cfg.magic != kMagic)
      return;
    if (cfg.version != kConfigVersion)
      return; // older/newer layout: fall back rather than misread offsets

    applyToGlobals(cfg);
    loadedFromNVS = true;
  }

  bool save()
  {
    StoredConfig cfg;
    fillFromGlobals(cfg);

    if (!prefs.begin(kNamespace, /*readOnly=*/false))
      return false;

    size_t written = prefs.putBytes(kBlobKey, &cfg, sizeof(cfg));
    prefs.end();

    if (written != sizeof(cfg))
      return false;

    loadedFromNVS = true;
    return true;
  }

  bool clear()
  {
    if (!prefs.begin(kNamespace, /*readOnly=*/false))
      return false;

    bool ok = prefs.remove(kBlobKey);
    prefs.end();

    loadedFromNVS = false;
    return ok;
  }

  uint32_t hash()
  {
    StoredConfig cfg;
    memset(&cfg, 0, sizeof(cfg));   // deterministic padding
    fillFromGlobals(cfg);
    const uint8_t *p = (const uint8_t *)&cfg;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < sizeof(cfg); i++)
    {
      h ^= p[i];
      h *= 16777619u;
    }
    return h;
  }

  bool isLoadedFromNVS()
  {
    return loadedFromNVS;
  }

  const char *machineId()
  {
    return machine_id;
  }

  void setMachineId(const char *id)
  {
    if (id == NULL)
      return;
    strncpy(machine_id, id, MACHINE_ID_MAX_LEN - 1);
    machine_id[MACHINE_ID_MAX_LEN - 1] = '\0';
  }

} // namespace MachineConfig
