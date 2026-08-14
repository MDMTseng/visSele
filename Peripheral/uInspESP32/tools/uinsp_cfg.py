#!/usr/bin/env python3
"""The one place that knows the setup document's flat<->grouped key mapping.

The device's setup document was reorganised into `plate` / `gate` / `cam` /
`skip_policy` objects and the old flat keys were removed. The parser does not
reject an unrecognised key -- `set_setup` returns `ack: true` whatever it
understood -- so a tool still sending `{"plate_freq": 0}` is told the command
succeeded while nothing at all happened.

That is not a theoretical hazard. Eight tools in this directory opened with
`{"set_setup", "plate_freq": 15000}` and closed with `{"plate_freq": 0}`;
after the regroup both were silent no-ops, so every run measured whatever
configuration the board happened to be carrying and reported it as the one it
had asked for, and every "the plate is stopped" line at the end was a claim
about a command that had done nothing. A 60-second run once reported
`=> clean` with `accept=0`: nothing had turned.

This module is deliberately pure -- a dict and a function over dicts, no
sockets, no device knowledge beyond the names. Import it at whatever choke
point a tool already has for outbound commands; do not re-copy the table,
because two copies of a mapping drift and the drift is invisible until a run
is silently void.
"""

# flat name -> (group, key in group). Mirrors genMachineSetup/setMachineSetup
# in LegacyFirmware.cpp; if a name changes there it must change here.
CFG_GROUP = {
    "plate_freq":            ("plate", "freq"),
    "plate_accel":           ("plate", "accel"),
    "speed_band_pct":        ("plate", "speed_band_pct"),
    "pulses_per_rev":        ("plate", "pulses_per_rev"),
    "plate_diameter_mm":     ("plate", "diameter_mm"),
    "stepper_en_active":     ("plate", "stepper_en_active"),
    "stepper_dir":           ("plate", "stepper_dir"),

    "min_detect_sep_us":     ("gate", "min_detect_sep_us"),
    "pulse_min_width":       ("gate", "pulse_min_width"),
    "pulse_max_width":       ("gate", "pulse_max_width"),
    "gate_debounce_rise":    ("gate", "debounce_rise"),
    "gate_debounce_fall":    ("gate", "debounce_fall"),
    "min_detect_dist_um":    ("gate", "min_detect_dist_um"),

    "report_match_ts":       ("cam", "report_match_ts"),
    "report_match_pcnt":     ("cam", "report_match_pcnt"),
    "cam_match_window_us":   ("cam", "match_window_us"),
    "cam_match_tolerance_mm":("cam", "match_tolerance_mm"),
    "cam_recal_idle_ms":     ("cam", "recal_idle_ms"),
    "cal_pulse_us":          ("cam", "cal_pulse_us"),
    "cam_drift_comp":        ("cam", "drift_comp"),

    # The ON/OFF switch for the whole skip policy, and the only way to reach
    # AUTO_RATE. It had no flat name at all, so the two tuning knobs below were
    # settable while the thing they tune could not be turned on.
    "skip_policy_mode":      ("skip_policy", "mode"),
    "unanswered_stop_after": ("skip_policy", "stop_after"),
    "auto_rate_floor_us":    ("skip_policy", "rate_floor_us"),
    "auto_rate_recover_n":   ("skip_policy", "recover_n"),
}

# The inverse, for reading a reply back into the flat shape a tool expects.
CFG_FLAT = {(g, k): flat for flat, (g, k) in CFG_GROUP.items()}


def regroup(cmd):
    """Rewrite a set_setup command's flat keys into their groups.

    Anything that is not a set_setup, and any key with no mapping, is passed
    through untouched -- this is a translation, not a validator. Keys already
    in grouped form are left alone, so it is safe to apply twice.
    """
    if not isinstance(cmd, dict) or cmd.get("type") != "set_setup":
        return cmd
    out = {}
    for k, v in cmd.items():
        g = CFG_GROUP.get(k)
        if g is None:
            out[k] = v
            continue
        out.setdefault(g[0], {})
        # A group the caller also supplied explicitly wins on its own keys;
        # merging rather than replacing keeps a mixed document meaningful.
        if isinstance(out[g[0]], dict):
            out[g[0]].setdefault(g[1], v)
    return out


def flatten(doc):
    """Rewrite a grouped setup reply back into flat keys.

    For tools that were written against the old shape and read fields like
    `plate_freq` straight out of get_setup. Unknown groups pass through.
    """
    if not isinstance(doc, dict):
        return doc
    out = {}
    for k, v in doc.items():
        if isinstance(v, dict):
            mapped_any = False
            for kk, vv in v.items():
                flat = CFG_FLAT.get((k, kk))
                if flat is not None:
                    out[flat] = vv
                    mapped_any = True
                else:
                    out.setdefault(k, {})[kk] = vv
            if not mapped_any and k not in out:
                out[k] = v
        else:
            out[k] = v
    return out
