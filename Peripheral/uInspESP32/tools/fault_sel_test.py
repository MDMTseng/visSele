#!/usr/bin/env python3
"""SEL_SUPPRESSED, driven end to end with numbers that are all known in advance.

  trig_phantom_pulse   one object per request, and the board ANNOUNCES its tid
  report tid cat:1     that exact object is given an NG verdict
  SWITCH               schedules its SEL1 actuation
  fault sel_suppress:M fails the guard for M of them

Expect: SEL1 = N-M, SEL_SUPPRESSED = M, sel_suppress_used = M.

No parts, no camera, no core inspection.

Two ways this was got wrong first, both worth not repeating:

  * dry run. It keeps the plate still, and it is also one of the three things
    the actuation guard tests -- so every actuation was suppressed by the
    harness, SEL_SUPPRESSED came out equal to the armed count by coincidence,
    and the injected fault was never consumed. sel_suppress_used is what caught
    it. The plate turns for real here.
  * guessing the tid. tid_counter is not reset by reset_running_stat and CAL
    consumes tids of its own, so 1..N is a guess. The cam_trig announcement
    carries the real one; read it rather than assume it.
"""
import json, re, socket, sys, time

sys.path.insert(0, "/Users/mdm/workspace/visSele/Peripheral/uInspESP32/tools")
from real_parts import CONN

N_OBJ, N_SUPPRESS, PLATE = 10, 4, 3000

sk = socket.create_connection(("127.0.0.1", 4099), timeout=5)
sk.settimeout(0.3)


def say(*a):
    print(*a, flush=True)


def raw(o, wait=1.2):
    sk.sendall((json.dumps(o) if isinstance(o, dict) else o).encode() + b"\n")
    buf, t0 = b"", time.time()
    while time.time() - t0 < wait:
        try:
            buf += sk.recv(8192)
        except socket.timeout:
            pass
    return buf.decode(errors="replace")


def objs(txt):
    out = []
    for l in txt.splitlines():
        l = l.strip()
        if l.startswith("{") and '"dbg"' not in l:
            try:
                out.append(json.loads(l.split("*")[0]))
            except Exception:
                pass
    return out


def stat():
    for m in objs(raw({"type": "get_running_stat"}, 1.8)):
        if "gate" in m:
            return m
    return None


raw("!pd " + json.dumps(CONN), 3.0)
time.sleep(5.0)
raw({"type": "clear_error"})
raw({"type": "fault", "clear": True})
raw({"type": "set_setup", "plate": {"freq": PLATE}})
raw({"type": "stepper_enable"})
raw({"type": "enter_insp_mode"})

t0, st = time.time(), None
while time.time() - t0 < 90:
    time.sleep(1.5)
    st = stat()
    if st and st.get("state") == 101:
        break
    if st and st.get("state") in (112, 113):
        say("halted: %s" % st.get("error_hist")); raise SystemExit(1)
if not st or st.get("state") != 101:
    say("never READY"); raise SystemExit(1)
say("READY after %.0fs" % (time.time() - t0))

raw({"type": "reset_running_stat"})
say("arm: suppress %d of %d actuations" % (N_SUPPRESS, N_OBJ))
raw({"type": "fault", "sel_suppress": N_SUPPRESS})

tids = []
for i in range(N_OBJ):
    txt = raw({"type": "trig_phantom_pulse"}, 1.4)
    tid = None
    for m in objs(txt):
        if m.get("type") == "cam_trig" and m.get("cam") == 1:
            tid = m.get("tid")
    if tid is None:
        say("  %d: no cam_trig announcement" % i); continue
    tids.append(tid)
    raw({"type": "report", "tid": tid, "cat": 1}, 0.4)   # 1 = NG = SEL1
    time.sleep(0.35)
say("injected and reported tids: %s" % tids)

time.sleep(5.0)
st = stat()
raw({"type": "set_setup", "plate": {"freq": 0}})
raw({"type": "exit_insp_mode"})
time.sleep(2.0)
raw({"type": "fault", "clear": True})

if st:
    c, f = st["count"], st.get("fault", {})
    used = f.get("sel_suppress_used") or 0
    say("")
    say("reported %d NG verdicts" % len(tids))
    say("  SEL1              = %s   (expect %d)" % (c["SEL1"], len(tids) - N_SUPPRESS))
    say("  SEL_SUPPRESSED    = %s   (expect %d)" % (c["SEL_SUPPRESSED"], N_SUPPRESS))
    say("  sel_suppress_used = %s   (expect %d)" % (used, N_SUPPRESS))
    say("  UNANSWERED=%s NA=%s SKIP=%s"
        % (c["UNANSWERED"], c["NA"], c["SKIP"]))
    ok = (used == N_SUPPRESS and c["SEL_SUPPRESSED"] == N_SUPPRESS
          and c["SEL1"] == len(tids) - N_SUPPRESS)
    say("")
    say("verdict: %s" % ("PASS" if ok else "MISMATCH -- read the numbers, not this line"))
