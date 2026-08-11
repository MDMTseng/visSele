#!/usr/bin/env python3
"""Ask the device something WITHOUT resetting it.

soak_sched.board_rpc opens 4099 and sends `!pd {"type":"CONNECT",...}` first,
because a headless core opens no peripheral channel and every later question
would read as a dead board. That CONNECT reopens the UART, which toggles DTR,
which resets the ESP32 -- fine on a bench, fatal to a run in progress.

When something else already holds the channel open (the WebUI, normally), the
channel exists and plain lines are enough: on this socket `!pd` addresses the
CORE and everything else goes verbatim to the DEVICE. So: no CONNECT, no
reset, no interruption.

Usage:
    peek.py                 one get_setup, print the blow geometry
    peek.py watch [secs]    poll until interrupted, one line per sample
"""
import json, socket, sys, time

PORT = 4099
# get_setup is ~3kB and the link is 230400 baud, so one reply is ~130ms of
# wire that the panel's own polling also wants. Slow, and never two at once.
PERIOD = 3.0


def ask(cmds, wait=3.0):
    """Send every command down ONE socket, back to back, then read.

    The first version opened a socket per command and waited 3s on each, so a
    get_setup and the get_running_stat beside it were up to a second apart.
    During a ramp that is 2000 Hz of speed change between the two halves of one
    "sample", which is enough to make a correctly-tracking window look badly
    wrong -- and it did: a mid-ramp row read 35ms against an asked 50ms purely
    because the tick count and the speed came from different moments.
    """
    if isinstance(cmds, dict):
        cmds = [cmds]
    s = socket.create_connection(("127.0.0.1", PORT), timeout=5)
    s.settimeout(0.4)
    for c in cmds:
        s.sendall((json.dumps(c) + "\n").encode())
    out, t0, buf = [], time.time(), b""
    while time.time() - t0 < wait:
        try:
            buf += s.recv(65536)
        except socket.timeout:
            continue
        except OSError:
            break
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            line = line.strip()
            if line.startswith(b"{"):
                try:
                    out.append(json.loads(line.split(b"*")[0].decode()))
                except Exception:
                    pass
    s.close()
    return out


def sample():
    """One round trip, both documents, so the ticks and the speed agree."""
    d = st = None
    for m in ask([{"type": "get_setup"}, {"type": "get_running_stat"}], wait=4.0):
        if "plate" in m and "stage_pulse_offset" in m:
            d = m
        elif "plate_freq_meas" in m or "gate" in m:
            st = m
    return d, st


def row(d, st):
    spo = d.get("stage_pulse_offset") or {}
    wus = d.get("stage_pulse_width_us") or {}
    ctr = d.get("stage_pulse_center") or {}
    setp = (d.get("plate") or {}).get("freq") or 0
    meas = (st or {}).get("plate_freq_meas")
    cur = meas if meas else setp
    parts = []
    for k in ("CAM1", "SEL1"):
        on, off = spo.get(k + "_on"), spo.get(k + "_off")
        if on is None or off is None:
            continue
        t = off - on
        # What that arc actually lasts at the speed the plate is doing now.
        ms = (t * 1e6 / (2.0 * cur) / 1000.0) if cur else float("nan")
        parts.append("%s %5d t = %6.1f ms (asked %.1f)%s"
                     % (k, t, ms, (wus.get(k) or 0) / 1000.0,
                        " [ctr %d]" % ctr[k] if ctr.get(k) else ""))
    return "setp %6.0f meas %8.1f | %s" % (setp, meas or 0, "  |  ".join(parts))


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "watch":
        n = float(sys.argv[2]) if len(sys.argv) > 2 else 1e9
        t0 = time.time()
        while time.time() - t0 < n:
            d, st = sample()
            print(("%6.1fs " % (time.time() - t0)) +
                  (row(d, st) if d else "no reply"), flush=True)
            time.sleep(PERIOD)
        return 0
    d, st = sample()
    if not d:
        print("no get_setup reply -- is a client holding the perif channel open?")
        return 1
    print(row(d, st))
    return 0


if __name__ == "__main__":
    sys.exit(main())
