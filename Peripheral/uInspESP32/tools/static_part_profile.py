#!/usr/bin/env python3
"""Profile the whole inspection chain against ONE real part, held still.

    python3 static_part_profile.py                     # jog a part in, 60s at 30/s
    python3 static_part_profile.py --seconds 300       # longer
    python3 static_part_profile.py --no-jog            # part already placed
    python3 static_part_profile.py --rate 35 --deffile data/4444.hydef

WHY THIS SHAPE

Every other harness here trades away the thing you want to measure.

  real_parts.py       production traffic, but each part is in shot for exactly
                      one frame and lands wherever it lands. A run can be
                      perfectly clean and still tell you nothing about the
                      inspection, because most frames locate nothing.
  trig_cam_burst      a real part in every frame, but it bypasses the pipeline:
                      no gate, no pairing, no cam_trig announcements, no stage
                      tasks. It measures the inspection and nothing around it.

This gets both at once. jog parks one part under the camera; the stepper is then
de-energised so the plate cannot physically move, while SYS_STEP_COUNT keeps
advancing -- the plate rotates LOGICALLY and every stage task fires on its normal
schedule. virt_pulse injects objects at the gate on the ISR's timebase. The wire
carries exactly what production carries, and every single frame contains the
same real part in the same place.

PLATE_FREQ_MEAS comes from SYS_STEP_COUNT, not an encoder, so spin-up converges
with the motor off.

WHAT IT CANNOT SEE

  * SEL actuation is gated on !SYS_STEPPER_DISABLED, so nothing is ejected.
    Verdicts are produced, paired and reported; only the blow is inert. Expect
    SEL1/SEL2/SEL3 to read 0 -- that is the rig, not a fault.
  * The part never changes pose. Real parts arrive at a different angle every
    time and matching costs more for them: measured 5.1ms here against 8.5ms
    with the plate actually turning. Treat this as the pipeline's FLOOR.

REFERENCE (2026-08-13, MV-CA050-11UM, test1.hydef, 30/s, two runs)

                                  hand-placed part      jogged-in part
    cam_lat   trigger -> verdict   18.20ms / max 51.0    20.94ms / max 43.5
    acquisition  -> frame at core  11.55ms / max 18.9    11.69ms / max 21.6
    core e2e  frame -> written      6.70ms / max 38.9     9.28ms / max 31.6
    match                           5.14ms / max 12.2     7.46ms / max 28.3
    judged                          1353/1353             951/951

    Both runs close on themselves -- 11.55+6.70 = 18.25 against a measured
    18.20, and 11.69+9.45 = 21.14 against 20.94 -- which is the check that this
    is a decomposition and not three unrelated averages. Print that line and
    distrust the run if it drifts far from 0.

    But `match` differs by 45% between them, and that is the rig, not noise:
    jog parks the part wherever the gate edge and the braking distance leave it,
    and matching cost depends on where in the frame the part sits. Compare runs
    of THIS script against each other only when the part has not been re-jogged,
    and never quote a single run's `match` as "the" inspection cost.

    The acquisition leg is the stable one: 11.55 vs 11.69ms across both, and
    99.9% of frames inside one 10-20ms bucket. It does not care where the part
    is, how fast objects arrive, or whether the plate turns.

The plate is left stopped and both lights off on every exit path, including
Ctrl-C and an early failure.
"""
import argparse
import json
import socket
import sys
import time

from real_parts import CONN

PORT = 4099
EDGES_MS = [5, 10, 20, 40, 80, 160, 320]


def say(*a):
    print(*a, flush=True)


class Link:
    def __init__(self, port=PORT):
        self.s = socket.create_connection(("127.0.0.1", port), timeout=5)
        self.s.settimeout(0.25)

    def send(self, obj, wait=1.2):
        line = obj if isinstance(obj, str) else json.dumps(obj)
        self.s.sendall(line.encode() + b"\n")
        buf, t0 = b"", time.time()
        while time.time() - t0 < wait:
            try:
                buf += self.s.recv(65536)
            except socket.timeout:
                pass
        return buf.decode(errors="replace")

    def pick(self, raw, key):
        for l in raw.splitlines():
            l = l.strip()
            if l.startswith("{") and key in l:
                try:
                    return json.loads(l.split("*")[0])
                except Exception:
                    pass
        return None

    def stat(self, wait=1.8):
        return self.pick(self.send({"type": "get_running_stat"}, wait), '"free_heap"')

    def setup(self, wait=2.0):
        return self.pick(self.send({"type": "get_setup"}, wait), "stage_pulse_offset")

    def spikes(self, wait=1.2):
        return self.pick(self.send({"type": "get_spikes"}, wait), '"get_spikes"')

    def lat(self, wait=3.5):
        """The core's per-stage table, via the perif console's '?lat'."""
        out = {}
        for ln in self.send("?lat", wait).splitlines():
            p = ln.split()
            if len(p) >= 4 and p[0] in ("queue", "match", "match_cpu", "rep_json",
                                        "insp_off", "inspect", "wait", "write", "e2e"):
                try:
                    out[p[0]] = (int(p[1]), float(p[2]), float(p[3]))
                except Exception:
                    pass
            elif ln.strip().startswith("verdict") or ln.strip().startswith("no_report"):
                for tok in ln.split():
                    if "=" in tok:
                        k, v = tok.split("=", 1)
                        try:
                            out.setdefault("v", {})[k] = int(v)
                        except ValueError:
                            pass
        return out


def bucket_label(i):
    if i == 0:
        return "<%dms" % EDGES_MS[0]
    if i == 7:
        return ">=%dms" % EDGES_MS[6]
    return "%d-%dms" % (EDGES_MS[i - 1], EDGES_MS[i])


def show_hist(title, n, avg_us, max_us, h):
    total = sum(h) or 1
    say("%-42s n=%s avg=%.2fms max=%.2fms" % (title, n, avg_us / 1000.0, max_us / 1000.0))
    for i, c in enumerate(h):
        if c:
            say("   %-9s %6d  %5.2f%%" % (bucket_label(i), c, 100.0 * c / total))


def jog_part_to_camera(lk, arm_freq, timeout_s):
    """Spin the plate until a part trips the gate, then park it at CAM1_on.

    The target is read from the device rather than hardcoded: stage offsets are
    tuning knobs and a stale constant here would silently place the part
    somewhere else. jog offsets are ABSOLUTE and in the same units.
    """
    st = lk.setup()
    if not st:
        say("could not read stage_pulse_offset"); return False
    target = st["stage_pulse_offset"]["CAM1_on"]

    r = lk.send({"type": "jog_arm", "freq": float(arm_freq)}, 1.5)
    j = lk.pick(r, '"jog_arm"') or {}
    if j.get("err"):
        say("jog_arm refused: %s (state %s)" % (j["err"], j.get("state")))
        return False
    say("jog armed at %s -- waiting for a part on the gate edge" % arm_freq)

    t0 = time.time()
    while time.time() - t0 < timeout_s:
        time.sleep(2.0)
        d = lk.stat(1.4) or {}
        if (d.get("jog") or {}).get("state") == 2:
            say("  caught: origin=%s" % d["jog"].get("origin"))
            break
    else:
        say("no part caught in %.0fs -- is there anything on the plate?" % timeout_s)
        lk.send({"type": "jog_end"}, 1.0)
        return False

    say("  moving to CAM1_on=%d" % target)
    lk.send({"type": "jog", "offset": int(target)}, 1.5)
    t0 = time.time()
    while time.time() - t0 < 90:
        time.sleep(1.0)
        j = (lk.stat(1.4) or {}).get("jog", {})
        if not j.get("moving"):
            say("  parked at disp=%s (target %d)" % (j.get("disp"), target))
            break
    lk.send({"type": "jog_end"}, 1.5)
    time.sleep(1.0)
    return True


def main(a, lk):
    # The perif console starts listening ~20s before bpg_pi.camera is assigned,
    # and the FI handler calls camera->TriggerMode() with no null check -- FI
    # inside that window segfaults the core, reproducibly. There is no readiness
    # signal to poll, so this waits it out and then checks the core survived.
    if a.core_wait > 0:
        say("waiting %.0fs for the core to finish opening the camera" % a.core_wait)
        time.sleep(a.core_wait)
    say(lk.send("!fi " + json.dumps({"deffile": a.deffile}), 10.0).strip()[:120])
    # Parsing the def takes a few seconds (trainShapeMatcher builds 360
    # variants), so one impatient probe reads as death when the core is simply
    # busy. Only a run of silences means it really went.
    for _ in range(4):
        if lk.lat(4.0):
            break
        time.sleep(2.0)
    else:
        say("no reply from the core after FI -- it probably died; check for a "
            "crash_*.dump and retry with a larger --core-wait")
        return 1

    lk.send("!pd " + json.dumps(CONN), 3.0)
    time.sleep(6)
    lk.send({"type": "clear_error"})
    lk.send({"type": "set_setup", "plate": {"freq": 0}})

    if not a.no_jog:
        # Needs the motor: this is the one phase where the plate really turns.
        lk.send({"type": "stepper_enable"})
        if not jog_part_to_camera(lk, a.jog_freq, a.jog_timeout):
            return 1

    # From here the plate must not move. stepper_disable needs IDLE + freq 0,
    # the same guard the NVS save uses.
    lk.send({"type": "set_setup", "plate": {"freq": 0}})
    time.sleep(1.0)
    r = lk.pick(lk.send({"type": "stepper_disable"}, 1.5), '"stepper_disable"') or {}
    if r.get("err"):
        say("stepper_disable refused: %s (state %s)" % (r["err"], r.get("state")))
        return 1
    say("stepper de-energised -- the plate now rotates logically only")

    period = int(round(2.0 * a.plate_freq / a.rate))
    lk.send({"type": "set_setup", "plate": {"freq": a.plate_freq}})
    lk.send({"type": "enter_insp_mode"})
    d = None
    for _ in range(45):
        time.sleep(2)
        d = lk.stat(1.4)
        if d and d.get("state") == 101:
            break
        if d and d.get("state") in (112, 113):
            say("HALTED before start: state=%s err=%s" % (d.get("state"), d.get("error_hist")))
            return 1
    if not d or d.get("state") != 101:
        say("never reached READY: state=%s err=%s"
            % ((d or {}).get("state"), (d or {}).get("error_hist")))
        return 1

    gate = d.get("gate", {})
    gap_us = 1e6 / a.rate
    if gate.get("min_sep_us") and gap_us < gate["min_sep_us"]:
        say("WARNING: %.1f/s is a %.0fus gap, under the gate's %sus floor -- it "
            "will alternate accept/reject and the real rate will be half"
            % (a.rate, gap_us, gate["min_sep_us"]))
    say("READY  plate_meas=%s (logical)  %.0fs at %.1f obj/s (%d ticks)"
        % (d.get("plate_freq_meas"), a.seconds, 2.0 * a.plate_freq / period, period))

    lk.send({"type": "reset_latency_stat"})
    c0 = (lk.stat(1.6) or {}).get("count", {})
    l0 = lk.lat()
    lk.send({"type": "virt_pulse", "period_ticks": period, "jitter_ticks": 0})
    t0 = time.time()
    while time.time() - t0 < a.seconds:
        time.sleep(min(20.0, max(1.0, a.seconds - (time.time() - t0))))
        d = lk.stat(1.6) or {}
        c, g = d.get("count", {}), d.get("gate", {})
        say("   t+%3.0fs edges=%s accept=%s rej_busy=%s NA=%s err=%s"
            % (time.time() - t0, g.get("edges"), g.get("accept"),
               g.get("rej_busy"), c.get("NA"), d.get("error_hist")))
        if d.get("state") in (112, 113):
            say("   HALTED -- stopping the run")
            break
    lk.send({"type": "virt_pulse", "period_ticks": 0})
    time.sleep(3.0)

    sp = lk.spikes(1.6) or {}
    l1 = lk.lat()
    say("")
    show_hist("cam_lat      trigger -> verdict in hand", sp.get("cam_n"),
              sp.get("cam_avg_us", 0), sp.get("cam_max_us", 0), sp.get("cam_hist", [0] * 8))
    say("")
    show_hist("acquisition  trigger -> frame at the core", sp.get("acq_n"),
              sp.get("acq_avg_us", 0), sp.get("acq_max_us", 0), sp.get("acq_hist", [0] * 8))
    say("   loop_max=%.2fms  spikes>60ms=%s  reports with no hus=%s"
        % (sp.get("loop_max_us", 0) / 1000.0, sp.get("n"), sp.get("acq_nohus")))

    say("")
    say("%-10s %8s %10s %10s" % ("core stage", "frames", "avg_ms", "max_ms"))
    for k in ("queue", "match", "match_cpu", "rep_json", "insp_off",
              "inspect", "wait", "write", "e2e"):
        b = l1.get(k)
        if not b:
            continue
        x = l0.get(k)
        dn = b[0] - (x[0] if x else 0)
        # Window average: the core's histograms accumulate from boot, so an
        # earlier run in the same core would otherwise dilute these.
        win = (((b[1] * b[0]) - (x[1] * x[0])) / dn) if (x and dn > 0) else b[1]
        say("%-10s %8d %10.3f %10.3f" % (k, dn, win, b[2]))

    v = l1.get("v", {})
    v0 = l0.get("v", {})
    say("")
    say("verdict path: judged=%d no_object=%d multi_object=%d  no_report=%d"
        % (v.get("judged", 0) - v0.get("judged", 0),
           v.get("no_object", 0) - v0.get("no_object", 0),
           v.get("multi_object", 0) - v0.get("multi_object", 0),
           v.get("no_report", 0) - v0.get("no_report", 0)))

    d = lk.stat(2.0) or {}
    c, g = d.get("count", {}), d.get("gate", {})
    say("device: " + " ".join("%s=%d" % (k, c.get(k, 0) - c0.get(k, 0))
                              for k in ("NA", "SEL1", "SEL2", "SEL3", "UNANSWERED", "SKIP")))
    say("gate:   edges=%s accept=%s rej_rate=%s rej_busy=%s  err=%s state=%s"
        % (g.get("edges"), g.get("accept"), g.get("rej_rate"), g.get("rej_busy"),
           d.get("error_hist"), d.get("state")))

    # The check that makes the decomposition believable rather than three
    # unrelated averages.
    acq, e2e = sp.get("acq_avg_us", 0) / 1000.0, (l1.get("e2e") or (0, 0, 0))[1]
    cam = sp.get("cam_avg_us", 0) / 1000.0
    if cam:
        say("")
        say("acquisition %.2f + core e2e %.2f = %.2f  vs measured cam_lat %.2f  (%.1f%%)"
            % (acq, e2e, acq + e2e, cam, 100.0 * (acq + e2e - cam) / cam))
    return 0


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--seconds", type=float, default=60.0)
    ap.add_argument("--rate", type=float, default=30.0, help="virtual objects per second")
    ap.add_argument("--plate-freq", type=int, default=3000)
    ap.add_argument("--deffile", default="data/test1.hydef",
                    help="path RELATIVE TO THE CORE's cwd, not this script's")
    ap.add_argument("--no-jog", action="store_true",
                    help="the part is already under the camera; skip the catch")
    ap.add_argument("--jog-freq", type=int, default=3000)
    ap.add_argument("--jog-timeout", type=float, default=90.0)
    ap.add_argument("--core-wait", type=float, default=30.0,
                    help="seconds to let the core open the camera before sending "
                         "FI; sending it early segfaults the core")
    a = ap.parse_args()

    rc, lk = 1, None
    try:
        lk = Link()
        rc = main(a, lk)
    except KeyboardInterrupt:
        say("\ninterrupted")
    finally:
        # Whatever went wrong above, the plate does not get left turning and the
        # backlight does not get left on.
        try:
            if lk is None:
                lk = Link()
            lk.send({"type": "virt_pulse", "period_ticks": 0})
            lk.send({"type": "set_setup", "plate": {"freq": 0}})
            lk.send({"type": "exit_insp_mode"}, 1.5)
            time.sleep(1.5)
            lk.send({"type": "jog_end"}, 0.8)
            lk.send({"type": "stepper_enable"}, 0.8)
            lk.send({"type": "light", "ch": 1, "on": False}, 0.6)
            lk.send({"type": "light", "ch": 2, "on": False}, 0.6)
            d = lk.stat(1.5) or {}
            say("left: state=%s plate_freq=%s -- plate stopped, backlight off, "
                "stepper re-enabled" % (d.get("state"), d.get("plate_freq")))
        except Exception as e:
            say("WARNING: could not confirm the machine was left safe: %s" % e)
    raise SystemExit(rc)
