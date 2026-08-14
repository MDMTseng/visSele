#!/usr/bin/env python3
"""Hold an object rate by setting the plate speed. Estimator plus a division.

WHEN THIS APPLIES
=================
Only when the PLATE SPEED DETERMINES THE ARRIVAL RATE -- a fixed or recirculating
set of parts carried round by the plate. Then

    rate = N * plate_freq / 30000       N = parts per revolution, a constant

and the speed is a division.

It does NOT apply to a machine fed by an independent feeder (a vibratory bowl,
say). There the feeder sets the rate: at steady state the gate sees
feed_rate / removal_fraction no matter how fast the plate turns, and the plate
speed only changes how far apart the parts sit. Measuring `rate ∝ speed` on a
fixed part set and assuming it holds with a feeder is the trap; it does not.

WHAT TO CONTROL: gate.edges, NOT gate.accept
============================================
`accept` is what survives the fire-rate limiter, the minimum-distance gate and
the width filter. Targeting it makes the controller fight the limiter, because
the limiter is a fixed TIME while part spacing is a fixed DISTANCE: spinning
faster puts more pairs inside the window, so more get rejected, so the loop
spins faster still. Measured -- a loop chasing 20 accepted/s ran the plate to
14400 Hz, where a linear model said 10253, and the accept rate looked as though
it saturated. On `edges` the plant is exactly linear and rejections show up as
what they are: a separate, visible loss.

HOW TO MEASURE IT: PER REVOLUTION, NOT PER SECOND
=================================================
The parts are not evenly spaced around the disc, so a window that is not a whole
number of revolutions ALIASES that angular distribution -- which parts you count
depends on the phase. Measured at the same speed: a 5 s window (1.57 rev at
9400 Hz) swung +-12%, a 12 s window (3.1 rev) swung 1.06%. That is not counting
noise, and treating it as counting noise is what sent three tuning attempts
wrong.

So count against the STEP COUNTER, not the clock:

    N = delta_edges * 60000 / delta_ticks         (60000 steps per revolution)
    plate_freq = target_rate * 30000 / N

N is speed-independent by construction, and since it is genuinely constant the
estimate can be smoothed hard for nothing.

WHY THERE IS NO LOOP HERE
=========================
Three were tried on real parts, same session, same material:

    feedforward, no loop         +0.29% error, per-window sd 1.56%
    P + I                        -1.3%,  sd ~3%
    P only, 8% deadband          +5.3%,  sd ~3%

Both loops made the rate about three times noisier than leaving it alone. They
were correcting a plant with nothing to correct: every speed change is a
disturbance, and measurement noise comes back as one. A loop earns its keep when
N CHANGES -- parts ejected to a chute, added, or lost -- and that is slow, so the
answer is a slow estimator, not a fast controller.

A correction still has to COMPLETE before the part reaches the camera, 9315
ticks from the gate: delta_f_max = sqrt(f^2 + 9315*accel) - f. At 10500 that is
8.1% at accel 2000 and ~36% at accel 10000. STEP_CLAMP enforces it.
"""
import json, os, signal, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from peek import ask, cmd, stop_plate

TARGET  = float(sys.argv[1]) if len(sys.argv) > 1 else 15.0   # edges / second
MINUTES = float(sys.argv[2]) if len(sys.argv) > 2 else 10.0
F_LO    = int(os.environ.get("RATE_F_LO", 3000))
F_HI    = int(os.environ.get("RATE_F_HI", 10000))   # machine speed ceiling
ACCEL   = int(os.environ.get("RATE_ACCEL", 10000))
ALPHA   = 0.15     # smoothing on N; N is a constant, so this can be heavy
STEP_CLAMP = 0.30  # camera-arrival bound, see docstring
PERIOD  = 5.0
LOG = os.environ.get("RATE_HOLD_LOG", "/tmp/rate_hold.jsonl")

fh = open(LOG, "a", buffering=1)
_stop = {"v": False}
for _s in (signal.SIGTERM, signal.SIGINT):
    signal.signal(_s, lambda *a: _stop.__setitem__("v", True))


def rec(**kw):
    kw["wall"] = time.strftime("%H:%M:%S")
    fh.write(json.dumps(kw) + "\n")


def stat():
    try:
        for m in ask([{"type": "get_running_stat"}], wait=1.0):
            if "health" in m:
                return m
    except Exception as e:
        rec(kind="stat_error", err=repr(e))
    return None


def main():
    f, N_est = 8000, None
    try:
        cmd({"type": "set_setup", "plate": {"freq": f, "accel": ACCEL},
             "skip_policy": {"mode": "slow_only"}})
        time.sleep(18)
        cmd({"type": "enter_insp_mode"})
        time.sleep(8)
        s = stat()
        pe, ptk, pt = s["gate"]["edges"], s["health"]["isr_ticks"], time.time()
        t0 = pt
        rec(kind="start", target=TARGET, f_hi=F_HI, accel=ACCEL, alpha=ALPHA)
        print("target %.2f edges/s   ceiling %d   accel %d" % (TARGET, F_HI, ACCEL),
              flush=True)
        print("    t    setp   meas   N_inst  N_est   revs  edges/s   step", flush=True)
        end = time.time() + MINUTES*60
        while time.time() < end and not _stop["v"]:
            time.sleep(PERIOD)
            s = stat()
            if not s:
                continue
            now = time.time()
            e, tk = s["gate"]["edges"], s["health"]["isr_ticks"]
            d_e, d_tk = e - pe, tk - ptk
            pe, ptk = e, tk
            if d_tk <= 0:
                continue
            revs = d_tk / 60000.0
            N_inst = d_e / revs
            rate = d_e / (now - pt) if now > pt else 0.0
            pt = now
            act = "hold"
            if N_inst > 1.0:
                N_est = N_inst if N_est is None else N_est + ALPHA*(N_inst - N_est)
                want = TARGET * 30000.0 / N_est
                lo, hi = f*(1.0-STEP_CLAMP), f*(1.0+STEP_CLAMP)
                nf = int(max(F_LO, min(F_HI, max(lo, min(hi, want)))))
                if abs(nf - f) >= 8:
                    try:
                        cmd({"type": "set_setup", "plate": {"freq": nf}})
                        act = "%+.1f%%" % ((nf/f - 1)*100)
                        f = nf
                    except Exception as ex:
                        rec(kind="cmd_error", err=repr(ex))
            rec(kind="s", t=round(now-t0, 1), setp=f, meas=s.get("plate_freq_meas"),
                n_inst=round(N_inst, 3), n_est=round(N_est or 0, 3),
                revs=round(revs, 3), rate=round(rate, 3), act=act,
                gate=s.get("gate"), health=s.get("health"))
            print("%6.1f  %6d %6.0f  %7.2f %6.2f %6.2f  %6.2f  %7s"
                  % (now-t0, f, s.get("plate_freq_meas") or 0, N_inst,
                     N_est or 0, revs, rate, act), flush=True)
        rec(kind="done", stopped_by_signal=_stop["v"])
    except Exception as e:
        rec(kind="fatal", err=repr(e))
    finally:
        try:
            cmd({"type": "set_setup", "plate": {"accel": 2000}}, must_ack=False)
            rec(kind="stopped", meas=stop_plate(wait_s=25))
        except Exception as e:
            rec(kind="teardown_failed", err=repr(e))
        fh.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
