#!/usr/bin/env python3
"""Close a speed loop on the measured object rate, with real parts.

There is no auto-speed in the firmware yet. This is the loop on the host, which
is the cheap way to find out whether the machine FOLLOWS before deciding what to
build into it.

The plant: parts sit on the plate at some areal density, so the rate at the gate
is density * speed. Density is what drifts (feeder, recirculation); speed is
what we have. So the correction is multiplicative -- f * target/measured -- not
additive, and a proportional loop on the ratio is the natural shape.

Two constraints from the machine itself, both measured earlier today:

  * A correction must COMPLETE before the part reaches the camera, or the image
    is taken at a speed the recipe did not assume. The camera is 9315 ticks from
    the gate, and solving "ramp done before arrival" gives
    df_max = sqrt(f^2 + 9315*accel) - f. At accel 2000 and f=10500 that is 853
    Hz, i.e. 8.1%. So each step is clamped to STEP_CLAMP.
  * The band is gone, so a correction of any size costs nothing in refused
    parts -- there is no drain and no admission pause. That is what makes
    "small corrections, often" a usable strategy rather than a slow one.

Deliberately starts at the WRONG speed so the interesting part -- convergence,
overshoot, settling -- actually happens instead of being assumed.
"""
import json, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from peek import ask, cmd, stop_plate

TARGET   = float(sys.argv[1]) if len(sys.argv) > 1 else 15.0   # objects / second
START_F  = int(sys.argv[2]) if len(sys.argv) > 2 else 12000
MINUTES  = float(sys.argv[3]) if len(sys.argv) > 3 else 5.0
ACCEL    = 2000
# Tuned against COUNTING NOISE, which is the dominant error here and was not
# obvious until the first run limit-cycled.
#
# CORRECTED 2026-08-12: arrivals here are NOT Poisson and the first noise model
# was wrong by 7x. The parts on this plate are a FIXED set riding a rotating
# disc, so they pass the gate at fixed angular spacing -- quasi-periodic, not
# random. Measured open-loop at a fixed 7800 Hz for 6.2 minutes: 12 s windows
# gave sd 1.1%, where Poisson predicts 7.4%. The 8% deadband chosen to cover the
# imagined noise is what parked the proportional loop 5.3% off target.
#
# The old (wrong) reasoning is kept below because the shape of the argument is
# right and only the distribution was wrong -- if the feeder ever runs
# continuously, arrivals become much closer to Poisson and it applies again.
#
# Arrivals are Poisson, so a window holding N parts knows the rate to 1/sqrt(N).
# The first attempt used a 4 s window: 60 parts at 15/s, sigma 12.9%, against a
# 3% deadband -- so the controller reacted to shot noise on almost every step and
# oscillated +-9% in rate and +-5% in speed for the entire run, never settling.
# It looked like a plant problem and was an instrumentation problem.
#
# 12 s holds ~180 parts, sigma 7.5%. The deadband now sits ABOVE one sigma so a
# noise excursion alone does not move the plate, and the gain is low enough that
# what does get through is corrected over several steps rather than in one.
#
# The real lesson for whatever ends up in the firmware: the loop's speed limit is
# how long it takes to KNOW the rate, not how fast the plate can ramp. At 15/s
# you cannot see a 5% density change in under about ten seconds, whatever the
# accel is.
GAIN     = 0.5
STEP_CLAMP = 0.08   # see docstring: the camera-arrival limit at accel 2000
DEADBAND = 0.025    # ~2 sigma of the MEASURED noise, which is 1.1%, not 7.5%
KI       = 0.03     # the plant has no drift to integrate away; this is trim only
PERIOD   = 12.0     # 180 parts per decision; measured sigma 1.1%, see below

F_LO, F_HI = 3000, 15000
LOG = os.environ.get("RATE_CTL_LOG", "/tmp/rate_ctl.jsonl")
fh = open(LOG, "w", buffering=1)


def stat():
    for m in ask([{"type": "get_running_stat"}], wait=1.2):
        if "health" in m:
            return m
    return None


def main():
    f = START_F
    acc = [0.0]
    try:
        cmd({"type": "set_setup", "plate": {"freq": f, "accel": ACCEL},
             "skip_policy": {"mode": "slow_only"}})
        time.sleep(18)
        cmd({"type": "enter_insp_mode"})
        time.sleep(8)
        s = stat()
        prev_n, prev_t = s["gate"]["accept"], time.time()
        print("target %.1f/s   start %d   step clamp %.0f%%   gain %.1f"
              % (TARGET, START_F, STEP_CLAMP*100, GAIN), flush=True)
        print("   t     setp   meas    rate   err    step   accept  pw   ovr", flush=True)

        end = time.time() + MINUTES*60
        t0 = time.time()
        while time.time() < end:
            time.sleep(PERIOD)
            s = stat()
            if not s:
                continue
            now = time.time()
            n = s["gate"]["accept"]
            rate = (n - prev_n) / (now - prev_t)
            prev_n, prev_t = n, now

            # Proportional OUTSIDE the deadband, integral everywhere.
            #
            # Deadband alone stops the limit cycle and then parks the loop
            # wherever it happened to stop: the previous run held 15.8/s against
            # a 15.0 target for four and a half minutes and never corrected,
            # because +5.3% sits inside the 8% deadband. The deadband cannot
            # simply be tightened -- it has to cover the counting noise, and
            # getting sigma under 3% needs ~1100 parts, a 74 s window.
            #
            # So the deadband keeps its job (do not let SHOT NOISE move the
            # plate) and the integral does the one thing it cannot (remove a
            # persistent offset). KI is small on purpose: it must be slow
            # relative to the noise it is averaging through, or it becomes a
            # second oscillator.
            act = "hold"
            if rate > 0.5:
                ratio = TARGET / rate
                acc[0] += (ratio - 1.0) * KI
                acc[0] = max(-0.25, min(0.25, acc[0]))   # no runaway
                corr = 1.0 + acc[0]
                if abs(ratio - 1.0) > DEADBAND:
                    corr += (ratio - 1.0) * GAIN
                corr = max(1.0-STEP_CLAMP, min(1.0+STEP_CLAMP, corr))
                nf = int(max(F_LO, min(F_HI, f * corr)))
                if abs(nf - f) >= 8:      # ignore sub-0.1% dithering
                    cmd({"type": "set_setup", "plate": {"freq": nf}})
                    act = "%+.2f%%" % ((nf/f - 1)*100)
                    f = nf
            h = s["health"]
            row = dict(t=round(now-t0, 2), setp=f, meas=s.get("plate_freq_meas"),
                       rate=round(rate, 2), err=round(rate-TARGET, 2), act=act,
                       accept=n, gate=s["gate"], health=h)
            fh.write(json.dumps(row) + "\n")
            print("%6.1f  %6d %6.0f  %6.2f %6.2f  %6s  %6d  %4s  %s"
                  % (now-t0, f, s.get("plate_freq_meas") or 0, rate, rate-TARGET,
                     act, n, h.get("cam1_pw_last_us"), h.get("isr_overrun_n")),
                  flush=True)
    finally:
        print("plate ->", stop_plate())
        fh.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
