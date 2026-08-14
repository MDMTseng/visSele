#!/usr/bin/env python3
"""A1: the real-verdict run. The sorting half of the machine, exercised.

The 8-hour soak of 2026-08-11 ran 394k parts and produced 393537 `NA` with
SEL1/SEL2/SEL3 all zero, and the reason was not the inspection: the core it ran
against was headless, and a headless core loads no def, so it answers NA to
every part. `act_cap`, `SEL_SUPPRESSED` and `FREQ_TXN` were consequently at zero
coverage -- the three mechanisms that only exist once verdicts are real.

The missing piece was one line. The perif console forwards `!ld` to the core as
the same BPG packet the WebUI sends (wiringPanel.cpp, "a headless core loads no
def"), so a rig can load the recipe the machine actually ships with and never
open a browser.

What this run is for, in order of what it covers that nothing else has:

  SEL1 / SEL2 / SEL3 > 0     the sorting path moved at all
  act_cap                     a verdict arriving inside a tight SEL1 window
  SEL_SUPPRESSED              a verdict whose actuation was not delivered
  FREQ_TXN / _TIMEOUT         staged speed changes. If these stay at zero on a
                              real-verdict run too, the honest outcome is to
                              DELETE the transaction machinery rather than keep
                              unreachable code -- decide it from this number.

Conservation is checked on the way out, and it now includes discard_stop, which
before 08-12 was the unnamed remainder:

  accept - judged - discard_stop == what is still in RBuf

TWO MODES, and picking the wrong one costs a run.

  --attach   OBSERVE ONLY. The WebUI is driving: it holds the peripheral
             channel, it loaded the def, it starts and stops the machine. This
             sends no CONNECT (which would reset the board mid-run), no !ld, and
             nothing that moves the plate. It samples counters and, at the end,
             reports coverage and conservation. Use this whenever a browser is
             open on the machine.

  (default)  DRIVES the machine headlessly: CONNECT, !ld, enter_insp_mode, run,
             stop. Use only when nothing else is attached -- two clients on this
             machine is a real failure mode, not a theoretical one (2026-08-12:
             a second WebUI on the same core cost the live preview until both
             were restarted).

The plate is stopped and inspection left on every exit path, including Ctrl-C --
in driving mode. --attach never touches the plate; whoever started the run owns
stopping it.

  python3 soak_verdicts.py --attach --seconds 300        # WebUI is driving
  python3 soak_verdicts.py --seconds 60                  # headless validation
  python3 soak_verdicts.py --hours 8 --def data/test1.hydef
"""
import argparse, json, socket, sys, time

PORT = 4099
CONN = {"type": "CONNECT", "uart_name": "/dev/cu.usbserial-0001",
        "baudrate": 230400, "machine_type": "uInspESP32",
        "cat_ok": 3, "cat_ng": 1, "cam_idx": 1, "pairing": "timestamp"}

REJ = ("rej_rate", "rej_dist", "rej_busy", "rej_width", "rej_unstable",
       "rej_blocked", "rej_stepper_off", "rej_gate_off", "rej_dryrun")


def sock():
    s = socket.create_connection(("127.0.0.1", PORT), timeout=5)
    s.settimeout(0.4)
    return s


def send(s, *cmds, gap=0.25):
    for c in cmds:
        s.sendall((c if isinstance(c, str) else json.dumps(c)).encode() + b"\n")
        time.sleep(gap)
        try:
            s.recv(65536)
        except socket.timeout:
            pass


def stat(s, listen=2.5):
    s.sendall(b'{"type":"get_running_stat"}\n')
    buf, t0 = b"", time.time()
    while time.time() - t0 < listen:
        try:
            buf += s.recv(8192)
        except socket.timeout:
            continue
    for l in buf.decode(errors="replace").splitlines():
        if "cam_sync" in l:
            try:
                return json.loads(l)
            except Exception:
                pass
    return None


def say(*a):
    print(*a, flush=True)


def verdicts(ct):
    """Parts that got an answer -- reconstructed, because no counter holds it.

    The five counters are not the same KIND of number, and adding them as if
    they were is the easy mistake here:

      SEL3_Count / NA_Count      incremented at SWITCH  -> VERDICTS
      SEL1_Count / SEL2_Count    incremented in the ACT_SEL stage -> ACTUATIONS

    SWITCH's case 1 and case 2 only push the actuation tasks; they count
    nothing. So an ejecting verdict is counted where the air fires, and if the
    conditions there fail it is counted in SEL_SUPPRESSED instead. Adding
    SEL1+SEL2+SEL3+NA therefore UNDER-counts verdicts by exactly the suppressed
    ones -- invisible while suppression is zero, which is every run so far.
    """
    return (ct["SEL1"] + ct["SEL2"] + ct["SEL_SUPPRESSED"]
            + ct["SEL3"] + ct["NA"])


def show(j, tag):
    """One line per sample. The verdict mix is the point of this run."""
    if not j:
        say("  %-9s NO STAT" % tag)
        return
    g, ct, h = j["gate"], j["count"], j["health"]
    judged = verdicts(ct)
    say("  %-9s state=%-4s accept=%-6s judged=%-6s | SEL1=%-5s SEL2=%-5s "
        "SEL3=%-5s NA=%-6s | SKIP=%-4s UNANS=%s"
        % (tag, j.get("state"), g["accept"], judged, ct["SEL1"], ct["SEL2"],
           ct["SEL3"], ct["NA"], ct["SKIP"], ct["UNANSWERED"]))
    say("            act_cap=%-5s cap_max_t=%-6s SEL_SUPPRESSED=%-5s "
        "FREQ_TXN=%-4s/%-4s drain_max=%sms"
        % (h.get("act_cap_n"), h.get("act_cap_max_t"), ct["SEL_SUPPRESSED"],
           ct["FREQ_TXN"], ct["FREQ_TXN_TIMEOUT"], ct["FREQ_TXN_DRAIN_MAX_MS"]))
    say("            edges=%-6s discard_stop=%-5s width=%s(lo%s/hi%s) "
        "w_mean=%-7s rate=%-5s busy=%-4s err=%s"
        % (g["edges"], g.get("discard_stop"), g["rej_width"],
           g.get("rej_width_lo"), g.get("rej_width_hi"),
           round(g.get("w_mean") or 0, 1), g["rej_rate"], g["rej_busy"],
           j.get("error_hist")))


def attach(a):
    """Observe a run somebody else is driving. Touches nothing."""
    s = sock()
    j = stat(s, listen=3.0)
    if not j:
        say("no stat -- is the WebUI connected? (--attach does not open the "
            "peripheral channel itself, deliberately)")
        return 1, s
    say("attached. state=%s plate_freq=%s" % (j.get("state"), j.get("plate_freq")))
    if a.reset:
        say("resetting counters (asked for)")
        send(s, {"type": "reset_running_stat"})
        time.sleep(0.5)
    show(j, "start")

    deadline = time.time() + a.seconds
    while time.time() < deadline:
        time.sleep(min(a.every, max(1.0, deadline - time.time())))
        show(stat(s), "t+%ds" % int(time.time() - (deadline - a.seconds)))
    return 0, s


def main(a):
    s = sock()
    say("CONNECT (reopens the UART, so the board resets)")
    send(s, "!pd " + json.dumps(CONN), gap=1.0)
    time.sleep(5.0)

    # The whole point. Without it every verdict is NA and the run measures
    # nothing the 08-11 soak did not already measure.
    #
    # FI, not LD. `!ld` loads a def and stops there -- it does not open an
    # inspection session, so the camera is never put in trigger mode and no
    # frame is ever inspected. `!fi` is the packet the WebUI's "full inspection"
    # sends: it loads the same def AND starts the session.
    #
    # FI rather than CI is not a detail either. FI is TriggerMode(2) --
    # hardware-triggered, one frame per registered part, station region
    # ENFORCED. CI free-runs the camera and turns the region filter off, which
    # is right for authoring and wrong for a run that claims to measure sorting.
    say("starting FI on def: %s" % a.definition)
    send(s, "!fi " + json.dumps({"deffile": a.definition, "frame_count": -1}),
         gap=1.0)
    time.sleep(3.0)

    send(s, {"type": "clear_error"},
            {"type": "set_setup", "plate": {"freq": a.plate_freq}},
            {"type": "stepper_enable"})
    send(s, {"type": "reset_running_stat"})

    say("enter_insp_mode: CAL -> SPINUP -> READY")
    send(s, {"type": "enter_insp_mode"})
    t0, j = time.time(), None
    while time.time() - t0 < 90:
        j = stat(s, listen=1.5)
        st = (j or {}).get("state")
        if st == 101:
            break
        if st in (112, 113):
            say("HALTED before starting: state=%s err=%s"
                % (st, (j or {}).get("error_hist")))
            return 1, s
        time.sleep(1.0)
    if not j or j.get("state") != 101:
        say("never reached READY (state=%s)" % (j or {}).get("state"))
        return 1, s
    say("READY after %.0fs" % (time.time() - t0))

    # Counters are reset AFTER READY: CAL fires its own pulses, and counting
    # them as production traffic is what makes a yield ladder lie.
    send(s, {"type": "reset_running_stat"})

    deadline = time.time() + a.seconds
    n = 0
    while time.time() < deadline:
        time.sleep(min(a.every, max(1.0, deadline - time.time())))
        n += 1
        show(stat(s), "t+%ds" % int(time.time() - (deadline - a.seconds)))
    return 0, s


def teardown(s, rc, attached=False):
    say("== summary ==" if attached else "== stopping ==")
    try:
        if s is None:
            s = sock()
        if not attached:
            send(s, {"type": "set_setup", "plate": {"freq": 0}},
                    {"type": "exit_insp_mode"}, gap=0.5)
            time.sleep(3.0)
        j = stat(s, listen=2.5)
        if not j:
            say("WARNING: no stat after stop -- plate state UNCONFIRMED")
            return rc
        show(j, "final")
        g, ct = j["gate"], j["count"]
        judged = verdicts(ct)
        inflight = g["accept"] - judged - (g.get("discard_stop") or 0)
        say("")
        say("conservation:")
        say("  edges %d  ==  accept %d + Sigma rej %d  ->  residual %+d"
            % (g["edges"], g["accept"], sum(g.get(k, 0) for k in REJ),
               g["edges"] - g["accept"] - sum(g.get(k, 0) for k in REJ)))
        say("  accept %d - judged %d - discard_stop %d  =  %d still in RBuf"
            % (g["accept"], judged, g.get("discard_stop") or 0, inflight))
        # A NEGATIVE remainder is not a leak, and reading it as one wastes an
        # afternoon. reset_running_stat zeroes `accept` wherever the pipeline
        # happens to be, so any part admitted BEFORE the reset still increments
        # a verdict counter after it. The excess is exactly the in-flight
        # population at reset time -- one or two parts when reset at READY,
        # more if reset mid-run. Reset with the plate stopped to avoid it.
        if inflight < 0:
            say("  (negative: %d part(s) were admitted before the counter reset "
                "and judged after it -- not a leak)" % (-inflight))
        say("")
        acted = ct["SEL1"] + ct["SEL2"] + ct["SEL3"]
        if acted == 0:
            say("*** SEL1/SEL2/SEL3 ALL ZERO -- the sorting path did not move.")
            say("    Same hole the 8h soak left. Check the def actually loaded ")
            say("    and that it produces non-NA verdicts before running long.")
            rc = rc or 2
        else:
            say("sorting path MOVED: SEL1=%d SEL2=%d SEL3=%d (NA=%d)"
                % (ct["SEL1"], ct["SEL2"], ct["SEL3"], ct["NA"]))
        say("plate: state=%s plate_freq=%s" % (j.get("state"), j.get("plate_freq")))
        s.close()
    except Exception as e:
        say("WARNING: could not confirm the plate stopped: %r" % e)
    return rc


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--seconds", type=int, default=60)
    ap.add_argument("--hours", type=float, default=0.0)
    ap.add_argument("--plate-freq", type=int, default=10000)
    ap.add_argument("--every", type=float, default=15.0, help="sample period s")
    ap.add_argument("--def", dest="definition", default="data/test1.hydef")
    ap.add_argument("--attach", action="store_true",
                    help="observe a WebUI-driven run; never touches the plate")
    ap.add_argument("--reset", action="store_true",
                    help="--attach: zero the counters before observing")
    a = ap.parse_args()
    if a.hours:
        a.seconds = int(a.hours * 3600)
    rc, s = 1, None
    try:
        rc, s = (attach if a.attach else main)(a)
    except KeyboardInterrupt:
        say("\ninterrupted")
    finally:
        rc = teardown(s, rc, attached=a.attach)
    raise SystemExit(rc)
