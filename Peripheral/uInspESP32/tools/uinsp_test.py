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
        elif args.cmd == "all":
            stage0(link, rep)
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
