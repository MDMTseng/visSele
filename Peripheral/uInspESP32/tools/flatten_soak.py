#!/usr/bin/env python3
"""Flatten the soak JSONL into a wide CSV, plus a separate event list.

Each sample record nests gate/pipe/count/health/cam_sync objects. Analysis wants
columns, and an agent reading a few MB of nested JSON to answer "did the pulse
width drift with speed" is paying a lot for very little. So: one row per sample,
one line per event, and the raw file stays for anything the columns miss.

Nothing is judged or filtered here. Derived columns are only arithmetic that
every consumer would otherwise redo (tick length, deltas of monotonic counters,
the asked-vs-delivered difference).
"""
import csv, json, sys

SRC = sys.argv[1]
CSV_OUT = sys.argv[2] if len(sys.argv) > 2 else SRC.replace(".jsonl", ".csv")
EVT_OUT = sys.argv[3] if len(sys.argv) > 3 else SRC.replace(".jsonl", ".events.txt")

FLAT = [
    ("t", lambda r: r.get("t")),
    ("setp", lambda r: r.get("setp")),
    ("pending", lambda r: r.get("pending")),
    ("meas", lambda r: r.get("meas")),
    ("state", lambda r: r.get("state")),
    ("band_pct", lambda r: r.get("band")),
    ("accel", lambda r: r.get("accel")),
    ("cam1_win_t", lambda r: r.get("cam1_win")),
    ("sel1_win_t", lambda r: r.get("sel1_win")),
    ("cam1_ask_us", lambda r: r.get("cam1_w_us")),
    ("sel1_ask_us", lambda r: r.get("sel1_w_us")),
    ("act_late_max", lambda r: r.get("act_late_max")),
]
GATE = ["accept", "rej_rate", "rej_dist", "rej_busy", "rej_width", "rej_unstable",
        "rej_stepper_off", "rej_gate_off", "rej_dryrun", "rej_blocked", "edges",
        "eff_sep_us", "eff_hz", "freq_stable", "auto_backoffs", "auto_recovers"]
PIPE = ["registered", "waiting"]
COUNT = ["SEL1", "SEL2", "SEL3", "NA", "UNANSWERED", "SKIP", "SEL_SUPPRESSED",
         "FREQ_TXN", "FREQ_TXN_TIMEOUT", "FREQ_TXN_DRAIN_MAX_MS"]
HEALTH = ["isr_dur_max_us", "isr_dur_env_us", "isr_dur_avg_us", "isr_dur_last_us",
          "isr_overrun_n", "isr_ticks", "isr_gap_max_us",
          "cam1_pw_last_us", "cam1_pw_min_us", "cam1_pw_max_us", "cam1_pw_mean_us",
          "cam1_pw_n", "cam1_pw_err_max_us", "cam1_pw_err_env_us",
          "cam1_pw_err_at_us", "cam1_pw_err_ask_us",
          "act_grow_n", "act_cap_n", "act_cap_max_t", "band_out_ms",
          "isr_npe_max_cy", "rbuf_peak", "free_heap", "min_heap", "max_block",
          "stack_hwm", "uptime_s", "consec_unanswered",
          "rx_frames", "rx_crc_ok", "rx_crc_fail"]
SYNC = ["valid", "agree", "disagree", "rejected", "rebuilds", "resid_max_us",
        "delta_max_us", "sync_pulses", "cal_runs", "cal_fails"]

cols = ([c for c, _ in FLAT]
        + ["gate_" + k for k in GATE] + ["pipe_" + k for k in PIPE]
        + ["cnt_" + k for k in COUNT] + ["h_" + k for k in HEALTH]
        + ["sync_" + k for k in SYNC]
        + ["tick_us", "pw_err_us", "d_accept", "d_isr_ticks", "dt"])

rows, events, prev = [], [], None
for line in open(SRC):
    r = json.loads(line)
    k = r.get("kind")
    if k != "s":
        events.append("%8.2f  %-16s %s" % (
            r.get("t", 0), k,
            json.dumps({a: b for a, b in r.items() if a not in ("t", "kind")})))
        continue
    g, p, c = r.get("gate") or {}, r.get("pipe") or {}, r.get("count") or {}
    h, y = r.get("health") or {}, r.get("cam_sync") or {}
    row = {n: f(r) for n, f in FLAT}
    for kk in GATE:   row["gate_" + kk] = g.get(kk)
    for kk in PIPE:   row["pipe_" + kk] = p.get(kk)
    for kk in COUNT:  row["cnt_" + kk] = c.get(kk)
    for kk in HEALTH: row["h_" + kk] = h.get(kk)
    for kk in SYNC:   row["sync_" + kk] = y.get(kk)

    f_now = r.get("meas") or r.get("setp") or 0
    row["tick_us"] = round(1e6 / (2.0 * f_now), 3) if f_now else ""
    ask, got = r.get("cam1_w_us"), h.get("cam1_pw_last_us")
    row["pw_err_us"] = (got - ask) if (ask and got) else ""
    if prev:
        row["dt"] = round(r["t"] - prev["t"], 3)
        for a, b in (("d_accept", ("gate", "accept")),
                     ("d_isr_ticks", ("health", "isr_ticks"))):
            cur = (r.get(b[0]) or {}).get(b[1])
            old = (prev.get(b[0]) or {}).get(b[1])
            row[a] = (cur - old) if (cur is not None and old is not None) else ""
    prev = r
    rows.append(row)

with open(CSV_OUT, "w", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=cols, extrasaction="ignore")
    w.writeheader()
    w.writerows(rows)
open(EVT_OUT, "w").write("\n".join(events) + "\n")
print("%s: %d samples -> %s" % (SRC, len(rows), CSV_OUT))
print("%d events -> %s" % (len(events), EVT_OUT))
