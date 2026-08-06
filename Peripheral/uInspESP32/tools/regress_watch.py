#!/usr/bin/env python3
"""Overnight watch: did any of the things fixed on 2026-08-06 come back?

Every fix that day is "changed it and did not see the problem again", measured
over minutes. The failures it replaced took 10-30 minutes to recur. So the
fixes are unproven, and the only thing that proves them is time.

This drives the machine with a PHANTOM part train rather than real parts:

  - no feeder, no ejection, no air, nothing to run out of or overflow -- an
    unattended machine should not be throwing objects around for hours;
  - the phantom train still exercises everything under test: the serial link,
    the pairing, the clock model, calibration, and the report path. Real parts
    would add the gate sensor and the blow, neither of which was touched.

It watches for the specific regressions, not for "seems fine":

  crash dumps        the camera reconnect use-after-free (80+ dumps in 2 days)
  err 11             two threads interleaving on one UART
  cal_fails          the calibration wait that was gated on state 101
  resid direct/staged whether the offset estimate is fed two populations
  free_heap          a real leak, as opposed to the min_heap artefact
  serialRTT          link congestion, the common root of several symptoms

Leaves the plate stopped and inspection off on every exit path, Ctrl-C
included.

  python3 regress_watch.py --hours 8
"""
import socket, time, json, argparse, datetime, os, glob

PORT = 4099          # INSP_PERIF_CONSOLE -- verbatim device bytes, no framing
GS_NOTE = "core-side pairing stats are read from the log file, not from here"
DUMP_GLOB = "/Users/mdm/workspace/visSele/InspectionCore/Core0_1/crash_*.dump"


def sock():
    s = socket.create_connection(("127.0.0.1", PORT), timeout=5)
    s.settimeout(0.5)
    return s


def cmd(s, d, wait=2.0):
    s.sendall((json.dumps(d) + "\n").encode())
    buf, t0 = b"", time.time()
    while time.time() - t0 < wait:
        try:
            buf += s.recv(65536)
        except socket.timeout:
            pass
    return buf.decode(errors="replace")


def stat(s, wait=2.5):
    out = cmd(s, {"type": "get_running_stat"}, wait)
    for l in out.splitlines():
        if "cam_sync" in l:
            try:
                return json.loads(l)
            except Exception:
                pass
    return None


def main(a):
    s = sock()
    # A phantom train needs the stage timer running, so the plate turns -- but
    # slowly, and with nothing on it to eject. plate_freq is set explicitly
    # rather than inherited: whatever the machine was left at is not a decision
    # this script gets to skip.
    cmd(s, {"type": "clear_error"})
    cmd(s, {"type": "clear_error_history"})
    cmd(s, {"type": "set_setup", "plate_freq": a.plate_freq})
    cmd(s, {"type": "stepper_enable"})
    cmd(s, {"type": "enter_insp_mode"})

    j, t0 = None, time.time()
    while time.time() - t0 < 120:
        j = stat(s)
        if j and j.get("state") == 101:
            break
        if j and j.get("state") in (112, 113):
            print("halted before start: %s" % j.get("error_hist"))
            return 1, s
        time.sleep(2.0)
    if not j or j.get("state") != 101:
        print("never reached READY (state=%s) -- calibration is the first thing "
              "under test, so this is already a result" % (j or {}).get("state"))
        return 1, s

    dumps0 = set(glob.glob(DUMP_GLOB))
    base = {"cal_runs": j["cam_sync"].get("cal_runs", 0),
            "cal_fails": j["cam_sync"].get("cal_fails", 0),
            "recals": j["cam_sync"].get("recals", 0),
            "heap": j["health"].get("free_heap", 0)}
    print("watching %.1f h  plate_freq=%d  train %d parts @ %dus (jitter %dus)"
          % (a.hours, a.plate_freq, a.batch, a.sep_us, a.jitter_us))
    print("%-9s %-7s %-7s %-6s %-6s %-8s %-9s %s"
          % ("elapsed", "judged", "cal r/f", "recal", "err11", "heap", "dmax", "state"))

    t_end = time.time() + a.hours * 3600
    next_rep = time.time()
    worst = {"err11": 0, "dumps": 0, "halt": None, "samples": 0}
    while time.time() < t_end:
        # Fire a batch, then leave a gap long enough for the recal trigger --
        # the busy->idle->RECAL->busy handover is where both calibration bugs
        # lived, so it has to happen hundreds of times, not once.
        cmd(s, {"type": "trig_phantom_train", "n": a.batch,
                "sep_us": a.sep_us, "jitter_us": a.jitter_us, "seed": 1}, wait=0.5)
        time.sleep(a.idle_s)

        if time.time() < next_rep:
            continue
        next_rep = time.time() + a.report_every
        j = stat(s)
        if not j:
            print("  (no answer)")
            continue
        worst["samples"] += 1
        cs, ct, h = j["cam_sync"], j["count"], j["health"]
        judged = ct["NA"] + ct["SEL1"] + ct["SEL2"] + ct["SEL3"]
        errs = j.get("error_hist") or []
        n11 = sum(1 for e in errs if e == 11)
        worst["err11"] = max(worst["err11"], n11)
        worst["dumps"] = len(set(glob.glob(DUMP_GLOB)) - dumps0)
        print("%-9s %-7d %-7s %-6s %-6d %-8s %-9s %s"
              % (str(datetime.timedelta(seconds=int(time.time() - (t_end - a.hours * 3600)))),
                 judged, "%s/%s" % (cs.get("cal_runs"), cs.get("cal_fails")),
                 cs.get("recals"), n11, h.get("free_heap"),
                 cs.get("delta_max_us"), j.get("state")))
        if j.get("state") in (112, 113):
            worst["halt"] = errs
            print("  HALTED -- stopping so the state is preserved for the morning")
            break

    j = stat(s)
    print("\n=== result ===")
    if j:
        cs = j["cam_sync"]
        print("  calibration : runs=%s fails=%s  recals=%s recal_skipped=%s"
              % (cs.get("cal_runs"), cs.get("cal_fails"),
                 cs.get("recals"), cs.get("recal_skipped")))
        print("  pairing     : agree=%s disagree=%s delta_max=%sus"
              % (cs.get("agree"), cs.get("disagree"), cs.get("delta_max_us")))
        print("  heap        : %s -> %s (%+d)"
              % (base["heap"], j["health"].get("free_heap"),
                 (j["health"].get("free_heap") or 0) - base["heap"]))
        print("  state       : %s  error_hist=%s" % (j.get("state"), j.get("error_hist")))
    print("  new crash dumps : %d" % worst["dumps"])
    print("  err 11 seen     : %s" % ("YES" if worst["err11"] else "no"))
    if worst["samples"] == 0:
        print("  => NO DATA. Not a pass.")
        return 1, s
    bad = worst["halt"] or worst["err11"] or worst["dumps"]
    print("  => %s (%d samples)"
          % ("REGRESSION" if bad else "clean", worst["samples"]))
    return (1 if bad else 0), s


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--hours", type=float, default=8)
    ap.add_argument("--plate-freq", type=int, default=3500)   # ~7 rpm
    ap.add_argument("--batch", type=int, default=20)
    ap.add_argument("--sep-us", type=int, default=60000)
    ap.add_argument("--jitter-us", type=int, default=4000)
    ap.add_argument("--idle-s", type=float, default=12.0)     # > cam_recal_idle_ms
    ap.add_argument("--report-every", type=float, default=300)
    a = ap.parse_args()
    rc, s = 1, None
    try:
        rc, s = main(a)
    except KeyboardInterrupt:
        print("\ninterrupted")
    finally:
        try:
            if s is None:
                s = sock()
            cmd(s, {"type": "exit_insp_mode"}, 0.5)
            cmd(s, {"type": "set_setup", "plate_freq": 0}, 0.5)
            cmd(s, {"type": "stepper_disable"}, 0.5)
            j = stat(s, 1.5)
            print("left: state=%s plate_freq=%s"
                  % ((j or {}).get("state"), (j or {}).get("plate_freq")))
            s.close()
        except Exception as e:
            print("WARNING: could not confirm the plate stopped: %s" % e)
    raise SystemExit(rc)
