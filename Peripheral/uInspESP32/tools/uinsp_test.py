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
        while not self._stop:
            try:
                chunk = self.ser.read(4096)
            except Exception:
                break
            if not chunk:
                continue
            for ch in chunk.decode("utf-8", errors="replace"):
                if depth == 0:
                    # Resync point. The firmware would fault on stray bytes;
                    # we just ignore them so a mid-stream attach still works.
                    if ch not in "{[":
                        continue
                    buf = ""
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


def _wait_ready(link, rep):
    """Bring the board into INSPECTION_MODE_READY with its timer actually
    ticking. Run_ACTS only executes from the timer ISR, so with plateFreq at 0
    a phantom pulse is accepted and then never acted on -- which looks exactly
    like a broken pipeline."""
    a = link.send({"type": "get_setup"}, timeout=3.0)
    if not a:
        return False, "no reply to get_setup"
    c1 = a.get("SYS_STEP_COUNT")
    time.sleep(0.6)
    b = link.send({"type": "get_setup"}, timeout=3.0)
    c2 = (b or {}).get("SYS_STEP_COUNT")
    running = (isinstance(c1, int) and isinstance(c2, int) and c2 != c1)
    return running, f"SYS_STEP_COUNT {c1} -> {c2}"


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
    base_counts, _ = _counts(link)

    r = link.send({"type": "set_setup", "plateFreq": freq}, timeout=3.0)
    rep.add("B.1", f"set plateFreq={freq}", bool(r), r)

    r = link.send({"type": "enter_insp_mode"}, timeout=3.0)
    rep.add("B.2", "enter_insp_mode", bool(r), r)

    running, detail = _wait_ready(link, rep)
    rep.add("B.3", "timer ISR is actually ticking", running, detail)
    if not running:
        print("  \033[31m  Run_ACTS only runs from the timer ISR. Without it a"
              " phantom pulse is accepted and then never acted on.\033[0m")

    st, _ = _state(link)
    rep.add("B.4", "state is INSPECTION_MODE_READY", st == ST_READY,
            f"state={st} ({ST_NAME.get(st, '?')})")

    # --- fire phantoms, answer each announced tid -------------------------
    link.drain_async()
    fired, seen, reported, rejected = 0, [], [], []

    for i in range(count):
        r = link.send({"type": "trig_phamton_pulse"}, timeout=3.0)
        fired += 1
        time.sleep(interval_ms / 1000.0)

        for _, msg in link.drain_async():
            if msg.get("type") != "bTrigInfo":
                continue
            tid = msg.get("tid")
            seen.append(tid)
            # report never answers (doRsp=false either way), so do not wait.
            link.send_nowait({"type": "report", "tid": tid, "cat": cat})
            reported.append(tid)

    time.sleep(0.5)
    for _, msg in link.drain_async():
        if msg.get("type") == "bTrigInfo":
            seen.append(msg.get("tid"))

    rep.add("B.5", "one bTrigInfo per phantom pulse", len(seen) == fired,
            f"fired={fired} announced={len(seen)}"
            + ("  (newPulseEvent rejects pulses closer than "
               "SYS_MIN_PULSE_TIME_SEP_us or 3.5mm of travel -- raise "
               "--interval-ms)" if len(seen) < fired else ""))

    if seen:
        gaps = [(a, b) for a, b in zip(seen, seen[1:]) if b != a + 1]
        rep.add("B.6", "tid strictly increasing by 1", not gaps,
                f"tid {seen[0]}..{seen[-1]}" + (f" gaps:{gaps}" if gaps else ""))

    st, stat = _state(link)
    rep.add("B.7", "no error state after a full reported run", st == ST_READY,
            f"state={st} ({ST_NAME.get(st, '?')}) "
            f"ERROR_HIST={(stat or {}).get('ERROR_HIST')}")

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
    if isinstance(orig_freq, (int, float)):
        link.send({"type": "set_setup", "plateFreq": orig_freq}, timeout=3.0)
    st, _ = _state(link)
    rep.add("B.13", "returned to IDLE with plateFreq restored",
            st == ST_IDLE, f"state={st} plateFreq={orig_freq}")


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
    time.sleep(0.8)

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
        link.send({"type": "enter_insp_mode"}, timeout=2.0)
        time.sleep(0.3)
        link.drain_async()

        period = 1.0 / hz
        n = max(1, int(dwell * hz))
        fired = 0
        seen = []
        qs_max = 0
        t0 = time.time()

        for i in range(n):
            deadline = t0 + i * period
            slack = deadline - time.time()
            if slack > 0:
                time.sleep(slack)
            # No round trip: waiting on the ack would itself become the limit.
            link.send_nowait({"type": "trig_phamton_pulse"})
            fired += 1

            for _, msg in link.drain_async():
                if msg.get("type") == "bTrigInfo":
                    tid = msg.get("tid")
                    seen.append(tid)
                    qs_max = max(qs_max, msg.get("Qs", 0) or 0)
                    if do_report:
                        link.send_nowait({"type": "report", "tid": tid, "cat": cat})

        # let the tail drain
        t_drain = time.time() + 1.5
        while time.time() < t_drain:
            time.sleep(0.05)
            for _, msg in link.drain_async():
                if msg.get("type") == "bTrigInfo":
                    tid = msg.get("tid")
                    seen.append(tid)
                    qs_max = max(qs_max, msg.get("Qs", 0) or 0)
                    if do_report:
                        link.send_nowait({"type": "report", "tid": tid, "cat": cat})

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

    link.send({"type": "set_setup", "plateFreq": 1000}, timeout=3.0)
    link.send({"type": "clear_error"}, timeout=2.0)
    link.send({"type": "enter_insp_mode"}, timeout=3.0)
    time.sleep(0.8)
    link.drain_async()

    period = 1.0 / hz
    answered = 0
    for i in range(int(3 * hz)):
        link.send_nowait({"type": "trig_phamton_pulse"})
        time.sleep(period)
        for _, msg in link.drain_async():
            if msg.get("type") == "bTrigInfo":
                link.send_nowait({"type": "report", "tid": msg["tid"], "cat": cat})
                answered += 1

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
    b.add_argument("--interval-ms", type=int, default=250,
                   help="must exceed SYS_MIN_PULSE_TIME_SEP_us (default ~67ms)")
    b.add_argument("--cat", type=int, default=1,
                   help="1=SEL1 2=SEL2 65535=NA")
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
        elif args.cmd == "stress":
            stress(link, rep, args.start_hz, args.max_hz, args.step_hz,
                   args.dwell, args.cat, not args.no_report)
        elif args.cmd == "stall":
            stall(link, rep, args.hz, args.stall_seconds, args.cat)
        elif args.cmd == "all":
            stage0(link, rep)
            bench(link, rep, 10, 1000, 250, 1)
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
