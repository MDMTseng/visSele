#!/usr/bin/env python3
"""Ask the board something over serial WITHOUT rebooting it.

Opening a serial port on an ESP32 dev board asserts DTR/RTS, and the auto-reset
circuit takes that as "reset now". So the obvious way to read error_hist --
open the port, ask, print -- reboots the board and clears the very history you
came for. That happened on 2026-08-10: the answer came back uptime_s=1,
reset_reason POWERON, error_hist [], and the halt it was meant to explain was
gone.

Both lines have to be de-asserted BEFORE the port is opened, which means
constructing the Serial object unopened.

  python3 board_query.py                       # get_running_stat, health+errors
  python3 board_query.py '{"type":"poll"}'
  python3 board_query.py --port /dev/cu.usb... '{"type":"get_setup"}'

Only safe while nothing else holds the port -- the core owns it during a run,
and two readers make the device look unresponsive.
"""
import serial, json, time, sys, argparse

ap = argparse.ArgumentParser()
ap.add_argument("cmd", nargs="?", default='{"type":"get_running_stat"}')
ap.add_argument("--port", default="/dev/cu.usbserial-0001")
ap.add_argument("--baud", type=int, default=230400)
ap.add_argument("--wait", type=float, default=2.0)
a = ap.parse_args()

s = serial.Serial()
s.port = a.port
s.baudrate = a.baud
s.timeout = 0.4
s.dtr = False          # both BEFORE open(), or the board resets
s.rts = False
s.open()
time.sleep(0.3)
s.reset_input_buffer()
s.write(a.cmd.encode() + b"\n")
time.sleep(a.wait)
raw = s.read(400000).decode("utf8", "replace")
s.close()

want = json.loads(a.cmd).get("type", "")
best = None
for l in raw.splitlines():
    if l.strip().startswith("{"):
        try:
            j = json.loads(l.split("*")[0])
        except Exception:
            continue
        if want == "get_running_stat" and ("cam_sync" in j or "error_hist" in j):
            best = j if best is None else {**best, **j}
        elif j.get("type") == want or want in ("get_setup",) and "gate" in j:
            best = j
if best is None:
    print("no reply matched %r; raw tail:\n%s" % (want, raw[-500:]))
    sys.exit(1)
print(json.dumps(best, indent=1)[:4000])
