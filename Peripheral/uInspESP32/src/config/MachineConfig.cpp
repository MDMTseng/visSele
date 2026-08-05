#include "config/MachineConfig.hpp"

#include <Preferences.h>
#include <ArduinoJson.h>
#include <cstring>
#include <cstddef>   // offsetof

namespace
{
  constexpr char kNamespace[] = "uinsp";
  constexpr char kBlobKey[] = "cfg";
  constexpr uint32_t kMagic = 0x75494E53; // 'uINS'

  // The config is stored as the same JSON the wire uses.
  //
  // It used to be a packed struct keyed by a version number, and adding one
  // field cost the machine its entire configuration: the version bump made
  // begin() reject the stored blob, the board came up on compiled defaults,
  // io_on_level inverted, and the light and air blow switched themselves on
  // with parts on the plate. A store whose failure mode is "lose the operator's
  // calibration" is the wrong store.
  //
  // JSON has no layout to get wrong. A key that is absent simply keeps its
  // compiled default, so old data stays readable when fields are added,
  // renamed or dropped, in any order. genMachineSetup/setMachineSetup already
  // exist for get_setup/set_setup, so this reuses them rather than maintaining
  // a second field-by-field mapping that can silently disagree with the first.
  constexpr const char *kJsonKey = "cfg_json";

  // Runtime state that genMachineSetup reports but nobody should persist.
  const char *kVolatileKeys[] = {
    "cur_state","step_count","error_hist","reset_reason","reset_reason_name",
    "xtal_mhz","cfg_from_nvs","cfg_crc","ver","name","id","ack"
  };

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
    // ---- APPEND ONLY BELOW THIS LINE ----------------------------------------
    // A stored blob from an older firmware is loaded as a PREFIX of this
    // struct: whatever it contains is used, whatever it predates keeps its
    // compiled default. That only works while fields are appended and never
    // inserted, reordered or resized. Inserting one silently shifts every field
    // after it, so an old blob would be read as garbage -- and the fields most
    // likely to be misread are the ones nobody checks, like output polarity.
    //
    // v9 was first added in the middle, above machine_id. The version bump then
    // discarded the whole stored config, the machine came up on compiled
    // defaults with io_on_level inverted, and the light and air blow switched
    // themselves on. Append.
    int32_t auto_rate;
    uint32_t auto_rate_floor_us;
    uint32_t auto_rate_recover_n;
  };

  Preferences prefs;
  bool loadedFromNVS = false;
  // Smallest blob we will accept: everything up to and including machine_id,
  // i.e. the v8 layout. Anything shorter predates fields that have no safe
  // default on a real machine.
  uint32_t cfg_upgraded_from = 0;

  // How many bytes of a stored blob this firmware is willing to believe, given
  // the version it declares. A blob is only a valid prefix of the current
  // struct for the fields that existed, laid out where they are now.
  //
  // v9 is the exception and the reason this function exists: it was released
  // (briefly, on one board) with auto_rate inserted BEFORE machine_id rather
  // than after. Same version number, same size, different meaning -- so reading
  // it with today's layout would put auto_rate's bytes into machine_id and
  // machine_id's into auto_rate. Everything up to host_timeout_ms is still laid
  // out identically, so that much is trusted and the rest falls back.
  size_t trustedPrefix(uint32_t version)
  {
    if (version <= 8)  return offsetof(StoredConfig, auto_rate);  // ends after machine_id
    if (version == 9)  return offsetof(StoredConfig, machine_id); // mis-ordered tail
    return sizeof(StoredConfig);
  }

  // Anything shorter than the v1 core is not worth reading at all.
  constexpr size_t kMinCompatBytes = offsetof(StoredConfig, machine_id);
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
    cfg.auto_rate = AUTO_RATE ? 1 : 0;
    cfg.auto_rate_floor_us = AUTO_RATE_FLOOR_us;
    cfg.auto_rate_recover_n = AUTO_RATE_RECOVER_N;
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
    AUTO_RATE = cfg.auto_rate == 1;
    AUTO_RATE_FLOOR_us = cfg.auto_rate_floor_us;
    AUTO_RATE_RECOVER_N = cfg.auto_rate_recover_n < 1 ? 1 : cfg.auto_rate_recover_n;
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

    // Preferred: the JSON document.
    if (prefs.isKey(kJsonKey))
    {
      String txt = prefs.getString(kJsonKey, "");
      prefs.end();
      if (txt.length() == 0) return;
      StaticJsonDocument<3072> jdoc;
      if (deserializeJson(jdoc, txt) != DeserializationError::Ok)
        return;                      // corrupt: defaults stand, nothing applied
      // Variables only -- pinMode has not run yet.
      setMachineSetup(jdoc, /*apply_hw=*/false);
      loadedFromNVS = true;
      return;
    }

    // Legacy packed blob, read once so an upgrade does not cost the machine its
    // calibration. The next save() writes JSON and this path stops being used.

    // Seed with the compiled defaults, then overlay whatever the stored blob
    // actually holds. A blob written by an older firmware is shorter, so the
    // fields it predates simply keep their defaults instead of taking the whole
    // configuration down with them -- adding a field must not cost the operator
    // their calibration.
    StoredConfig cfg;
    fillFromGlobals(cfg);

    // Two passes: the header says how much of the rest can be believed.
    size_t stored = prefs.getBytesLength(kBlobKey);
    if (stored < kMinCompatBytes) { prefs.end(); return; }

    StoredConfig raw;
    size_t head = prefs.getBytes(kBlobKey, &raw,
                                 stored < sizeof(raw) ? stored : sizeof(raw));
    if (head < kMinCompatBytes || raw.magic != kMagic ||
        raw.version > kConfigVersion)
    {
      prefs.end();
      return; // absent, corrupt, or written by a firmware newer than this one
    }

    size_t want = trustedPrefix(raw.version);
    if (want > head) want = head;
    memcpy(&cfg, &raw, want);
    size_t got = want;
    prefs.end();

    applyToGlobals(cfg);
    loadedFromNVS = true;
    if (cfg.version < kConfigVersion)
    {
      // Not an error: the blob is simply older. Saving once rewrites it at the
      // current version, but everything it did carry is already in effect.
      cfg_upgraded_from = cfg.version;
    }
  }

  bool save()
  {
    StaticJsonDocument<3072> jdoc;
    genMachineSetup(jdoc);
    // genMachineSetup also reports live state, which has no business in a
    // configuration store -- persisting cur_state or an error history would
    // restore yesterday's situation on top of today's machine.
    for (size_t i = 0; i < sizeof(kVolatileKeys)/sizeof(kVolatileKeys[0]); i++)
      jdoc.remove(kVolatileKeys[i]);

    String txt;
    if (serializeJson(jdoc, txt) == 0) return false;
    if (jdoc.overflowed()) return false;   // truncated JSON is worse than none

    if (!prefs.begin(kNamespace, /*readOnly=*/false))
      return false;
    size_t written = prefs.putString(kJsonKey, txt);
    // The packed blob is now stale. Drop it so a future firmware cannot read a
    // superseded configuration back out.
    if (written == txt.length() && prefs.isKey(kBlobKey)) prefs.remove(kBlobKey);
    prefs.end();

    if (written != txt.length())
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
