#!/usr/bin/env python3
"""Load-vs-quality sweep on the still-plate rig.

Each trial: hard-reset the board (re-CONNECT), configure dry-run, run a
device-generated phantom train at a fixed rate, drain, read the counters.

The rates bracket the camera's measured 35-36Hz trigger ceiling, so the sweep
shows where the pairing starts to degrade and how sharply.
"""
import socket, time, json, sys, datetime

PORT = 4099
OUT = '/private/tmp/claude-501/-Users-mdm-workspace-visSele/a4128deb-34af-40d7-aa32-0cf3aabee171/scratchpad/soak_results.txt'
CONN = {"type": "CONNECT", "uart_name": "/dev/cu.usbserial-0001",
        "baudrate": 115200, "machine_type": "uInspESP32",
        "cat_ok": 1, "cat_ng": 2, "cam_idx": 1, "pairing": "timestamp"}


def sock():
    s = socket.create_connection(('127.0.0.1', PORT), timeout=5)
    s.settimeout(0.4)
    return s


def send(s, *cmds, gap=0.25):
    for c in cmds:
        s.sendall((c if isinstance(c, str) else json.dumps(c)).encode() + b'\n')
        time.sleep(gap)
        try: s.recv(65536)
        except socket.timeout: pass


def stat(s, listen=3.0):
    s.sendall(b'{"type":"get_running_stat"}\n')
    buf, t0 = b'', time.time()
    while time.time() - t0 < listen:
        try: buf += s.recv(8192)
        except socket.timeout: continue
    for l in buf.decode(errors='replace').splitlines():
        if 'cam_sync' in l:
            try: return json.loads(l)
            except Exception: pass
    return None


def trial(hz, n, pairing="timestamp"):
    s = sock()
    c = dict(CONN); c["pairing"] = pairing
    send(s, "!pd " + json.dumps(c), gap=1.0)
    time.sleep(4.5)
    send(s, {"type": "clear_error"},
            {"type": "set_setup", "min_detect_sep_us": 12000,
             "unanswered_stop_after": 1000000},
            {"type": "enter_insp_mode"},
            {"type": "set_setup", "plate_freq": 15000},
            {"type": "stepper_disable"})
    time.sleep(2)
    send(s, {"type": "reset_running_stat"})
    send(s, {"type": "trig_phantom_train", "count": n, "hz": hz}, gap=0.5)
    time.sleep(n / float(hz) + 12)
    j = stat(s)
    send(s, {"type": "trig_phantom_train", "count": 0},
            {"type": "set_setup", "plate_freq": 0},
            {"type": "exit_insp_mode"})
    s.close()
    return j


def main():
    rates = [30, 33, 35, 38, 41]
    n = 500
    sweeps = int(sys.argv[1]) if len(sys.argv) > 1 else 4
    with open(OUT, 'a') as f:
        f.write("\n==== sweep session %s ====\n" % datetime.datetime.now().isoformat(timespec='seconds'))
        f.write("%-6s %-5s %-7s %-6s %-6s %-8s %-9s %-8s %-8s\n" %
                ("sweep", "hz", "judged", "SKIP", "UNANS", "disagree", "rejected", "rebuilds", "resid"))
        f.flush()
        for sw in range(sweeps):
            for hz in rates:
                try:
                    j = trial(hz, n)
                except Exception as e:
                    f.write("%-6d %-5d  ERROR %s\n" % (sw, hz, e)); f.flush(); continue
                if not j:
                    f.write("%-6d %-5d  NO STAT\n" % (sw, hz)); f.flush(); continue
                cs, ct = j['cam_sync'], j['count']
                judged = ct['NA'] + ct['SEL1'] + ct['SEL2'] + ct['SEL3']
                f.write("%-6d %-5d %-7d %-6d %-6d %-8d %-9s %-8s %-8s\n" % (
                    sw, hz, judged, ct['SKIP'], ct['UNANSWERED'], cs['disagree'],
                    cs.get('rejected', '-'), cs.get('rebuilds', '-'), cs['resid_us']))
                f.flush()
    print("done")


if __name__ == '__main__':
    main()
