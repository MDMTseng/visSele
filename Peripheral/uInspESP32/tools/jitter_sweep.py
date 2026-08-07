#!/usr/bin/env python3
"""Where does the pairing actually break? Sweep the noise, not the load.

Two earlier instruments were wrong in ways worth recording, because both looked
like they were working:

  Host-side injection. Every phantom test until now paced pulses from Python
  over TCP. Measured spacing: min 42ms, median 80ms, p95 90ms, against a
  requested 33ms -- jitter of +-47ms, NINE TIMES the 5000us match window. So the
  harness could not even hit its own set point, and the regime where a slip is
  possible (neighbour near the window) was never reachable. Every clean result
  from that rig carries that discount. This uses the device's own train
  (trig_phantom_train), scheduled against esp_timer, and reports the spacing it
  actually achieved so the claim is checkable rather than assumed.

  Perfectly even spacing. A regular train is degenerate: every object sits at
  the same offset from its neighbour, so the match either always works or always
  fails and the boundary is never explored. Real lines are regular-ISH, and the
  risk is a tail event -- the occasional short interval that brings a neighbour
  close enough. So: nominal pitch plus swept jitter, which measures the margin
  as a distribution.

The noise is seeded on the device, so a jitter value that produces a mis-sort
replays exactly. That matters here: one misplaced-verdict run earlier in this
work could not be reproduced in thirteen attempts and so could not be
attributed, which is most of the way to not having found it at all.

Pass condition per row is unchanged and absolute: zero misplaced verdicts. A
halt is acceptable -- refusing to answer is the designed behaviour. Answering
the wrong part is not.

  INSP_PERIF_VERDICT_PATTERN=20260806 <core>
  python3 jitter_sweep.py --period-us 40000
"""
import socket, time, json, argparse

PORT = 4099
CONN = {"type": "CONNECT", "uart_name": "/dev/cu.usbserial-0001",
        "baudrate": 230400, "machine_type": "uInspESP32",
        "cat_ok": 3, "cat_ng": 1, "cam_idx": 1, "pairing": "timestamp"}
M32 = 0xFFFFFFFF


def expect(tid, seed):
    h = ((tid & M32) * 2654435761 + seed) & M32
    h ^= h >> 15
    h = (h * 2246822519) & M32
    h ^= h >> 13
    h = (h * 3266489917) & M32
    h ^= h >> 16
    return CONN['cat_ok'] if (h & 1) == 0 else CONN['cat_ng']


def sock():
    s = socket.create_connection(('127.0.0.1', PORT), timeout=5)
    s.settimeout(0.4)
    return s


def send(s, *cmds, gap=0.3):
    for c in cmds:
        s.sendall((c if isinstance(c, str) else json.dumps(c)).encode() + b'\n')
        time.sleep(gap)
        try:
            s.recv(65536)
        except socket.timeout:
            pass


def ask(s, obj, key, listen=2.5):
    s.sendall(json.dumps(obj).encode() + b'\n')
    buf, t0 = b'', time.time()
    while time.time() - t0 < listen:
        try:
            buf += s.recv(8192)
        except socket.timeout:
            continue
    for l in buf.decode(errors='replace').splitlines():
        if key in l:
            try:
                return json.loads(l)
            except Exception:
                pass
    return None


def one(s, a, jitter_us):
    """One jitter value. Returns a dict of what happened."""
    send(s, {"type": "clear_error"},
            {"type": "set_setup", "min_detect_sep_us": a.gate_sep_us,
             "cam_match_window_us": a.window_us,
             "unanswered_stop_after": 100000},
            {"type": "set_setup", "plate_freq": 15000},
            {"type": "stepper_disable"},
            {"type": "enter_insp_mode"})
    j, t0 = None, time.time()
    while time.time() - t0 < 60:
        j = ask(s, {"type": "get_running_stat"}, 'cam_sync', listen=1.5)
        if j and j.get('state') == 101:
            break
        if j and j.get('state') in (112, 113):
            break
        time.sleep(1.0)
    if not j or j.get('state') != 101:
        return {"jitter": jitter_us, "err": "never READY (state=%s)"
                % (j or {}).get('state')}

    send(s, {"type": "clear_verdict_log"})
    n = int(a.seconds * 1e6 / a.period_us)
    send(s, {"type": "trig_phantom_train", "count": n,
             "period_us": a.period_us, "jitter_us": jitter_us,
             "seed": a.seed})

    seen = {}

    def harvest():
        vl = ask(s, {"type": "get_verdict_log"}, '"cat"', listen=1.0)
        if vl and 'tid' in vl and 'cat' in vl:
            for t, c in zip(vl['tid'], vl['cat']):
                seen[t] = c

    t_end = time.time() + a.seconds + 8
    halted = None
    while time.time() < t_end:
        time.sleep(1.2)
        harvest()
        st = ask(s, {"type": "get_running_stat"}, 'cam_sync', listen=1.0)
        if st and st.get('state') in (112, 113):
            halted = st
            break
    time.sleep(3)
    harvest()
    st = halted or ask(s, {"type": "get_running_stat"}, 'cam_sync')

    # The train reports the spacing it actually achieved -- read it back by
    # re-issuing with count 0, which returns the previous train's min/max before
    # arming a new one. Without this the whole sweep is an assumption again.
    tr = ask(s, {"type": "trig_phantom_train", "count": 0}, 'prev_min_us')

    pairs = [(t, c) for t, c in sorted(seen.items()) if t < 0x40000000]
    bad = [(t, c) for t, c in pairs if c != expect(t, a.seed)]
    cs = (st or {}).get('cam_sync', {})
    send(s, {"type": "set_setup", "plate_freq": 0}, {"type": "exit_insp_mode"})
    return {
        "jitter": jitter_us,
        "n": len(pairs),
        "bad": len(bad),
        "state": (st or {}).get('state'),
        "err": (st or {}).get('error_hist'),
        "delta_max": cs.get('delta_max_us'),
        "miss_max": cs.get('miss_delta_max_us'),
        "rejected": cs.get('rejected'),
        "disagree": cs.get('disagree'),
        "sp_min": (tr or {}).get('prev_min_us'),
        "sp_max": (tr or {}).get('prev_max_us'),
        "first_bad": bad[:3],
    }


def main(a):
    s = sock()
    send(s, "!pd " + json.dumps(CONN), gap=1.0)
    time.sleep(4.0)
    print("nominal pitch %dus, window %dus, gate %dus, seed %d, %ds per row"
          % (a.period_us, a.window_us, a.gate_sep_us, a.seed, a.seconds))
    # DISAGREE is the attribution, and without it a BAD count is unreadable.
    #
    # The pattern is keyed on the tid the CORE named; the device assigns by its
    # own timestamp match. A mismatch therefore means the two picked different
    # objects -- it does NOT say which one was wrong. `disagree` counts the same
    # events from the device's side, and delta_max says how well the device's
    # match was doing: a large BAD alongside a tiny delta_max means the device
    # was landing on its object cleanly and the CORE's pairing is the one that
    # moved.
    print("%-9s %-6s %-5s %-5s %-6s %-9s %-9s %-8s %s"
          % ("jitter", "n", "BAD", "DISAG", "state", "spacing", "delta_max",
             "miss_max", "err"))
    rows = []
    for jit in a.jitters:
        r = one(s, a, jit)
        rows.append(r)
        if r.get("err") and "n" not in r:
            print("%-9d %s" % (jit, r["err"]))
            continue
        print("%-9d %-6d %-5d %-5s %-6s %-9s %-9s %-8s %s"
              % (jit, r["n"], r["bad"], r["disagree"], r["state"],
                 "%s-%s" % (r["sp_min"], r["sp_max"]),
                 r["delta_max"], r["miss_max"], r["err"]))
        if r["bad"]:
            print("          first misplaced: %s" % r["first_bad"])
    s.close()

    worst = [r for r in rows if r.get("bad")]
    print("")
    if worst:
        print("MIS-SORTED at jitter >= %d us (nominal %d, window %d)"
              % (worst[0]["jitter"], a.period_us, a.window_us))
        print("replay: --jitters %d --seed %d" % (worst[0]["jitter"], a.seed))
        return 1
    print("no mis-sort at any jitter up to %d us on nominal %d us"
          % (a.jitters[-1], a.period_us))
    return 0


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument("--period-us", type=int, default=40000)
    ap.add_argument("--window-us", type=int, default=5000)
    # The gate must not be the thing that rejects the tight intervals, or the
    # sweep measures the gate instead of the pairing.
    ap.add_argument("--gate-sep-us", type=int, default=2000)
    ap.add_argument("--seconds", type=int, default=45)
    ap.add_argument("--seed", type=int, default=20260806)
    ap.add_argument("--jitters", type=int, nargs="+",
                    default=[0, 2000, 5000, 10000, 20000, 35000, 39000])
    a = ap.parse_args()
    raise SystemExit(main(a))
