#include "config/MachineConfig.hpp"

#include <Preferences.h>
#include <ArduinoJson.h>
#include <Arduino.h>   // ESP.getEfuseMac() for the derived machine id
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
    "xtal_mhz","cfg_from_nvs","cfg_crc","ver","name","id","ack",
    // Reported, never stored: it describes where this boot's config CAME FROM,
    // and persisting it would make the answer permanent.
    "cfg_legacy_blob"
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
  // Set when begin() had to fall back to the packed struct. The struct exists
  // ONLY for that migration -- save() writes JSON and deletes the blob -- so
  // this is the flag that says whether the migration is finished across the
  // fleet, and therefore whether ~150 lines of append-only-struct discipline
  // (and the trap that once inverted io_on_level) can be deleted.
  //
  // Reported as cfg_legacy_blob. Clears on the next save.
  bool legacyBlob = false;
  // cfg_crc, cached. poll() asks for it every call and poll() exists to be the
  // CHEAP one: canonicalImage() declares a local StaticJsonDocument<3072>, runs
  // the whole of genMachineSetup into it, and serialises through an Arduino
  // String on the heap. A host polling at 50Hz did 50 of those a second on the
  // loop task, against a stack high-water measured as low as 2500 bytes -- and
  // this file already says elsewhere that a 3KB document per frame overflows
  // the stack rather than merely being slow.
  //
  // Invalidated by every path that changes the live config. Starts dirty, so
  // the first ask is always a real one.
  uint32_t hashCached = 0;
  bool     hashDirty  = true;

  // Whether the config that was actually LOADED defines output polarity for
  // every output. Deliberately answered from the stored document rather than
  // from the globals: the globals always hold something, and what they hold
  // when a key is absent is the compiled default -- which is the failure being
  // guarded against, not evidence against it.
  bool ioCfgValid = false;

  // 128 bits: cfgKeyAt currently enumerates ~68 keys. Computed at boot from the
  // stored document and read during the same boot -- never stored, so the
  // positional index needs no version.
  uint32_t defaulted[4] = {0,0,0,0};
  int defaultedN = 0;

  void computeDefaulted(JsonObject in)
  {
    defaulted[0]=defaulted[1]=defaulted[2]=defaulted[3]=0;
    defaultedN = 0;
    const char *grp; const char *k;
    for (int i = 0; (k = cfgKeyAt(i, &grp)) != NULL; i++)
    {
      if (i >= 128) break;      // the mask is the limit, and it is not close
      if (!cfgKeyAbsent(in, grp, k)) continue;
      defaulted[i>>5] |= (1u << (i & 31));
      defaultedN++;
    }
  }
  // Keys the stored config carries that this firmware no longer knows, and
  // what they were set to. Filled at begin(), never acted on, reported so a
  // person can decide. See begin() for why this is not migrated.
  int  staleN = 0;
  char staleKeys[160] = {0};
  char staleVals[224] = {0};

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

  // The canonical config image: what save() writes and hash() fingerprints.
  // One producer, so the stored bytes and the reported CRC can never disagree.
  bool canonicalImage(String &out)
  {
    // STATIC, not a local. This is 3KB and the loop task's stack high-water has
    // been measured with as little as 2500 bytes free -- the note below about
    // caching the hash describes the same document as something that "overflows
    // the stack rather than merely being slow", and then mitigates it by calling
    // it less often. Calling it less often is not the same as not putting 3KB on
    // the stack.
    //
    // Not reentrant, which it never was: both callers are the main loop, and it
    // is cleared on entry so a previous image cannot leak into this one.
    static StaticJsonDocument<3072> jdoc;
    jdoc.clear();
    genMachineSetup(jdoc);
    // genMachineSetup also reports live state, which has no business in a
    // configuration store -- persisting cur_state or an error history would
    // restore yesterday's situation on top of today's machine.
    for (size_t i = 0; i < sizeof(kVolatileKeys)/sizeof(kVolatileKeys[0]); i++)
      jdoc.remove(kVolatileKeys[i]);
    out = "";
    if (serializeJson(jdoc, out) == 0) return false;
    if (jdoc.overflowed()) return false;   // truncated JSON is worse than none
    return true;
  }

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
    // The automatic trigger rate is gone (2026-08-12). The FIELDS stay: this
    // struct is a byte layout that older images are read through, and
    // trustedPrefix() measures the v<=8 boundary with offsetof(auto_rate).
    // Removing them would silently re-point every offset in the tail.
    cfg.auto_rate = 0;
    cfg.auto_rate_floor_us = 0;
    cfg.auto_rate_recover_n = 0;
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
    legacyBlob = false;

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

      // What this firmware no longer understands, recorded WITH the old values
      // and never acted on.
      //
      // The config is stored as wire JSON, so a renamed or dropped key does not
      // fail -- it simply stops being read, and whatever it configured silently
      // reverts to the compiled default. That is how io_on_level was lost once,
      // on a machine that is active-low: the light and the air came on by
      // themselves with parts on the plate.
      //
      // Deliberately NOT migrated and NOT re-saved. Nothing but an explicit
      // save_setup / set_setup persist:true may write NVS, so a firmware
      // upgrade can never quietly rewrite an operator's calibration into a
      // shape they did not choose. Reporting it instead lets the UI say which
      // settings were dropped, show what they used to be, and offer the
      // conversion -- a decision with a person behind it.
      staleN = cfgUnknownKeys(jdoc.as<JsonObject>(), staleKeys, sizeof(staleKeys));
      if (staleN > 0)
      {
        // The values too, so the UI can show what is about to be lost rather
        // than only the names.
        // Walk groups too -- staleKeys names them dotted (cam.cal_pulse_us), and
        // a first version of this skipped every JsonObject, so the names came
        // out and the values, which are the whole point for the operator, did
        // not.
        staleVals[0] = '\0';
        auto note = [&](const char *grp, const char *k, JsonVariantConst v)
        {
          char dotted[64];
          if (grp) snprintf(dotted, sizeof(dotted), "%s.%s", grp, k);
          else     snprintf(dotted, sizeof(dotted), "%s", k);
          if (!strstr(staleKeys, dotted)) return;
          char one[64]; one[0]='\0';
          serializeJson(v, one, sizeof(one));
          if (strlen(staleVals)+strlen(dotted)+strlen(one)+3 >= sizeof(staleVals)) return;
          if (staleVals[0]) strcat(staleVals, ",");
          strcat(staleVals, dotted); strcat(staleVals, "="); strcat(staleVals, one);
        };
        for (JsonPair kv : jdoc.as<JsonObject>())
        {
          if (kv.value().is<JsonObject>())
          {
            for (JsonPair p : kv.value().as<JsonObject>())
              note(kv.key().c_str(), p.key().c_str(), p.value());
          }
          else note(NULL, kv.key().c_str(), kv.value());
        }
      }

      // Variables only -- pinMode has not run yet.
      // Before applying: does this document say what ON means for every
      // output? If not, the outputs are never armed and the machine sits in
      // safe mode. The rest of the config is still applied -- a machine that
      // cannot actuate should still show the operator its own settings.
      // Which settings this document did NOT carry, and are therefore about to
      // run on compiled defaults. The opposite question to staleKeys, and the
      // one nothing used to ask.
      computeDefaulted(jdoc.as<JsonObject>());

      ioCfgValid = ioConfigCheck(jdoc.as<JsonObject>(),
                                 IO_SAFE_WHY, sizeof(IO_SAFE_WHY)) != 0;

      setMachineSetup(jdoc, /*apply_hw=*/false);
      loadedFromNVS = true;
    hashDirty = true;
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
    prefs.end();

    applyToGlobals(cfg);
    // The blob does carry output polarity -- io_inv_mask, added in v4 -- as a
    // bitmask, so every output is defined by construction and a v4-or-later
    // blob can arm. Older than that the field sits outside the trusted prefix,
    // so the mask in hand is the compiled default, which is precisely what
    // safe mode exists to refuse.
    ioCfgValid = (raw.version >= 4);
    if (!ioCfgValid)
      snprintf(IO_SAFE_WHY, sizeof(IO_SAFE_WHY),
               "legacy config v%u predates io_inv_mask", (unsigned)raw.version);
    loadedFromNVS = true;
    legacyBlob = true;      // this board has not been saved by a JSON firmware
    hashDirty = true;
  }

  bool ioConfigValid() { return ioCfgValid; }
  const uint32_t *defaultedMask() { return defaulted; }
  int defaultedCount() { return defaultedN; }

  bool save()
  {
    String txt;
    if (!canonicalImage(txt)) return false;

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
    legacyBlob = false;     // migrated: the blob above was just removed
    hashDirty = true;
    return true;
  }

  bool clear()
  {
    if (!prefs.begin(kNamespace, /*readOnly=*/false))
      return false;

    // Both stores, not just the one this firmware writes. Clearing only the
    // packed blob would leave the JSON in place, so clear_saved_setup would
    // report success and the next boot would come up on the same config it
    // was asked to forget.
    bool ok = false;
    if (prefs.isKey(kJsonKey)) ok |= prefs.remove(kJsonKey);
    if (prefs.isKey(kBlobKey)) ok |= prefs.remove(kBlobKey);
    prefs.end();

    loadedFromNVS = false;
    return ok;
  }

  // ---- sorting counters ---------------------------------------------------
  //
  // Stored under their own key, NOT inside the config document, and that is
  // deliberate. The config JSON is policed by cfgUnknownKeys(): anything this
  // firmware does not recognise is reported as stale and never read again.
  // Counters are not settings -- they change constantly, they are written by a
  // different path at a different time, and mixing them in would mean every
  // counter write rewrites the operator's calibration too.
  //
  // A missing or unreadable record reads as all-zero, which is the safe
  // direction: a lost count under-reports, it never invents parts.

  static constexpr const char *kCntKey = "cnt_json";

  bool countersLoad(Counters &out)
  {
    out = Counters();
    if (!prefs.begin(kNamespace, /*readOnly=*/true)) return false;
    if (!prefs.isKey(kCntKey)) { prefs.end(); return false; }
    String txt = prefs.getString(kCntKey, "");
    prefs.end();
    if (txt.length() == 0) return false;

    StaticJsonDocument<384> j;
    if (deserializeJson(j, txt) != DeserializationError::Ok) return false;

    out.sel1           = j["sel1"]  | 0u;
    out.sel2           = j["sel2"]  | 0u;
    out.sel3           = j["sel3"]  | 0u;
    out.na             = j["na"]    | 0u;
    out.skip           = j["skip"]  | 0u;
    out.unanswered     = j["unans"] | 0u;
    out.sel_suppressed = j["supp"]  | 0u;
    out.sel1_no_quota  = j["noq"]   | 0u;
    out.gate_accept    = j["gacc"]  | 0u;
    out.save_lat_ms    = j["lat"]   | 0u;
    out.save_seq       = j["seq"]   | 0u;
    return true;
  }

  bool countersSave(const Counters &c)
  {
    StaticJsonDocument<384> j;
    j["sel1"]  = c.sel1;
    j["sel2"]  = c.sel2;
    j["sel3"]  = c.sel3;
    j["na"]    = c.na;
    j["skip"]  = c.skip;
    j["unans"] = c.unanswered;
    j["supp"]  = c.sel_suppressed;
    j["noq"]   = c.sel1_no_quota;
    j["gacc"]  = c.gate_accept;
    j["lat"]   = c.save_lat_ms;
    j["seq"]   = c.save_seq;
    String txt;
    serializeJson(j, txt);

    if (!prefs.begin(kNamespace, /*readOnly=*/false)) return false;
    size_t written = prefs.putString(kCntKey, txt);
    prefs.end();
    return written == txt.length();
  }

  bool countersClear()
  {
    if (!prefs.begin(kNamespace, /*readOnly=*/false)) return false;
    bool ok = true;
    if (prefs.isKey(kCntKey)) ok = prefs.remove(kCntKey);
    prefs.end();
    return ok;
  }

  void invalidateHash() { hashDirty = true; }

  uint32_t hash()
  {
    if (!hashDirty) return hashCached;
    // FNV-1a over the same image save() writes, so cfg_crc tracks what is
    // actually persisted. Hashing the legacy packed struct instead would be
    // blind to every field added since -- the struct exists only for migration
    // now, and a field that is not in it would drift without changing the CRC.
    String txt;
    if (!canonicalImage(txt)) return 0;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < txt.length(); i++)
    {
      h ^= (uint8_t)txt[i];
      h *= 16777619u;
    }
    hashCached = h; hashDirty = false;
    return h;
  }

  int staleKeyCount(){ return staleN; }
  const char* staleKeyNames(){ return staleKeys; }
  const char* staleKeyValues(){ return staleVals; }

  bool isLoadedFromNVS()
  {
    return loadedFromNVS;
  }

  bool isLegacyBlob()
  {
    return legacyBlob;
  }

  // Derived from the chip, not configured -- and therefore not copyable.
  //
  // This used to be an ordinary settable string, which made it part of any
  // configuration export. The everyday use of an export is to bring a new
  // machine up from a known-good one, so the identity travelled with the
  // settings and two boards answered to the same name. That failure is silent:
  // both machines run correctly, and the damage is in the inspection records,
  // attributed to the wrong machine and found later.
  //
  // The eFuse MAC is unique per chip, immutable, and readable before anything
  // is stored -- so there is no "not generated yet" window and no write to get
  // wrong. It is preferred over a random value written to NVS for the case
  // that actually happens: erasing NVS to reset a machine. A random id would
  // come back different and orphan every record the machine had already
  // filed; this comes back the same, because it is the same board.
  //
  // A stored id still wins if one is present, so boards commissioned under the
  // old scheme keep the name their records were filed under.
  const char *machineId()
  {
    if (machine_id[0] == '\0')
    {
      const uint64_t mac = ESP.getEfuseMac();
      snprintf(machine_id, MACHINE_ID_MAX_LEN, "uI-%02X%02X%02X%02X%02X%02X",
               (unsigned)((mac >> 40) & 0xFF), (unsigned)((mac >> 32) & 0xFF),
               (unsigned)((mac >> 24) & 0xFF), (unsigned)((mac >> 16) & 0xFF),
               (unsigned)((mac >>  8) & 0xFF), (unsigned)((mac      ) & 0xFF));
    }
    return machine_id;
  }

  // Kept for the stored-config load path, which still carries a machine_id
  // field and must be able to restore it. It is NOT reachable from set_setup
  // any more -- see LegacyFirmware.cpp's setMachineSetup.
  void setMachineId(const char *id)
  {
    if (id == NULL)
      return;
    strncpy(machine_id, id, MACHINE_ID_MAX_LEN - 1);
    machine_id[MACHINE_ID_MAX_LEN - 1] = '\0';
  }

} // namespace MachineConfig
