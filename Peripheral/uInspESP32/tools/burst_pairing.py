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
        "baudrate": 115200, "machine_type": "uInspESP32",
        "cat_ok": 1, "cat_ng": 2, "cam_idx": 1, "pairing": "timestamp"}

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


def run(seconds, pairing, seed, window_us, hz_lo_f, hz_hi_f):
    rng = random.Random(seed)
    s = sock()
    c = dict(CONN); c["pairing"] = pairing
    send(s, "!pd " + json.dumps(c), gap=1.0)
    time.sleep(4.0)

    setup = {"type": "set_setup", "min_detect_sep_us": 12000,
             "unanswered_stop_after": 100000}
    if window_us:
        setup["cam_match_window_us"] = window_us
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
    ap.add_argument("--hz-lo", type=float, default=0.6)
    ap.add_argument("--hz-hi", type=float, default=2.0)
    a = ap.parse_args()
    seed = a.seed if a.seed is not None else random.randrange(1 << 30)

    print("burst trial: %ss  pairing=%s  seed=%d" % (a.seconds, a.pairing, seed))
    j, cycles, injected, lo, hi = run(a.seconds, a.pairing, seed, a.window_us,
                                      a.hz_lo, a.hz_hi)
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
    print("  cal_runs=%s cal_fails=%s cal_ms=%s  miss_last=%s miss_max=%s"
          % (cs.get('cal_runs'), cs.get('cal_fails'), cs.get('cal_ms'),
             cs.get('miss_delta_last_us'), cs.get('miss_delta_max_us')))
    print("  state=%s error_hist=%s" % (j.get('state'), j.get('error_hist')))

    # 13 == GEN_ERROR_CODE::CAM_CLOCK_LOST
    bad = cs['disagree'] or 13 in (j.get('error_hist') or [])
    print("  => %s" % ("FAIL (see disagree / CAM_CLOCK_LOST)" if bad else "clean"))
    raise SystemExit(1 if bad else 0)
