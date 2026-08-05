#!/usr/bin/env python3
"""Tiny client for the core's INSP_PERIF_CONSOLE port.

The core owns the serial link, so this is the only way to reach the device
while inspection is actually running.
"""
import socket, sys, time, json

PORT = 4099

def talk(cmds, listen=2.0, want=None):
    s = socket.create_connection(('127.0.0.1', PORT), timeout=5)
    s.settimeout(0.4)
    for c in cmds:
        s.sendall((c if isinstance(c, str) else json.dumps(c)).encode() + b'\n')
        time.sleep(0.15)
    out, t0 = [], time.time()
    while time.time() - t0 < listen:
        try:
            d = s.recv(8192)
        except socket.timeout:
            continue
        if not d:
            break
        out.append(d)
    s.close()
    lines = b''.join(out).decode(errors='replace').splitlines()
    if want:
        return [l for l in lines if want in l]
    return lines

def stat():
    for l in talk(['{"type":"get_running_stat"}'], 2.0, 'cam_sync'):
        try:
            return json.loads(l)
        except Exception:
            pass
    return None

if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'stat':
        j = stat()
        print(json.dumps(j, indent=1) if j else "no reply")
    else:
        for l in talk(sys.argv[1:], 2.5):
            print(l[:400])
