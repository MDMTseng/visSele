#!/usr/bin/env python3
"""Does a verdict ever land on the wrong part?

Every other check here answers a weaker question. `agree`/`disagree` compares
two pairings against each other, so it goes quiet the moment one of them is
switched off. Totals (SEL1/SEL2/judged/UNANSWERED) cannot see a slip at all: an
off-by-one pairing gives EXACTLY the same totals as a correct one, because every
part still gets exactly one answer and only the assignment changes. And the
real-parts runs so far produce nothing but NA, which hides a slip completely --
the same way it hid the original positional off-by-five until it was measured.

So: make the verdict a known function of the part. The core is told
(INSP_PERIF_VERDICT_PATTERN=<seed>) to derive each verdict from a hash of the
object id -- keyed on the object rather than on a send counter, because a
counter is a property of the stream, so a legitimately lost frame shifts it and
is indistinguishable from a slip. The device records which object each verdict
actually landed on, and this reads that back and checks it.

Noise rather than a regular pattern, and that is not fussiness. This started as
blocks of 5 OK / 5 NG, which is periodic with 10 -- so a slip of exactly 10
shifts the pattern onto itself and passes perfectly. Measured: a real 10-part
slip gave a CLEAN PASS over 510 parts. Any multiple of the period does the same.
A hash has no period, so every nonzero slip disagrees on about half the parts
and the chance of one hiding falls off as 2^-k.

One misplaced verdict is one mis-sorted part, so the pass mark is zero.

Requires PERIF_CORE_PAIRING 1: with no tid there is no object id to key on, and
the script says so rather than reporting a vacuous pass.

  INSP_PERIF_VERDICT_PATTERN=20260806 <restart core>
  python3 slip_probe.py --seconds 120 --seed 20260806
"""
import socket, time, json, argparse

PORT = 4099
CONN = {"type": "CONNECT", "uart_name": "/dev/cu.usbserial-0001",
        "baudrate": 115200, "machine_type": "uInspESP32",
        "cat_ok": 1, "cat_ng": 2, "cam_idx": 1, "pairing": "timestamp"}


M32 = 0xFFFFFFFF


def expect(tid, a, shift=0):
    """The verdict the pattern says this part should get.

    Must match the core's arithmetic exactly (wiringPanel.cpp, the
    INSP_PERIF_VERDICT_PATTERN block), which is why the hash is written out in
    plain uint32 steps rather than using anything Python-specific.
    """
    h = (((tid + shift) & M32) * 2654435761 + a.seed) & M32
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


def send(s, *cmds, gap=0.25):
    for c in cmds:
        s.sendall((c if isinstance(c, str) else json.dumps(c)).encode() + b'\n')
        time.sleep(gap)
        try:
            s.recv(65536)
        except socket.timeout:
            pass


def ask(s, obj, key, listen=3.0):
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


def run(a):
    s = sock()
    CONN['pairing'] = a.pairing
    send(s, "!pd " + json.dumps(CONN), gap=1.0)
    time.sleep(4.0)
    # --real: turn the plate and let parts trip the sensor, instead of holding
    # it still and injecting phantom pulses.
    #
    # Worth having as the same script rather than a second one: the phantom rig
    # controls rate but supplies synthetic timing, and a clean result on it says
    # nothing about real pipeline depth or real jitter. The check applied to the
    # verdicts is identical either way, which is the point -- only the traffic
    # differs.
    send(s, {"type": "clear_error"},
            {"type": "set_setup", "min_detect_sep_us": a.min_sep_us,
             "unanswered_stop_after": 100000},
            {"type": "set_setup", "plate_freq": a.plate_freq},
            {"type": "stepper_enable" if a.real else "stepper_disable"},
            {"type": "enter_insp_mode"})
    # Wait for READY rather than sleeping a guessed interval. CAL -> SPINUP ->
    # READY takes as long as the ramp takes, and a fixed wait turns a slow ramp
    # into a spurious abort.
    j = None
    t0 = time.time()
    while time.time() - t0 < 60:
        j = ask(s, {"type": "get_running_stat"}, 'cam_sync', listen=1.5)
        if j and j.get('state') == 101:
            break
        if j and j.get('state') in (112, 113):
            print("  halted before start: state=%s err=%s"
                  % (j.get('state'), j.get('error_hist')))
            return 1, s
        time.sleep(1.0)
    if not j or j.get('state') != 101:
        print("  never reached READY (state=%s) -- aborting" % (j or {}).get('state'))
        return 1, s
    send(s, {"type": "clear_verdict_log"})

    # The device ring holds 64, which at 30Hz is about two seconds. Reading it
    # only at the end would check the last two seconds of a five-minute run and
    # call that a pass, so poll and accumulate. Keyed by tid, so overlapping
    # reads merge instead of double-counting.
    seen = {}

    def harvest():
        # Match on both keys: other traffic on this link carries a "tid" too,
        # and picking one of those up crashes on the missing "cat".
        vl = ask(s, {"type": "get_verdict_log"}, '"cat"', listen=1.2)
        if vl and 'tid' in vl and 'cat' in vl:
            for t, c in zip(vl['tid'], vl['cat']):
                seen[t] = c

    cmd = b'{"type":"trig_phantom_pulse"}\n'
    t_end = time.time() + a.seconds
    n = 0
    next_poll = time.time() + 1.5
    while time.time() < t_end:
        if a.real:
            # The parts arrive on their own; this loop only harvests.
            time.sleep(0.2)
        else:
            s.sendall(cmd)
            n += 1
            time.sleep(a.min_sep_us / 1e6)
            try:
                s.recv(65536)
            except socket.timeout:
                pass
        if time.time() >= next_poll:
            harvest()
            next_poll = time.time() + 1.5
    time.sleep(8)                        # drain
    harvest()

    st = ask(s, {"type": "get_running_stat"}, 'cam_sync')
    if not seen:
        print("  no verdict log returned")
        return 1, s

    tids = sorted(seen)
    cats = [seen[t] for t in tids]
    print("  %s   collected %d distinct verdicts"
          % ("real parts, plate at %d" % a.plate_freq if a.real
             else "injected=%d" % n, len(tids)))

    # Calibration objects live in their own tid space (0x40000000+) and are not
    # parts; they carry no verdict worth checking.
    pairs = [(t, c) for t, c in zip(tids, cats) if t < 0x40000000]
    if not pairs:
        print("  no part verdicts recorded")
        return 1, s

    bad = [(t, c, expect(t, a)) for t, c in pairs if c != expect(t, a)]

    lo, hi = pairs[0][0], pairs[-1][0]
    print("  checked %d part verdicts, tid %d..%d, seed=%d"
          % (len(pairs), lo, hi, a.seed))
    show = pairs[:80]
    print("  sequence: %s"
          % ''.join('O' if c == CONN['cat_ok'] else
                    ('N' if c == CONN['cat_ng'] else '.') for _, c in show))
    print("  expected: %s"
          % ''.join('O' if expect(t, a) == CONN['cat_ok'] else 'N'
                    for t, _ in show))
    # Gaps are not slips. A part the gate rejected, or one whose frame the
    # camera declined, simply has no verdict -- and refusing to answer is the
    # designed behaviour, not a defect. Print it so a sparse run is not mistaken
    # for a dense one.
    span = pairs[-1][0] - pairs[0][0] + 1
    if span != len(pairs):
        print("  note: %d verdicts across a span of %d tids (%d without one)"
              % (len(pairs), span, span - len(pairs)))

    if st:
        cs, ct = st['cam_sync'], st['count']
        print("  SEL1=%s SEL2=%s NA=%s UNANS=%s  agree=%s DISAGREE=%s "
              "delta_max=%s err=%s"
              % (ct['SEL1'], ct['SEL2'], ct['NA'], ct['UNANSWERED'],
                 cs['agree'], cs['disagree'], cs.get('delta_max_us'),
                 st.get('error_hist')))

    if bad:
        print("  MISPLACED %d of %d verdict(s) -- each one is a mis-sorted part:"
              % (len(bad), len(pairs)))
        for t, c, w in bad[:10]:
            print("    tid %d got cat %d, pattern says %d" % (t, c, w))
        # How far out is it? Re-check against the pattern shifted by k. A clean
        # fit at some k says the pairing is uniformly k parts out of step, which
        # is a very different fault from scattered mismatches (noise, or a
        # pairing that slips and recovers).
        # Scan wide. A range that does not contain the actual slip reports "no
        # single shift explains it", which reads as scattered corruption and is
        # exactly the wrong conclusion -- seen at first hand with a +-8 scan
        # against a real slip of 10.
        fits = [(sum(1 for t, c in pairs if c != expect(t, a, k)), abs(k), k)
                for k in range(-64, 65)]
        # Ties broken toward the smallest |k|. With noise a tie should not
        # happen -- that is the point of using it -- but reporting the nearest
        # fit is the honest answer if one ever does.
        fits.sort()
        n_bad, _, k = fits[0]
        if k != 0 and n_bad == 0:
            print("    => uniform slip of %+d parts (pattern fits exactly when "
                  "shifted by %d)" % (k, k))
        else:
            print("    => no single shift explains it; best is %+d with %d "
                  "still wrong" % (k, n_bad))
        return 1, s

    if all(c == pairs[0][1] for _, c in pairs):
        # Every verdict identical means the pattern never reached the device --
        # exactly the blind spot this script exists to close, so it must not be
        # reported as a pass.
        print("  INCONCLUSIVE: every verdict is the same. Is "
              "INSP_PERIF_VERDICT_PATTERN set, and is PERIF_CORE_PAIRING 1?")
        return 1, s

    print("  => clean: every verdict landed on the part the pattern predicts")
    return 0, s


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument("--seconds", type=int, default=120)
    ap.add_argument("--seed", type=int, default=20260806,
                    help="must match INSP_PERIF_VERDICT_PATTERN")
    ap.add_argument("--min-sep-us", type=int, default=33000)
    ap.add_argument("--real", action="store_true",
                    help="turn the plate and use real sensor detections "
                         "instead of injecting phantom pulses")
    ap.add_argument("--plate-freq", type=int, default=15000,
                    help="10000 is the validated real-parts speed")
    # Negative control. A check that has never failed proves nothing, and
    # positional pairing is the known-bad case -- measured one object out of
    # step. If this script cannot catch that, it cannot catch anything.
    ap.add_argument("--pairing", default="timestamp",
                    choices=["timestamp", "positional"])
    a = ap.parse_args()
    rc, s = 1, None
    try:
        rc, s = run(a)
    finally:
        try:
            if s is None:
                s = sock()
            send(s, {"type": "set_setup", "plate_freq": 0},
                    {"type": "exit_insp_mode"}, gap=0.5)
            s.close()
        except Exception as e:
            print("WARNING: could not confirm shutdown: %s" % e)
    raise SystemExit(rc)
