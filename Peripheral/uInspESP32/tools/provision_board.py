#!/usr/bin/env python3
"""Provision a freshly flashed board from a WebUI setup export.

    python tools/provision_board.py <export.json> [--port COM3] [--baud 230400]
    python tools/provision_board.py <export.json> --persist     # also write NVS

WHY THIS IS NOT "just send the file"

The export is the WebUI's FLAT shape (`plate_freq`, `min_detect_sep_us`,
`cam_match_window_us`, `skip_policy_mode`). The firmware's set_setup takes
NESTED groups (`plate.freq`, `gate.min_detect_sep_us`, `cam.match_window_us`,
`skip_policy.mode`), and two of the export's keys -- `unanswered_stop_after`
and `skip_policy_mode` -- are not firmware keys at all; PerifAPI.js translates
them. Sending the file as-is configures nothing.

That is not hypothetical. From LegacyFirmware.cpp, above the schema tables:

    That is how eight tools in tools/ spent a week configuring nothing: the
    setup document was regrouped into plate/gate/cam, their flat
    `{"plate_freq": 0}` stopped meaning anything, and every one of them was
    told the command had succeeded.

set_setup now names unrecognised keys back instead of acking silently, so a
raw send would at least complain. This translates properly, then VERIFIES by
reading get_setup back and comparing every value it meant to set. A
provisioning step that does not read back is how a board ends up half
configured with nobody the wiser.

ON *_off OFFSETS. stage_pulse_offset's `*_off` fields are DERIVED, not stored
config: STAGE_PULSE_WIDTH_apply() computes `*off = *on + width_in_ticks` at the
current plate speed, and returns early when the speed is 0. An export taken at
plate_freq 0 therefore carries whatever was last left in them -- values that
can even sit BEFORE their matching `_on`. They are sent for faithfulness and
they self-correct the first time a real plate speed is commanded; the verify
pass reports them separately so they cannot be mistaken for a mismatch.

ON NVS. `persist` is omitted unless --persist is passed. Without it the change
is RAM-only and a power cycle undoes it, which is the right default for a step
whose whole purpose is to be checked before it is committed.

ON io_on_level. It is all-or-nothing and it ARMS THE OUTPUTS. On a board wired
to real valves, applying an active-low map drives them. Know which board you
have before running this.
"""
import argparse
import json
import re
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is required:  pip install pyserial")

# Flat export key -> (group, firmware key). Group None means top level.
# Every target below is checked against the firmware's own K_* tables in
# LegacyFirmware.cpp; anything not in them would be named back by set_setup.
FLAT_MAP = {
    "plate_freq":             ("plate", "freq"),
    "plate_accel":            ("plate", "accel"),
    "speed_band_pct":         ("plate", "speed_band_pct"),
    "pulses_per_rev":         ("plate", "pulses_per_rev"),
    "plate_diameter_mm":      ("plate", "diameter_mm"),
    "stepper_en_active":      ("plate", "stepper_en_active"),
    "stepper_dir":            ("plate", "stepper_dir"),

    "min_detect_sep_us":      ("gate", "min_detect_sep_us"),
    "pulse_min_width":        ("gate", "pulse_min_width"),
    "pulse_max_width":        ("gate", "pulse_max_width"),
    "gate_debounce_rise":     ("gate", "debounce_rise"),
    "gate_debounce_fall":     ("gate", "debounce_fall"),
    "min_detect_dist_um":     ("gate", "min_detect_dist_um"),
    "gate_ref":               ("gate", "gate_ref"),

    "report_match_ts":        ("cam", "report_match_ts"),
    "cam_match_window_us":    ("cam", "match_window_us"),
    "cam_match_tolerance_mm": ("cam", "match_tolerance_mm"),
    "cam_recal_idle_ms":      ("cam", "recal_idle_ms"),
    "cal_pulse_us":           ("cam", "cal_pulse_us"),
    "cam_drift_comp":         ("cam", "drift_comp"),

    "skip_policy_mode":       ("skip_policy", "mode"),
    "unanswered_stop_after":  ("skip_policy", "stop_after"),

    "host_timeout_ms":        (None, "host_timeout_ms"),
}
# Groups the export already carries in nested form.
PASSTHROUGH = ("stage_pulse_offset", "stage_pulse_width_us",
               "stage_pulse_center", "io_on_level")
DERIVED_SPO = ("L1A_off", "CAM1_off", "L2A_off", "CAM2_off",
               "SEL1_off", "SEL2_off", "SEL3_off")


def translate(export):
    out, skipped = {}, []
    for k, v in export.items():
        if k in PASSTHROUGH:
            out[k] = dict(v)
            continue
        tgt = FLAT_MAP.get(k)
        if tgt is None:
            skipped.append(k)
            continue
        group, key = tgt
        if group is None:
            out[key] = v
        else:
            out.setdefault(group, {})[key] = v
    return out, skipped


def txn(ser, obj, settle=1.2, want=None, timeout=8.0):
    """Send one command, return the first reply line that looks like the answer."""
    ser.reset_input_buffer()
    ser.write((json.dumps(obj) + "\n").encode())
    t0, buf = time.time(), b""
    while time.time() - t0 < timeout:
        buf += ser.read(8192)
        if want and want.encode() in buf:
            break
    time.sleep(settle)
    buf += ser.read(65536)
    for line in buf.decode("latin1", "replace").splitlines():
        line = line.strip()
        if not line.startswith("{"):
            continue
        # Every frame carries an integrity trailer -- "{...}*HHHH", CRC16-CCITT
        # over the JSON bytes (Data_Layer_Protocol.cpp). It is not part of the
        # object, and json.loads rejects the whole line because of it. The core
        # strips it in its data layer; a tool talking to the raw port has to do
        # the same, and forgetting to is indistinguishable from "the board did
        # not answer".
        line = re.sub(r"\*[0-9A-Fa-f]{4}$", "", line)
        try:
            j = json.loads(line)
        except ValueError:
            continue
        if want is None or want in j:
            return j, buf
    return None, buf


def flatten_expected(payload):
    """The (path, value) pairs the caller intends to have set."""
    for k, v in payload.items():
        if isinstance(v, dict):
            for kk, vv in v.items():
                yield f"{k}.{kk}", vv
        else:
            yield k, v


def lookup(setup, path):
    cur = setup
    for part in path.split("."):
        if not isinstance(cur, dict) or part not in cur:
            return None, False
        cur = cur[part]
    return cur, True


def near(a, b):
    if isinstance(a, bool) or isinstance(b, bool):
        return bool(a) == bool(b)
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        return abs(float(a) - float(b)) < 1e-6
    return a == b


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("export")
    ap.add_argument("--port", default="COM3")
    ap.add_argument("--baud", type=int, default=230400)
    ap.add_argument("--persist", action="store_true",
                    help="also commit to NVS (set_setup persist:true)")
    ap.add_argument("--dry-run", action="store_true",
                    help="print the translated payload and exit")
    a = ap.parse_args()

    with open(a.export, encoding="utf-8") as f:
        export = json.load(f)

    # machine_id must never ride in from another board's export. It leaked once
    # (fixed 2026-08-18); refuse rather than trust the file.
    if "machine_id" in export:
        sys.exit("REFUSING: this export carries machine_id -- it would stamp "
                 "another board's identity. Remove it and re-run.")

    payload, skipped = translate(export)
    if skipped:
        print("not sent (not firmware config):", ", ".join(sorted(skipped)))

    if a.dry_run:
        print(json.dumps(payload, indent=1))
        return 0

    print(f"opening {a.port} @ {a.baud} (this reboots the ESP32)")
    ser = serial.Serial(a.port, a.baud, timeout=0.4)
    time.sleep(2.5)

    cmd = dict(payload)
    cmd["type"] = "set_setup"
    if a.persist:
        cmd["persist"] = True          # MachineConfig::save() -- writes NVS
    reply, raw = txn(ser, cmd, want="ack")
    if reply is None:
        ser.close()
        sys.exit("no reply to set_setup. raw tail:\n" + raw.decode("latin1", "replace")[-400:])
    print("set_setup ack:", reply.get("ack"))
    for k in ("unknown", "unknown_keys", "stale", "persisted", "persist_err"):
        if k in reply:
            print(f"  {k}: {reply[k]}")

    # Read back and compare. This is the half that makes it a provisioning step
    # rather than a hopeful write.
    setup, raw = txn(ser, {"type": "get_setup"}, want="machine_id")
    ser.close()
    if setup is None:
        sys.exit("no get_setup reply to verify against")

    ok = bad = derived = 0
    problems = []
    for path, want in flatten_expected(payload):
        if path == "type":
            continue
        got, found = lookup(setup, path)
        leaf = path.split(".")[-1]
        if path.startswith("stage_pulse_offset.") and leaf in DERIVED_SPO:
            derived += 1
            continue
        if not found:
            bad += 1
            problems.append((path, want, "(absent)"))
        elif near(got, want):
            ok += 1
        else:
            bad += 1
            problems.append((path, want, got))

    print(f"\nverify: {ok} match, {bad} mismatch, {derived} derived (skipped)")
    if problems:
        print(f"{'key':34} {'wanted':>14}  got")
        for p, w, g in problems:
            print(f"{p:34} {str(w):>14}  {g}")
    print(f"\nmachine_id   {setup.get('machine_id')}")
    print(f"cfg_from_nvs {setup.get('cfg_from_nvs')}   io_armed {setup.get('io_armed')}"
          f"   cfg_crc {setup.get('cfg_crc')}")
    if not a.persist:
        print("\nRAM ONLY -- nothing written to NVS. Re-run with --persist to commit.")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
