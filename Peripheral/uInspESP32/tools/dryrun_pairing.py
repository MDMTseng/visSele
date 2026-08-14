#!/usr/bin/env python3
"""One dry-run trial, from a hard-reset board.

Re-issuing CONNECT tears the peripheral channel down and reopens the UART,
which toggles DTR and resets the ESP32 -- so every trial starts with
CAM_SYNC (learned/agree/disagree/resid_max) genuinely at zero. reset_running_stat
does NOT clear those, which is why the earlier deltas were contaminated.

The board reloads NVS on that reset, so the run config is re-applied each time.
"""
import socket, sys, time, json
from uinsp_cfg import regroup

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


def trial(hz, n, pairing="timestamp"):
    s = sock()
    c = dict(CONN); c["pairing"] = pairing
    send(s, "!pd " + json.dumps(c), gap=1.0)
    time.sleep(4.0)                       # board reboot + core channel setup
    j = stat(s)
    base = j['cam_sync'] if j else None
    print("  after reset: learned=%s agree=%s disagree=%s" %
          (base['learned'], base['agree'], base['disagree']) if base else "  no stat")

    send(s, {"type": "clear_error"},
            {"type": "set_setup", "min_detect_sep_us": 15000,
             "unanswered_stop_after": 100000},
            {"type": "enter_insp_mode"},
            {"type": "set_setup", "plate_freq": 15000},
            {"type": "stepper_disable"})
    time.sleep(3)

    cmd = b'{"type":"trig_phantom_pulse"}\n'
    t0 = time.time()
    for i in range(n):
        s.sendall(cmd)
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
    rate = n / (time.time() - t0)
    time.sleep(10)                        # let the pipeline drain

    j = stat(s)
    send(s, {"type": "set_setup", "plate_freq": 0}, {"type": "exit_insp_mode"})
    s.close()
    return rate, j


if __name__ == '__main__':
    hz = float(sys.argv[1]); n = int(sys.argv[2])
    pairing = sys.argv[3] if len(sys.argv) > 3 else "timestamp"
    print("trial: %.0f/s x %d, core pairing=%s" % (hz, n, pairing))
    rate, j = trial(hz, n, pairing)
    if not j:
        print("  NO STAT"); raise SystemExit(1)
    cs, ct = j['cam_sync'], j['count']
    tot = ct['NA'] + ct['SEL1'] + ct['SEL2'] + ct['SEL3']
    print("  injected %.1f/s  accept=%s" % (rate, j['gate']['accept']))
    print("  judged=%-5s SKIP=%-5s UNANS=%-4s" % (tot, ct['SKIP'], ct['UNANSWERED']))
    print("  learned=%-5s agree=%-5s disagree=%-4s rejected=%-5s rebuilds=%-3s" %
          (cs['learned'], cs['agree'], cs['disagree'],
           cs.get('rejected','-'), cs.get('rebuilds','-')))
    print("  resid=%s  resid_max=%s" % (cs['resid_us'], cs['resid_max_us']))
