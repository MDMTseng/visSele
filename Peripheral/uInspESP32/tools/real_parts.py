#!/usr/bin/env python3
"""Real-parts validation run: plate turning, real sensor detections.

Everything else in this directory injects phantom pulses. That is deliberate --
it makes rate and timing controllable -- but it means the object stream is
synthetic: pulses arrive when the harness says so, not when a part passes a
sensor, and the plate is usually held still. Every clean result from
burst_pairing.py is therefore evidence about the CLOCK and the pairing
arithmetic, and none of it is evidence that the machine sorts real parts.

This is the other half. The plate turns, the feeder runs, parts trip the sensor
on their own schedule, and the selectors actually fire. The numbers to read
afterwards are the same ones, but now they describe production:

  judged == accept        every accepted part got a verdict
  UNANSWERED == 0         nothing reached its selector without one
  disagree == 0           only meaningful while the core also pairs
                          (PERIF_CORE_PAIRING 1); at 0 there is no second
                          opinion to disagree with and agree/disagree are 0
                          BY CONSTRUCTION, not because things went well
  error_hist == []        no halt

The plate is left stopped and the machine out of inspection mode on every exit
path, including Ctrl-C -- a spinning plate is not something to leave behind
because a script died.

  python3 real_parts.py --seconds 300
  python3 real_parts.py --seconds 300 --plate-freq 10000
"""
import socket, sys, time, json, argparse

PORT = 4099
CONN = {"type": "CONNECT", "uart_name": "/dev/cu.usbserial-0001",
        "baudrate": 230400, "machine_type": "uInspESP32",
        "cat_ok": 3, "cat_ng": 1, "cam_idx": 1, "pairing": "timestamp"}


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


def stat(s, listen=3.0):
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


def show(j, tag):
    if not j:
        print("  %-8s NO STAT" % tag)
        return None
    cs, ct, g = j['cam_sync'], j['count'], j['gate']
    judged = ct['NA'] + ct['SEL1'] + ct['SEL2'] + ct['SEL3']
    print("  %-8s state=%-4s accept=%-6s judged=%-6s SEL1=%-5s SEL2=%-5s "
          "NA=%-5s SKIP=%-4s UNANS=%s"
          % (tag, j.get('state'), g['accept'], judged, ct['SEL1'], ct['SEL2'],
             ct['NA'], ct['SKIP'], ct['UNANSWERED']))
    print("           learned=%-4s agree=%-6s DISAGREE=%-4s rejected=%-4s "
          "rebuilds=%-3s recals=%s"
          % (cs['learned'], cs['agree'], cs['disagree'], cs.get('rejected'),
             cs.get('rebuilds'), cs.get('recals')))
    print("           delta_max=%-6s miss_max=%-6s window=%-6s "
          "cal_runs=%s cal_fails=%s sync_late=%s recal_skipped=%s"
          % (cs.get('delta_max_us'), cs.get('miss_delta_max_us'),
             cs.get('window_us'), cs.get('cal_runs'), cs.get('cal_fails'),
             cs.get('sync_late'), cs.get('recal_skipped')))
    print("           plate_freq=%-7s rbuf_peak=%-4s error_hist=%s"
          % (j.get('plate_freq'), j['health'].get('rbuf_peak'),
             j.get('error_hist')))
    return j


def main(a):
    s = sock()
    print("connecting, hard-resetting the board (CONNECT reopens the UART)")
    send(s, "!pd " + json.dumps(CONN), gap=1.0)
    time.sleep(4.0)

    send(s, {"type": "clear_error"},
            {"type": "set_setup", "plate_freq": a.plate_freq},
            {"type": "stepper_enable"})

    print("entering inspection mode: CAL (plate held still) -> SPINUP -> READY")
    send(s, {"type": "enter_insp_mode"})

    # CAL holds the plate at zero and takes its samples; SPINUP then ramps.
    # Wait for READY rather than guessing a duration -- a ramp to 10k pulse/s
    # at accel 2000 is several seconds on its own.
    t0 = time.time()
    while time.time() - t0 < 60:
        j = stat(s, listen=1.5)
        if j and j.get('state') == 101:
            break
        if j and j.get('state') in (112, 113):
            print("  HALTED before it started: state=%s err=%s"
                  % (j.get('state'), j.get('error_hist')))
            return 1, s
        time.sleep(1.0)
    show(j, "ready")
    if not j or j.get('state') != 101:
        print("  never reached READY")
        return 1, s

    print("running %ds with real parts" % a.seconds)
    t_end = time.time() + a.seconds
    while time.time() < t_end:
        time.sleep(min(30, max(1, t_end - time.time())))
        j = stat(s, listen=1.5)
        show(j, "t+%ds" % int(a.seconds - max(0, t_end - time.time())))
        if j and j.get('state') in (112, 113):
            print("  HALTED -- stopping the run")
            break

    time.sleep(6)          # let the pipeline drain
    j = stat(s)
    print("final:")
    show(j, "final")
    if not j:
        return 1, s

    cs, ct, g = j['cam_sync'], j['count'], j['gate']
    judged = ct['NA'] + ct['SEL1'] + ct['SEL2'] + ct['SEL3']
    bad = (cs['disagree'] or ct['UNANSWERED'] or j.get('error_hist'))
    print("  => %s" % ("FAIL" if bad else "clean"))
    if judged != g['accept']:
        # Not automatically a failure: parts still in flight at the last read
        # show up here too. Worth printing rather than hiding.
        print("     note: judged(%d) != accept(%d), delta %d"
              % (judged, g['accept'], g['accept'] - judged))
    return (1 if bad else 0), s


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument("--seconds", type=int, default=300)
    ap.add_argument("--plate-freq", type=int, default=10000)
    a = ap.parse_args()
    rc, s = 1, None
    try:
        rc, s = main(a)
    except KeyboardInterrupt:
        print("\ninterrupted")
    finally:
        # The plate must not be left turning, whatever went wrong above.
        try:
            if s is None:
                s = sock()
            send(s, {"type": "set_setup", "plate_freq": 0},
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
