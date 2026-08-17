#!/usr/bin/env python3
"""Overnight watch: did any of the things fixed on 2026-08-06 come back?

Every fix that day is "changed it and did not see the problem again", measured
over minutes. The failures it replaced took 10-30 minutes to recur. So the
fixes are unproven, and the only thing that proves them is time.

It watches for the specific regressions, not for "seems fine":

  crash dumps        the camera reconnect use-after-free (80+ dumps in 2 days)
  err 11             two threads interleaving on one UART
  cal_fails          the calibration wait that was gated on state 101
  rejected           samples the clock's outlier guard threw out. The
                     tid-vs-timestamp cross-check (`disagree`) it replaced was
                     removed 2026-08-18 with the voting scheme; the ONLY
                     instrument here that can see a part matched to the WRONG
                     frame, as opposed to matched to none
  rejected/rebuilds  the match-window guard firing
  free_heap          a real leak, as opposed to the min_heap artefact
  serialRTT          link congestion, the common root of several symptoms


WHAT THE FIRST VERSION OF THIS SCRIPT GOT WRONG (2026-08-06, 4.5 hours lost)
---------------------------------------------------------------------------
It sent {"type":"trig_phantom_train", "n":20, "sep_us":60000}. The firmware
reads `count` and `period_us` (LegacyFirmware.cpp:4011). Absent keys default
to count=0, and count=0 CANCELS a running train. So every command was a
cancel, the phantom train emitted nothing for the entire run, and the traffic
being measured was the real gate sensor at ~10 objects/s -- unseeded and
unreproducible, the opposite of what the seeded jitter was for.

The board said so in every single reply:

    {"type":"trig_phantom_train","prev_emitted":0,...,"count":0,...}

and the script threw the reply away. The evidence was in hand for 4.5 hours.

Hence the rule this rewrite is built on: **the rig proves its own stimulus
before it measures anything, and re-proves it every cycle.** A soak that
cannot say how many objects it injected has not measured a load, and
"clean" from such a run means nothing at all.

The second lesson is the same shape: the old pass criterion was
`bad = halt or err11 or dumps`. A core that died in hour one produced
`(no answer)` on every poll, was never counted, and printed "clean". Silence
is not a pass -- see soak_real (commit 2ef24ea7), which learned this first.


WHY PHANTOMS RATHER THAN REAL PARTS
-----------------------------------
  - no feeder, no ejection, no air, nothing to run out of or overflow -- an
    unattended machine should not be throwing objects around for hours;
  - the phantom train still exercises everything under test: the serial link,
    the pairing, the clock model, calibration, and the report path;
  - it is seeded, so a run that finds a mis-sort can be replayed exactly.

`set_gate_disable` shuts the real sensor path while leaving injected pulses
working, so the phantoms are the only stimulus BY CONSTRUCTION. Without it
the plate's own traffic fills every idle gap and RECAL -- the busy->idle->
RECAL->busy handover where both calibration bugs lived -- never fires once.

Leaves the plate stopped, the gate re-enabled and inspection off on every exit
path, Ctrl-C included.

  python3 -u regress_watch.py --hours 8
"""
import socket, time, json, argparse, datetime, glob, signal
from uinsp_cfg import regroup


# A backgrounded process has SIGINT set to ignore by the shell, so `kill -INT`
# on an overnight run does nothing -- twice tonight it took a SIGTERM, which
# skips the finally block entirely and leaves the plate turning and the real
# gate disabled. Routing SIGTERM into the same KeyboardInterrupt the Ctrl-C
# path already handles makes `kill` shut the machine down properly.
signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(KeyboardInterrupt))

PORT = 4099          # INSP_PERIF_CONSOLE -- verbatim device bytes, no framing

# The peripheral channel does not exist until SOMETHING connects it, and a
# headless core opens nothing. Without this the console answers nothing at all
# and the watch reports "never reached READY" -- which looks exactly like a
# calibration failure and is not one. `!pd` addresses the core rather than the
# device; everything else on this socket goes to the board.
CONN = {"type": "CONNECT", "uart_name": "/dev/cu.usbserial-0001",
        "baudrate": 230400, "machine_type": "uInspESP32",
        # cat IS the severity class and SMALLER IS WORSE: SEL1 is the most
        # severe reject, the last selector is OK. So NG goes to 1 and OK to 3.
        # These were 1 and 2 -- i.e. OK routed to the most severe station --
        # which phantom traffic could never expose, because nothing is ejected
        # and the slip check is self-consistent under any relabelling. On a
        # real machine every good part would have gone into the reject bin.
        "cat_ok": 3, "cat_ng": 1, "cam_idx": 1, "pairing": "timestamp"}
DUMP_GLOB = "/Users/mdm/workspace/visSele/InspectionCore/Core0_1/crash_*.dump"

# Announcements share this stream with the replies, and at 10 objects/s on a
# 115200 link the round trip was measured at 1.4-2.9 SECONDS. A 2s wait was
# reading "no answer" off a healthy machine.
RPC_WAIT = 9.0


def sock():
    s = socket.create_connection(("127.0.0.1", PORT), timeout=5)
    s.settimeout(0.5)
    return s


_ID = [1000]


def rpc(s, d, wait=RPC_WAIT):
    """Send a command and return ITS reply, correlated by id.

    Every reply carries the request's id and an ack flag -- unconditionally,
    for every command (LegacyFirmware.cpp:4191, `retdoc["id"]=doc["id"];
    retdoc["ack"]=rspAck;`). That is the correlation key, and it is the only
    one that works: matching on "type" fails for get_running_stat, get_setup
    and set_gate_disable (which never set retdoc["type"]), and matching on a
    distinctive field is guesswork that goes stale the moment the reply shape
    changes. An id match is exact, immune to the announcement traffic
    interleaved on this stream, and cannot mistake a stale reply from the
    previous command for this one.

    Returns None if the reply never arrived. Callers must treat that as a
    failure rather than as an absence -- it means the command did not land or
    the link is gone, and both are results.
    """
    _ID[0] += 1
    # Grouped setup keys. must() below insists on ack:true precisely so that a
    # refused command cannot be mistaken for one that ran -- but the device
    # acks true for keys it did not recognise, so a flat `plate_freq` passes
    # that check having done nothing at all. The translation has to happen
    # here, before the assurance is claimed.
    d = regroup(d)
    d = dict(d, id=_ID[0])
    key = '"id":%d' % _ID[0]
    s.sendall((json.dumps(d) + "\n").encode())
    buf, t0 = b"", time.time()
    while time.time() - t0 < wait:
        try:
            buf += s.recv(65536)
        except socket.timeout:
            pass
        # Announcements are interleaved with the replies and a reply can be
        # split across recvs, so rescan everything held every pass. Lines on
        # this port are bare JSON -- the console tap adds no prefix.
        for line in buf.decode(errors="replace").splitlines():
            if key not in line:
                continue
            i = line.find("{")
            if i < 0:
                continue
            try:
                return json.loads(line[i:])
            except Exception:
                pass          # truncated tail; keep reading
    return None


def must(s, d, wait=RPC_WAIT):
    """rpc() that insists the board both answered and said yes.

    A command that was refused is not a command that ran. `ack:false` on
    set_setup or enter_insp_mode would otherwise leave the whole soak
    measuring a machine that never accepted its configuration.
    """
    r = rpc(s, d, wait)
    if r is None:
        return None, "%s: no reply" % d.get("type")
    if r.get("ack") is False:
        return r, "%s: refused (ack=false%s)" % (
            d.get("type"), ", err=%s" % r["err"] if "err" in r else "")
    return r, None


def stat(s, wait=RPC_WAIT):
    return rpc(s, {"type": "get_running_stat"}, wait)


def setup(s, wait=RPC_WAIT):
    return rpc(s, {"type": "get_setup"}, wait)


def expect_cat(tid, seed):
    """The verdict the core must have sent for this object.

    Must match wiringPanel.cpp:5595 and slip_probe.py:52 BIT FOR BIT -- a
    checker that computes a different function from the thing it checks agrees
    with nothing and reports it as a slip. Written as explicit uint32 steps for
    that reason: a splitmix-style finaliser, no language-specific shortcuts.

    Keyed on the tid, which is a property of the PART, not on a send counter,
    which is a property of the STREAM -- a legitimately lost frame shifts a
    counter and looks exactly like a slip.

    A hash and not blocks of n OK / n NG: blocks have period 2n, so a slip of
    exactly 2n maps the pattern onto itself and passes perfectly. Measured on
    this machine, not theorised -- a real 10-part slip gave a clean pass over
    510 parts against 5/5 blocks. A hash has no period, so every nonzero slip
    disagrees on about half the parts.
    """
    M = 0xFFFFFFFF
    h = ((tid & M) * 2654435761 + seed) & M
    h ^= h >> 15
    h = (h * 2246822519) & M
    h ^= h >> 13
    h = (h * 3266489917) & M
    h ^= h >> 16
    return CONN["cat_ok"] if (h & 1) == 0 else CONN["cat_ng"]


def check_verdicts(s, seed, wait=RPC_WAIT):
    """Read the device's own (tid, cat) log and check each verdict landed on
    the object it was computed for.

    This is the only instrument here that sees a SLIP -- a verdict applied to
    the wrong part. Counters cannot: an off-by-one pairing produces identical
    SEL1/SEL2 totals, because every part still gets an answer and only the
    assignment changes. `disagree` compares two device-side lookups of the same
    report; this compares what the CORE decided against which object the DEVICE
    actually applied it to, which is the boundary a slip crosses.

    Returns (checked, [complaints]).
    """
    vl = rpc(s, {"type": "get_verdict_log"}, wait)
    if vl is None:
        return 0, ["get_verdict_log: no reply"]
    tids, cats = vl.get("tid") or [], vl.get("cat") or []
    if not tids:
        return 0, []
    # Calibration objects carry their own tid space (0x40000000, so they are
    # never confused with gate-registered parts) and they get verdicts too --
    # ~8 per calibration. Counting them as parts made coverage read 108% of
    # the judged total, which is the denominator being wrong rather than the
    # check being wrong. They are still verified; they just are not parts.
    CAL_BIT = 0x40000000
    bad, applied, cal = [], 0, 0
    for tid, cat in zip(tids, cats):
        if cat not in (CONN["cat_ok"], CONN["cat_ng"]):
            continue                      # NA: the pattern did not reach it
        if tid & CAL_BIT:
            cal += 1
        else:
            applied += 1
        want = expect_cat(tid, seed)
        if cat != want:
            bad.append((tid, cat, want))
    out = []
    if applied == 0 and cal == 0:
        out.append("%d verdicts logged and NONE carried the pattern -- the "
                   "core is not running with INSP_PERIF_VERDICT_PATTERN, so "
                   "nothing here can see a slip" % len(tids))
    if bad:
        out.append("SLIP: %d of %d verdicts landed on the wrong object, first "
                   "%s" % (len(bad), applied + cal,
                           ["tid=%d got cat=%d want=%d" % x for x in bad[:5]]))
    rpc(s, {"type": "clear_verdict_log"}, wait)
    return applied, out


def absorb(prev, new):
    """New entries appended to error_hist since the last poll.

    error_hist is a 20-deep ring (errorBuf[20]) that drops the oldest when
    full, oldest-first on the wire. Counting occurrences of err 11 in a
    snapshot -- what the old version did, once every 15 minutes -- answers
    "is 11 still among the last 20 errors", not "did 11 happen". Twenty-one
    later errors erase it completely.

    Returns (appended, gap) where gap means the ring turned over entirely
    between polls, so an unknown number of entries were missed.
    """
    if not new:
        return [], False
    if not prev:
        return list(new), False
    for k in range(min(len(prev), len(new)), 0, -1):
        if prev[-k:] == new[:k]:
            return list(new[k:]), False
    return list(new), True


def preflight(s, a):
    """Prove the stimulus lands before measuring anything with it.

    Fire a short train, wait for it to finish, then read prev_emitted off the
    NEXT train command -- the firmware reports the previous train's emitted
    count and measured min/max interval, which is the rig stating the load it
    actually applied rather than the one it asked for.
    """
    n = 10
    period = a.period_us
    r = rpc(s, {"type": "trig_phantom_train", "count": n,
                "period_us": period, "jitter_us": a.jitter_us, "seed": 1},
            )
    if r is None:
        return "preflight: no answer to trig_phantom_train"
    if r.get("count") != n or r.get("period_us") != period:
        return ("preflight: board echoed count=%s period_us=%s, asked %s/%s "
                "-- the command is not being parsed as sent"
                % (r.get("count"), r.get("period_us"), n, period))
    time.sleep(n * period / 1e6 + 3.0)
    r2 = rpc(s, {"type": "trig_phantom_train", "count": 0})
    if r2 is None:
        return "preflight: no answer to the confirming trig_phantom_train"
    emitted = r2.get("prev_emitted", 0)
    if emitted < n:
        return ("preflight: asked for %d phantoms, board emitted %d -- the "
                "soak has no stimulus and every later number would be about "
                "whatever else is on the plate" % (n, emitted))
    print("preflight: %d/%d phantoms emitted, interval %s..%s us (asked %d+-%d)"
          % (emitted, n, r2.get("prev_min_us"), r2.get("prev_max_us"),
             period, a.jitter_us))
    return None


def main(a):
    s = sock()
    # CONNECT reboots the board (opening the port toggles DTR), so every counter
    # below starts from zero -- which is what a regression watch wants anyway.
    s.sendall(("!pd " + json.dumps(CONN) + "\n").encode())
    time.sleep(5.0)
    # A phantom train needs the stage timer running, so the plate turns -- but
    # slowly, and with nothing on it to eject. plate_freq is set explicitly
    # rather than inherited: whatever the machine was left at is not a decision
    # this script gets to skip.
    setup_cmds = [{"type": "clear_error"},
                  {"type": "clear_error_history"},
                  {"type": "set_setup", "plate_freq": a.plate_freq},
                  {"type": "stepper_enable"}]
    for c in setup_cmds:
        _, e = must(s, c)
        if e:
            print("%s -- refusing to run, the machine is not in the state this "
                  "measurement assumes" % e)
            return 1, s

    # Set AFTER the CONNECT, because CONNECT toggles DTR and reboots the board
    # back to its NVS values -- anything set before it is silently gone. Runtime
    # only: no save_setup, so the machine returns to its configured rate on the
    # next power cycle and this run cannot leave a test value on a production
    # machine.
    if a.min_sep_us:
        _, e = must(s, {"type": "set_setup", "min_detect_sep_us": a.min_sep_us})
        got = (setup(s) or {}).get("min_detect_sep_us")
        if e or got != a.min_sep_us:
            print("min_detect_sep_us: asked %d, board reports %s (%s) -- "
                  "refusing to run, the rate gate is not where this test needs "
                  "it" % (a.min_sep_us, got, e or "no error"))
            return 1, s
        print("min_detect_sep_us = %d us (%.1f Hz ceiling), runtime only"
              % (got, 1e6 / got))

    # report_match_ts promotes the TIMESTAMP to authoritative for deciding
    # which object a verdict belongs to (`tarP` in the report handler). With it
    # off, tid decides and a corrupted timestamp costs a clock sample rather
    # than a part -- which is why a FAULT_TS_US sweep to 75% of the spacing
    # produced zero mis-sorts. With it on, the timestamp IS the routing
    # decision, so this is the setting under which a mis-sort is reachable at
    # all. Runtime only, like min_detect_sep_us.
    if a.report_match_ts is not None:
        _, e = must(s, {"type": "set_setup",
                        "report_match_ts": bool(a.report_match_ts)})
        got = (setup(s) or {}).get("report_match_ts")
        if e or bool(got) != bool(a.report_match_ts):
            print("report_match_ts: asked %s, board reports %s (%s)"
                  % (bool(a.report_match_ts), got, e or "no error"))
            return 1, s
        print("report_match_ts = %s, runtime only" % bool(got))

    # The real sensor path off, so the phantoms are the only objects in the
    # machine and the idle gaps are actually idle. Verified, not assumed: with
    # it silently not applied the plate's own traffic fills every gap and the
    # RECAL handover -- half of what this run is for -- never happens.
    if a.gate_disable:
        g, e = must(s, {"type": "set_gate_disable", "on": True})
        if e or g.get("gate_disabled") is not True:
            print("could not disable the real gate (%s, reply=%s) -- refusing "
                  "to run, the injected load would not be the only load"
                  % (e or "no error", g))
            return 1, s

    rpc(s, {"type": "clear_verdict_log"})
    _, e = must(s, {"type": "enter_insp_mode"})
    if e:
        print("%s" % e)
        return 1, s

    j, t0 = None, time.time()
    while time.time() - t0 < 180:
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

    err = preflight(s, a)
    if err:
        print(err)
        return 1, s

    dumps0 = set(glob.glob(DUMP_GLOB))
    cs0 = j["cam_sync"]
    g0 = j.get("gate") or {}
    base = {"rej_busy": g0.get("rej_busy", 0),
            "rej_rate": g0.get("rej_rate", 0),
            "rej_dist": g0.get("rej_dist", 0),
            "cal_fails": cs0.get("cal_fails", 0),
            "rejected": cs0.get("rejected", 0),
            "rejected": cs0.get("rejected", 0),
            "rebuilds": cs0.get("rebuilds", 0),
            "heap": j["health"].get("free_heap", 0)}
    emit_s = a.batch * a.period_us / 1e6

    print("watching %.1f h  plate_freq=%d  gate_disabled=%s"
          % (a.hours, a.plate_freq, a.gate_disable))
    # The idle gap has to exceed cam_recal_idle_ms or the RECAL handover never
    # happens, so read the board's value rather than trusting the default.
    recal_ms = (setup(s) or {}).get("cam_recal_idle_ms")
    if recal_ms and a.idle_s * 1000 <= recal_ms:
        print("--idle-s %.0fs does not exceed cam_recal_idle_ms=%sms -- RECAL "
              "would never fire and half this run would prove nothing"
              % (a.idle_s, recal_ms))
        return 1, s
    print("train %d @ %dus (jitter %dus) = %.1fs emitting, then %.0fs idle "
          "(cam_recal_idle_ms=%sms)"
          % (a.batch, a.period_us, a.jitter_us, emit_s, a.idle_s,
             recal_ms if recal_ms is not None else "?"))
    print("%-9s %-8s %-6s %-7s %-6s %-5s %-5s %-6s %-8s %-7s %s"
          % ("elapsed", "judged", "+/s", "cal r/f", "recal", "dis", "rej",
             "err", "heap", "dmax", "state"))

    t_end = time.time() + a.hours * 3600
    next_rep = time.time()
    prev_hist = []
    # TWO baselines, deliberately. `last_judged` moves every cycle and answers
    # "is anything flowing right now"; `rep_judged`/`rep_t` move only at report
    # time and answer "what rate did this interval run at". Sharing one pair
    # divides a single cycle's delta by the whole report interval and prints a
    # rate 4x too low -- which is what the first run of this rewrite did.
    last_judged, rep_judged, rep_t = None, None, time.time()
    seen_err, ring_gaps, no_answer, no_answer_run = [], 0, 0, 0
    fail, samples, emitted_total, verdicts_checked = [], 0, 0, 0

    while time.time() < t_end and not fail:
        r = rpc(s, {"type": "trig_phantom_train", "count": a.batch,
                    "period_us": a.period_us, "jitter_us": a.jitter_us,
                    "seed": a.seed})
        # Re-proved every cycle, not just at preflight: a board that reboots
        # mid-run comes back with the train cancelled, and from that moment on
        # every number would be about an empty machine.
        if r is None:
            fail.append("no answer to trig_phantom_train -- link or board gone")
            break
        if r.get("count") != a.batch:
            fail.append("board echoed count=%s, asked %d" % (r.get("count"), a.batch))
            break
        emitted_total += r.get("prev_emitted", 0)

        # Harvest DURING the burst, not once at the end of the cycle.
        #
        # The device's verdict log is 64 records deep (VERD_LOG_N) and a burst
        # is 100 parts, so a single harvest per cycle can only ever return the
        # last 64 -- the first 36 of every burst were never checked, the same
        # 36 every time. That is a systematic blind spot, not a sample: a slip
        # that only happens at the start of a burst would have been invisible
        # while the run reported "512 verdicts checked".
        #
        # 50 parts of headroom against a 64-deep ring, so a late reply on a
        # loaded link does not silently drop records between harvests.
        # The budget is a RECORD COUNT, not a wall-clock interval, and the
        # harvest's own cost has to come out of it. The ring holds 64; at
        # 30 objects/s it fills in 2.1 s, and a get_verdict_log round trip is
        # itself of that order -- so sleeping a fixed 1.67 s and then spending
        # 1.8 s fetching overruns the ring every time and silently drops half
        # the parts while still reporting a large "verdicts checked" number.
        # Subtract the measured fetch time and keep 40 records of headroom.
        budget_s = 40.0 * a.period_us / 1e6
        t_wake = time.time() + emit_s + a.idle_s
        last_fetch = 0.0
        while time.time() < t_wake:
            nap = max(0.05, min(budget_s - last_fetch, t_wake - time.time()))
            time.sleep(nap)
            if a.verdict_seed:
                t_f = time.time()
                n_chk, complaints = check_verdicts(s, a.verdict_seed)
                last_fetch = time.time() - t_f
                verdicts_checked += n_chk
                fail.extend(complaints)
        if fail:
            break

        # Polled EVERY cycle, not once per report: the error ring is 20 deep
        # and a 15-minute gap is long enough to lose everything that happened.
        j = stat(s)
        if j is None:
            no_answer += 1
            no_answer_run += 1
            print("  (no answer x%d)" % no_answer_run)
            if no_answer_run >= a.no_answer_max:
                fail.append("%d consecutive polls unanswered -- the machine "
                            "stopped talking and silence is not a pass"
                            % no_answer_run)
                break
            continue
        no_answer_run = 0

        hist = j.get("error_hist") or []
        new_err, gap = absorb(prev_hist, hist)
        prev_hist = hist
        seen_err += new_err
        if gap:
            ring_gaps += 1
        cs, ct, h = j["cam_sync"], j["count"], j["health"]
        judged = ct["NA"] + ct["SEL1"] + ct["SEL2"] + ct["SEL3"]

        # Anything the device chose to record as an error is a result. The old
        # version looked only for 11 and would have shrugged at CAM_CLOCK_LOST.
        if new_err:
            fail.append("new errors %s" % new_err)
        if cs.get("rejected", 0) > base["rejected"]:
            fail.append("rejected %s -> %s: the clock refused samples -- a frame "
                        "landed where the model did not expect it"
                        % (base["rejected"], cs.get("rejected")))
        # Parts dropped at the gate, silently. rej_busy means the pipeline had
        # no room (PIPE_INFO_LEN=100) and the object was never registered at
        # all -- it does not appear in judged, in the verdict log, or in any
        # count this run otherwise reads, so a machine quietly losing every
        # tenth part would still print a clean line. Measured occupancy sits at
        # 30, which is why this has never fired; that is a reason to watch it,
        # not a reason to assume it.
        gt = j.get("gate") or {}
        for k, why in (("rej_busy", "the pipeline had no room"),
                       ("rej_rate", "the rate gate refused it"),
                       ("rej_dist", "it was too close to the previous object")):
            if gt.get(k, 0) > base[k]:
                fail.append("%s %s -> %s: parts were dropped at the gate "
                            "because %s" % (k, base[k], gt.get(k), why))
        if cs.get("cal_fails", 0) > base["cal_fails"]:
            fail.append("cal_fails %s -> %s" % (base["cal_fails"], cs.get("cal_fails")))
        if cs.get("rebuilds", 0) > base["rebuilds"]:
            fail.append("rebuilds %s -> %s: the clock was lost and rebuilt"
                        % (base["rebuilds"], cs.get("rebuilds")))
        new_dumps = set(glob.glob(DUMP_GLOB)) - dumps0
        if new_dumps:
            fail.append("%d new crash dumps" % len(new_dumps))
        if j.get("state") in (112, 113):
            fail.append("HALTED state=%s error_hist=%s" % (j.get("state"), hist))

        # A machine that is up, answering, and judging nothing is not passing.
        # It is the failure mode the old script printed as "clean".
        if last_judged is not None and judged <= last_judged and j.get("state") == 101:
            fail.append("judged stuck at %d while state=101 and %d phantoms "
                        "were injected -- nothing is flowing" % (judged, a.batch))

        last_judged = judged
        if time.time() < next_rep and not fail:
            continue
        next_rep = time.time() + a.report_every
        samples += 1
        now = time.time()
        rate = (judged - rep_judged) / (now - rep_t) if rep_judged is not None else 0.0
        print("%-9s %-8d %-6.1f %-7s %-6s %-5s %-5s %-6s %-8s %-7s %s"
              % (str(datetime.timedelta(seconds=int(now - (t_end - a.hours * 3600)))),
                 judged, rate,
                 "%s/%s" % (cs.get("cal_runs"), cs.get("cal_fails")),
                 cs.get("recals"), cs.get("rejected"),
                 len(seen_err), h.get("free_heap"), cs.get("delta_max_us"),
                 j.get("state")))
        rep_judged, rep_t = judged, now
        if fail:
            print("  stopping so the state is preserved for the morning")
            break

    j = stat(s)
    print("\n=== result ===")
    print("  stimulus    : %d phantoms confirmed emitted (%d/train @ %dus"
          " jitter %d seed %d)" % (emitted_total, a.batch, a.period_us,
                                   a.jitter_us, a.seed))
    if j:
        cs = j["cam_sync"]
        print("  calibration : runs=%s fails=%s  recals=%s recal_skipped=%s"
              % (cs.get("cal_runs"), cs.get("cal_fails"),
                 cs.get("recals"), cs.get("recal_skipped")))
        print("  pairing     : rejected=%s rebuilds=%s"
              % (
                 cs.get("rejected"), cs.get("rebuilds")))
        # The distribution, not the high-water mark. delta_max cannot tell
        # "one outlier at 240us" from "routinely near 240", and those two want
        # very different match windows -- which is the number this run exists
        # to inform. Bucket i is [32<<i, 32<<(i+1)) us.
        dh = cs.get("delta_hist")
        if dh:
            print("  delta dist  : %s"
                  % "  ".join("%d-%dus:%d" % (32 << i, 32 << (i + 1), v)
                              for i, v in enumerate(dh) if v))
        print("  delta       : last=%sus max=%sus  miss_max=%sus"
              % (cs.get("delta_last_us"), cs.get("delta_max_us"),
                 cs.get("miss_delta_max_us")))
        gf = j.get("gate") or {}
        print("  gate        : accept=%s rej_busy=%s rej_rate=%s rej_dist=%s"
              % (gf.get("accept"), gf.get("rej_busy"),
                 gf.get("rej_rate"), gf.get("rej_dist")))
        print("  heap        : %s -> %s (%+d)"
              % (base["heap"], j["health"].get("free_heap"),
                 (j["health"].get("free_heap") or 0) - base["heap"]))
        print("  state       : %s  error_hist=%s" % (j.get("state"), j.get("error_hist")))
    print("  errors seen : %s%s"
          % (seen_err or "none",
             "  (ring turned over %d times -- some were missed)" % ring_gaps
             if ring_gaps else ""))
    total_judged = 0
    if j:
        ctf = j["count"]
        total_judged = ctf["NA"] + ctf["SEL1"] + ctf["SEL2"] + ctf["SEL3"]
    print("  verdicts    : %d checked against the tid pattern (seed %s)%s"
          % (verdicts_checked, a.verdict_seed or "OFF",
             "  = %.0f%% of the %d judged" % (100.0 * verdicts_checked /
                                              total_judged, total_judged)
             if total_judged else ""))
    if a.verdict_seed and total_judged and verdicts_checked < 0.9 * total_judged:
        print("  NOTE: %d parts were judged without their verdict being "
              "checked -- a slip confined to those is not covered by this run"
              % (total_judged - verdicts_checked))
    print("  unanswered  : %d polls" % no_answer)
    print("  new dumps   : %d" % len(set(glob.glob(DUMP_GLOB)) - dumps0))

    # Failures FIRST, before any verdict about the run.
    #
    # These two branches were the other way round, and it cost the negative
    # control: an injected slip was detected on the first cycle, `fail` broke
    # the loop before a report interval elapsed, and "NO DATA" returned before
    # a single complaint was printed. The run looked like it had found nothing
    # when it had found exactly what it was told to look for. Whatever else a
    # rig gets wrong, it must never swallow a detection.
    for f in fail:
        print("  !! %s" % f)
    if fail:
        print("  => REGRESSION (%d samples)" % samples)
        return 1, s
    if samples == 0:
        print("  => NO DATA. Not a pass.")
        return 1, s
    if a.verdict_seed and verdicts_checked == 0:
        print("  => NOT A PASS: the verdict pattern was requested but no "
              "verdict was ever checked, so a slip could not have been seen")
        return 1, s
    if not a.verdict_seed:
        print("  NOTE: run with --verdict-seed and the core started with "
              "INSP_PERIF_VERDICT_PATTERN=<same seed>, or this run cannot see "
              "a verdict landing on the wrong part at all.")
    print("  => clean (%d samples, %d phantoms, %d verdicts)"
          % (samples, emitted_total, verdicts_checked))
    return 0, s


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--hours", type=float, default=8)
    ap.add_argument("--plate-freq", type=int, default=3500)   # ~7 rpm
    # Defaults chosen to match what the machine actually does, measured: the
    # real gate registered 10.15 objects/s (tid 142953->144039 in 107s). A
    # 20-part burst every 16s averages 1.2/s, which is an eighth of that and
    # would leave the link -- the current bottleneck, 1.4-2.9s round trip --
    # barely loaded. 100 parts at 100ms reproduces the real rate for 10s, then
    # goes quiet long enough for RECAL. Peak load and the idle handover both,
    # instead of neither.
    ap.add_argument("--batch", type=int, default=100)
    # Named for what the firmware reads. The first version called these `n` and
    # `sep_us`, the board ignored both, and a 4.5 hour run measured nothing.
    ap.add_argument("--period-us", type=int, default=100000)
    ap.add_argument("--jitter-us", type=int, default=4000)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--idle-s", type=float, default=15.0)     # > cam_recal_idle_ms
    ap.add_argument("--report-every", type=float, default=300)
    ap.add_argument("--no-answer-max", type=int, default=3)
    # The rate gate's ceiling, in us between parts. Left alone by default: the
    # machine's configured 28571 (35 Hz) is a real setting, not a test knob.
    ap.add_argument("--min-sep-us", type=int, default=0)
    ap.add_argument("--report-match-ts", type=int, default=None)
    # Must equal the core's INSP_PERIF_VERDICT_PATTERN. 0 disables the slip
    # check, which makes this a soak for crashes and errors only.
    ap.add_argument("--verdict-seed", type=int, default=20260806)
    ap.add_argument("--no-gate-disable", dest="gate_disable",
                    action="store_false", default=True)
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
            # 6s, not 2s: the round trip on a loaded link was measured at
            # 1.4-2.9s, and a shutdown that "could not confirm" because it gave
            # up too early is how a plate gets left turning overnight.
            #
            # exit_insp_mode does NOT stop the plate -- IDLE's loop body is
            # PLATE_FREQ_TARGET = PLATE_FREQ_SETPOINT, so it drives the plate at
            # the setpoint every pass. Only writing plate_freq:0 stops it.
            # Leaving the real gate disabled would be worse than leaving it
            # turning: the next person's machine would silently ignore its own
            # parts, and nothing on the panel would say so.
            gate_back = None
            for c in ({"type": "trig_phantom_train", "count": 0},
                      {"type": "exit_insp_mode"},
                      {"type": "set_setup", "plate_freq": 0},
                      {"type": "stepper_disable"},
                      {"type": "set_gate_disable", "on": False}):
                for _try in range(3):
                    r = rpc(s, c, wait=6.0)
                    if r is not None:
                        # gate_disabled comes back on THIS reply and nowhere
                        # else -- it is not in get_running_stat, so reading it
                        # from the stat printed None and the shutdown could not
                        # actually confirm the thing it claimed to confirm.
                        if c["type"] == "set_gate_disable":
                            gate_back = r.get("gate_disabled")
                        break
                else:
                    print("WARNING: %s got no reply after 3 tries" % c["type"])
            j, g = stat(s, 6.0), setup(s, 6.0)
            freq = (g or {}).get("plate_freq")
            print("left: state=%s plate_freq(setpoint)=%s gate_disabled=%s"
                  % ((j or {}).get("state"), freq, gate_back))
            if gate_back is not False:
                print("WARNING: the real gate was NOT confirmed re-enabled "
                      "(gate_disabled=%s) -- this machine would ignore its own "
                      "parts and nothing on the panel would say so" % gate_back)
            if freq not in (0, 0.0, None):
                print("WARNING: the plate is still commanded to turn "
                      "(plate_freq=%s) -- stop it by hand" % freq)
            s.close()
        except Exception as e:
            print("WARNING: could not confirm the plate stopped and the gate "
                  "was re-enabled: %s" % e)
    raise SystemExit(rc)
