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
    float plateFreq;
    uint32_t minDetectTimeSep_us;
    int32_t pulse_minWidth;
    int32_t pulse_maxWidth;
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
    cfg.plateFreq = SETUP_TAR_FREQ;
    cfg.minDetectTimeSep_us = SYS_MIN_PULSE_TIME_SEP_us;
    cfg.pulse_minWidth = minWidth;
    cfg.pulse_maxWidth = maxWidth;
    memcpy(cfg.machine_id, machine_id, MACHINE_ID_MAX_LEN);
    cfg.machine_id[MACHINE_ID_MAX_LEN - 1] = '\0';
  }

  void applyToGlobals(const StoredConfig &cfg)
  {
    STAGE_PULSE_OFFSET = cfg.spo;
    SETUP_TAR_FREQ = cfg.plateFreq;
    SYS_MIN_PULSE_TIME_SEP_us = cfg.minDetectTimeSep_us;
    minWidth = cfg.pulse_minWidth;
    maxWidth = cfg.pulse_maxWidth;
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
