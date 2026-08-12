#!/usr/bin/env python3
"""Does a stop cut a blow that is already out, or let it finish?

SELn_Count is incremented when the blow STARTS. If a stop truncates it, the
counter claims an ejection the bin never received, and nothing afterwards can
explain the discrepancy.

The blow is 50ms in production, which a JSON round trip cannot resolve. So the
width is temporarily set to 500ms: same code path, same decision, ten times the
observation window. Restored at the end.

  phantom object + NG verdict   schedules a blow
  poll SEL1 until it goes ON
  exit_insp_mode immediately    the stop path, mid-blow
  keep polling                  cut short, or held to full width?
"""
import json, socket, sys, time

sys.path.insert(0, "/Users/mdm/workspace/visSele/Peripheral/uInspESP32/tools")
from real_parts import CONN

WIDE_US = 500000      # 500ms, so the window is observable
PLATE   = 3000

sk = socket.create_connection(("127.0.0.1", 4099), timeout=5)
sk.settimeout(0.12)


def say(*a):
    print(*a, flush=True)


def raw(o, wait=0.9):
    sk.sendall((json.dumps(o) if isinstance(o, dict) else o).encode() + b"\n")
    buf, t0 = b"", time.time()
    while time.time() - t0 < wait:
        try:
            buf += sk.recv(8192)
        except socket.timeout:
            pass
    out = []
    for l in buf.decode(errors="replace").splitlines():
        l = l.strip()
        if l.startswith("{") and '"dbg"' not in l:
            try:
                out.append(json.loads(l.split("*")[0]))
            except Exception:
                pass
    return out


def sel1():
    for m in raw({"type": "pin_read", "pins": [25]}, 0.35):
        if "vals" in m:
            return m["vals"][0]       # 0 = ON (active low)
    return None


raw("!pd " + json.dumps(CONN), 3.0)
time.sleep(5.0)
raw({"type": "clear_error"})
raw({"type": "fault", "clear": True})
say("widen SEL1 to %d us so the blow is observable" % WIDE_US)
raw({"type": "set_setup", "stage_pulse_width_us": {"SEL1": WIDE_US}})
raw({"type": "set_setup", "plate": {"freq": PLATE}})
raw({"type": "stepper_enable"})
raw({"type": "enter_insp_mode"})

t0 = time.time()
ready = False
while time.time() - t0 < 90:
    time.sleep(1.5)
    for m in raw({"type": "get_running_stat"}, 1.2):
        if m.get("state") == 101:
            ready = True
    if ready:
        break
if not ready:
    say("never READY"); raise SystemExit(1)
say("READY")

# One object, one NG verdict -> one blow, roughly SEL1_on ticks later.
# The announcement can lag the request; keep listening rather than assuming.
tid = None
for attempt in range(4):
    for m in raw({"type": "trig_phantom_pulse"} if attempt == 0 else {"type": "PING"}, 2.5):
        if m.get("type") == "cam_trig" and m.get("cam") == 1:
            tid = m.get("tid")
    if tid is not None:
        break
say("phantom tid=%s" % tid)
if tid is not None:
    raw({"type": "report", "tid": tid, "cat": 1}, 0.4)

say("watching SEL1 for the blow...")
t0 = time.time()
on_at = None
while time.time() - t0 < 25:
    v = sel1()
    if v == 0:
        on_at = time.time()
        say("  BLOW ON at t+%.2fs -- stopping now" % (on_at - t0))
        raw({"type": "exit_insp_mode"}, 0.15)
        break
    time.sleep(0.05)

if on_at is None:
    say("no blow seen");
else:
    off_at = None
    t1 = time.time()
    while time.time() - t1 < 3.0:
        if sel1() == 1:
            off_at = time.time()
            break
        time.sleep(0.04)
    if off_at:
        held = (off_at - on_at) * 1000.0
        say("")
        say("blow held %.0f ms after the stop was issued" % held)
        say("configured width %.0f ms" % (WIDE_US / 1000.0))
        say("")
        if held > WIDE_US / 1000.0 * 0.6:
            say("VERDICT: the blow was allowed to finish")
        else:
            say("VERDICT: the blow was CUT SHORT (%.0f%% of its width)"
                % (held / (WIDE_US / 1000.0) * 100.0))
    else:
        say("valve never released -- that is the OTHER failure, and worse")

raw({"type": "set_setup", "plate": {"freq": 0}})
raw({"type": "exit_insp_mode"})
time.sleep(1.5)
raw({"type": "set_setup", "stage_pulse_width_us": {"SEL1": 50000}})
say("SEL1 width restored to 50000 us")
for m in raw({"type": "pin_read", "pins": [25, 26, 32]}, 0.6):
    if "vals" in m:
        say("final SEL pins %s (1=OFF)" % m["vals"])
