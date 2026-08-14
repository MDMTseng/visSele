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
from uinsp_cfg import regroup

PORT = 4099
CONN = {"type": "CONNECT", "uart_name": "/dev/cu.usbserial-0001",
        "baudrate": 230400, "machine_type": "uInspESP32",
        "cat_ok": 3, "cat_ng": 1, "cam_idx": 1, "pairing": "timestamp"}


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
        # Grouped setup keys. The device silently ignores an unrecognised
        # key and still acks true, so a flat `plate_freq` here is a no-op
        # that reads as success -- including the one that stops the plate.
        c = regroup(c)
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


def check(pairs, a, tag):
    """Verdicts vs pattern. Returns the misplaced list."""
    bad = [(t, c, expect(t, a)) for t, c in pairs if c != expect(t, a)]
    print("    %-9s %d verdicts, tid %d..%d, misplaced=%d"
          % (tag, len(pairs), pairs[0][0], pairs[-1][0], len(bad)))
    for t, c, w in bad[:5]:
        print("      tid %d got cat %d, pattern says %d" % (t, c, w))
    return bad


def boundary(s, a, harvest, seen, stat_of):
    """Outside the boundary it must be PERCEIVED; inside it must be RECOVERABLE.

    Two properties that are easy to assume and were never actually tested:

      outside -- driven past what the camera can service, the machine must HALT.
                 Refusing to answer is fine. Answering the wrong part is not, so
                 every verdict it did emit before halting is checked too: a halt
                 with a mis-sort already committed would be a failure, not a
                 pass.

      inside  -- after the halt, clear_error must bring it back through CAL to
                 READY and it must go on sorting correctly. A machine that stops
                 safely but cannot be restarted is not much use on a line.
    """
    print("  [1/3] overload: gate at %dus (%.0f Hz) into a ~35Hz camera"
          % (a.overload_sep_us, 1e6 / a.overload_sep_us))
    send(s, {"type": "set_setup", "min_detect_sep_us": a.overload_sep_us})
    cmd = b'{"type":"trig_phantom_pulse"}\n'
    t_end = time.time() + a.overload_seconds
    next_poll = time.time() + 1.5
    halted = None
    while time.time() < t_end:
        s.sendall(cmd)
        time.sleep(a.overload_sep_us / 1e6)
        try:
            s.recv(65536)
        except socket.timeout:
            pass
        if time.time() >= next_poll:
            harvest()
            j = stat_of()
            if j and j.get('state') in (112, 113):
                halted = j
                break
            next_poll = time.time() + 1.5
    harvest()
    if halted is None:
        halted = stat_of()

    st8 = halted.get('state') if halted else None
    errs = (halted or {}).get('error_hist') or []
    print("    state=%s error_hist=%s" % (st8, errs))
    pairs = [(t, c) for t, c in sorted(seen.items()) if t < 0x40000000]
    bad_overload = check(pairs, a, "overload") if pairs else []

    # 13 == CAM_CLOCK_LOST. Any halt is acceptable perception; this is the one
    # that means "I could not place a frame", which is the point.
    perceived = st8 in (112, 113)
    if not perceived:
        print("    NOT PERCEIVED: ran past the camera ceiling without halting")
    elif 13 not in errs:
        print("    halted, but not on CAM_CLOCK_LOST (err_hist=%s)" % errs)

    print("  [2/3] recover: clear_error, expect CAL -> READY")
    send(s, {"type": "set_setup", "min_detect_sep_us": a.min_sep_us},
            {"type": "clear_error"})
    j, t0 = None, time.time()
    while time.time() - t0 < 60:
        j = stat_of()
        if j and j.get('state') == 101:
            break
        time.sleep(1.0)
    recovered = bool(j and j.get('state') == 101)
    print("    state=%s -> %s" % ((j or {}).get('state'),
                                  "recovered" if recovered else "DID NOT RECOVER"))
    if not recovered:
        return perceived, False, bad_overload, []

    print("  [3/3] resume: %ds at %dus, verdicts must be correct again"
          % (a.resume_seconds, a.min_sep_us))
    send(s, {"type": "clear_verdict_log"})
    seen.clear()
    t_end = time.time() + a.resume_seconds
    next_poll = time.time() + 1.5
    while time.time() < t_end:
        s.sendall(cmd)
        time.sleep(a.min_sep_us / 1e6)
        try:
            s.recv(65536)
        except socket.timeout:
            pass
        if time.time() >= next_poll:
            harvest()
            next_poll = time.time() + 1.5
    time.sleep(6)
    harvest()
    pairs2 = [(t, c) for t, c in sorted(seen.items()) if t < 0x40000000]
    bad_resume = check(pairs2, a, "resume") if pairs2 else []
    if not pairs2:
        print("    no verdicts after recovery")
        recovered = False
    return perceived, recovered, bad_overload, bad_resume


def sporadic(s, a, harvest, seen, stat_of):
    """Isolated frame loss must self-heal by skipping, not by halting.

    This is a different question from the sustained overload in boundary(). A
    line that is driven past the camera forever SHOULD stop. But a real line
    hiccups: a burst tightens the spacing for a few parts, one exposure runs
    late, the camera declines a trigger. Those parts have no answer and must
    simply recirculate -- stopping the machine for each one would make it
    useless, and the halt rule (LOST_N consecutive misses) is a guess until it
    has been run against traffic that hiccups on purpose.

    So: mostly safe spacing, with short bursts that overrun the camera. The
    pass condition is all three of --

      keeps running   no halt
      loses parts     some registered parts end up without a verdict, i.e. the
                      hiccup was real and this is not just a slow clean run
      never mis-sorts every verdict that IS emitted matches the pattern
    """
    # The GATE has to permit the burst or there is no hiccup to heal from.
    #
    # First attempt held the gate at the safe spacing and injected faster: the
    # gate simply rejected the burst, nothing extra reached the camera, and the
    # run came back accept == verdicts with nothing lost. It looked like a pass
    # and tested nothing. The gate protecting the camera is correct behaviour,
    # but it is not the behaviour under test here -- what is under test is what
    # happens when parts DO get through faster than the camera can shoot.
    print("  sporadic: gate opened to %dus; injecting at %dus with %d-pulse "
          "bursts every ~%.1fs, for %ds"
          % (a.overload_sep_us, a.min_sep_us, a.burst_n, a.burst_every,
             a.seconds))
    send(s, {"type": "set_setup", "min_detect_sep_us": a.overload_sep_us})
    cmd = b'{"type":"trig_phantom_pulse"}\n'
    t_end = time.time() + a.seconds
    next_poll = time.time() + 1.5
    next_burst = time.time() + a.burst_every
    halted = None
    bursts = 0
    while time.time() < t_end:
        if time.time() >= next_burst:
            for _ in range(a.burst_n):
                s.sendall(cmd)
                time.sleep(a.overload_sep_us / 1e6)
            bursts += 1
            next_burst = time.time() + a.burst_every
        else:
            s.sendall(cmd)
            time.sleep(a.min_sep_us / 1e6)
        try:
            s.recv(65536)
        except socket.timeout:
            pass
        if time.time() >= next_poll:
            harvest()
            j = stat_of()
            if j and j.get('state') in (112, 113):
                halted = j
                break
            next_poll = time.time() + 1.5
    time.sleep(6)
    harvest()
    j = halted or stat_of()

    pairs = [(t, c) for t, c in sorted(seen.items()) if t < 0x40000000]
    bad = check(pairs, a, "sporadic") if pairs else []
    g = (j or {}).get('gate', {})
    ct = (j or {}).get('count', {})
    print("    %d bursts injected" % bursts)
    cs = (j or {}).get('cam_sync', {})
    print("    state=%s err=%s accept=%s SKIP=%s UNANS=%s rejected=%s "
          "rebuilds=%s miss_max=%s"
          % ((j or {}).get('state'), (j or {}).get('error_hist'),
             g.get('accept'), ct.get('SKIP'), ct.get('UNANSWERED'),
             cs.get('rejected'), cs.get('rebuilds'),
             cs.get('miss_delta_max_us')))
    # Which side is wrong? The pattern is keyed on the tid the CORE named, and
    # the device assigns by its own timestamp match. A mismatch therefore means
    # the two disagreed -- and `disagree` counts exactly that, so a misplaced
    # count with disagree=0 would mean something else entirely is going on.
    print("    agree=%s DISAGREE=%s  (misplaced should track disagree)"
          % (cs.get('agree'), cs.get('disagree')))

    # "Still running" is not "== READY". RECAL (104) is a normal transient the
    # machine enters on its own, and treating it as a halt reported a FAIL on a
    # run whose error_hist was empty. Only the error states are a halt.
    ran = (j or {}).get('state') not in (112, 113)
    # accept counts parts the gate let in; a part with no verdict is one the
    # camera or the pairing declined to answer for. That is the hiccup.
    lost = max(0, (g.get('accept') or 0) - len(pairs))
    if not ran:
        print("    HALTED on a sporadic hiccup -- this is what must not happen")
    elif lost == 0:
        print("    INCONCLUSIVE: nothing was actually lost, so nothing had to "
              "heal. Raise --burst-n or lower --overload-sep-us.")
    return ran, lost, bad


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

    if a.sporadic:
        def stat_of():
            return ask(s, {"type": "get_running_stat"}, 'cam_sync', listen=1.5)
        ran, lost, bad = sporadic(s, a, harvest, seen, stat_of)
        ok = ran and lost > 0 and not bad
        print("  => %s" % ("clean: %d part(s) lost and skipped, no halt, "
                           "nothing mis-sorted" % lost if ok else
                           "FAIL (kept_running=%s lost=%d misplaced=%d)"
                           % (ran, lost, len(bad))))
        return (0 if ok else 1), s

    if a.boundary:
        def stat_of():
            return ask(s, {"type": "get_running_stat"}, 'cam_sync', listen=1.5)
        perceived, recovered, bo, br = boundary(s, a, harvest, seen, stat_of)
        ok = perceived and recovered and not bo and not br
        print("  => %s" % ("clean: halted outside the boundary with nothing "
                           "mis-sorted, and recovered inside it" if ok else
                           "FAIL (perceived=%s recovered=%s misplaced=%d/%d)"
                           % (perceived, recovered, len(bo), len(br))))
        return (0 if ok else 1), s

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
    ap.add_argument("--boundary", action="store_true",
                    help="overload past the camera ceiling (must halt with "
                         "nothing mis-sorted), then clear_error and confirm it "
                         "sorts correctly again")
    ap.add_argument("--overload-sep-us", type=int, default=12000,
                    help="83Hz into a ~35Hz camera")
    ap.add_argument("--overload-seconds", type=int, default=60)
    ap.add_argument("--sporadic", action="store_true",
                    help="mostly safe spacing with occasional over-ceiling "
                         "bursts: must skip the lost parts and keep running")
    ap.add_argument("--burst-n", type=int, default=6)
    ap.add_argument("--burst-every", type=float, default=4.0)
    ap.add_argument("--resume-seconds", type=int, default=45)
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
