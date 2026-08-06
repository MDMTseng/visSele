#!/usr/bin/env python3
"""Long real-parts soak. Answers three open questions in one run.

  1. What is the residual DISTRIBUTION, not just its maximum?
     The match window has to sit above the worst residual that legitimately
     occurs, or good parts get refused. delta_max over four minutes says
     nothing about the tail a machine running all day meets, and it cannot
     tell "one outlier at 240us, everything else under 60" from "routinely
     near 240" -- which want very different windows. The device now keeps a
     log2 histogram; this reads it and prints the tail.

  2. Does the busy -> idle -> RECAL -> busy TRANSITION work with real parts?
     Not "RECAL under load" -- that is a contradiction. While parts flow, every
     report re-measures the offset, so it is never more than one part old and
     the recal trigger cannot fire by construction. Every real-parts run so far
     ended with recals=0 for exactly that reason, and correctly so.
     What is untested is the handover, which is where both RECAL bugs lived:
     parts still in flight when the recal begins. Real lines pause -- jams,
     batch gaps, shift changes -- so this shuts the gate periodically, lets the
     machine go quiet enough to recal, and opens it again.

  3. Does anything degrade over hours?
     min_heap, rbuf_peak, report latency, drift rate. Nothing here has run
     longer than four minutes.

The plate is stopped and inspection left on every exit path including Ctrl-C.

  python3 soak_real.py --hours 2
  python3 soak_real.py --minutes 20 --idle-every 300 --idle-for 15
"""
import socket, time, json, argparse, datetime

PORT = 4099
CONN = {"type": "CONNECT", "uart_name": "/dev/cu.usbserial-0001",
        "baudrate": 115200, "machine_type": "uInspESP32",
        "cat_ok": 1, "cat_ng": 2, "cam_idx": 1, "pairing": "timestamp"}


def sock():
    s = socket.create_connection(('127.0.0.1', PORT), timeout=5)
    s.settimeout(0.4)
    return s


def send(s, *cmds, gap=0.3):
    for c in cmds:
        s.sendall((c if isinstance(c, str) else json.dumps(c)).encode() + b'\n')
        time.sleep(gap)
        try:
            s.recv(65536)
        except socket.timeout:
            pass


def stat(s, listen=2.0):
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


def hist_line(h):
    """Buckets are [32<<i, 32<<(i+1)) us."""
    if not h:
        return "(none)"
    tot = sum(h) or 1
    out, cum = [], 0
    for i, n in enumerate(h):
        lo = 32 << i
        cum += n
        if n:
            out.append("%d-%d:%d(%.2f%%cum)" % (lo, lo * 2, n, cum * 100.0 / tot))
    return "  ".join(out) if out else "(empty)"


def tail_us(h):
    """Highest bucket with anything in it -- the tail the window must clear."""
    for i in range(len(h) - 1, -1, -1):
        if h[i]:
            return (32 << i) * 2
    return 0


def main(a):
    s = sock()
    print("connecting (CONNECT reboots the board, so counters start at zero)")
    send(s, "!pd " + json.dumps(CONN), gap=1.0)
    time.sleep(4.0)
    send(s, {"type": "clear_error"},
            {"type": "set_setup", "plate_freq": a.plate_freq},
            {"type": "stepper_enable"},
            {"type": "enter_insp_mode"})

    j, t0 = None, time.time()
    while time.time() - t0 < 90:
        j = stat(s, listen=1.5)
        if j and j.get('state') == 101:
            break
        if j and j.get('state') in (112, 113):
            print("halted before start: %s" % j.get('error_hist'))
            return 1, s
        time.sleep(1.0)
    if not j or j.get('state') != 101:
        print("never reached READY (state=%s)" % (j or {}).get('state'))
        return 1, s

    total = a.hours * 3600 + a.minutes * 60
    t_end = time.time() + total
    next_idle = time.time() + a.idle_every
    next_rep = time.time() + a.report_every
    print("soaking %.1f min: idle %ds every %ds to force RECAL"
          % (total / 60.0, a.idle_for, a.idle_every))
    print("%-8s %-7s %-7s %-6s %-5s %-6s %-7s %-8s %-9s %s"
          % ("elapsed", "accept", "judged", "recal", "state", "heap", "rbuf",
             "dmax", "tail", "err"))

    # A run that read nothing must FAIL, not pass.
    #
    # The first version judged on `halt or disagree`, both of which stay at
    # their initial values when no stat is ever read -- so a soak whose link
    # died on the first second printed "(no stat)" eleven times and then
    # "=> clean". Absence of evidence was being reported as evidence of
    # absence, which is the exact failure this whole day's tooling kept
    # producing.
    worst = {"halt": None, "disagree": 0, "samples": 0}
    while time.time() < t_end:
        now = time.time()
        if now >= next_idle:
            # Stop admitting parts and let the machine go quiet. The recal
            # trigger needs BOTH the clock stale and no real part registered
            # for cam_recal_idle_ms, so pausing the feeder is the only way to
            # reach it without stopping the line.
            send(s, {"type": "set_gate_disable", "on": True})
            time.sleep(a.idle_for)
            send(s, {"type": "set_gate_disable", "on": False})
            next_idle = time.time() + a.idle_every
        time.sleep(2.0)

        if time.time() >= next_rep:
            next_rep = time.time() + a.report_every
            j = stat(s)
            if not j:
                print("  (no stat)")
                continue
            cs, ct, g, h = j['cam_sync'], j['count'], j['gate'], j['health']
            judged = ct['NA'] + ct['SEL1'] + ct['SEL2'] + ct['SEL3']
            hs = cs.get('delta_hist') or []
            print("%-8s %-7s %-7s %-6s %-5s %-6s %-7s %-8s %-9s %s"
                  % (str(datetime.timedelta(seconds=int(time.time() - (t_end - total)))),
                     g['accept'], judged, cs.get('recals'), j.get('state'),
                     h.get('min_heap'), h.get('rbuf_peak'),
                     cs.get('delta_max_us'), tail_us(hs), j.get('error_hist')))
            worst['samples'] += 1
            if cs.get('disagree'):
                worst['disagree'] = cs['disagree']
            if j.get('state') in (112, 113):
                worst['halt'] = j.get('error_hist')
                print("  HALTED -- stopping the soak")
                break

    j = stat(s)
    print("\nfinal:")
    if j:
        cs = j['cam_sync']
        h = cs.get('delta_hist') or []
        print("  residual histogram (us): %s" % hist_line(h))
        print("  tail bucket top = %d us   delta_max = %s us"
              % (tail_us(h), cs.get('delta_max_us')))
        print("  recals=%s recal_skipped=%s cal_fails=%s rejected=%s rebuilds=%s"
              % (cs.get('recals'), cs.get('recal_skipped'), cs.get('cal_fails'),
                 cs.get('rejected'), cs.get('rebuilds')))
        print("  agree=%s DISAGREE=%s  min_heap=%s rbuf_peak=%s"
              % (cs.get('agree'), cs.get('disagree'),
                 j['health'].get('min_heap'), j['health'].get('rbuf_peak')))
        print("  report_latency avg=%sus max=%sus"
              % (j['report_latency'].get('avg_us'), j['report_latency'].get('max_us')))
        print("  state=%s error_hist=%s" % (j.get('state'), j.get('error_hist')))
    if worst['samples'] == 0:
        print("  => NO DATA: the link never answered. This is not a pass -- "
              "check the serial device is present.")
        return 1, s
    bad = worst['halt'] or worst['disagree']
    print("  => %s  (%d samples)"
          % ("FAIL (halt=%s disagree=%s)" % (worst['halt'], worst['disagree'])
             if bad else "clean", worst['samples']))
    return (1 if bad else 0), s


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument("--hours", type=float, default=0)
    ap.add_argument("--minutes", type=float, default=20)
    ap.add_argument("--plate-freq", type=int, default=10000)
    ap.add_argument("--idle-every", type=int, default=300)
    ap.add_argument("--idle-for", type=int, default=15)
    ap.add_argument("--report-every", type=int, default=60)
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
            send(s, {"type": "set_gate_disable", "on": False},
                    {"type": "set_setup", "plate_freq": 0},
                    {"type": "exit_insp_mode"}, gap=0.5)
            time.sleep(1.0)
            j = stat(s, listen=1.5)
            if j:
                print("left: state=%s plate_freq=%s"
                      % (j.get('state'), j.get('plate_freq')))
            s.close()
        except Exception as e:
            print("WARNING: could not confirm the plate stopped: %s" % e)
    raise SystemExit(rc)
