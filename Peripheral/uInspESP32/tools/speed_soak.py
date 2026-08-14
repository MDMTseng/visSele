#!/usr/bin/env python3
"""Long soak with the plate speed moving continuously, logged for analysis.

The point is not to show that it works -- that has been measured one condition
at a time. The point is to run enough DIFFERENT speed changes, back to back, for
long enough, that anything rare gets a chance to happen: retargets landing mid
drain, a drain that does not finish, a delivered pulse that wanders, an ISR that
overruns near the ceiling, a counter that stops adding up.

So the speed program is deliberately awkward:
  * a random walk, not a sweep, so no two changes are the same
  * a mix of in-band (<10%, applies immediately) and out-of-band (stages and
    drains) magnitudes, in both directions
  * occasional double-taps -- a second change sent 0.4-1.5 s after the first, to
    land inside a drain and exercise the retarget path
  * excursions near the ISR ceiling and down to slow speeds where the drain is
    longest

Everything is written as JSONL, one object per sample, plus event records for
every command issued and every refusal. Nothing is judged here; the log is the
artifact.

Ends with the plate stopped and PROVEN stopped, on every path.
"""
import json, os, random, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from peek import ask, cmd, stop_plate, Refused

OUT = sys.argv[1] if len(sys.argv) > 1 else "/tmp/speed_soak.jsonl"
MINUTES = float(sys.argv[2]) if len(sys.argv) > 2 else 25.0

# Deliberately spans the useful range: 3000 is where a drain is longest, 14000
# is close to where the step ISR stops fitting its tick (worst tick ~30 us
# against 35.7 us there). Production is 10500.
F_LO, F_HI = 3000, 14000

rng = random.Random(20260811)
t0 = time.time()
fh = open(OUT, "w", buffering=1)


def rec(kind, **kw):
    kw["t"] = round(time.time() - t0, 3)
    kw["kind"] = kind
    fh.write(json.dumps(kw) + "\n")


def send(c, tag="", wait=2.5):
    """Issue a command, recording whether the device took it.

    wait was 1.0 s in the first run and enter_insp_mode / reset_running_stat
    both logged as "no reply" while plainly having executed -- the reply is just
    slower than that. A timeout recorded as a refusal is a lie in the log, which
    is worse than no record, so the default is generous.
    """
    try:
        cmd(c, wait=wait)
        rec("cmd", cmd=c, tag=tag, ok=True)
        return True
    except Refused as e:
        rec("refused", cmd=c, tag=tag, err=str(e))
        return False
    except Exception as e:                      # socket, parse, anything
        rec("cmd_error", cmd=c, tag=tag, err=repr(e))
        return False


def sample(tag=""):
    """One coherent read of both documents down a single socket."""
    d = st = None
    try:
        for m in ask([{"type": "get_setup"}, {"type": "get_running_stat"}], wait=1.3):
            if "plate" in m and "stage_pulse_offset" in m:
                d = m
            elif "health" in m:
                st = m
    except Exception as e:
        rec("sample_error", tag=tag, err=repr(e))
        return None
    if st is None or d is None:
        # A missing half is itself a finding: the board did not answer in time.
        rec("sample_partial", tag=tag, have_setup=d is not None,
            have_stat=st is not None)
        return None
    spo = d["stage_pulse_offset"]
    rec("s", tag=tag,
        setp=d["plate"].get("freq"),
        pending=d["plate"].get("freq_pending"),
        band=d["plate"].get("speed_band_pct"),
        accel=d["plate"].get("accel"),
        meas=st.get("plate_freq_meas"),
        state=st.get("state"),
        cam1_win=spo["CAM1_off"] - spo["CAM1_on"],
        sel1_win=spo["SEL1_off"] - spo["SEL1_on"],
        cam1_w_us=(d.get("stage_pulse_width_us") or {}).get("CAM1"),
        sel1_w_us=(d.get("stage_pulse_width_us") or {}).get("SEL1"),
        act_late_max=st.get("act_late_max"),
        gate=st.get("gate"), pipe=st.get("pipe"), count=st.get("count"),
        health=st.get("health"), cam_sync=st.get("cam_sync"),
        yield_=st.get("yield"))
    return st


def main():
    rec("meta", out=OUT, minutes=MINUTES, f_lo=F_LO, f_hi=F_HI,
        note="speed soak; see docstring")
    f = 8000
    try:
        send({"type": "set_setup", "plate": {"freq": f},
              "skip_policy": {"mode": "slow_only"}}, "init")
        time.sleep(16)
        send({"type": "enter_insp_mode"}, "init", wait=5.0)
        time.sleep(7)
        send({"type": "virt_pulse", "period_ticks": 1200}, "init")
        time.sleep(5)
        send({"type": "reset_running_stat", "hwm": True}, "init", wait=5.0)
        sample("baseline")

        end = time.time() + MINUTES * 60
        n = 0
        while time.time() < end:
            n += 1
            # Mix of magnitudes. Small ones apply immediately; large ones stage
            # and drain. Both directions, and the walk is reflected at the ends
            # rather than clamped, so it does not pile up against a limit.
            mag = rng.choice([0.03, 0.06, 0.09,      # inside the band
                              0.18, 0.30, 0.45, 0.60])  # outside it
            step = f * mag * rng.choice([-1, 1])
            nf = f + step
            if nf < F_LO or nf > F_HI:
                nf = f - step
            nf = int(max(F_LO, min(F_HI, nf)))

            send({"type": "set_setup", "plate": {"freq": nf}}, "change#%d" % n)
            rec("change", n=n, frm=f, to=nf, mag=round(mag, 3))
            f = nf

            # Double-tap: land a second change inside the first one's drain.
            if rng.random() < 0.30:
                time.sleep(rng.uniform(0.4, 1.5))
                nf2 = int(max(F_LO, min(F_HI, f * rng.uniform(0.92, 1.35))))
                send({"type": "set_setup", "plate": {"freq": nf2}},
                     "retarget#%d" % n)
                rec("change", n=n, frm=f, to=nf2, retarget=True)
                f = nf2

            # Watch it settle. Sampling through the change is the point, so this
            # is several short samples rather than one long sleep.
            for _ in range(rng.randint(4, 9)):
                if time.time() > end:
                    break
                sample("after#%d" % n)
                time.sleep(rng.uniform(0.8, 2.2))
        rec("done", changes=n)
    except Exception as e:
        rec("fatal", err=repr(e))
    finally:
        try:
            send({"type": "virt_pulse", "period_ticks": 0}, "teardown")
            meas = stop_plate()
            rec("stopped", meas=meas)
        except Exception as e:
            rec("teardown_failed", err=repr(e))
        sample("final")
        fh.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
