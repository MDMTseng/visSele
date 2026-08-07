#!/usr/bin/env python3
"""Random-burst pairing stress, for the frame-loss case that actually
discriminates timestamp matching from positional.

dryrun_pairing.py injects at a constant rate. That is the easy shape: a steady
stream either fits under the camera's ~35Hz ceiling or does not, and the
pipeline reaches a steady state either way. Real lines are not steady, and the
interesting failures live in the transitions -- a burst that overruns the camera
mid-flight, then a gap that drains the queue, then another burst starting from a
different pipeline depth.

Each cycle is a burst of N pulses at some rate, then an idle gap. Both are drawn
per cycle, and the rate is drawn to straddle the camera ceiling deliberately, so
a single run contains bursts the camera can service and bursts it cannot. Some
gaps are longer than the firmware's 3s real-work guard, which lets sync pulses
resume -- that exercises the handover between the two sample sources, which a
constant-rate run never touches.

The seed is printed and settable, so a failure is reproducible.

The board is hard-reset by re-issuing CONNECT (see dryrun_pairing.py), so every
trial starts with the counters genuinely at zero.

  python3 burst_pairing.py --seconds 180
  python3 burst_pairing.py --seconds 180 --pairing positional --seed 12345
"""
import socket, sys, time, json, random, argparse

PORT = 4099
CONN = {"type": "CONNECT", "uart_name": "/dev/cu.usbserial-0001",
        "baudrate": 230400, "machine_type": "uInspESP32",
        "cat_ok": 3, "cat_ng": 1, "cam_idx": 1, "pairing": "timestamp"}

# The camera services ~35-36Hz and silently ignores triggers above that -- it
# does not report a drop. Straddling that number is the whole point: below it
# timestamp and positional matching are indistinguishable, and every previous
# clean result was collected below it.
CAM_CEILING_HZ = 35.0


def sock():
    s = socket.create_connection(('127.0.0.1', PORT), timeout=5)
    s.settimeout(0.4)
    return s


def send(s, *cmds, gap=0.25):
    for c in cmds:
        s.sendall((c if isinstance(c, str) else json.dumps(c)).encode() + b'\n')
        time.sleep(gap)
        try:
            s.recv(65536)
        except socket.timeout:
            pass


def stat(s, listen=2.5):
    s.sendall(b'{"type":"get_running_stat"}\n')
    buf, t0 = b'', time.time()
    while time.time() - t0 < listen:
        try:
            buf += s.recv(8192)
        except socket.timeout:
            continue
    for l in buf.decode(errors='replace').splitlines():
        if 'cam_sync' in l:
            try:
                return json.loads(l)
            except Exception:
                pass
    return None


def run(seconds, pairing, seed, window_us, hz_lo_f, hz_hi_f, min_sep_us,
        drift_comp, recal_idle_ms):
    rng = random.Random(seed)
    s = sock()
    c = dict(CONN); c["pairing"] = pairing
    send(s, "!pd " + json.dumps(c), gap=1.0)
    time.sleep(4.0)

    setup = {"type": "set_setup", "min_detect_sep_us": min_sep_us,
             "unanswered_stop_after": 100000}
    if window_us:
        setup["cam_match_window_us"] = window_us
    if drift_comp is not None:
        setup["cam_drift_comp"] = bool(drift_comp)
    if recal_idle_ms is not None:
        setup["cam_recal_idle_ms"] = recal_idle_ms
    # Plate first: entering inspection mode now goes through INSPECTION_MODE_CAL,
    # and a phantom pulse is scheduled at a future step count, so a stationary
    # plate would leave calibration waiting for a step that never comes.
    send(s, {"type": "clear_error"}, setup,
            {"type": "set_setup", "plate_freq": 15000},
            {"type": "stepper_disable"},
            {"type": "enter_insp_mode"})
    time.sleep(10)                        # let CAL converge -> READY

    j = stat(s)
    if j:
        print("  start: state=%s window=%s authoritative=%s" %
              (j.get('state'), j['cam_sync'].get('window_us'),
               j['cam_sync'].get('authoritative')))

    cmd = b'{"type":"trig_phantom_pulse"}\n'
    t_end = time.time() + seconds
    cycles, injected = 0, 0
    hz_lo, hz_hi = 1e9, 0
    while time.time() < t_end:
        # Straddle the ceiling: roughly half the bursts overrun the camera.
        hz = rng.uniform(hz_lo_f * CAM_CEILING_HZ, hz_hi_f * CAM_CEILING_HZ)
        n = rng.randint(3, 40)
        # A gap past the firmware's 3s real-work guard lets sync pulses resume.
        gap = rng.choice([rng.uniform(0.05, 0.5), rng.uniform(0.5, 2.5),
                          rng.uniform(3.2, 5.0)])
        hz_lo, hz_hi = min(hz_lo, hz), max(hz_hi, hz)

        t0 = time.time()
        for i in range(n):
            s.sendall(cmd)
            injected += 1
            tgt = t0 + (i + 1) / hz
            while True:
                d = tgt - time.time()
                if d <= 0:
                    break
                time.sleep(min(d, 0.002))
            try:
                s.recv(65536)
            except socket.timeout:
                pass
        cycles += 1
        time.sleep(gap)

    time.sleep(12)                        # let the pipeline drain
    j = stat(s)
    send(s, {"type": "set_setup", "plate_freq": 0}, {"type": "exit_insp_mode"})
    s.close()
    return j, cycles, injected, hz_lo, hz_hi


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument("--seconds", type=int, default=180)
    ap.add_argument("--pairing", default="timestamp",
                    choices=["timestamp", "positional"])
    ap.add_argument("--seed", type=int, default=None)
    ap.add_argument("--window-us", type=int, default=None)
    # Fractions of the camera ceiling to draw burst rates between. The
    # default straddles it; "--hz-hi 0.7" gives a no-loss control run that
    # still has the gaps the clock needs to bootstrap.
    # The gate's minimum separation, and the single most important knob here.
    #
    # Default 33000us (30Hz) keeps the GATE below the camera's ~35Hz ceiling, so
    # every accepted object can actually be photographed and the run measures
    # pairing. Note the burst RATES still straddle the ceiling -- the gate is
    # what refuses the excess, which is the real machine's behaviour too.
    #
    # Lowering it to 12000us (83Hz) lets the injector drive the camera far past
    # what it can service, and then this stops being a pairing test: frames come
    # back with multi-millisecond timing error and the machine correctly halts
    # on CAM_CLOCK_LOST. Measured 2026-08-05, same seed:
    #   33000us -> accept 816, judged 816, miss_max 0,     clean
    #   12000us -> accept  37, judged  14, miss_max 9089,  halt
    # Both are correct. Use the low value to exercise the halt path on purpose,
    # not to judge the pairing.
    ap.add_argument("--min-sep-us", type=int, default=33000)
    ap.add_argument("--drift-comp", type=int, default=None,
                    help="1/0: project the offset by the measured slope")
    ap.add_argument("--recal-idle-ms", type=int, default=None,
                    help="idle before re-measuring; 0 disables")
    ap.add_argument("--hz-lo", type=float, default=0.6)
    ap.add_argument("--hz-hi", type=float, default=2.0)
    a = ap.parse_args()
    seed = a.seed if a.seed is not None else random.randrange(1 << 30)

    print("burst trial: %ss  pairing=%s  seed=%d  min_sep=%dus (%.0f Hz)"
          % (a.seconds, a.pairing, seed, a.min_sep_us, 1e6/a.min_sep_us))
    j, cycles, injected, lo, hi = run(a.seconds, a.pairing, seed, a.window_us,
                                      a.hz_lo, a.hz_hi, a.min_sep_us,
                                      a.drift_comp, a.recal_idle_ms)
    if not j:
        print("  NO STAT"); raise SystemExit(1)

    cs, ct, g = j['cam_sync'], j['count'], j['gate']
    judged = ct['NA'] + ct['SEL1'] + ct['SEL2'] + ct['SEL3']
    print("  %d bursts, %d injected, rates %.0f-%.0f Hz (camera ceiling ~%.0f)"
          % (cycles, injected, lo, hi, CAM_CEILING_HZ))
    print("  accept=%-6s judged=%-6s SKIP=%-6s UNANS=%s"
          % (g['accept'], judged, ct['SKIP'], ct['UNANSWERED']))
    print("  learned=%-6s agree=%-6s DISAGREE=%-5s rejected=%-5s rebuilds=%s"
          % (cs['learned'], cs['agree'], cs['disagree'],
             cs.get('rejected', '-'), cs.get('rebuilds', '-')))
    print("  resid=%-8s resid_max=%-8s delta_max=%-8s window=%s"
          % (cs['resid_us'], cs['resid_max_us'],
             cs.get('delta_max_us', '-'), cs.get('window_us', '-')))
    # resid alone says nothing -- it is drift accrued over gap_us.
    print("  gap=%.2fs -> drift=%.1f us/s   (resid is drift x gap, not an error)"
          % ((cs.get('gap_us') or 0)/1e6, cs.get('drift_us_per_s') or 0))
    print("  drift_comp=%s slope=%s ppb (%.1f us/s) from n=%s   recals=%s"
          % (cs.get('drift_comp'), cs.get('slope_ppb'),
             (cs.get('slope_ppb') or 0)/1000.0, cs.get('slope_n'), cs.get('recals')))
    print("  cal_runs=%s cal_fails=%s cal_ms=%s  miss_last=%s miss_max=%s"
          % (cs.get('cal_runs'), cs.get('cal_fails'), cs.get('cal_ms'),
             cs.get('miss_delta_last_us'), cs.get('miss_delta_max_us')))
    print("  state=%s error_hist=%s" % (j.get('state'), j.get('error_hist')))

    # 13 == GEN_ERROR_CODE::CAM_CLOCK_LOST
    bad = cs['disagree'] or 13 in (j.get('error_hist') or [])
    print("  => %s" % ("FAIL (see disagree / CAM_CLOCK_LOST)" if bad else "clean"))
    raise SystemExit(1 if bad else 0)
