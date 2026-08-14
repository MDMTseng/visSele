#!/usr/bin/env python3
"""Ask the board something over serial. THIS REBOOTS THE BOARD. Read on.

Opening a serial port on this ESP32 asserts DTR/RTS and the auto-reset circuit
takes that as "reset now" -- so asking the board for its error history clears
the history you came for. On 2026-08-10 that destroyed the evidence for two
separate faults before anyone noticed.

This file was first written claiming to avoid it by de-asserting both lines
BEFORE open(). MEASURED: that does not work here, and neither does clearing
HUPCL. Three consecutive queries all reported uptime_s=0 / POWERON:

    stty -f /dev/cu.usbserial-0001 -hupcl   +   dtr=False, rts=False pre-open
    -> query 1: uptime_s=0    query 2: uptime_s=0    query 3: uptime_s=0

So treat a reset as unavoidable on this rig, and pick the tool accordingly:

  * machine RUNNING, or you need state that would be lost -> DO NOT use this.
    Ask through the core's peripheral console (INSP_PERIF_CONSOLE, port 4099).
    The core holds the port open continuously, so nothing is reset:
        printf '{"type":"get_running_stat"}\n' | nc 127.0.0.1 4099
  * board IDLE and a reboot is acceptable (reading NVS-backed settings, or
    confirming what the board came up with) -> this is fine, and --allow-reset
    is required so it is never a surprise.

  python3 board_query.py --allow-reset
  python3 board_query.py --allow-reset '{"type":"poll"}'

Also: only one thing may hold the port. The core owns it during a run, and a
second reader makes the device look unresponsive.
"""
import serial, json, time, sys, argparse

ap = argparse.ArgumentParser()
ap.add_argument("cmd", nargs="?", default='{"type":"get_running_stat"}')
ap.add_argument("--port", default="/dev/cu.usbserial-0001")
ap.add_argument("--baud", type=int, default=230400)
ap.add_argument("--wait", type=float, default=2.0)
ap.add_argument("--allow-reset", action="store_true",
                help="required: opening the port reboots this board")
a = ap.parse_args()

if not a.allow_reset:
    sys.exit("refusing: opening the port REBOOTS this board and clears "
             "error_hist/uptime/counters. Pass --allow-reset if the board is "
             "idle and that is acceptable; otherwise ask through the core's "
             "peripheral console on port 4099 (see the module docstring).")

s = serial.Serial()
s.port = a.port
s.baudrate = a.baud
s.timeout = 0.4
# Kept because they are harmless and correct in principle -- but measured NOT
# to prevent the reset on this hardware. See the docstring.
s.dtr = False
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
