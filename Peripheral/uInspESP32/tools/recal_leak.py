#!/usr/bin/env python3
"""Is the 96 bytes per RECAL a leak, or a transient?

A 12-minute real-parts soak showed min_heap stepping down by exactly 96 bytes
on every recal and at no other moment:

    recals 0 -> 191512      recals 2 -> 191320
    recals 1 -> 191416      recals 3 -> 191224

min_heap cannot answer the question, because it is a high-water mark: a genuine
leak and a transient allocation that grows a little each time both make it step
down and never come back. So this samples the CURRENT free heap instead, and
runs enough recals that a 96-byte slope is unmistakable.

No parts and no plate motion are needed. The recal trigger only wants READY, a
stale clock, no recent registration and an empty RBuf -- all of which an idle
machine satisfies -- so dropping cam_recal_idle_ms to its 2000ms floor cycles
recals every couple of seconds.

Two phases, because "heap fell while recals happened" is not the same claim as
"recals caused it". Phase A leaves recal ON, phase B turns it OFF
(cam_recal_idle_ms 0) for the same duration. If the slope follows the recals
rather than the clock, the attribution holds.

  python3 recal_leak.py --minutes 4
"""
import socket, time, json, argparse

PORT = 4099
CONN = {"type": "CONNECT", "uart_name": "/dev/cu.usbserial-0001",
        "baudrate": 230400, "machine_type": "uInspESP32",
        "cat_ok": 3, "cat_ng": 1, "cam_idx": 1, "pairing": "timestamp"}


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


def stat(s, listen=1.8):
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


def phase(s, a, label, idle_ms):
    send(s, {"type": "set_setup", "cam_recal_idle_ms": idle_ms})
    j = stat(s)
    if not j:
        print("  %s: no stat" % label)
        return None
    r0 = j['cam_sync'].get('recals')
    h0 = j['health'].get('free_heap')
    print("  %-9s start recals=%s free_heap=%s (idle_ms=%d)"
          % (label, r0, h0, idle_ms))
    t_end = time.time() + a.minutes * 60
    last = None
    while time.time() < t_end:
        time.sleep(a.sample_every)
        j = stat(s)
        if not j:
            continue
        r = j['cam_sync'].get('recals')
        h = j['health'].get('free_heap')
        if last != (r, h):
            print("    recals=%-5s free_heap=%-8s min_heap=%-8s d_heap=%+d"
                  % (r, h, j['health'].get('min_heap'), (h - h0) if h and h0 else 0))
            last = (r, h)
    j = stat(s)
    if not j:
        return None
    r1 = j['cam_sync'].get('recals')
    h1 = j['health'].get('free_heap')
    dr, dh = r1 - r0, h1 - h0
    print("  %-9s end   recals=%s (+%d)  free_heap=%s (%+d)"
          % (label, r1, dr, h1, dh))
    if dr:
        print("            %.1f bytes per recal" % (dh / float(dr)))
    return {"recals": dr, "heap": dh}


def main(a):
    s = sock()
    send(s, "!pd " + json.dumps(CONN), gap=1.0)
    time.sleep(4.0)
    # plate_freq 15000 with the stepper disabled: the plate does not move, but
    # the stage timer runs. Calibration failed to converge (err 14) with
    # plate_freq 0, so this matches the setting every other tool here uses
    # rather than inventing a new one.
    send(s, {"type": "clear_error"},
            {"type": "set_setup", "plate_freq": 15000},
            {"type": "stepper_disable"},
            {"type": "enter_insp_mode"})
    j, t0 = None, time.time()
    while time.time() - t0 < 90:
        j = stat(s)
        if j and j.get('state') == 101:
            break
        if j and j.get('state') in (112, 113):
            print("halted before start: %s" % j.get('error_hist'))
            return 1, s
        time.sleep(1.0)
    if not j or j.get('state') != 101:
        print("never reached READY (state=%s)" % (j or {}).get('state'))
        return 1, s
    if j['health'].get('free_heap') is None:
        print("free_heap missing -- firmware not reflashed?")
        return 1, s

    print("idle machine, %g min per phase" % a.minutes)
    A = phase(s, a, "recal ON", 2000)
    B = phase(s, a, "recal OFF", 0)
    print("")
    if not A or not B:
        print("  => INCONCLUSIVE (a phase produced no data)")
        return 1, s
    print("  recal ON : %+d bytes over %d recals" % (A['heap'], A['recals']))
    print("  recal OFF: %+d bytes over %d recals" % (B['heap'], B['recals']))
    if A['recals'] == 0:
        print("  => INCONCLUSIVE: no recals fired, nothing was tested")
        return 1, s
    per = A['heap'] / float(A['recals'])
    # The OFF phase is the control: an idle machine still runs its main loop,
    # its serial handling and its logging, so a slope present in BOTH phases is
    # not the recal's doing.
    if A['heap'] < -16 and B['heap'] >= -16:
        print("  => LEAK: %.1f bytes per recal, and none without recals" % per)
        return 1, s
    if A['heap'] < -16 and B['heap'] < -16:
        print("  => heap falls in BOTH phases -- not attributable to recal; "
              "something else is consuming")
        return 1, s
    print("  => no leak: free heap is stable across %d recals" % A['recals'])
    return 0, s


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument("--minutes", type=float, default=4)
    ap.add_argument("--sample-every", type=float, default=3.0)
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
            send(s, {"type": "set_setup", "cam_recal_idle_ms": 10000},
                    {"type": "set_setup", "plate_freq": 0},
                    {"type": "exit_insp_mode"}, gap=0.5)
            s.close()
        except Exception as e:
            print("WARNING: could not restore: %s" % e)
    raise SystemExit(rc)
