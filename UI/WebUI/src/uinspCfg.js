// The flat<->grouped key mapping for the uInspESP32 setup document.
//
// A JS port of Peripheral/uInspESP32/tools/uinsp_cfg.py, which carries the
// same table for the Python rigs. Two copies of a mapping drift, and the drift
// here is invisible: `set_setup` answers `ack: true` whatever it understood, so
// a command using a name the device no longer knows reports success and does
// nothing. If a name changes in LegacyFirmware.cpp's genMachineSetup /
// setMachineSetup, it must change in BOTH files.
//
// What this fixes in the UI: the device's setup document was reorganised into
// plate / gate / cam / skip_policy objects and the flat names removed. The UI
// still spoke flat in both directions, so
//   * outbound, every machine setting it wrote was silently discarded -- the
//     Inspection UI's start button raised "轉速沒有寫進裝置,未進入檢測模式"
//     because the speed never reached the board; and
//   * inbound, get_setup's grouped reply matched none of SETTABLE_KEYS, so the
//     whole configuration was filed as read-only device state and the panel
//     could display values it had no way to write back.

// flat name -> [group, key within group]
export const CFG_GROUP = {
  plate_freq:            ["plate", "freq"],
  plate_accel:           ["plate", "accel"],
  speed_band_pct:        ["plate", "speed_band_pct"],
  pulses_per_rev:        ["plate", "pulses_per_rev"],
  plate_diameter_mm:     ["plate", "diameter_mm"],
  stepper_en_active:     ["plate", "stepper_en_active"],
  stepper_dir:           ["plate", "stepper_dir"],

  min_detect_sep_us:     ["gate", "min_detect_sep_us"],
  pulse_min_width:       ["gate", "pulse_min_width"],
  pulse_max_width:       ["gate", "pulse_max_width"],
  gate_debounce_rise:    ["gate", "debounce_rise"],
  gate_debounce_fall:    ["gate", "debounce_fall"],
  min_detect_dist_um:    ["gate", "min_detect_dist_um"],
  gate_cam_mode:         ["gate", "cam_mode"],
  gate_cam_margin_pct:   ["gate", "cam_margin_pct"],
  gate_proc_mode:        ["gate", "proc_mode"],
  gate_proc_rate_hz:     ["gate", "proc_rate_hz"],
  gate_proc_sep_us:      ["gate", "proc_sep_us"],
  gate_proc_iir_shift:   ["gate", "proc_iir_shift"],

  report_match_ts:       ["cam", "report_match_ts"],
  // report_match_pcnt is NOT mapped any more: pulse-count pairing was removed
  // from the firmware 2026-08-18. The device still ACCEPTS the key (so old
  // backups are not refused wholesale) but refuses it set to true, so there is
  // nothing for the UI to offer.
  cam_match_window_us:   ["cam", "match_window_us"],
  cam_match_tolerance_mm:["cam", "match_tolerance_mm"],
  cam_recal_idle_ms:     ["cam", "recal_idle_ms"],
  cal_pulse_us:          ["cam", "cal_pulse_us"],
  cam_drift_comp:        ["cam", "drift_comp"],

  // The ON/OFF switch for the whole skip policy. It had no flat name at all,
  // so its tuning values were settable while the thing they tune could not be
  // turned on -- and set_setup answered ack:true to every attempt.
  // "stop_only" | "none"; the firmware still parses the older names.
  skip_policy_mode:      ["skip_policy", "mode"],
  unanswered_stop_after: ["skip_policy", "stop_after"],
  nomatch_stop_after:    ["skip_policy", "nomatch_stop_after"],
};

// [group, key] -> flat name
const CFG_FLAT = {};
Object.keys(CFG_GROUP).forEach((flat) => {
  const g = CFG_GROUP[flat];
  CFG_FLAT[g[0] + " " + g[1]] = flat;
});

// Rewrite a set_setup command's flat keys into their groups.
//
// A translation, not a validator: anything without a mapping passes through
// untouched, and keys already grouped are left alone, so applying it twice is
// safe. An explicitly supplied group wins on its own keys.
export function regroup(cmd) {
  if (!cmd || typeof cmd !== "object" || cmd.type !== "set_setup") return cmd;
  const out = {};
  Object.keys(cmd).forEach((k) => {
    const g = CFG_GROUP[k];
    if (!g) { out[k] = cmd[k]; return; }
    if (out[g[0]] === undefined) out[g[0]] = {};
    if (out[g[0]] && typeof out[g[0]] === "object" &&
        out[g[0]][g[1]] === undefined) out[g[0]][g[1]] = cmd[k];
  });
  return out;
}

// Rewrite a grouped setup reply back into flat keys, for the UI code that was
// written against the old shape. Unknown groups pass through.
export function flatten(doc) {
  if (!doc || typeof doc !== "object") return doc;
  const out = {};
  Object.keys(doc).forEach((k) => {
    const v = doc[k];
    if (v && typeof v === "object" && !Array.isArray(v)) {
      let mappedAny = false;
      Object.keys(v).forEach((kk) => {
        const flat = CFG_FLAT[k + " " + kk];
        if (flat !== undefined) { out[flat] = v[kk]; mappedAny = true; }
        else { if (!out[k]) out[k] = {}; out[k][kk] = v[kk]; }
      });
      if (!mappedAny && out[k] === undefined) out[k] = v;
    } else {
      out[k] = v;
    }
  });
  return out;
}
