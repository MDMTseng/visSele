#!/usr/bin/env python3
"""
uInspESP32 hardware verification helper.

Drives the firmware's JSON-over-serial protocol directly, so stages 0-3 of
docs/HW_VERIFICATION_CHECKLIST.md can be run without the core or the WebUI in
the loop. That separation is the point: when something fails here, the firmware
is the only thing that could have failed.

Wire format (verified against src/comm/Data_Layer_Protocol.cpp): plain JSON
text, no binary framing, no delimiter. The firmware's receiver counts braces
and dispatches when nesting returns to zero. Anything that is not '{' or '['
at message start latches a protocol error which blocks every command except
RESET -- so this tool never writes stray bytes, and offers `reset` to clear it.

Usage:
    python uinsp_test.py --port COM6 ports
    python uinsp_test.py --port COM6 stage0
    python uinsp_test.py --port COM6 monitor --seconds 60
    python uinsp_test.py --port COM6 selectors
    python uinsp_test.py --port COM6 send '{"type":"PING"}'
"""

import argparse
import json
import sys
import threading
import time
from collections import deque

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("pyserial is required:  pip install pyserial")


# --- framing ---------------------------------------------------------------

class UInspLink:
    """Serial link speaking the firmware's brace-framed JSON.

    Replies and asynchronous device messages arrive on the same stream, so the
    reader thread splits them: anything carrying an 'id' we sent is routed to
    the waiting caller, everything else (bTrigInfo, systemInfo, dbg) goes to
    the async queue.
    """

    def __init__(self, port, baud=115200, verbose=False):
        self.ser = serial.Serial(port, baud, timeout=0.05)
        self.verbose = verbose
        self._id = 1000
        self._pending = {}
        self._lock = threading.Lock()
        self._async = deque(maxlen=20000)
        self._async_ev = threading.Event()
        self._raw_log = deque(maxlen=5000)
        self._stop = False
        self._rx = threading.Thread(target=self._reader, daemon=True)
        self._rx.start()

    def close(self):
        self._stop = True
        time.sleep(0.15)
        try:
            self.ser.close()
        except Exception:
            pass

    def _reader(self):
        buf = ""
        depth = 0
        in_str = False
        esc = False
        fails = 0
        frame_t0 = None
        while not self._stop:
            try:
                chunk = self.ser.read(4096)
                fails = 0
            except Exception as exc:
                # A transient driver hiccup must not silently kill reception:
                # writes would keep working, the board would keep executing
                # commands, and every reply would just vanish for the rest of
                # the run -- which looks exactly like a dead board.
                if self._stop:
                    break
                fails += 1
                if fails >= 50:
                    print(f"  [serial read failed {fails}x, giving up: {exc}]")
                    break
                print(f"  [serial read error, retrying: {exc}]")
                time.sleep(0.1)
                continue
            if not chunk:
                # A frame that has been open for over a second is not a slow
                # frame -- at 115200 nothing legitimate takes that long. It is
                # a poisoned one: the firmware's recv_ERROR dbg embeds RAW
                # received bytes, and a stray quote in there flips the string
                # parity so every later frame looks like string content and
                # reception goes permanently silent. Drop it and resync.
                if depth and frame_t0 and time.time() - frame_t0 > 1.0:
                    print("  [dropping a stale half-frame; resyncing]")
                    buf, depth, in_str, esc = "", 0, False, False
                    frame_t0 = None
                continue
            for ch in chunk.decode("utf-8", errors="replace"):
                if depth == 0:
                    # Resync point. The firmware would fault on stray bytes;
                    # we just ignore them so a mid-stream attach still works.
                    if ch not in "{[":
                        continue
                    buf = ""
                    frame_t0 = time.time()
                buf += ch

                if in_str:
                    if esc:
                        esc = False
                    elif ch == "\\":
                        esc = True
                    elif ch == '"':
                        in_str = False
                    continue
                if ch == '"':
                    in_str = True
                elif ch in "{[":
                    depth += 1
                elif ch in "}]":
                    depth -= 1
                    if depth == 0:
                        # Belt and braces: nothing a device can send should be
                        # able to take the reader thread down.
                        try:
                            self._dispatch(buf)
                        except Exception as exc:      # pragma: no cover
                            print(f"  [dispatch error: {exc}]")
                        buf = ""

    def _dispatch(self, text):
        self._raw_log.append((time.time(), text))
        if self.verbose:
            print("  RX", text[:200])
        try:
            msg = json.loads(text)
        except json.JSONDecodeError:
            return

        # Only objects carry a command id. A top-level array (or any other
        # value) is device chatter, not a reply -- and must not be allowed to
        # raise, because an exception here would kill the reader thread and
        # stop all reception silently for the rest of the run.
        mid = msg.get("id") if isinstance(msg, dict) else None

        with self._lock:
            slot = self._pending.pop(mid, None) if mid is not None else None
        if slot is not None:
            slot["reply"] = msg
            slot["event"].set()
        else:
            self._async.append((time.time(), msg))
            self._async_ev.set()

    def send(self, obj, timeout=2.0):
        """Send a command and wait for the reply that echoes its id."""
        obj = dict(obj)
        self._id += 1
        mid = self._id
        obj["id"] = mid

        ev = threading.Event()
        slot = {"event": ev, "reply": None}
        with self._lock:
            self._pending[mid] = slot

        raw = json.dumps(obj, separators=(",", ":")).encode()
        if self.verbose:
            print("  TX", raw.decode())
        self.ser.write(raw)
        self.ser.flush()

        got = ev.wait(timeout)
        with self._lock:
            # _dispatch removes it on a hit; this only matters on timeout.
            self._pending.pop(mid, None)
        return slot["reply"] if got else None

    def send_nowait(self, obj):
        """Fire and forget.

        Some commands never answer -- `report` ends with doRsp=false in the
        firmware whether it matched an object or not. Waiting on those would
        burn the full timeout per part, which for a paced test also stretches
        the gap between parts far beyond what was asked for.
        """
        obj = dict(obj)
        self._id += 1
        obj["id"] = self._id
        raw = json.dumps(obj, separators=(",", ":")).encode()
        if self.verbose:
            print("  TX", raw.decode(), "(no reply expected)")
        self.ser.write(raw)
        self.ser.flush()

    def drain_async(self):
        out = []
        while self._async:
            out.append(self._async.popleft())
        return out


# --- reporting -------------------------------------------------------------

class Report:
    def __init__(self):
        self.rows = []

    def add(self, ref, desc, ok, detail=""):
        self.rows.append((ref, desc, ok, detail))
        mark = {True: "PASS", False: "FAIL", None: "MANUAL"}[ok]
        colour = {True: "\033[32m", False: "\033[31m", None: "\033[33m"}[ok]
        print(f"  {colour}{mark:<6}\033[0m {ref:<6} {desc}")
        if detail:
            for line in str(detail).splitlines():
                print(f"         {line}")

    def summary(self):
        p = sum(1 for r in self.rows if r[2] is True)
        f = sum(1 for r in self.rows if r[2] is False)
        m = sum(1 for r in self.rows if r[2] is None)
        print(f"\n  {p} passed, {f} failed, {m} need a human")
        return f == 0

    def write_markdown(self, path):
        with open(path, "w", encoding="utf-8") as fh:
            fh.write("# uInspESP32 hardware verification run\n\n")
            fh.write(time.strftime("Run at %Y-%m-%d %H:%M:%S\n\n"))
            fh.write("| # | Check | Result | Detail |\n|---|---|---|---|\n")
            for ref, desc, ok, detail in self.rows:
                mark = {True: "PASS", False: "**FAIL**", None: "manual"}[ok]
                d = str(detail).replace("\n", "<br>").replace("|", "\\|")
                fh.write(f"| {ref} | {desc} | {mark} | {d} |\n")
        print(f"\n  written: {path}")


def ask(prompt):
    """Yes/no question for the checks only a person standing at the machine
    can answer. Returns True/False."""
    while True:
        a = input(f"\033[33m  ?  {prompt} [y/n/skip] \033[0m").strip().lower()
        if a in ("y", "yes"):
            return True
        if a in ("n", "no"):
            return False
        if a in ("s", "skip", ""):
            return None


# --- stage 0: firmware alone ----------------------------------------------

def stage0(link, rep):
    print("\n\033[1m== Stage 0: firmware alone ==\033[0m")

    r = link.send({"type": "PING"})
    rep.add("0.1", "PING -> PONG", bool(r and r.get("type") == "PONG"), r)

    setup = link.send({"type": "get_setup"}, timeout=3.0)
    rep.add("0.2", "get_setup returns machine config",
            bool(setup and "stage_pulse_offset" in setup),
            json.dumps(setup, ensure_ascii=False)[:400] if setup else "no reply")

    if not setup:
        print("  aborting stage 0: the board is not answering get_setup")
        return

    for key in ("machine_id", "cfg_from_nvs", "pulse_minWidth", "pulse_maxWidth"):
        rep.add("0.2", f"  field present: {key}", key in setup,
                repr(setup.get(key)))

    rep.add("0.3", "cfg_from_nvs reported", "cfg_from_nvs" in setup,
            f"cfg_from_nvs={setup.get('cfg_from_nvs')} "
            f"(False on a board that has never been persisted)")

    # 0.4 persist
    probe_id = time.strftime("T%H%M%S")
    r = link.send({"type": "set_setup", "machine_id": probe_id, "persist": True},
                  timeout=4.0)
    rep.add("0.4", f"set_setup persist machine_id={probe_id}",
            bool(r and r.get("persisted") is True), r)

    r2 = link.send({"type": "get_setup"}, timeout=3.0)
    rep.add("0.4", "machine_id readable before power cycle",
            bool(r2 and r2.get("machine_id") == probe_id),
            f"machine_id={r2.get('machine_id') if r2 else None}")

    print("\n  \033[1m0.5 needs a power cycle.\033[0m")
    print(f"  Power the board OFF and ON, then press Enter. Looking for"
          f" machine_id=={probe_id!r} and cfg_from_nvs==True.")
    input("  [Enter when the board is back] ")

    # The port usually re-enumerates; give the link a moment and retry.
    ok, detail = False, "no reply after power cycle"
    for _ in range(10):
        r3 = link.send({"type": "get_setup"}, timeout=2.0)
        if r3:
            ok = (r3.get("machine_id") == probe_id and
                  r3.get("cfg_from_nvs") is True)
            detail = (f"machine_id={r3.get('machine_id')} "
                      f"cfg_from_nvs={r3.get('cfg_from_nvs')}")
            break
        time.sleep(1.0)
    rep.add("0.5", "NVS survives power cycle", ok, detail)
    if not ok:
        print("  \033[31m  STOP. Config management for both machines rests on"
              " this. Do not continue until 0.5 passes.\033[0m")

    # 0.6 clear
    r = link.send({"type": "clear_saved_setup"}, timeout=4.0)
    rep.add("0.6", "clear_saved_setup accepted",
            bool(r and r.get("cleared") is True), r)
    print("  Power cycle again to confirm it came back on compiled defaults.")
    if ask("power cycled?") is not None:
        r4 = link.send({"type": "get_setup"}, timeout=3.0)
        rep.add("0.6", "cfg_from_nvs back to False after clear",
                bool(r4 and r4.get("cfg_from_nvs") is False),
                f"cfg_from_nvs={r4.get('cfg_from_nvs') if r4 else None}")


# --- stage 0.7: error path -------------------------------------------------

def stage_error(link, rep):
    print("\n\033[1m== Stage 0.7: error path (ISR deferral, commit 535d92fb) ==\033[0m")
    print("  Before this change an inspection error called pinMode/digitalWrite")
    print("  from inside the timer ISR. A regression shows up as a HANG or")
    print("  REBOOT, not as a clean error state -- that is what to watch for.")

    r = link.send({"type": "enter_insp_mode"}, timeout=3.0)
    rep.add("0.7", "enter_insp_mode", bool(r), r)

    print("\n  Block the gate by hand to create a part that never gets a result.")
    input("  [Enter once you have done that] ")

    time.sleep(1.0)
    st = link.send({"type": "get_running_stat"}, timeout=3.0)
    rep.add("0.7a", "board still responding after the error",
            bool(st), "no reply = hang/reboot, i.e. a regression" if not st else st)

    a = ask("did the board enter an error state cleanly (no reboot / no hang)?")
    rep.add("0.7a", "clean INSPECTION_MODE_ERROR", a)

    a = ask("did the selectors drop immediately, without waiting for the plate"
            " to finish slowing down?")
    rep.add("0.7b", "outputs dropped on error entry", a)

    r = link.send({"type": "clear_error"}, timeout=3.0)
    rep.add("0.7c", "clear_error returns to IDLE", bool(r), r)


# --- stage 2: trigger stream ----------------------------------------------

def monitor(link, rep, seconds):
    """Watch bTrigInfo and check the assumption the whole tid pairing rests on:
    that tid arrives strictly increasing, one per part, with no gaps."""
    print(f"\n\033[1m== Trigger monitor ({seconds}s) ==\033[0m")
    print("  Run parts through. Watching bTrigInfo for tid continuity.\n")

    link.drain_async()
    t_end = time.time() + seconds
    tids, gaps, others = [], [], {}
    qs_max = -1
    last_print = 0.0

    while time.time() < t_end:
        time.sleep(0.1)
        for ts, msg in link.drain_async():
            mtype = msg.get("type", "?")
            if mtype == "bTrigInfo":
                tid = msg.get("tid")
                qs = msg.get("Qs", -1)
                qs_max = max(qs_max, qs if isinstance(qs, int) else -1)
                if tids and tid != tids[-1] + 1:
                    gaps.append((tids[-1], tid))
                    print(f"  \033[31mtid gap: {tids[-1]} -> {tid}\033[0m")
                tids.append(tid)
            else:
                others[mtype] = others.get(mtype, 0) + 1
                if mtype == "systemInfo":
                    print(f"  \033[33msystemInfo: {json.dumps(msg)[:160]}\033[0m")

        if time.time() - last_print > 2.0:
            last_print = time.time()
            print(f"  triggers:{len(tids)} gaps:{len(gaps)} Qs_max:{qs_max} "
                  f"other:{others}", end="\r")

    print(" " * 100, end="\r")
    n = len(tids)
    rep.add("2.3", "bTrigInfo observed", n > 0, f"{n} triggers in {seconds}s")
    if n:
        rep.add("2.3", "tid strictly increasing by 1", len(gaps) == 0,
                f"{len(gaps)} gap(s): {gaps[:10]}" if gaps else
                f"tid {tids[0]} .. {tids[-1]}, continuous")
        rep.add("2.4", "firmware queue depth (Qs) stayed bounded", qs_max < 50,
                f"max Qs={qs_max} (PIPE_INFO_LEN is 100; near it means the "
                f"host is not answering fast enough)")
    if others:
        rep.add("--", "other async messages seen", None, json.dumps(others))
    return tids


# --- bench: whole tid round trip on a bare board --------------------------

# State codes from FirmwareTypes.hpp (SMM_STATE_DECLARE).
ST_INIT, ST_IDLE, ST_READY, ST_ERROR, ST_FATAL, ST_TEST = 0, 100, 101, 112, 113, 140
ST_NAME = {ST_INIT: "INIT", ST_IDLE: "IDLE", ST_READY: "INSPECTION_MODE_READY",
           ST_ERROR: "INSPECTION_MODE_ERROR", ST_FATAL: "INSPECTION_MODE_FATAL",
           ST_TEST: "INSPECTION_MODE_TEST"}

CAT_NA = 0xFFFF


def _state(link):
    r = link.send({"type": "get_running_stat"}, timeout=3.0)
    return (r or {}).get("state"), r


def _counts(link):
    r = link.send({"type": "get_running_stat"}, timeout=3.0)
    return (r or {}).get("count", {}), r


# Steps to push the selector past the camera trigger for the duration of a
# bench. The real geometry leaves only SWITCH-L1A_on ~= 43 steps between the
# bTrigInfo announcement and the selector -- a window the in-firmware C++ core
# answers inside but a host scripting over serial cannot. This buys a
# comfortable margin; the original offsets are restored when the bench ends.
BENCH_WINDOW = 600


def _widen_selector_window(link, orig_spo):
    """Push SWITCH and the SEL outputs BENCH_WINDOW steps past the camera
    trigger (where bTrigInfo is announced) so a host scripting over serial can
    answer each tid before the part reaches the selector. Returns the SWITCH
    offset it set on success, else None. Caller restores orig_spo when done."""
    win = int((orig_spo or {}).get("L1A_on", 654)) + BENCH_WINDOW
    link.send({"type": "set_setup", "stage_pulse_offset": {
        "SWITCH": win,
        "SEL1_on": win + 3,  "SEL1_off": win + 4,
        "SEL2_on": win + 13, "SEL2_off": win + 14,
        "SEL3_on": win + 23, "SEL3_off": win + 24}}, timeout=3.0)
    back = ((link.send({"type": "get_setup"}, timeout=3.0) or {})
            .get("stage_pulse_offset") or {})
    return win if back.get("SWITCH") == win else None


def _wait_at_speed(link, settle=0.15, tries=30):
    """Block until the plate is turning at a steady rate.

    newPulseEvent drops any pulse within 3.5 mm of the previous one, so
    phantoms fired while SYS_CUR_FREQ is still ramping up from zero are silently
    rejected and never become objects. Poll SYS_STEP_COUNT until the
    per-interval delta stops climbing (the ramp has plateaued)."""
    def ssc():
        return (link.send({"type": "get_setup"}, timeout=3.0) or {}).get(
            "SYS_STEP_COUNT")
    prev = ssc()
    last = None
    for _ in range(tries):
        time.sleep(settle)
        cur = ssc()
        if not (isinstance(prev, int) and isinstance(cur, int)):
            return False, "no SYS_STEP_COUNT in get_setup"
        d = cur - prev
        prev = cur
        if d > 0 and last is not None and d <= last * 1.05:
            return True, f"steady ~{int(d / settle)} steps/s"
        last = d
    return (last or 0) > 0, f"~{int((last or 0) / settle)} steps/s (no plateau)"


def bench(link, rep, count, freq, interval_ms, cat):
    """Exercise the whole firmware-side tid handshake with no rig attached.

    trig_phamton_pulse calls newPulseEvent() directly, bypassing the gate
    sensor entirely, so a bare board on USB can produce real objects with real
    tids and run them all the way to the selector outputs. The gate pin is
    INPUT_PULLUP with sense inverted, so an unconnected input reads as
    "no object" and contributes nothing.
    """
    print("\n\033[1m== Bench: tid round trip, no rig required ==\033[0m")
    print(f"  {count} phantom parts at plateFreq={freq}, {interval_ms}ms apart,"
          f" reported as cat={cat}\n")

    orig = link.send({"type": "get_setup"}, timeout=3.0) or {}
    orig_freq = orig.get("plateFreq")
    orig_spo = dict(orig.get("stage_pulse_offset") or {})
    base_counts, _ = _counts(link)
    # ERROR_HIST is a cumulative ring; clear it so the B.7/B.9/B.12 readouts
    # reflect only what this run produced, not stale faults from earlier runs.
    link.send({"type": "clear_error"}, timeout=2.0)
    link.send({"type": "clear_error_history"}, timeout=2.0)

    # Widen the selector window with firmware params so the round trip measures
    # "does the pipeline route tids correctly", not "can the host answer inside
    # 17 ms". Restored in the teardown below.
    win = _widen_selector_window(link, orig_spo)
    rep.add("B.0", "widen the selector window for a bare-board round trip",
            win is not None,
            f"SWITCH {orig_spo.get('SWITCH')} -> {win}"
            f" ({BENCH_WINDOW} steps past the camera trigger)")

    r = link.send({"type": "set_setup", "plateFreq": freq}, timeout=3.0)
    rep.add("B.1", f"set plateFreq={freq}", bool(r), r)

    r = link.send({"type": "enter_insp_mode"}, timeout=3.0)
    rep.add("B.2", "enter_insp_mode", bool(r), r)

    # Fire only once the plate is at speed -- see _wait_at_speed: phantoms fired
    # mid-ramp fall inside the 3.5 mm de-dup gate and never become objects.
    running, detail = _wait_at_speed(link)
    rep.add("B.3", "timer ISR ticking at speed", running, detail)
    if not running:
        print("  \033[31m  Run_ACTS only runs from the timer ISR. Without it a"
              " phantom pulse is accepted and then never acted on.\033[0m")

    st, _ = _state(link)
    rep.add("B.4", "state is INSPECTION_MODE_READY", st == ST_READY,
            f"state={st} ({ST_NAME.get(st, '?')})")

    # --- fire phantoms, answer each object once, the instant it announces ---
    # Every object announces bTrigInfo twice (CAM1 tidx=1, CAM2 tidx=2) at the
    # same offset, so dedup by tid and report exactly once: a second report for
    # a tid whose object has already passed would itself desync the machine.
    # Answering on-announce (not after a fixed sleep) is what keeps the verdict
    # ahead of the part even before the window is widened.
    link.drain_async()
    fired, seen, reported = 0, [], []
    answered = set()

    def _pump():
        for _, msg in link.drain_async():
            if msg.get("type") != "bTrigInfo":
                continue
            tid = msg.get("tid")
            seen.append(tid)
            if tid not in answered:
                link.send_nowait({"type": "report", "tid": tid, "cat": cat})
                answered.add(tid)
                reported.append(tid)

    for i in range(count):
        link.send({"type": "trig_phamton_pulse"}, timeout=3.0)
        fired += 1
        deadline = time.time() + interval_ms / 1000.0
        while time.time() < deadline:      # answer ASAP, keep phantoms spaced
            _pump()
            time.sleep(0.002)

    drain_until = time.time() + 0.5
    while time.time() < drain_until:       # let the last parts announce/clear
        _pump()
        time.sleep(0.005)

    objs = sorted(set(seen))
    per = {t: seen.count(t) for t in objs}
    twice = bool(objs) and all(v == 2 for v in per.values())
    rep.add("B.5", "one object per pulse, announced twice (CAM1+CAM2)",
            len(objs) == fired and twice,
            f"fired={fired} objects={len(objs)} announcements={len(seen)}"
            + ("  (newPulseEvent rejects pulses closer than "
               "SYS_MIN_PULSE_TIME_SEP_us or 3.5mm of travel -- raise "
               "--interval-ms)" if len(objs) < fired
               else ("" if twice else f"  (not 2 per object: {per})")))

    if objs:
        gaps = [(a, b) for a, b in zip(objs, objs[1:]) if b != a + 1]
        rep.add("B.6", "tid strictly increasing by 1", not gaps,
                f"tid {objs[0]}..{objs[-1]}" + (f" gaps:{gaps}" if gaps else ""))

    st, stat = _state(link)
    rep.add("B.7", "no error state after a full reported run", st == ST_READY,
            f"state={st} ({ST_NAME.get(st, '?')}) "
            f"ERROR_HIST={_errors_of(stat)}")

    now_counts, _ = _counts(link)
    key = {1: "SEL1", 2: "SEL2", 3: "SEL3", CAT_NA: "NA"}.get(cat)
    if key:
        before = base_counts.get(key, 0)
        after = now_counts.get(key, 0)
        rep.add("B.8", f"{key} counter advanced by the reported parts",
                after - before == len(reported),
                f"{key}: {before} -> {after} (reported {len(reported)})")

    # --- negative: a tid the firmware never issued ------------------------
    print("\n  Negative check: reporting a tid that does not exist should fault")
    print("  the machine. That fault is the safety net the whole design leans")
    print("  on -- if it does NOT fire, a desync would sort parts silently.")
    bogus = (max(seen) + 100000) if seen else 999999
    link.send_nowait({"type": "report", "tid": bogus, "cat": cat})
    time.sleep(0.5)
    st, stat = _state(link)
    rep.add("B.9", "unknown tid faults the machine", st == ST_ERROR,
            f"state={st} ({ST_NAME.get(st, '?')}) "
            f"ERROR_HIST={(stat or {}).get('ERROR_HIST')} "
            f"(expect INSP_RESULT_MATCHES_NO_OBJECT=1)")

    r = link.send({"type": "clear_error"}, timeout=3.0)
    st, _ = _state(link)
    rep.add("B.10", "clear_error recovers", st in (ST_IDLE, ST_READY),
            f"state={st} ({ST_NAME.get(st, '?')})")

    # --- negative: a part that never gets a verdict -----------------------
    # This is stage 0.7 without needing anyone to block a gate by hand, and it
    # is the path that used to call pinMode/digitalWrite from inside the ISR:
    # a regression shows up as a hang or reboot rather than a clean fault.
    print("\n  Negative check: a part with no verdict at all (ISR error path,")
    print("  commit 535d92fb). A hang or reboot here is the regression.")
    link.send({"type": "enter_insp_mode"}, timeout=3.0)
    time.sleep(0.3)
    link.drain_async()
    link.send({"type": "trig_phamton_pulse"}, timeout=3.0)
    time.sleep(2.0)

    st, stat = _state(link)
    rep.add("B.11", "board still answers after the ISR error path",
            st is not None, "no reply = hang/reboot = regression")
    rep.add("B.12", "unjudged part faults cleanly", st == ST_ERROR,
            f"state={st} ({ST_NAME.get(st, '?')}) "
            f"ERROR_HIST={(stat or {}).get('ERROR_HIST')} "
            f"(expect OBJECT_HAS_NO_INSP_RESULT=2)")

    # --- restore ----------------------------------------------------------
    link.send({"type": "clear_error"}, timeout=3.0)
    link.send({"type": "exit_insp_mode"}, timeout=3.0)
    if orig_spo:
        link.send({"type": "set_setup", "stage_pulse_offset": orig_spo},
                  timeout=3.0)
    if isinstance(orig_freq, (int, float)):
        link.send({"type": "set_setup", "plateFreq": orig_freq}, timeout=3.0)
    chk = ((link.send({"type": "get_setup"}, timeout=3.0) or {})
           .get("stage_pulse_offset") or {})
    st, _ = _state(link)
    restored = (orig_spo.get("SWITCH") is None
                or chk.get("SWITCH") == orig_spo.get("SWITCH"))
    rep.add("B.13", "returned to IDLE, window + plateFreq restored",
            st == ST_IDLE and restored,
            f"state={st} SWITCH={chk.get('SWITCH')} plateFreq={orig_freq}")


# --- stress: find the pipeline ceiling ------------------------------------

# GEN_ERROR_CODE (FirmwareTypes.hpp)
ERR_NAME = {
    1: "INSP_RESULT_MATCHES_NO_OBJECT (tid desync)",
    2: "OBJECT_HAS_NO_INSP_RESULT (no verdict before the selector)",
    3: "INSP_RESULT_COUNTER_ERROR",
    4: "INSP_RESULT_PULSE_TIME_OUT_OF_SYNC",
    5: "INSP_RESULT_HAS_NO_TIME_STAMP",
    10: "INSP_CAM_TRIG_INFO_CANNOT_BE_SENT (comm queue overflow)",
    11: "SERIAL_PROTOCOL_ERROR",
    0xff: "SEL_ACT_LIMIT_REACHES",
}

# Pipeline limits worth knowing before reading the numbers:
#   RBuf / every ACT_SCH queue      PIPE_INFO_LEN = 100 objects
#   TaskQ2CommInfoQ                 20 entries -- overflow FAULTS the machine
#                                   (INSP_CAM_TRIG_INFO_CANNOT_BE_SENT)
#   Every object announces bTrigInfo for CAM1 *and* CAM2 -> 2 messages/part
#   115200 8N1 ~= 11.5 kB/s; a bTrigInfo frame is ~90 B
PIPE_INFO_LEN = 100
COMM_Q_DEPTH = 20


def _errors_of(stat):
    return [e for e in (stat or {}).get("ERROR_HIST", []) if e not in (0, -1)]


def stress(link, rep, start_hz, max_hz, step_hz, dwell, cat, do_report):
    """Ramp the phantom-object rate until the pipeline gives, and say what gave.

    The firmware rate-limits new objects two ways, both of which have to be
    opened up first or the ramp measures the limiter instead of the pipeline:
    a minimum time separation (SYS_MIN_PULSE_TIME_SEP_us, default ~67ms = 15/s)
    and a minimum travel distance (3.5mm, ~91 pulses) whose wall-clock cost
    depends on plateFreq.
    """
    print("\n\033[1m== Pipeline stress ==\033[0m")

    orig = link.send({"type": "get_setup"}, timeout=3.0) or {}
    orig_freq = orig.get("plateFreq")
    orig_sep = orig.get("minDetectTimeSep_us")
    orig_spo = dict(orig.get("stage_pulse_offset") or {})

    # Widen the selector window (see _widen_selector_window). Without this the
    # ramp measures how fast the host can answer inside the ~43-step gate, not
    # the pipeline's real ceiling; the first late verdict faults the machine and
    # the ramp stops at rate 1. Restored in the teardown below.
    _widen_selector_window(link, orig_spo)

    # plateFreq needed so the 3.5mm distance gate clears fast enough for the
    # top rate we intend to ask for. ISR ticks at 2*plateFreq.
    need_freq = int(91 * max_hz / 2.0 * 1.5) + 500
    sep_us = max(200, int(1e6 / max_hz / 3))

    print(f"  opening the rate limiters: plateFreq={need_freq} "
          f"minDetectTimeSep_us={sep_us}")
    print(f"  (defaults {orig_freq} / {orig_sep} cap objects at "
          f"{1e6/orig_sep:.1f}/s)" if orig_sep else "")

    link.send({"type": "set_setup", "plateFreq": need_freq,
               "minDetectTimeSep_us": sep_us}, timeout=3.0)
    link.send({"type": "enter_insp_mode"}, timeout=3.0)
    _wait_at_speed(link)

    st, _ = _state(link)
    if st != ST_READY:
        rep.add("S.0", "reached READY before ramping", False,
                f"state={st} ({ST_NAME.get(st, '?')})")
        return
    rep.add("S.0", "reached READY before ramping", True, f"plateFreq={need_freq}")

    print(f"\n  {'rate':>6} {'fired':>7} {'seen':>7} {'ratio':>7} "
          f"{'maxQs':>6}  result")
    print("  " + "-" * 62)

    results = []
    best = 0
    broke_at = None
    broke_why = ""

    hz = start_hz
    while hz <= max_hz:
        link.drain_async()
        link.send({"type": "clear_error"}, timeout=2.0)
        # ERROR_HIST is a cumulative ring; without clearing it this rate would
        # inherit every fault from earlier rates (and earlier sessions) and be
        # judged as broken on stale history. Clear so errs reflects THIS rate.
        link.send({"type": "clear_error_history"}, timeout=2.0)
        link.send({"type": "enter_insp_mode"}, timeout=2.0)
        # clear_error drops the plate back to a standstill, so wait for it to
        # spin back up to speed. Firing during the ramp lands pulses inside the
        # 3.5mm de-dup gate -- they are dropped, and a late tail verdict for a
        # part that already faulted then reads as a tid desync.
        _wait_at_speed(link)
        link.drain_async()

        period = 1.0 / hz
        n = max(1, int(dwell * hz))
        fired = 0
        seen = set()          # unique object tids (each announces twice)
        reported = set()      # tids answered once -- a 2nd report desyncs
        qs_max = 0
        t0 = time.time()

        for i in range(n):
            deadline = t0 + i * period
            # Drain continuously while pacing to the deadline: a bTrigInfo left
            # sitting until the next fire would be answered up to a full period
            # late and miss the selector even with the window widened.
            while time.time() < deadline:
                for _, msg in link.drain_async():
                    if msg.get("type") == "bTrigInfo":
                        tid = msg.get("tid")
                        seen.add(tid)
                        qs_max = max(qs_max, msg.get("Qs", 0) or 0)
                        if do_report and tid not in reported:
                            reported.add(tid)
                            link.send_nowait({"type": "report", "tid": tid,
                                              "cat": cat})
                time.sleep(0.002)
            # No round trip: waiting on the ack would itself become the limit.
            link.send_nowait({"type": "trig_phamton_pulse"})
            fired += 1

        # Let the tail drain -- finely (2ms), the same cadence as the fire
        # loop. A 50ms poll here can leave the last part's verdict later than
        # its ~window-sized runway to the selector and fault an otherwise clean
        # run on the final object alone.
        t_drain = time.time() + 1.5
        while time.time() < t_drain:
            for _, msg in link.drain_async():
                if msg.get("type") == "bTrigInfo":
                    tid = msg.get("tid")
                    seen.add(tid)
                    qs_max = max(qs_max, msg.get("Qs", 0) or 0)
                    if do_report and tid not in reported:
                        reported.add(tid)
                        link.send_nowait({"type": "report", "tid": tid, "cat": cat})
            time.sleep(0.002)

        st, stat = _state(link)
        errs = _errors_of(stat)
        ratio = len(seen) / fired if fired else 0.0

        if st == ST_ERROR or errs:
            why = ", ".join(ERR_NAME.get(e, f"code {e}") for e in sorted(set(errs)))
            verdict = f"\033[31mFAULT\033[0m {why}"
            broke_at, broke_why = hz, why
        elif ratio < 0.98:
            verdict = f"\033[33mdropped {fired - len(seen)} at the rate gate\033[0m"
        else:
            verdict = "\033[32mok\033[0m"
            best = hz

        print(f"  {hz:>5}/s {fired:>7} {len(seen):>7} {ratio:>6.0%} "
              f"{qs_max:>6}  {verdict}")
        results.append((hz, fired, len(seen), ratio, qs_max, errs))

        if broke_at:
            break
        hz += step_hz

    # --- verdicts ---------------------------------------------------------
    rep.add("S.1", "highest clean sustained object rate", best > 0,
            f"{best}/s with reporting {'on' if do_report else 'OFF'} "
            f"(ramped {start_hz}..{max_hz} step {step_hz}, {dwell}s dwell)")

    if broke_at:
        rep.add("S.2", f"first failure at {broke_at}/s", None, broke_why)
        if "comm queue overflow" in broke_why:
            print(f"\n  \033[33mTaskQ2CommInfoQ is {COMM_Q_DEPTH} deep and every"
                  f" object announces bTrigInfo twice (CAM1 and CAM2).")
            print(f"  At 115200 a ~90B frame costs ~8ms, so the serial link"
                  f" alone caps this well before the pipeline does.\033[0m")
    else:
        rep.add("S.2", "no failure up to the ceiling tested", None,
                f"survived {max_hz}/s -- raise --max-hz to find the real limit")

    qs_peak = max((r[4] for r in results), default=0)
    rep.add("S.3", "firmware queue depth stayed clear of PIPE_INFO_LEN",
            qs_peak < PIPE_INFO_LEN * 0.8,
            f"peak Qs={qs_peak} of {PIPE_INFO_LEN}")

    # --- restore ----------------------------------------------------------
    link.send({"type": "clear_error"}, timeout=3.0)
    link.send({"type": "exit_insp_mode"}, timeout=3.0)
    restore = {"type": "set_setup"}
    if isinstance(orig_freq, (int, float)):
        restore["plateFreq"] = orig_freq
    if isinstance(orig_sep, (int, float)):
        restore["minDetectTimeSep_us"] = orig_sep
    if orig_spo:
        restore["stage_pulse_offset"] = orig_spo
    link.send(restore, timeout=3.0)
    print(f"\n  restored plateFreq={orig_freq} minDetectTimeSep_us={orig_sep}")


def stall(link, rep, hz, stall_s, cat):
    """Stop answering mid-run and confirm the machine degrades the way it
    should: it must fault (OBJECT_HAS_NO_INSP_RESULT), not sort parts by stale
    or guessed verdicts."""
    print("\n\033[1m== Host stall ==\033[0m")
    print(f"  Reporting normally, then going silent for {stall_s}s.")
    print("  Expect OBJECT_HAS_NO_INSP_RESULT(2) -- a part reaching the")
    print("  selector with no verdict must stop the line, not guess.\n")

    orig = link.send({"type": "get_setup"}, timeout=3.0) or {}
    orig_spo = dict(orig.get("stage_pulse_offset") or {})
    # Widen the selector window so the host can answer inside it while healthy.
    # The silent phase still faults -- those parts get no verdict at all, window
    # or no window -- which is exactly what T.2 is checking for.
    _widen_selector_window(link, orig_spo)
    link.send({"type": "set_setup", "plateFreq": 1000}, timeout=3.0)
    link.send({"type": "clear_error"}, timeout=2.0)
    link.send({"type": "clear_error_history"}, timeout=2.0)   # judge this run only
    link.send({"type": "enter_insp_mode"}, timeout=3.0)
    _wait_at_speed(link)
    link.drain_async()

    period = 1.0 / hz
    answered = 0
    reported = set()          # each object announces twice -- answer it once

    def _answer():
        nonlocal answered
        for _, msg in link.drain_async():
            if msg.get("type") == "bTrigInfo":
                tid = msg["tid"]
                if tid not in reported:
                    reported.add(tid)
                    link.send_nowait({"type": "report", "tid": tid, "cat": cat})
                    answered += 1

    t0 = time.time()
    for i in range(int(3 * hz)):
        deadline = t0 + i * period
        while time.time() < deadline:   # answer on-announce while pacing
            _answer()
            time.sleep(0.002)
        link.send_nowait({"type": "trig_phamton_pulse"})
    settle = time.time() + 0.4          # answer the last parts before checking
    while time.time() < settle:
        _answer()
        time.sleep(0.002)

    st, _ = _state(link)
    rep.add("T.1", "healthy while answering", st == ST_READY,
            f"answered {answered} parts, state={ST_NAME.get(st, st)}")

    print(f"  going silent for {stall_s}s ...")
    t_end = time.time() + stall_s
    while time.time() < t_end:
        link.send_nowait({"type": "trig_phamton_pulse"})
        time.sleep(period)
        link.drain_async()          # deliberately discard, do not report

    st, stat = _state(link)
    errs = _errors_of(stat)
    rep.add("T.2", "unanswered parts fault the line", st == ST_ERROR,
            f"state={ST_NAME.get(st, st)} errors="
            f"{[ERR_NAME.get(e, e) for e in sorted(set(errs))]}")
    rep.add("T.3", "board still responsive after the stall", stat is not None,
            "no reply = hang/reboot")

    link.send({"type": "clear_error"}, timeout=3.0)
    link.send({"type": "exit_insp_mode"}, timeout=3.0)
    if orig_spo:
        link.send({"type": "set_setup", "stage_pulse_offset": orig_spo},
                  timeout=3.0)


# --- probe: the protocol + camera-trigger surface -------------------------

def probe(link, rep):
    """The command handlers bench/edge/stress never touch, kept to the ones
    that are both safe to fire on a bare board and observable in the reply.

    Deliberately excluded, because they actuate an output with nothing to read
    back here and the checklist gates them behind a human at the machine:
    PIN_ON/PIN_OFF/PIN_MODE (raw GPIO), sel_act (fires a valve -- stage 3,
    "the one that cannot self-correct"), stepper_enable/disable (moves the
    plate), and save_setup (burns a flash cycle; NVS survival is stage 0.5,
    which needs a real power cycle anyway).
    """
    print("\n\033[1m== Probe: protocol + camera-trigger surface ==\033[0m")

    # Keep everything in IDLE: none of these need inspection mode, and IDLE has
    # no INSPECTION_ERROR transition so a stray report here cannot fault.
    link.send({"type": "clear_error"}, timeout=2.0)
    st, _ = _state(link)

    # --- P.1: version handshake -------------------------------------------
    # ask_JsonRaw_version stores the peer version and answers with the
    # firmware's own -- the core leans on this reply to know it is talking to
    # uInsp firmware and not, say, the CNC image that was on this very board.
    # The reply carries a HARDCODED id (100446), not the command's, so it
    # arrives as an async message rather than a matched reply -- drain for it.
    link.drain_async()
    link.send_nowait({"type": "ask_JsonRaw_version", "version": "probe-tool"})
    ver, rtype = None, None
    t_end = time.time() + 2.0
    while time.time() < t_end and ver is None:
        for _, m in link.drain_async():
            if m.get("type") == "rsp_JsonRaw_version":
                rtype, ver = m.get("type"), m.get("version")
        time.sleep(0.02)
    rep.add("P.1", "version handshake answers with a firmware version",
            bool(rtype == "rsp_JsonRaw_version" and ver),
            f"rsp={rtype} version={ver!r}")

    # --- P.2: reset_running_stat ------------------------------------------
    # Zeroes the SEL/NA tallies. Untested until now, and it is the only way to
    # make a run assert on absolute counts instead of deltas.
    link.send({"type": "reset_running_stat"}, timeout=3.0)
    counts, _ = _counts(link)
    zeroed = all(counts.get(k, -1) == 0 for k in ("SEL1", "SEL2", "SEL3", "NA"))
    rep.add("P.2", "reset_running_stat zeroes every counter", zeroed,
            f"counts={counts}")

    # --- P.3: trigCamPulse -------------------------------------------------
    # The camera-trigger simulation, distinct from trig_phamton_pulse: it
    # announces ONE bTrigInfo carrying the caller's trigger_id and pulses the
    # CAM/light pins, but does NOT call newPulseEvent, so no pipeline object is
    # created (Qs stays where it was). This is the announce path stage 1 leans
    # on -- verifiable before any camera is attached. A phantom pulse, by
    # contrast, announces twice (CAM1+CAM2) and does enqueue an object.
    link.drain_async()
    marker = 424242
    r = link.send({"type": "trigCamPulse", "trigger_id": marker},
                  timeout=3.0)
    time.sleep(0.5)
    anns = [m for _, m in link.drain_async() if m.get("type") == "bTrigInfo"]
    mine = [m for m in anns if m.get("tid") == marker]
    qs = mine[0].get("Qs") if mine else None
    rep.add("P.3", "trigCamPulse announces once with the caller's trigger_id,"
            " enqueues no object",
            bool(r) and len(mine) == 1 and mine[0].get("tidx") == 1
            and qs == 0,
            f"announcements for {marker}: {len(mine)} "
            f"(tidx={mine[0].get('tidx') if mine else None}, Qs={qs}); "
            f"a phantom pulse would announce twice and set Qs>0")

    st, _ = _state(link)
    rep.add("P.4", "still IDLE and responsive after the probes",
            st == ST_IDLE, f"state={st} ({ST_NAME.get(st, '?')})")


# --- edge: firmware paths bench/stress/stall never touch -------------------

# RBuf-saturation run. Fire E6_COUNT phantoms spaced E6_SPACING steps apart
# into a runway E6_WINDOW steps long. The spacing clears the ~91-step distance
# gate with margin so every pulse is accepted, and the runway is long enough
# that all E6_COUNT are still in flight when the last is fired
# (E6_WINDOW > E6_COUNT*E6_SPACING), forcing RBuf past its 100-object cap. The
# firmware must then reject the excess silently -- no announce, no fault -- so
# objects settle at the cap. Paced off the SET plateFreq (ISR = 2*freq), not a
# measured rate: measuring live proved noisy (serial contention on the ISR's
# core) and mis-spaced the pulses.
E6_FREQ = 1000
E6_SPACING = 150
E6_WINDOW = 26000
E6_COUNT = 115


def edge(link, rep, only=None):
    """Deep firmware paths the bench never exercises.

    E.1 the NA verdict (cat=0xFFFF counts, actuates nothing) -- what stage 2
        leans on while sorting is off; E.2 SKIP absorption (a newer report
        marks older unanswered objects SKIP instead of faulting); E.3 the
        pulse de-dup gates reject without consuming a tid; E.4 the SEL1
        countdown fires exactly N then goes quiet; E.5 the serial
        protocol-error latch (garbage -> fault + silence -> one RESET recovers
        and redeems); E.6 RBuf saturation rejects gracefully.
    """
    def _want(ref):
        return only is None or ref in only

    print("\n\033[1m== Edge: deep firmware paths, no rig required ==\033[0m")

    orig = link.send({"type": "get_setup"}, timeout=3.0) or {}
    orig_freq = orig.get("plateFreq")
    orig_sep = orig.get("minDetectTimeSep_us")
    orig_spo = dict(orig.get("stage_pulse_offset") or {})
    link.send({"type": "clear_error"}, timeout=2.0)
    link.send({"type": "clear_error_history"}, timeout=2.0)

    def _pump(answer_cat, seen, answered, qs=None):
        for _, msg in link.drain_async():
            if msg.get("type") != "bTrigInfo":
                continue
            tid = msg.get("tid")
            seen.append(tid)
            if qs is not None:
                qs[0] = max(qs[0], msg.get("Qs", 0) or 0)
            if answer_cat is not None and tid not in answered:
                answered.add(tid)
                link.send_nowait({"type": "report", "tid": tid,
                                  "cat": answer_cat})

    def _run_parts(n, interval_s, answer_cat):
        """Fire n phantoms, answering (or deliberately not) on-announce."""
        seen, answered = [], set()
        link.drain_async()
        for i in range(n):
            link.send({"type": "trig_phamton_pulse"}, timeout=3.0)
            deadline = time.time() + interval_s
            while time.time() < deadline:
                _pump(answer_cat, seen, answered)
                time.sleep(0.002)
        return seen, answered

    def _settle(seconds, answer_cat, seen, answered):
        t_end = time.time() + seconds
        while time.time() < t_end:
            _pump(answer_cat, seen, answered)
            time.sleep(0.005)

    try:
        win = _widen_selector_window(link, orig_spo)
        rep.add("E.0", "widen the selector window", win is not None,
                f"SWITCH {orig_spo.get('SWITCH')} -> {win}")

        # Slow plate: at ~600 the 3.5mm distance gate (~91 steps) costs ~76ms,
        # just past the ~67ms time gate, and E.2's oldest part gets a ~0.56s
        # runway to the selector -- room to observe three announcements and
        # answer only the newest before anything arrives unjudged.
        link.send({"type": "set_setup", "plateFreq": 600}, timeout=3.0)
        link.send({"type": "enter_insp_mode"}, timeout=3.0)
        _wait_at_speed(link)
        st, _ = _state(link)
        if st != ST_READY:
            rep.add("E.0", "reached READY", False,
                    f"state={st} ({ST_NAME.get(st, '?')}) -- aborting edge")
            return

        # --- E.1: NA is a verdict, not an error ---------------------------
        # Stage 2 runs the whole line with sorting off: the core reports every
        # part NA and expects it to recirculate. bench always reports cat=1,
        # so this path -- count NA, actuate nothing, no fault -- was untested.
        if _want("E.1"):
            base, _ = _counts(link)
            seen, answered = _run_parts(3, 0.12, CAT_NA)
            _settle(2.0, CAT_NA, seen, answered)
            st, _ = _state(link)
            now, _ = _counts(link)
            dna = now.get("NA", 0) - base.get("NA", 0)
            dsel = sum(now.get(k, 0) - base.get(k, 0)
                       for k in ("SEL1", "SEL2", "SEL3"))
            rep.add("E.1", "cat=0xFFFF is a verdict: NA counts, no selector,"
                    " no fault",
                    st == ST_READY and dna == 3 and dsel == 0,
                    f"NA +{dna} of 3, SEL +{dsel}, "
                    f"state={ST_NAME.get(st, st)}")

        # --- E.2: SKIP absorption -----------------------------------------
        # The report handler marks older UNSET objects SKIP when a newer tid
        # is answered. This is the FIFO's desync absorber: skipped parts pass
        # the selector silently instead of faulting the line.
        if _want("E.2"):
            base, _ = _counts(link)
            seen, _ = _run_parts(3, 0.12, None)     # answer nothing yet
            t_lim = time.time() + 0.25
            while time.time() < t_lim and len(set(seen)) < 3:
                _pump(None, seen, set())
                time.sleep(0.002)
            tids = sorted(set(seen))
            if len(tids) == 3:
                link.send_nowait({"type": "report", "tid": tids[-1],
                                  "cat": CAT_NA})
            _settle(2.0, None, seen, set())
            st, _ = _state(link)
            now, _ = _counts(link)
            dna = now.get("NA", 0) - base.get("NA", 0)
            rep.add("E.2", "older unanswered parts SKIP on a newer report,"
                    " no fault",
                    len(tids) == 3 and st == ST_READY and dna == 1,
                    f"answered newest of {tids}, NA +{dna} (skipped parts "
                    f"count nowhere), state={ST_NAME.get(st, st)}")

        # --- E.3: the de-dup gates reject without consuming a tid ---------
        # A pulse inside SYS_MIN_PULSE_TIME_SEP_us / 3.5mm is dropped by
        # newPulseEvent before a tid is issued. If a rejection ever consumed a
        # tid, every bounce at the gate would desync the pairing from then on.
        if _want("E.3"):
            link.drain_async()
            seen, answered = [], set()
            link.send({"type": "trig_phamton_pulse"}, timeout=3.0)
            link.send_nowait({"type": "trig_phamton_pulse"})   # inside both gates
            time.sleep(0.2)
            link.send({"type": "trig_phamton_pulse"}, timeout=3.0)
            _settle(1.5, CAT_NA, seen, answered)
            tids = sorted(set(seen))
            st, _ = _state(link)
            rep.add("E.3", "a gated-out pulse consumes no tid",
                    len(tids) == 2 and tids[1] == tids[0] + 1
                    and st == ST_READY,
                    f"fired 3 (one back-to-back) -> objects {tids}, "
                    f"state={ST_NAME.get(st, st)}")

        # --- E.4: SEL1 countdown ------------------------------------------
        # set_sel1_cd N: SEL1 actuates and counts exactly N more times, then
        # goes quiet -- silently (the SEL_ACT_LIMIT_REACHES fault is compiled
        # out), which is worth pinning down because a batch run that hits the
        # limit looks exactly like a dead valve.
        if _want("E.4"):
            base, _ = _counts(link)
            link.send({"type": "set_sel1_cd", "count": 2}, timeout=3.0)
            cd0 = (link.send({"type": "get_sel1_cd"}, timeout=3.0)
                   or {}).get("sel1_cd")
            seen, answered = _run_parts(4, 0.12, 1)
            _settle(2.5, 1, seen, answered)
            cd1 = (link.send({"type": "get_sel1_cd"}, timeout=3.0)
                   or {}).get("sel1_cd")
            now, _ = _counts(link)
            d1 = now.get("SEL1", 0) - base.get("SEL1", 0)
            st, _ = _state(link)
            rep.add("E.4", "SEL1 countdown fires exactly N then goes quiet,"
                    " no fault",
                    cd0 == 2 and cd1 == 0 and d1 == 2
                    and len(answered) == 4 and st == ST_READY,
                    f"sel1_cd {cd0}->{cd1}, SEL1 +{d1} of 4 reported cat=1, "
                    f"state={ST_NAME.get(st, st)}")
            link.send({"type": "set_sel1_cd", "count": -1}, timeout=3.0)

        # --- E.5: the serial protocol-error latch -------------------------
        # Garbage latches the link: the machine faults (SERIAL_PROTOCOL_ERROR
        # =11) and the data layer eats every byte while scanning for a RESET.
        # A single RESET recovers everything -- the framing recovery routes
        # through handleResetCommand, which clears the command lock AND
        # auto-redeems the fault (hardware-verified: state 112 -> 101 on the
        # first RESET). The "send RESET twice per connection" convention
        # (checklist 5.5) is belt and braces, not a requirement.
        # Note IDLE has no INSPECTION_ERROR transition in the state table --
        # this must run while READY or nothing faults.
        if _want("E.5"):
            link.send({"type": "clear_error_history"}, timeout=2.0)
            link.drain_async()
            link.ser.write(b"@@@@@@@@")           # not JSON: latch the link
            time.sleep(0.4)
            r1 = link.send({"type": "PING"}, timeout=1.0)      # eaten silently
            link.ser.write(b'{"type":"RESET"}')   # recovers and auto-redeems
            time.sleep(0.4)
            link.drain_async()
            r2 = link.send({"type": "PING"}, timeout=1.5)
            st, stat = _state(link)
            errs = _errors_of(stat)
            rep.add("E.5", "garbage latches the link: silent until one RESET"
                    " recovers it",
                    r1 is None and bool(r2 and r2.get("type") == "PONG"),
                    f"after garbage: {'silent' if r1 is None else r1}; "
                    f"after RESET: {(r2 or {}).get('type')}")
            rep.add("E.5b", "protocol error faulted (11) and RESET redeemed it",
                    11 in errs and st == ST_READY,
                    f"ERROR_HIST={errs} state={ST_NAME.get(st, st)} "
                    f"(expect SERIAL_PROTOCOL_ERROR=11, back to READY)")

        # --- E.6: RBuf saturation -----------------------------------------
        # Give objects a runway long enough that more of them are in flight
        # than RBuf can hold. The firmware must cap silently -- reject the
        # excess pulses, announce nothing for them, fault nothing -- and drain
        # clean. Answer everything on-announce so nothing faults for the
        # legitimate reason.
        if _want("E.6"):
            link.send({"type": "clear_error_history"}, timeout=2.0)
            l1a = int(orig_spo.get("L1A_on", 654))
            win2 = l1a + E6_WINDOW
            link.send({"type": "set_setup", "plateFreq": E6_FREQ,
                       "minDetectTimeSep_us": 5000,
                       "stage_pulse_offset": {
                           "SWITCH": win2,
                           "SEL1_on": win2 + 3,  "SEL1_off": win2 + 4,
                           "SEL2_on": win2 + 13, "SEL2_off": win2 + 14,
                           "SEL3_on": win2 + 23, "SEL3_off": win2 + 24}},
                      timeout=3.0)
            _wait_at_speed(link)

            # ISR ticks at 2*plateFreq, so steps/s is known exactly -- pace off
            # that rather than a live measurement (see the E6_* comment above).
            steps_per_s = 2.0 * E6_FREQ
            interval = E6_SPACING / steps_per_s
            seen, answered = [], set()
            qs = [0]
            fired = 0
            link.drain_async()
            t0 = time.time()
            for i in range(E6_COUNT):
                deadline = t0 + i * interval
                while time.time() < deadline:
                    _pump(CAT_NA, seen, answered, qs)
                    time.sleep(0.002)
                link.send_nowait({"type": "trig_phamton_pulse"})
                fired += 1
            # Every accepted object was reported on-announce, so none can fault;
            # a short settle is enough to drain the announcement tail and catch
            # any late fault without waiting the whole window out.
            _settle(4.0, CAT_NA, seen, answered)
            objs = set(seen)
            st, stat = _state(link)
            errs = _errors_of(stat)
            rep.add("E.6", "over-capacity pulses rejected silently, no fault",
                    qs[0] >= 95 and 95 <= len(objs) <= PIPE_INFO_LEN
                    and len(objs) < fired and st == ST_READY and not errs,
                    f"fired={fired} objects={len(objs)} "
                    f"(RBuf holds {PIPE_INFO_LEN}) peak Qs={qs[0]} "
                    f"errors={errs} state={ST_NAME.get(st, st)}")

    finally:
        # --- E.7: restore --------------------------------------------------
        link.send({"type": "clear_error"}, timeout=3.0)
        link.send({"type": "exit_insp_mode"}, timeout=3.0)
        link.send({"type": "set_sel1_cd", "count": -1}, timeout=3.0)
        link.send({"type": "clear_error_history"}, timeout=2.0)
        restore = {"type": "set_setup"}
        if isinstance(orig_freq, (int, float)):
            restore["plateFreq"] = orig_freq
        if isinstance(orig_sep, (int, float)):
            restore["minDetectTimeSep_us"] = orig_sep
        if orig_spo:
            restore["stage_pulse_offset"] = orig_spo
        link.send(restore, timeout=3.0)
        chk = ((link.send({"type": "get_setup"}, timeout=3.0) or {})
               .get("stage_pulse_offset") or {})
        cd = (link.send({"type": "get_sel1_cd"}, timeout=3.0)
              or {}).get("sel1_cd")
        st, _ = _state(link)
        rep.add("E.7", "board restored: IDLE, offsets back, countdown off",
                st == ST_IDLE and cd == -1
                and (orig_spo.get("SWITCH") is None
                     or chk.get("SWITCH") == orig_spo.get("SWITCH")),
                f"state={ST_NAME.get(st, st)} SWITCH={chk.get('SWITCH')} "
                f"sel1_cd={cd}")


# --- iotrace: the actuator sequence, straight from the firmware -----------

# The raw PIN_O_* GPIO numbers the firmware records, plus the synthetic id 0
# for the SWITCH dispatch (which has no pin of its own). Keep in step with
# HardwareConfig.hpp and IOT_PIN_SWITCH in LegacyFirmware.cpp.
IOT_PIN = {0: "SWITCH", 16: "L1A", 17: "CAM1", 18: "L2A", 19: "CAM2",
           25: "SEL1", 26: "SEL2", 32: "SEL3"}

# Each recorded (pin_name, val) edge and the stage_pulse_offset key that should
# place it. SWITCH is handled apart -- its val is the decided cat, not 0/1.
IOT_EDGE_KEY = {
    ("L1A", 1): "L1A_on",   ("L1A", 0): "L1A_off",
    ("CAM1", 1): "CAM1_on", ("CAM1", 0): "CAM1_off",
    ("L2A", 1): "L2A_on",   ("L2A", 0): "L2A_off",
    ("CAM2", 1): "CAM2_on", ("CAM2", 0): "CAM2_off",
    ("SEL1", 1): "SEL1_on", ("SEL1", 0): "SEL1_off",
    ("SEL2", 1): "SEL2_on", ("SEL2", 0): "SEL2_off",
}


def iotrace(link, rep, plate_freq, cat):
    """Dump the actuator edge sequence the firmware records, and check every
    edge lands on its configured stage_pulse_offset -- at REAL geometry.

    The firmware logs each L1A/CAM/SWITCH/SEL GPIO edge with the pulse count it
    fired at (io_trace_arm/dump). Firing one phantom and dumping turns the board
    into its own logic analyzer: it verifies the physical output timing and
    ordering that a counter cannot see, with no scope and -- unlike bench --
    WITHOUT widening the window. A low plateFreq is the trick: the real ~43-step
    camera-to-selector gap becomes tens of ms in wall time, long enough that the
    host verdict still lands in the true window, so SWITCH dispatches the part
    and the SEL edges fire on schedule too.
    """
    print("\n\033[1m== IO trace: actuator sequence at real geometry ==\033[0m")

    orig = link.send({"type": "get_setup"}, timeout=3.0) or {}
    orig_freq = orig.get("plateFreq")
    spo = dict(orig.get("stage_pulse_offset") or {})
    link.send({"type": "clear_error"}, timeout=2.0)
    link.send({"type": "clear_error_history"}, timeout=2.0)

    try:
        # Deliberately NOT widening the window -- the whole point is real
        # offsets. Low plateFreq stretches the 43-step gap into answerable ms.
        link.send({"type": "set_setup", "plateFreq": plate_freq}, timeout=3.0)
        link.send({"type": "enter_insp_mode"}, timeout=3.0)
        _wait_at_speed(link)
        st, _ = _state(link)
        if st != ST_READY:
            rep.add("I.0", "reached READY", False,
                    f"state={st} ({ST_NAME.get(st, '?')}) -- aborting iotrace")
            return

        r = link.send({"type": "io_trace_arm"}, timeout=3.0)
        rep.add("I.1", "io_trace armed", bool(r and r.get("armed") is True), r)
        link.drain_async()

        # One phantom, answered the instant it announces so the verdict makes
        # the real window and SWITCH/SEL fire (cat -> SEL1/SEL2).
        answered = set()
        link.send({"type": "trig_phamton_pulse"}, timeout=3.0)
        t_end = time.time() + 3.0
        while time.time() < t_end:
            for _, m in link.drain_async():
                if m.get("type") == "bTrigInfo":
                    tid = m.get("tid")
                    if tid not in answered:
                        answered.add(tid)
                        link.send_nowait({"type": "report", "tid": tid,
                                          "cat": cat})
            time.sleep(0.002)

        dump = link.send({"type": "io_trace_dump"}, timeout=3.0) or {}
        ev = dump.get("ev") or []
        n, emitted = dump.get("n"), dump.get("emitted")
        rep.add("I.2", "trace recorded and dumped intact",
                bool(ev) and n == emitted and emitted == len(ev),
                f"n={n} emitted={emitted} rows={len(ev)}"
                + ("  (n>emitted means the 3KB dump buffer truncated -- fire "
                   "fewer parts per trace)" if (n or 0) != (emitted or 0)
                   else ""))
        if not ev:
            return

        named = [(IOT_PIN.get(p, p), v, pulse, tid)
                 for pulse, p, v, tid in ev]

        # I.3: monotonic pulse order -- the sequence must not reorder.
        pulses = [pulse for _, _, pulse, _ in named]
        rep.add("I.3", "edges in nondecreasing pulse order",
                all(b >= a for a, b in zip(pulses, pulses[1:])),
                " -> ".join(f"{nm}{'+' if v else '-'}@{pl}"
                            for nm, v, pl, _ in named))

        # Anchor on the first L1A rising edge: gate_pulse = its pulse - L1A_on.
        anchor = next((pulse for nm, v, pulse, _ in named
                       if nm == "L1A" and v == 1), None)
        gate = (anchor - spo.get("L1A_on", 0)) if anchor is not None else None

        # I.4: every light/camera edge on its configured offset (this set fires
        # regardless of the host verdict, so it always tells the timing truth).
        core = [(nm, v, pulse) for nm, v, pulse, _ in named
                if nm in ("L1A", "CAM1", "L2A", "CAM2")]
        bad = []
        for nm, v, pulse in core:
            want = spo.get(IOT_EDGE_KEY[(nm, v)])
            got = (pulse - gate) if gate is not None else None
            if want is None or got is None or abs(got - want) > 1:
                bad.append(f"{nm}{'+' if v else '-'}: off {got} != {want}")
        have_all = {(nm, v) for nm, v, _ in core} == {
            ("L1A", 1), ("L1A", 0), ("CAM1", 1), ("CAM1", 0),
            ("L2A", 1), ("L2A", 0), ("CAM2", 1), ("CAM2", 0)}
        rep.add("I.4", "light+camera edges each on their configured offset",
                have_all and not bad,
                ("missing edges" if not have_all else "")
                + (" ".join(bad) if bad else
                   "L1A/CAM1/L2A/CAM2 on+off all match stage_pulse_offset"))

        # I.5: CAM1 and CAM2 rising edges coincide (dual-camera, same gate).
        c1 = next((pl for nm, v, pl, _ in named if nm == "CAM1" and v == 1),
                  None)
        c2 = next((pl for nm, v, pl, _ in named if nm == "CAM2" and v == 1),
                  None)
        rep.add("I.5", "CAM1 and CAM2 trigger on the same pulse",
                c1 is not None and c1 == c2, f"CAM1@{c1} CAM2@{c2}")

        # I.6: SWITCH dispatched the part with the verdict we sent, at the
        # SWITCH offset -- proof the report landed inside the REAL window with
        # no widening (val would be a large UNSET sentinel if it had missed).
        sw = next(((v, pulse) for nm, v, pulse, _ in named if nm == "SWITCH"),
                  None)
        sw_off = (sw[1] - gate) if (sw and gate is not None) else None
        rep.add("I.6", "SWITCH dispatched the real verdict inside the true "
                "window (no widening)",
                bool(sw) and sw[0] == cat and sw_off is not None
                and abs(sw_off - spo.get("SWITCH", -999)) <= 1,
                f"SWITCH val={sw[0] if sw else None} (reported cat={cat}) "
                f"off={sw_off} vs {spo.get('SWITCH')}")

        # I.7: the selector for that verdict fired on+off at its offsets.
        sel = "SEL1" if cat == 1 else ("SEL2" if cat == 2 else None)
        if sel:
            on = next((pl for nm, v, pl, _ in named if nm == sel and v == 1),
                      None)
            off = next((pl for nm, v, pl, _ in named if nm == sel and v == 0),
                       None)
            on_ok = on is not None and gate is not None and abs(
                (on - gate) - spo.get(f"{sel}_on", -999)) <= 1
            off_ok = off is not None and gate is not None and abs(
                (off - gate) - spo.get(f"{sel}_off", -999)) <= 1
            rep.add("I.7", f"{sel} fired on+off at its configured offsets",
                    on_ok and off_ok,
                    f"{sel}_on off={None if on is None else on - gate} vs "
                    f"{spo.get(f'{sel}_on')}, "
                    f"{sel}_off off={None if off is None else off - gate} vs "
                    f"{spo.get(f'{sel}_off')}")

    finally:
        link.send({"type": "io_trace_stop"}, timeout=3.0)
        link.send({"type": "clear_error"}, timeout=3.0)
        link.send({"type": "exit_insp_mode"}, timeout=3.0)
        if isinstance(orig_freq, (int, float)):
            link.send({"type": "set_setup", "plateFreq": orig_freq},
                      timeout=3.0)
        st, _ = _state(link)
        rep.add("I.8", "returned to IDLE, plateFreq restored",
                st == ST_IDLE, f"state={ST_NAME.get(st, st)} "
                f"plateFreq={orig_freq}")


# --- stage 3: which selector feeds which bin ------------------------------

def selectors(link, rep):
    print("\n\033[1m== Stage 3.1/3.2: physical outlet mapping ==\033[0m")
    print("  \033[1mThis is the one that cannot self-correct.\033[0m Ejected parts")
    print("  do not recirculate, so a swapped good/bad mapping produces a bin")
    print("  full of the wrong parts and nothing anywhere reports a problem.")
    print("  Determine it by firing each selector and looking, not by reasoning.\n")

    mapping = {}
    for idx in (1, 2, 3):
        input(f"  [Enter to fire SEL{idx}] ")
        r = link.send({"type": "sel_act", "idx": idx, "delay": 50}, timeout=3.0)
        if idx == 3:
            print("  (SEL3 has no scheduler queue in the firmware -- this manual")
            print("   fire works, but cat=3 during a run actuates nothing.)")
        fired = ask(f"did SEL{idx} actually fire?")
        if fired is False:
            rep.add(f"3.{idx}", f"SEL{idx} fires", False, r)
            continue
        where = input(f"  which bin does SEL{idx} feed? (short label) ").strip()
        mapping[idx] = where
        rep.add(f"3.{idx}", f"SEL{idx} -> {where or '(unrecorded)'}",
                True if where else None, r)

    if mapping:
        print("\n  \033[1mPut this in machine_setting.json:\033[0m")
        print("    \"machine_type\": \"uInspESP32\",")
        print(f"    \"cat_ok\": <the SEL feeding the GOOD bin>,   // {mapping}")
        print("    \"cat_ng\": <the SEL feeding the REJECT bin>")
        print("\n  Until both are set the core reports every part NA and nothing")
        print("  is ejected -- which is the safe state to leave it in until you")
        print("  are sure.")
        rep.add("3.3", "outlet mapping recorded", None, json.dumps(mapping))


# --- main ------------------------------------------------------------------

def cmd_ports(_args):
    for p in list_ports.comports():
        print(f"  {p.device:<10} {p.description}")
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="serial port, e.g. COM6")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("-v", "--verbose", action="store_true", help="log every frame")
    ap.add_argument("-o", "--out", default="uinsp_verify_report.md")
    sub = ap.add_subparsers(dest="cmd", required=True)

    sub.add_parser("ports", help="list serial ports")
    sub.add_parser("stage0", help="firmware-alone checks incl. NVS persistence")
    sub.add_parser("errorpath", help="stage 0.7 inspection-error behaviour")
    sub.add_parser("selectors", help="stage 3.1/3.2 outlet mapping")
    m = sub.add_parser("monitor", help="watch bTrigInfo / tid continuity")
    m.add_argument("--seconds", type=int, default=60)
    b = sub.add_parser("bench",
                       help="full tid round trip using phantom pulses -- "
                            "needs only the board and USB, no rig")
    b.add_argument("--count", type=int, default=10)
    b.add_argument("--freq", type=float, default=1000)
    b.add_argument("--interval-ms", type=int, default=120,
                   help="phantom spacing; must exceed SYS_MIN_PULSE_TIME_SEP_us "
                        "(~67ms) and clear the 3.5mm de-dup gate at --freq")
    b.add_argument("--cat", type=int, default=1,
                   help="1=SEL1 2=SEL2 65535=NA")
    sub.add_parser("probe",
                   help="protocol + camera-trigger surface: version handshake, "
                        "reset_running_stat, trigCamPulse -- board only, no rig")
    sub.add_parser("edge",
                   help="deep firmware paths: NA verdict, SKIP absorption, "
                        "pulse-gate rejection, SEL1 countdown, protocol-error "
                        "latch, RBuf saturation -- board only, no rig")
    it = sub.add_parser("iotrace",
                        help="dump the firmware's own record of the actuator "
                             "edge sequence and check it against the real "
                             "stage_pulse_offset -- board only, no scope")
    it.add_argument("--freq", type=float, default=200,
                    help="plateFreq; low stretches the real 43-step window "
                         "into answerable ms so SWITCH/SEL fire in-window")
    it.add_argument("--cat", type=int, default=1, help="1=SEL1 2=SEL2")
    st_ = sub.add_parser("stress",
                         help="ramp the object rate until the pipeline gives "
                              "-- board only, no rig")
    st_.add_argument("--start-hz", type=int, default=10)
    st_.add_argument("--max-hz", type=int, default=120)
    st_.add_argument("--step-hz", type=int, default=10)
    st_.add_argument("--dwell", type=float, default=3.0,
                     help="seconds to hold each rate")
    st_.add_argument("--cat", type=int, default=CAT_NA)
    st_.add_argument("--no-report", action="store_true",
                     help="never answer -- measures the announce path alone")

    sl = sub.add_parser("stall", help="stop answering mid-run; must fault, "
                                      "not guess")
    sl.add_argument("--hz", type=int, default=10)
    sl.add_argument("--stall-seconds", type=float, default=5.0)
    sl.add_argument("--cat", type=int, default=CAT_NA)

    s = sub.add_parser("send", help="send one raw JSON command")
    s.add_argument("json")
    sub.add_parser("all", help="stage0 + errorpath + monitor + selectors")

    args = ap.parse_args()

    if args.cmd == "ports":
        return 0 if cmd_ports(args) else 1
    if not args.port:
        ap.error("--port is required (use `ports` to list them)")

    link = UInspLink(args.port, args.baud, verbose=args.verbose)
    rep = Report()
    # A previous run may have left the protocol error latched, which blocks
    # every command except RESET.
    link.ser.write(b'{"type":"RESET"}')
    time.sleep(0.3)
    link.drain_async()

    try:
        if args.cmd == "send":
            print(json.dumps(link.send(json.loads(args.json), timeout=5.0),
                             indent=1, ensure_ascii=False))
            return 0
        if args.cmd == "stage0":
            stage0(link, rep)
        elif args.cmd == "errorpath":
            stage_error(link, rep)
        elif args.cmd == "selectors":
            selectors(link, rep)
        elif args.cmd == "monitor":
            monitor(link, rep, args.seconds)
        elif args.cmd == "bench":
            bench(link, rep, args.count, args.freq, args.interval_ms, args.cat)
        elif args.cmd == "probe":
            probe(link, rep)
        elif args.cmd == "edge":
            edge(link, rep)
        elif args.cmd == "iotrace":
            iotrace(link, rep, args.freq, args.cat)
        elif args.cmd == "stress":
            stress(link, rep, args.start_hz, args.max_hz, args.step_hz,
                   args.dwell, args.cat, not args.no_report)
        elif args.cmd == "stall":
            stall(link, rep, args.hz, args.stall_seconds, args.cat)
        elif args.cmd == "all":
            stage0(link, rep)
            probe(link, rep)
            bench(link, rep, 10, 1000, 120, 1)
            edge(link, rep)
            iotrace(link, rep, 200, 1)
            stage_error(link, rep)
            monitor(link, rep, 60)
            selectors(link, rep)
    except KeyboardInterrupt:
        print("\n  interrupted")
    finally:
        ok = rep.summary() if rep.rows else True
        if rep.rows:
            rep.write_markdown(args.out)
        link.close()
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
