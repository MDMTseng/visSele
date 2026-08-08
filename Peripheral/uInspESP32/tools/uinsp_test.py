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
    python uinsp_test.py --port COM6 send '{"type":"ping"}'
"""

import argparse
import json
import math
import random
import os
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

class LinkReset(Exception):
    """Raised after the serial link dropped and was transparently reopened.

    A USB-UART re-enumeration (cable re-seat, hub/power hiccup) power-cycles the
    ESP32, so once the port comes back the board has rebooted: IDLE, config and
    tid counter reset, pipeline empty. Callers that hold board state (chaos)
    catch this, re-establish that state, and continue -- turning a USB drop into
    a hiccup instead of an end-of-run.
    """


class LinkDead(Exception):
    """The port never came back within the reconnect timeout -- genuinely
    unrecoverable (board unplugged and left off), not a transient drop."""


def _crc16_ccitt(data):
    """CRC16-CCITT (poly 0x1021, init 0xFFFF) -- must match the firmware's
    Data_JsonRaw_Layer::crc16_ccitt."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 \
                else (crc << 1) & 0xFFFF
    return crc


class UInspLink:
    """Serial link speaking the firmware's brace-framed JSON.

    Replies and asynchronous device messages arrive on the same stream, so the
    reader thread splits them: anything carrying an 'id' we sent is routed to
    the waiting caller, everything else (cam_trig, system_info, dbg) goes to
    the async queue.
    """

    def __init__(self, port, baud=230400, verbose=False):
        self.port = port
        self.baud = baud
        # Pin the modem lines BEFORE opening -- both DEASSERTED. This
        # adapter wires RTS straight to EN (no auto-program transistor pair),
        # so an asserted RTS holds the chip in reset for as long as the port
        # is open. Deasserted lines = EN released, IO0 high: the board keeps
        # running across our opens.
        self.ser = serial.Serial()
        self.ser.port = port
        self.ser.baudrate = baud
        self.ser.timeout = 0.05
        self.ser.dtr = False
        self.ser.rts = False
        self.ser.open()
        self.verbose = verbose
        self._id = 1000
        self._pending = {}
        self._lock = threading.Lock()
        # Serializes id assignment + the wire write, so concurrent callers
        # (e.g. a UI poller and a low-latency auto-reporter) cannot interleave
        # frames or mint duplicate ids. Reply *waiting* happens outside it.
        self._tx_lock = threading.Lock()
        self._async = deque(maxlen=20000)
        self._trig_us = {}          # tid -> board t_us, for report cam_ts
        self._async_ev = threading.Event()
        self._raw_log = deque(maxlen=5000)
        # Frame-integrity + event-loss accounting (see *HHHH trailer / "q").
        self.rx_frames = 0
        self.rx_crc_ok = 0
        self.rx_crc_fail = 0
        self.event_gaps = 0
        self._last_q = None
        # Consecutive serial read failures. A port NODE that exists but whose
        # reads throw "Device not configured" is a ZOMBIE bridge -- enumerated
        # shell, dead endpoints -- and must not be mistaken for a firmware
        # hang (that mistake ended a 1.3M-part soak).
        self.read_errors = 0
        self._stop = False
        # Auto-reconnect: when on, a dropped port is transparently reopened
        # instead of killing the run. The write path signals the reopen to
        # callers via LinkReset so board state can be re-established; the reader
        # thread just waits it out. Off by default -> unchanged behaviour.
        self.auto_reconnect = False
        self.reconnects = 0
        self._reconn_lock = threading.Lock()
        self._last_reset = 0.0
        # Identity of the port node we opened. A USB re-enumeration destroys and
        # re-creates the node (new inode/birthtime); a board that just goes
        # silent leaves it intact. That difference lets a silent-death USB drop
        # be recovered WITHOUT masking a real firmware hang/reboot.
        self._port_sig0 = self._port_sig()
        self._rx = threading.Thread(target=self._reader, daemon=True)
        self._rx.start()

    def _port_sig(self):
        """A stable-per-enumeration signature of the port node, or None if the
        node is currently absent (definitely re-enumerating/gone)."""
        try:
            s = os.stat(self.port)
            return (s.st_ino, getattr(s, "st_birthtime", s.st_ctime))
        except OSError:
            return None

    def port_reenumerated(self):
        """True if the port node vanished or was re-created since we opened it
        -- a USB re-enumeration -- as opposed to the board going silent with the
        node intact (a firmware hang/reboot, which must NOT be masked)."""
        cur = self._port_sig()
        return cur is None or cur != self._port_sig0

    def close(self):
        self._stop = True
        time.sleep(0.15)
        try:
            self.ser.close()
        except Exception:
            pass

    def _reopen_serial(self, wait_timeout=600.0):
        """Close the dead handle and wait for the port node to re-appear, then
        reopen it. Serialized so the reader thread and the write path don't both
        reopen the same drop (or double-count it). Returns True once the link is
        back, False if the port stays gone past wait_timeout."""
        with self._reconn_lock:
            # Coalesce: if a sibling thread reopened this same drop moments ago,
            # adopt its handle rather than cycling the port a second time.
            if time.time() - self._last_reset < 1.5:
                return True
            try:
                self.ser.close()
            except Exception:
                pass
            deadline = time.time() + wait_timeout
            delay = 0.2
            announced = False
            while not self._stop and time.time() < deadline:
                if os.path.exists(self.port):
                    try:
                        self.ser = serial.Serial()
                        self.ser.port = self.port
                        self.ser.baudrate = self.baud
                        self.ser.timeout = 0.05
                        self.ser.dtr = False
                        self.ser.rts = False
                        self.ser.open()
                        # A freshly re-enumerated adapter often needs a beat
                        # before it accepts I/O, and the MCU it just power-cycled
                        # needs to finish booting.
                        time.sleep(0.6)
                        self._port_sig0 = self._port_sig()   # adopt the new node
                        self.reconnects += 1
                        self._last_reset = time.time()
                        print(f"  [link reconnected (#{self.reconnects}) on "
                              f"{self.port}]", flush=True)
                        return True
                    except Exception:
                        pass
                if not announced:
                    print(f"  [link down; waiting for {self.port} to "
                          f"re-appear...]", flush=True)
                    announced = True
                time.sleep(delay)
                delay = min(delay * 1.5, 2.0)
            return False

    def _tx(self, raw):
        """Write raw bytes, transparently reopening a dropped link. Raises
        LinkReset once the link is back (board state now unknown) or LinkDead if
        it never returned. With auto_reconnect off, the original error
        propagates -- unchanged behaviour."""
        try:
            self.ser.write(raw)
            self.ser.flush()
            return
        except (serial.SerialException, OSError) as exc:
            if not self.auto_reconnect:
                raise
            print(f"  [link write failed: {exc} -- reconnecting]", flush=True)
            if self._reopen_serial():
                raise LinkReset()
            raise LinkDead()

    def _reader(self):
        buf = ""
        depth = 0
        in_str = False
        esc = False
        fails = 0
        frame_t0 = None
        noise = ""
        pend = None    # completed frame awaiting its optional *HHHH trailer
        tr = ""
        pend_t = 0.0

        def _sd(frame):
            # Belt and braces: nothing a device can send should be able to
            # take the reader thread down.
            try:
                self._dispatch(frame)
            except Exception as exc:              # pragma: no cover
                print(f"  [dispatch error: {exc}]")

        while not self._stop:
            try:
                chunk = self.ser.read(4096)
                fails = 0
                self.read_errors = 0
            except Exception as exc:
                self.read_errors = getattr(self, "read_errors", 0) + 1
                # A transient driver hiccup must not silently kill reception:
                # writes would keep working, the board would keep executing
                # commands, and every reply would just vanish for the rest of
                # the run -- which looks exactly like a dead board.
                if self._stop:
                    break
                if self.auto_reconnect:
                    # A full port drop (re-enumeration) fails reads too. Wait for
                    # the link to come back rather than giving up; the write path
                    # drives re-establishing board state. Resync framing after
                    # the gap.
                    self._reopen_serial()
                    buf, depth, in_str, esc, frame_t0 = "", 0, False, False, None
                    pend, tr = None, ""
                    continue
                fails += 1
                if fails >= 50:
                    print(f"  [serial read failed {fails}x, giving up: {exc}]")
                    break
                print(f"  [serial read error, retrying: {exc}]")
                time.sleep(0.1)
                continue
            if not chunk:
                # A completed frame with no trailer bytes following: a legacy
                # peer with nothing else to say. Grace period then dispatch.
                if pend is not None and time.time() - pend_t > 0.05:
                    self.rx_frames += 1
                    if tr:
                        self.rx_crc_fail += 1
                    else:
                        _sd(pend)
                    pend, tr = None, ""
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
                if pend is not None:
                    # Between a frame and its optional CRC trailer. A bad CRC
                    # drops the frame (a corrupted frame is exactly the one
                    # not to act on); no trailer = legacy peer, passes through.
                    if tr == "" and ch == "*":
                        tr = "*"
                        continue
                    if tr and len(tr) < 5 and ch in "0123456789abcdefABCDEF":
                        tr += ch
                        continue
                    if ch in "\r\n":
                        self.rx_frames += 1
                        if tr:
                            ok = (len(tr) == 5 and
                                  int(tr[1:], 16) == _crc16_ccitt(pend.encode()))
                            if ok:
                                self.rx_crc_ok += 1
                                _sd(pend)
                            else:
                                self.rx_crc_fail += 1
                        else:
                            _sd(pend)
                        pend, tr = None, ""
                        continue
                    # unexpected char: trailer absent or garbled
                    self.rx_frames += 1
                    if tr:
                        self.rx_crc_fail += 1
                    else:
                        _sd(pend)
                    pend, tr = None, ""
                    # fall through: process ch normally
                if depth == 0:
                    # Resync point. The firmware would fault on stray bytes;
                    # we ignore them for framing -- but KEEP them in the raw
                    # log: a panic backtrace or the boot ROM banner is plain
                    # text and is exactly what explains a dead board.
                    if ch not in "{[":
                        if ch == "\n":
                            if noise.strip():
                                self._raw_log.append(
                                    (time.time(), "NOISE: " + noise.strip()))
                            noise = ""
                        elif len(noise) < 400:
                            noise += ch
                        continue
                    if noise.strip():
                        self._raw_log.append(
                            (time.time(), "NOISE: " + noise.strip()))
                    noise = ""
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
                        # Hold the frame until we know whether a CRC trailer
                        # follows; dispatch happens in the trailer logic above.
                        pend, tr = buf, ""
                        pend_t = time.time()
                        frame_t0 = None
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

        q = msg.get("q") if (isinstance(msg, dict) and mid is None) else None
        if q is not None:
            if self._last_q is not None and q > self._last_q + 1:
                self.event_gaps += q - self._last_q - 1
            # q < last_q means the board rebooted; just resync.
            self._last_q = q

        with self._lock:
            slot = self._pending.pop(mid, None) if mid is not None else None
        if slot is not None:
            slot["reply"] = msg
            slot["event"].set()
        else:
            self._async.append((time.time(), msg))
            self._async_ev.set()

    # --- config shape adapter ------------------------------------------
    # The firmware groups its config (2026-08-08): plate / gate / cam /
    # skip_policy / stage_pulse_offset / stage_pulse_width_us / io_on_level,
    # and the flat keys are gone. Rather than rewrite 118 call sites -- and
    # make every one of them carry the mapping -- the translation lives here,
    # once, where it can be read and corrected in one place.
    #
    # This is a compatibility adapter, not a pretence: `poll` and
    # get_running_stat are untouched, and anything reading a group directly
    # (_read_spo, skip_policy) goes straight through.
    _CFG_GROUP = {
        "plate_freq": ("plate", "freq"),
        "plate_accel": ("plate", "accel"),
        "pulses_per_rev": ("plate", "pulses_per_rev"),
        "plate_diameter_mm": ("plate", "diameter_mm"),
        "stepper_en_active": ("plate", "stepper_en_active"),
        "stepper_dir": ("plate", "stepper_dir"),
        "min_detect_sep_us": ("gate", "min_detect_sep_us"),
        "pulse_min_width": ("gate", "pulse_min_width"),
        "pulse_max_width": ("gate", "pulse_max_width"),
        "gate_debounce_rise": ("gate", "debounce_rise"),
        "gate_debounce_fall": ("gate", "debounce_fall"),
        "report_match_ts": ("cam", "report_match_ts"),
        "cam_match_window_us": ("cam", "match_window_us"),
        "cam_recal_idle_ms": ("cam", "recal_idle_ms"),
        "cal_pulse_us": ("cam", "cal_pulse_us"),
        "cam_drift_comp": ("cam", "drift_comp"),
        "unanswered_stop_after": ("skip_policy", "stop_after"),
        "auto_rate_floor_us": ("skip_policy", "rate_floor_us"),
        "auto_rate_recover_n": ("skip_policy", "recover_n"),
    }

    @classmethod
    def _regroup(cls, obj):
        """Flat keys in a set_setup -> the groups the firmware now expects."""
        if obj.get("type") != "set_setup":
            return obj
        out = {}
        for k, v in obj.items():
            g = cls._CFG_GROUP.get(k)
            if g:
                out.setdefault(g[0], {})[g[1]] = v
            else:
                out[k] = v
        return out

    @classmethod
    def _flatten(cls, rep):
        """Groups in a get_setup reply -> the flat names callers still read.
        The groups stay too, so a caller can use either."""
        if not isinstance(rep, dict) or "plate" not in rep:
            return rep
        for flat, (grp, key) in cls._CFG_GROUP.items():
            g = rep.get(grp)
            if isinstance(g, dict) and key in g:
                rep.setdefault(flat, g[key])
        return rep

    def send(self, obj, timeout=2.0):
        """Send a command and wait for the reply that echoes its id."""
        with self._tx_lock:
            obj = self._regroup(dict(obj))
            self._id += 1
            mid = self._id
            obj["id"] = mid

            ev = threading.Event()
            slot = {"event": ev, "reply": None}
            with self._lock:
                self._pending[mid] = slot

            raw = json.dumps(obj, separators=(",", ":")).encode()
            raw += b"*%04X\n" % _crc16_ccitt(raw)
            if self.verbose:
                print("  TX", raw.decode())
            try:
                self._tx(raw)
            except LinkReset:
                # The reply we were about to wait for will never come from the
                # pre-reboot board; drop the slot and let the caller
                # re-establish.
                with self._lock:
                    self._pending.pop(mid, None)
                raise

        got = ev.wait(timeout)
        with self._lock:
            # _dispatch removes it on a hit; this only matters on timeout.
            self._pending.pop(mid, None)
        return self._flatten(slot["reply"]) if got else None

    # A synthetic camera clock, so reports can teach CAM_SYNC.
    #
    # READ THIS BEFORE TRUSTING ANY PAIRING RESULT FROM THIS HARNESS.
    #
    # cam_ts here is derived from the board's OWN t_us, so it is not a second
    # clock at all -- it is the same clock plus a constant. The offset the
    # firmware learns is exactly CAM_TS_EPOCH, the residual is identically
    # zero, and the timestamp match can therefore never fail. That is the
    # opposite of what report_match_ts exists to check: it cross-checks tid
    # pairing against timestamp pairing precisely so that two INDEPENDENT
    # clocks can be seen to disagree.
    #
    # So this makes CAL finish and the rig-less suites run. It does NOT test
    # pairing, and it can hide a pairing defect. It also MANUFACTURES false
    # ones: cam1 and cam2 announce the same object with an identical t_us, so a
    # report citing that t_us leaves the nearest-object search tied and the
    # firmware logs CAMSYNC MISMATCH d=0 against the neighbour.
    #
    # Pairing is verified with the real thing -- core + camera + board. Measured
    # 2026-08-07, 3148 frames, test1 def, 20 rpm, ~12.4 parts/s:
    #
    #   matched 3148/3148, ts_matched 3134, no_candidate 0, stale 0, drops 0
    #   residual mean -16.7us, max 155us, against a 5000us match window
    #   zero CAMSYNC MISMATCH lines in the core log
    #
    # A real camera clock has a -17us systematic offset and a 155us tail. That
    # is the number this synthetic one cannot produce and must not be read as.
    #
    # The firmware learns its camera-clock offset ONLY from a report that
    # carries a non-zero cam_ts:
    #
    #     if (teach != NULL && cam_ts != 0) CAM_SYNC.observe(cam_ts, cam_us);
    #
    # and INSPECTION_MODE_CAL will not hand over to READY until CAM_SYNC is
    # valid. This harness answered with {tid, cat} and no timestamp, so on a
    # board configured for timestamp pairing (report_match_ts) CAL could never
    # finish: every rig-less suite -- chaos, edge, stress, grill -- died at its
    # first step with "reached READY: state=102".
    #
    # cam_trig already carries t_us, the board's own trigger time, so the
    # answer can be t_us + a fixed epoch. That is a perfect camera clock: the
    # offset the board learns is exactly CAM_TS_EPOCH with zero jitter, and
    # every later report lands dead centre of cam_match_window_us. A wall clock
    # would work too but would add serial latency as noise against a 5ms window.
    #
    # Injected here, in the one place every report passes through, rather than
    # at the eighteen call sites that build one.
    CAM_TS_EPOCH = 1_600_000_000_000_000   # us, arbitrary, just non-zero

    def note_trig(self, msg):
        """Remember a cam_trig's board timestamp so a later report can cite it."""
        if msg.get("type") == "cam_trig" and msg.get("tid") is not None:
            t = msg.get("t_us")
            if isinstance(t, (int, float)) and t:
                self._trig_us[msg["tid"]] = int(t)
                if len(self._trig_us) > 4096:      # bounded; a soak runs for days
                    for k in list(self._trig_us)[:2048]:
                        self._trig_us.pop(k, None)

    def _with_cam_ts(self, obj):
        if obj.get("type") != "report" or "cam_ts" in obj:
            return obj
        t = self._trig_us.get(obj.get("tid"))
        if t is None:
            return obj      # unannounced tid (bogus-tid tests) -- leave it bare
        obj = dict(obj)
        obj["cam_ts"] = self.CAM_TS_EPOCH + t
        return obj

    def send_nowait(self, obj):
        """Fire and forget.

        Some commands never answer -- `report` ends with doRsp=false in the
        firmware whether it matched an object or not. Waiting on those would
        burn the full timeout per part, which for a paced test also stretches
        the gap between parts far beyond what was asked for.
        """
        obj = self._with_cam_ts(obj)
        with self._tx_lock:
            obj = self._regroup(dict(obj))
            self._id += 1
            obj["id"] = self._id
            raw = json.dumps(obj, separators=(",", ":")).encode()
            raw += b"*%04X\n" % _crc16_ccitt(raw)
            if self.verbose:
                print("  TX", raw.decode(), "(no reply expected)")
            self._tx(raw)

    def drain_async(self):
        out = []
        while self._async:
            item = self._async.popleft()
            try:
                self.note_trig(item[1])
            except Exception:
                pass
            out.append(item)
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

    r = link.send({"type": "ping"})
    rep.add("0.1", "PING -> PONG", bool(r and r.get("type") == "pong"), r)

    setup = link.send({"type": "get_setup"}, timeout=3.0)
    rep.add("0.2", "get_setup returns machine config",
            bool(setup and "stage_pulse_offset" in setup),
            json.dumps(setup, ensure_ascii=False)[:400] if setup else "no reply")

    if not setup:
        print("  aborting stage 0: the board is not answering get_setup")
        return

    for key in ("machine_id", "cfg_from_nvs", "pulse_min_width", "pulse_max_width"):
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
    """Watch cam_trig and check the assumption the whole tid pairing rests on:
    that tid arrives strictly increasing, one per part, with no gaps."""
    print(f"\n\033[1m== Trigger monitor ({seconds}s) ==\033[0m")
    print("  Run parts through. Watching cam_trig for tid continuity.\n")

    link.drain_async()
    t_end = time.time() + seconds
    tids, gaps, others = [], [], {}
    qs_max = -1
    last_print = 0.0

    while time.time() < t_end:
        time.sleep(0.1)
        for ts, msg in link.drain_async():
            mtype = msg.get("type", "?")
            if mtype == "cam_trig":
                tid = msg.get("tid")
                qs = msg.get("Qs", -1)
                qs_max = max(qs_max, qs if isinstance(qs, int) else -1)
                if tids and tid != tids[-1] + 1:
                    gaps.append((tids[-1], tid))
                    print(f"  \033[31mtid gap: {tids[-1]} -> {tid}\033[0m")
                tids.append(tid)
            else:
                others[mtype] = others.get(mtype, 0) + 1
                if mtype == "system_info":
                    print(f"  \033[33msystem_info: {json.dumps(msg)[:160]}\033[0m")

        if time.time() - last_print > 2.0:
            last_print = time.time()
            print(f"  triggers:{len(tids)} gaps:{len(gaps)} Qs_max:{qs_max} "
                  f"other:{others}", end="\r")

    print(" " * 100, end="\r")
    n = len(tids)
    rep.add("2.3", "cam_trig observed", n > 0, f"{n} triggers in {seconds}s")
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


def _poll(link, timeout=3.0):
    """The hot poll: state / plate_freq / step_count / q / err / cfg_crc.

    122 bytes against get_running_stat's 1327 and get_setup's 1174, and those
    two were the only way to read `state` and `step_count` -- so every wait
    loop in here polled a full configuration document to read one counter. On
    the wire that is ~52ms of transmission the firmware's main loop blocks in
    Serial.write for, and that same loop is what drains ISRTrigQ (32 entries,
    2 per object = ~16 objects of headroom). Use this wherever only the small
    fields are wanted; _state still fetches the whole document for callers
    that read pipe/gate/counters."""
    return link.send({"type": "poll"}, timeout=timeout) or {}


def _state(link):
    r = link.send({"type": "get_running_stat"}, timeout=3.0)
    return (r or {}).get("state"), r


def _counts(link):
    r = link.send({"type": "get_running_stat"}, timeout=3.0)
    return (r or {}).get("count", {}), r


# How long a host scripting over serial gets to answer each tid. The real
# geometry leaves only SWITCH-L1A_on ~= 43 steps between the cam_trig
# announcement and the selector -- a window the in-firmware C++ core answers
# inside but a host over serial cannot. This buys a comfortable margin; the
# original offsets are restored when the caller ends.
#
# Stated in SECONDS, because that is the thing that has to be true. The offset
# itself is in plate steps, and steps are only worth wall-time at a given
# plate_freq: 600 steps is 300ms at plate_freq 1000 and 50ms at 5960. `stress`
# opens plate_freq up to ~6000 so the 3.5mm distance gate clears at high rates,
# and in doing so it was shrinking its own answer budget 6x -- every verdict
# arrived late, the machine faulted at the first rate tried, and the report
# read "first failure at 10/s (tid desync)" for a pipeline nowhere near its
# limit. chaos already sizes its window this way; this puts bench and stress on
# the same footing.
BENCH_WINDOW_S = 0.30
BENCH_WINDOW = 600          # == BENCH_WINDOW_S at plate_freq 1000


def _read_spo(link, tries=5):
    """The board's stage_pulse_offset, or None.

    get_setup's reply is long and does arrive truncated now and then -- the
    whole stage_pulse_offset object simply missing. Every caller here used to
    take that at face value and fall back to a built-in default (L1A_on 654),
    which on this machine (L1A_on 9314) put SWITCH at 4230: the SELECTOR
    BEFORE THE CAMERA. Parts then reached the outlet before they were ever
    announced, and the machine error-stopped with OBJECT_HAS_NO_INSP_RESULT --
    reported by the harness as a pipeline failure at 10/s. The same empty dict
    was then handed to the teardown as "the original", so nothing was restored
    either.

    A default geometry is not a safe answer to a dropped read. Retry, and say
    None if the board really will not tell us."""
    for _ in range(tries):
        spo = ((link.send({"type": "get_setup"}, timeout=3.0) or {})
               .get("stage_pulse_offset") or {})
        if spo.get("SWITCH") is not None and any(
                k.startswith("CAM") and k.endswith("_on") for k in spo):
            return dict(spo)
        time.sleep(0.15)
    return None


def _widen_selector_window(link, orig_spo, freq=None):
    """Push SWITCH and the SEL outputs past the camera trigger (where cam_trig
    is announced) so a host scripting over serial can answer each tid before
    the part reaches the selector.

    `freq` is the plate_freq the caller intends to run at; the offset is sized
    to give BENCH_WINDOW_S of wall-time at that speed. Omit it to keep the
    legacy fixed step count. Returns the SWITCH offset set on success, else
    None. Caller restores orig_spo when done."""
    spo = orig_spo if (orig_spo or {}).get("SWITCH") is not None \
        else _read_spo(link)
    if not spo:
        return None                    # no geometry, no guess -- see _read_spo
    span = int(BENCH_WINDOW_S * 2 * freq) if freq else BENCH_WINDOW
    win = int(spo.get("L1A_on", 654)) + span
    # The selector must sit AFTER the camera, or the part is sorted before it
    # is announced -- a window that is not merely tight but backwards.
    cam_on = max((v for k, v in spo.items()
                  if k.startswith("CAM") and k.endswith("_on")), default=0)
    if win <= cam_on:
        return None
    link.send({"type": "set_setup", "stage_pulse_offset": {
        "SWITCH": win,
        "SEL1_on": win + 3,  "SEL1_off": win + 4,
        "SEL2_on": win + 13, "SEL2_off": win + 14,
        "SEL3_on": win + 23, "SEL3_off": win + 24}}, timeout=3.0)
    back = _read_spo(link) or {}
    return win if back.get("SWITCH") == win else None


ST_CAL, ST_SPINUP, ST_RECAL = 102, 103, 104
# States the machine passes THROUGH on its way somewhere. Waiting must never
# be satisfied by one of these: that was the bug in every wait here -- they
# asked "have we left X" when the question is "have we arrived at Y", and a
# board caught mid-sequence answered yes to the first and no to the second.
ST_TRANSIENT = (ST_INIT, ST_CAL, ST_SPINUP, ST_RECAL)


def _gate_of(link):
    """The gate's own rejection counters -- the evidence for or against
    'the gate dropped it'."""
    stat = link.send({"type": "get_running_stat"}, timeout=3.0) or {}
    return stat.get("gate") or {}


def _pump_until(link, targets, timeout=25.0, tid_floor=0):
    """Answer the calibration pulses while waiting for a settled state.

    INSPECTION_MODE_CAL fires phantom "sync" pulses and hands over only once
    CAM_SYNC is valid, and CAM_SYNC learns only from a report carrying a
    non-zero cam_ts (Link._with_cam_ts supplies it). Nothing here was
    answering, so CAL never finished -- and _wait_at_speed cannot be the thing
    that gets past it either: CAL stops the plate on purpose, so a poll for
    plate motion waits for what CAL is designed to prevent.

    `targets` is what counts as arrival. Transient states never do, however
    long they last, which is what makes this safe to call immediately after
    enter_insp_mode / clear_error -- before the board has even left IDLE.

    `tid_floor` refuses to answer anything at or below it. Announcements
    outlive the run that produced them -- a mode re-entry flushes the pipe but
    not the queue already holding their cam_trigs -- so answering everything
    means answering for objects that no longer exist, which the machine calls
    a tid desync. Callers that have just flushed pass the highest tid they saw
    while flushing.

    Returns (state, detail); state is None if the deadline passed.
    """
    t0 = time.time()
    answered = 0
    last = None
    while time.time() - t0 < timeout:
        for _, msg in link.drain_async():
            if msg.get("type") == "cam_trig" and msg.get("tid") is not None:
                if isinstance(msg["tid"], int) and msg["tid"] <= tid_floor:
                    continue          # flushed before we re-entered; not ours
                link.send_nowait({"type": "report", "tid": msg["tid"],
                                  "cat": CAT_NA})
                answered += 1
        st = _poll(link).get("state")
        last = st
        if st in targets and st not in ST_TRANSIENT:
            return st, f"{ST_NAME.get(st, st)} after {time.time()-t0:.1f}s, {answered} sync pulses answered"
        time.sleep(0.02)
    return None, (f"timeout after {timeout:.0f}s in {ST_NAME.get(last, last)}, "
                  f"{answered} sync pulses answered")


def _pump_cal(link, timeout=25.0):
    """Get to a settled state, whatever it is. Used by callers that only need
    the board to stop being mid-sequence."""
    return _pump_until(link, (ST_READY, ST_IDLE, ST_ERROR, ST_FATAL), timeout)


def _settle_cal(link, freq, cat=None):
    """Let the calibration objects finish their journey, and return the tid
    high-water mark that separates them from ours.

    INSPECTION_MODE_CAL fires its own sync pulses, and those are objects: they
    register, they walk to the camera, and they announce cam_trig -- some of
    them AFTER a test has started firing and counting. Two ways that bites:

      * counted as ours, so "fired=10" reads back "objects=21" (bench B.5);
      * answered as ours, and by then they have already passed SWITCH, so the
        report matches no object and the machine faults. That is what made
        `stress` die at the first rate it tried, 10/s, reporting a tid desync
        for a rate the pipeline handles comfortably -- 6x below where it
        actually gives.

    So: drain what has already announced, note the highest tid, and if the pipe
    still holds registered parts, wait out one camera transit answering them NA
    (they are not under test; an unanswered object is what the safety stop is
    for). Everything above the returned tid_base belongs to the caller.

    `freq` is plate_freq -- transit is CAM*_on / (2*freq), so this waits for
    arrival rather than for a fixed guess.
    """
    tid_base = 0
    for _, m in link.drain_async():
        if m.get("type") == "cam_trig" and isinstance(m.get("tid"), int):
            tid_base = max(tid_base, m["tid"])
    pipe = (link.send({"type": "get_running_stat"}, timeout=3.0) or {}).get("pipe") or {}
    if not (pipe.get("registered") or 0):
        return tid_base
    spo = ((link.send({"type": "get_setup"}, timeout=3.0) or {})
           .get("stage_pulse_offset") or {})
    cam_ticks = max((v for k, v in spo.items()
                     if k.startswith("CAM") and k.endswith("_on")), default=0)
    travel = (cam_ticks / (2.0 * freq)) if freq else 0.0
    # Wait for the pipe to be EMPTY, not for one transit to elapse.
    #
    # tid_base can only be raised by an announcement we actually saw, so a
    # calibration object still walking to the camera when this returns
    # announces later, carries a higher tid, and is counted as the caller's.
    # B.5 read "fired=10 objects=25" that way -- 50 announcements, exactly two
    # per object, so the property under test held perfectly while the count it
    # was compared against was somebody else's parts.
    #
    # Bounded, because on a loaded plate the pipe never empties (the plate
    # feeds 23-26 parts/s of its own at speed): the deadline is then the old
    # behaviour, and the caller is running a phantom suite on a loaded plate,
    # which UINSP_CAVEATS says not to do.
    t_end = time.time() + travel * 2.0 + 1.0
    while time.time() < t_end:
        for _, m in link.drain_async():
            if m.get("type") == "cam_trig" and isinstance(m.get("tid"), int):
                tid_base = max(tid_base, m["tid"])
                link.send_nowait({"type": "report", "tid": m["tid"],
                                  "cat": cat if cat is not None else CAT_NA})
        p = (link.send({"type": "get_running_stat"}, timeout=3.0) or {}).get("pipe") or {}
        if not (p.get("registered") or 0):
            break
        time.sleep(0.005)
    return tid_base


def _quiesce(link, freq, timeout=None):
    """Leave inspection mode and wait for the line to go quiet, DISCARDING what
    arrives rather than answering it.

    Announcements outlive the run that caused them. A rate step ends with parts
    still walking to the camera; they announce into the next step, where
    _pump_until answers them NA -- for objects the mode re-entry has already
    flushed. The machine calls that what it is, a report matching no object,
    and faults. In the ramp that showed up as a clean 15/s followed by an
    instant fault at 20/s: not a ceiling between the two, just the previous
    step's tail arriving.

    Discarding is the whole point. An answer is a claim about an object that
    exists; there is nothing here to make a claim about.

    Returns (frames dropped, highest tid seen) -- the tid is what a caller
    passes to _pump_until as tid_floor, so the announcements discarded here are
    not answered a moment later."""
    link.send({"type": "exit_insp_mode"}, timeout=3.0)
    hi_tid = 0
    if timeout is None:
        timeout = 3.0
    dropped, quiet_since = 0, time.time()
    deadline = time.time() + timeout
    while time.time() < deadline:
        got = link.drain_async()
        if got:
            for _, m in got:
                if m.get("type") == "cam_trig" and isinstance(m.get("tid"), int):
                    hi_tid = max(hi_tid, m["tid"])
            dropped += len(got)
            quiet_since = time.time()
        elif time.time() - quiet_since > 0.4:
            break
        time.sleep(0.01)
    return dropped, hi_tid


def _background_rate(link, seconds=3.0):
    """Objects the MACHINE detects on its own, with the host firing nothing.

    The plate carries real material; spinning it feeds real parts past the real
    sensor. At plate_freq 5960 that is ~16/s arriving unbidden, and a harness
    that assumes every object is one it fired reports 60 fired / 124 seen and
    calls the ratio 207%. Measure it, so `fired` and `seen` can be compared to
    something true.

    Requires the board already in READY. Returns detections/second."""
    g0 = _gate_of(link).get("accept") or 0
    t0 = time.time()
    while time.time() - t0 < seconds:
        for _, m in link.drain_async():
            if m.get("type") == "cam_trig" and m.get("tid") is not None:
                link.send_nowait({"type": "report", "tid": m["tid"],
                                  "cat": CAT_NA})
        time.sleep(0.002)
    dt = time.time() - t0
    g1 = _gate_of(link).get("accept") or 0
    return (g1 - g0) / dt if dt > 0 else 0.0


def _wait_at_speed(link, settle=0.15, tries=30):
    """Block until the plate is turning at a steady rate.

    newPulseEvent drops any pulse within 3.5 mm of the previous one, so
    phantoms fired while PLATE_FREQ_CURRENT is still ramping up from zero are silently
    rejected and never become objects. Poll SYS_STEP_COUNT until the
    per-interval delta stops climbing (the ramp has plateaued)."""
    # CAL comes first and stops the plate on purpose; get past it before
    # asking whether the plate is moving.
    _pump_cal(link)
    def ssc():
        return _poll(link).get("step_count")
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

    trig_phantom_pulse calls newPulseEvent() directly, bypassing the gate
    sensor entirely, so a bare board on USB can produce real objects with real
    tids and run them all the way to the selector outputs. The gate pin is
    INPUT_PULLUP with sense inverted, so an unconnected input reads as
    "no object" and contributes nothing.
    """
    print("\n\033[1m== Bench: tid round trip, no rig required ==\033[0m")
    print(f"  {count} phantom parts at plate_freq={freq}, {interval_ms}ms apart,"
          f" reported as cat={cat}\n")

    orig = link.send({"type": "get_setup"}, timeout=3.0) or {}
    orig_freq = orig.get("plate_freq")
    orig_spo = _read_spo(link) or {}
    base_counts, _ = _counts(link)
    # ERROR_HIST is a cumulative ring; clear it so the B.7/B.9/B.12 readouts
    # reflect only what this run produced, not stale faults from earlier runs.
    link.send({"type": "clear_error"}, timeout=2.0)
    link.send({"type": "clear_error_history"}, timeout=2.0)

    # Widen the selector window with firmware params so the round trip measures
    # "does the pipeline route tids correctly", not "can the host answer inside
    # 17 ms". Restored in the teardown below.
    # Blind the real sensor for the duration. This is a PHANTOM suite: with a
    # loaded plate the machine feeds 23-26 parts/s of its own at speed, and
    # every count here would be measuring those instead (UINSP_CAVEATS). The
    # firmware separates the two paths precisely so this is possible --
    # GATE_DISABLED stops the sensor and leaves trig_phantom_pulse working.
    # It is volatile and defaults false, so a reboot re-arms the sensor; it is
    # set here rather than left to whoever remembers.
    _gate_was = (link.send({"type": "set_gate_disable", "on": True},
                           timeout=3.0) or {})
    win = _widen_selector_window(link, orig_spo)
    rep.add("B.0", "widen the selector window for a bare-board round trip",
            win is not None,
            f"SWITCH {orig_spo.get('SWITCH')} -> {win}"
            f" ({BENCH_WINDOW} steps past the camera trigger)")

    r = link.send({"type": "set_setup", "plate_freq": freq}, timeout=3.0)
    rep.add("B.1", f"set plate_freq={freq}", bool(r), r)

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
    # Every object announces cam_trig twice (CAM1 cam=1, CAM2 cam=2) at the
    # same offset, so dedup by tid and report exactly once: a second report for
    # a tid whose object has already passed would itself desync the machine.
    # Answering on-announce (not after a fixed sleep) is what keeps the verdict
    # ahead of the part even before the window is widened.
    link.drain_async()
    fired, seen, reported = 0, [], []
    answered = set()

    # CAL's own sync pulses are objects too, and they are still walking to the
    # camera when this starts: they announce during the fire loop and used to
    # be counted as ours. B.5 then read "fired=10 objects=21" and called it a
    # failure of one-object-per-pulse. Everything above this tid belongs to us.
    tid_base = 0
    for _, m in link.drain_async():
        if m.get("type") == "cam_trig" and isinstance(m.get("tid"), int):
            tid_base = max(tid_base, m["tid"])
    _p = (link.send({"type": "get_running_stat"}, timeout=3.0) or {}).get("pipe") or {}
    cal_inflight = _p.get("registered") or 0

    def _pump():
        for _, msg in link.drain_async():
            if msg.get("type") != "cam_trig":
                continue
            if msg.get("sync"):
                # The machine's own calibration pulse, said so by the firmware
                # rather than guessed at from tid ordering. syncPulseService
                # keeps firing these in READY, so they arrive mixed in with the
                # parts under test and used to be counted as ours.
                if msg.get("tid") is not None:
                    link.send_nowait({"type": "report", "tid": msg["tid"],
                                      "cat": CAT_NA})
                continue
            tid = msg.get("tid")
            if isinstance(tid, int) and tid <= tid_base:
                continue                      # a calibration object, not ours
            # EVERY announcement, not just the first per tid: B.5's whole
            # claim is that one pulse produces one object announced TWICE
            # (CAM1 and CAM2), which needs the duplicates kept. `seen` was
            # never appended to at all -- only `reported` was -- so B.5 could
            # only ever print "objects=0 announcements=0" and no amount of
            # widening its wait was going to change that. It was not a slow
            # test, it was a test that never looked.
            seen.append(tid)
            if tid not in answered:
                link.send_nowait({"type": "report", "tid": tid, "cat": cat})
                answered.add(tid)
                reported.append(tid)

    # Through _read_spo, not a bare get_setup: a dropped reply left cam_ticks0
    # at its `default=0`, which collapsed both the calibration settle and B.5's
    # drain window to nothing and reported "fired=10 objects=0" for parts that
    # were simply still in transit. Measured at plate_freq 1000: predicted
    # 4.66s, actual 4.71s -- the formula is right, the read was not.
    spo_now = _read_spo(link) or {}
    cam_ticks0 = max((v for k, v in spo_now.items()
                      if k.startswith("CAM") and k.endswith("_on")), default=0)
    if not cam_ticks0:
        rep.add("B.5", "one object per pulse, announced twice (CAM1+CAM2)",
                False, "get_setup never returned stage_pulse_offset -- refusing "
                       "to time a journey whose length is unknown")
        return
    travel0 = (cam_ticks0 / (2.0 * freq)) if freq else 0.0
    if cal_inflight:
        # Let the calibration objects announce and be counted into tid_base,
        # rather than into ours.
        t_end = time.time() + travel0 * 1.4 + 0.3
        while time.time() < t_end:
            for _, m in link.drain_async():
                if m.get("type") == "cam_trig" and isinstance(m.get("tid"), int):
                    tid_base = max(tid_base, m["tid"])
                    link.send_nowait({"type": "report", "tid": m["tid"],
                                      "cat": CAT_NA})
            time.sleep(0.005)
    for i in range(count):
        link.send({"type": "trig_phantom_pulse"}, timeout=3.0)
        fired += 1
        deadline = time.time() + interval_ms / 1000.0
        while time.time() < deadline:      # answer ASAP, keep phantoms spaced
            _pump()
            time.sleep(0.002)

    # Wait for the last part to REACH THE CAMERA, not for a fixed half second.
    #
    # An object is announced when it arrives at the CAM stage, and that is a
    # journey: CAM1_on ticks at 2*plate_freq ticks/s. With this suite's own
    # defaults (CAM1_on 9315, plate_freq 1000) it takes 4.7s, and the old
    # deadline gave the whole run 2.7s -- so B.5 reported "fired=10 objects=0,
    # newPulseEvent rejected them", naming a gate that had rejected nothing
    # (gate.rej_dist = rej_rate = 0, pipe.registered = 7 parts in flight).
    #
    # It is worth being precise about what that cost: a test that says the
    # gate dropped the parts sends you to read the gate. The parts were fine
    # and on their way.
    cam_ticks = max((v for k, v in (spo_now or {}).items() if k.startswith("CAM")
                     and k.endswith("_on")), default=0)
    travel_s = (cam_ticks / (2.0 * freq)) if freq else 0.0
    drain_until = time.time() + max(0.5, travel_s * 1.4 + 0.5)
    while time.time() < drain_until:       # let the last parts announce/clear
        _pump()
        time.sleep(0.005)

    objs = sorted(set(seen))
    per = {t: seen.count(t) for t in objs}
    twice = bool(objs) and all(v == 2 for v in per.values())
    rep.add("B.5", "one object per pulse, announced twice (CAM1+CAM2)",
            len(objs) == fired and twice,
            f"fired={fired} objects={len(objs)} announcements={len(seen)}"
            + (f"  (gate rejects: rate={_gate_of(link).get('rej_rate')} "
               f"dist={_gate_of(link).get('rej_dist')} "
               f"busy={_gate_of(link).get('rej_busy')}; if all zero the parts "
               f"were accepted and simply had not reached CAM yet)"
               if len(objs) < fired
               else ("" if twice else f"  (not 2 per object: {per})")))

    if objs:
        gaps = [(a, b) for a, b in zip(objs, objs[1:]) if b != a + 1]
        rep.add("B.6", "tid strictly increasing by 1", not gaps,
                f"tid {objs[0]}..{objs[-1]}" + (f" gaps:{gaps}" if gaps else ""))

    st, stat = _state(link)
    rep.add("B.7", "no error state after a full reported run", st == ST_READY,
            f"state={st} ({ST_NAME.get(st, '?')}) "
            f"ERROR_HIST={_errors_of(stat)}")

    # The selector is further down the plate than the camera, so a part can be
    # reported and counted at CAM while still travelling to SEL. Reading the
    # counter the moment the last report goes out gave "SEL1: 0 -> 20
    # (reported 21)" -- an off-by-one that is really a not-there-yet.
    sel_ticks = max((v for k, v in (spo_now or {}).items()
                     if k.startswith("SEL") and k.endswith("_off")), default=0)
    sel_wait = ((sel_ticks / (2.0 * freq)) if freq else 0.0) * 1.4 + 0.4
    t_end = time.time() + sel_wait
    while time.time() < t_end:
        _pump()
        time.sleep(0.005)

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
    # Settled state first. With report_match_ts on -- the machine's real
    # setting -- RECAL runs periodically, and a bogus tid delivered mid-RECAL
    # is answered by a state machine that is somewhere else: B.9 read
    # "state=104" and called a missing fault a failure. This check is about
    # what the machine does with an unknown tid, not about catching it between
    # states.
    _pump_until(link, (ST_READY,), timeout=40.0)
    bogus = (max(seen) + 100000) if seen else 999999
    link.send_nowait({"type": "report", "tid": bogus, "cat": cat})
    time.sleep(0.5)
    st, stat = _state(link)
    rep.add("B.9", "unknown tid faults the machine", st == ST_ERROR,
            f"state={st} ({ST_NAME.get(st, '?')}) "
            f"ERROR_HIST={(stat or {}).get('error_hist')} "
            f"(expect INSP_RESULT_MATCHES_NO_OBJECT=1)")

    r = link.send({"type": "clear_error"}, timeout=3.0)
    # Clearing the error hands the board back to inspection mode, which starts
    # at CAL -- and CAL only finishes if somebody answers its sync pulses. Read
    # the state before that and the answer is 102 every time, which is what
    # this check used to report as "clear_error does not recover".
    _pump_until(link, (ST_READY, ST_IDLE, ST_ERROR))
    st, _ = _state(link)
    rep.add("B.10", "clear_error recovers", st in (ST_IDLE, ST_READY),
            f"state={st} ({ST_NAME.get(st, '?')})")

    # --- negative: a part that never gets a verdict -----------------------
    # This is stage 0.7 without needing anyone to block a gate by hand, and it
    # is the path that used to call pinMode/digitalWrite from inside the ISR:
    # a regression shows up as a hang or reboot rather than a clean fault.
    print("\n  Negative check: a part with no verdict at all (ISR error path,")
    print("  commit 535d92fb). A hang or reboot here is the regression.")
    # How many unjudged parts it takes. UNANSWERED_STOP_AFTER is configurable
    # and this machine runs it at 10, so firing a single phantom and expecting
    # OBJECT_HAS_NO_INSP_RESULT tested a policy the board stopped having. Read
    # it and fire enough to cross the threshold.
    _setup_now = link.send({"type": "get_setup"}, timeout=3.0) or {}
    stop_after = _setup_now.get("unanswered_stop_after")
    if not isinstance(stop_after, int) or stop_after < 1:
        stop_after = 1
    link.send({"type": "enter_insp_mode"}, timeout=3.0)
    # CAL first, and CAL shuts the gate: a phantom fired there is rejected, so
    # it never becomes an object, never goes unjudged, and the fault this check
    # is named after cannot happen. Get to READY before firing.
    cal_st, cal_detail = _pump_until(link, (ST_READY, ST_ERROR))
    time.sleep(0.3)
    link.drain_async()
    for _ in range(stop_after + 1):
        link.send({"type": "trig_phantom_pulse"}, timeout=3.0)
        time.sleep(0.12)
    # They fault at SWITCH, which is the far end of the journey -- wait for the
    # last one to get there rather than for a fixed two seconds.
    sw = (spo_now or {}).get("SWITCH") or 0
    time.sleep(min(30.0, ((sw / (2.0 * freq)) if freq else 0.0) * 1.4 + 2.0))

    st, stat = _state(link)
    rep.add("B.11", "board still answers after the ISR error path",
            st is not None, "no reply = hang/reboot = regression")
    cal_ok = cal_st == ST_READY
    rep.add("B.12", "unjudged part faults cleanly",
            (st == ST_ERROR) if cal_ok else None,
            (f"state={st} ({ST_NAME.get(st, '?')}) "
             f"ERROR_HIST={(stat or {}).get('error_hist')} "
             f"(expect OBJECT_HAS_NO_INSP_RESULT=2 after "
             f"{stop_after} unjudged parts)") if cal_ok else
            (f"inconclusive: never left CAL ({cal_detail}) -- the phantom "
             f"would have been gate-rejected, so a missing fault says nothing"))

    # --- restore ----------------------------------------------------------
    link.send({"type": "clear_error"}, timeout=3.0)
    link.send({"type": "exit_insp_mode"}, timeout=3.0)
    if orig_spo:
        link.send({"type": "set_setup", "stage_pulse_offset": orig_spo},
                  timeout=3.0)
    if isinstance(orig_freq, (int, float)):
        link.send({"type": "set_setup", "plate_freq": orig_freq}, timeout=3.0)
    chk = ((link.send({"type": "get_setup"}, timeout=3.0) or {})
           .get("stage_pulse_offset") or {})
    st, _ = _state(link)
    restored = (orig_spo.get("SWITCH") is None
                or chk.get("SWITCH") == orig_spo.get("SWITCH"))
    link.send({"type": "set_gate_disable", "on": False}, timeout=3.0)
    rep.add("B.13", "returned to IDLE, window + plate_freq restored",
            st == ST_IDLE and restored,
            f"state={st} SWITCH={chk.get('SWITCH')} plate_freq={orig_freq}")


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
#   Every object announces cam_trig for CAM1 *and* CAM2 -> 2 messages/part
#   115200 8N1 ~= 11.5 kB/s; a cam_trig frame is ~90 B
PIPE_INFO_LEN = 100
COMM_Q_DEPTH = 20


def _errors_of(stat):
    return [e for e in (stat or {}).get("error_hist", []) if e not in (0, -1)]


def profile(link, rep, start_hz, max_hz, step_hz, parts_per_point,
            max_seconds, drop_frac, cat, plate_sweep=None):
    """Measure skip rate against object rate, and find where throughput peaks.

    The question this exists to answer: auto-rate drives the machine to a
    FIXED skip rate (0.476% with the shipped constants -- see UINSP_CAVEATS),
    but good throughput is r*(1-p(r)) and its maximum sits where
    dp/dr = (1-p)/r. Those two coincide only by accident. Nobody has measured
    p(r) on this machine, so nobody knows what the fixed setpoint costs.

    Two shapes are possible and they need opposite controllers:

      interior peak   p rises smoothly, r*(1-p) has a maximum, and a slow
                      hill-climb is the right control law.
      cliff           p is ~0 until a hard limit (the camera's 35-36Hz
                      ceiling, or the answer window) and then jumps. Then
                      r*(1-p) is monotonic up to the edge, there is no
                      interior maximum, and the right law is "measure the
                      edge, sit a margin below it" -- which is what auto-rate
                      already does, except its setpoint is arithmetic rather
                      than measurement.

    AUTO-RATE IS FROZEN FOR THE SWEEP (skip_policy -> stop_only, restored
    after). Leaving it on would let the controller move the rate while the
    rate is being measured, and the resulting p belongs to no r at all.

    Statistics set the clock. Estimating p to within +-e relative needs about
    (1-p)/(p*e^2) parts: at p=0.5% that is ~5000 parts for +-20%, ~2200 for
    +-30%. Each point prints the precision it actually achieved, because a
    curve drawn from 200-part samples is noise with a line through it.
    """
    print("\n\033[1m== Throughput profile: skip rate vs object rate ==\033[0m")

    orig = link.send({"type": "get_setup"}, timeout=3.0) or {}
    orig_freq = orig.get("plate_freq")
    orig_sep  = orig.get("min_detect_sep_us")
    orig_spo  = _read_spo(link)
    orig_pol  = (orig.get("skip_policy") or {}).get("mode")
    if not orig_spo:
        rep.add("P.0", "read the station geometry", False,
                "get_setup never returned stage_pulse_offset")
        return

    need_freq = int(91 * max_hz / 2.0 * 1.5) + 500
    sep_us = max(200, int(1e6 / max_hz / 3))
    orig_gate = dict(orig.get("gate") or {})
    _widen_selector_window(link, orig_spo, freq=need_freq)
    # Open every gate filter for the sweep.
    #
    # The point of the profile is where the PIPELINE gives, and each filter in
    # front of it censors the sample differently: the width window drops parts
    # in total silence, the distance and rate gates drop them at a threshold of
    # their own choosing. Leaving them set means measuring the filters and
    # calling it the machine. They are restored at the end, and the counters
    # below say how many parts each one would have taken.
    link.send({"type": "set_setup", "plate_freq": need_freq,
               "gate": {"min_detect_sep_us": sep_us,
                        "min_detect_dist_um": 0,
                        "pulse_min_width": 0,
                        "pulse_max_width": 1000000}}, timeout=3.0)
    # Freeze the controller; keep the safety stop.
    link.send({"type": "set_setup", "skip_policy": {"mode": "stop_only"}},
              timeout=3.0)
    print(f"  plate_freq={need_freq} min_detect_sep_us={sep_us} "
          f"auto-rate FROZEN (was {orig_pol})")
    if drop_frac:
        print(f"  leaving {drop_frac*100:.1f}% of parts unanswered on purpose "
              f"-- the dry-run mode. It manufactures skips the way the machine "
              f"really makes them (reporting tid N sweeps every older UNSET "
              f"object to SKIP), so the instrument can be checked without a "
              f"camera. Answering LATE instead does not work: past the window "
              f"the report matches no object and the machine faults on tid "
              f"desync, which is a different failure wearing the same clothes.")

    link.send({"type": "enter_insp_mode"}, timeout=3.0)
    st, _ = _pump_until(link, (ST_READY,), timeout=60.0)
    rep.add("P.0", "reached READY with the controller frozen", st == ST_READY,
            f"plate_freq={need_freq}")
    if st != ST_READY:
        return

    print(f"\n  {'asked':>6} {'admitted':>9} {'judged':>8} {'p_lost':>8} "
          f"{'p_skip':>8} {'unans':>7} {'+-':>6} {'good/s':>8}  note")
    print("  " + "-" * 84)

    # Two ways to vary the rate, and only one of them is how the machine works.
    #
    # phantom  fire injected pulses at a chosen rate. Repeatable, needs no
    #          material, and bypasses GateSensing -- so it measures the
    #          pipeline, never the gate.
    # plate    spin the plate faster and let the real parts arrive as fast as
    #          the geometry carries them past the sensor. You do not SET the
    #          rate here, you set the speed and MEASURE the rate -- and it is
    #          the only mode where the end-to-end ratio means anything, because
    #          a part has to pass the sensor to be counted at `edges`.
    rows = []
    sweep = list(plate_sweep) if plate_sweep else None
    points = list(sweep) if sweep else None
    hz = start_hz
    while (points if sweep else hz <= max_hz):
        # NOTE: the speed is changed AFTER the pipe is flushed, never before.
        # Setting it here would move the plate while the previous point's parts
        # are still walking to their stages, and their announcements then land
        # in a pipeline that has been re-entered -- reported as tid desync,
        # which is what killed the first plate sweep at its second point.
        freq_here = points.pop(0) if sweep else need_freq
        _, floor = _quiesce(link, freq_here, timeout=6.0)
        link.send({"type": "clear_error"}, timeout=2.0)
        link.send({"type": "clear_error_history"}, timeout=2.0)
        link.send({"type": "set_setup", "plate_freq": freq_here}, timeout=3.0)
        link.send({"type": "enter_insp_mode"}, timeout=3.0)
        if _pump_until(link, (ST_READY,), timeout=60.0,
                       tid_floor=floor)[0] != ST_READY:
            print("  never reached READY"); break
        tb = _settle_cal(link, freq_here, cat=cat)
        link.send({"type": "reset_running_stat"}, timeout=2.0)
        link.drain_async()

        g0 = _gate_of(link)
        c0 = (link.send({"type": "get_running_stat"}, timeout=3.0) or {}).get("count") or {}
        t0 = time.time(); i = 0; period = 1.0 / hz
        rng = random.Random(1234 + int(hz))
        fired = 0
        while True:
            now = time.time()
            if (now - t0) >= max_seconds:
                break
            while time.time() < t0 + i * period:
                for _, m in link.drain_async():
                    if m.get("type") != "cam_trig" or m.get("sync"):
                        continue
                    tid = m.get("tid")
                    if not isinstance(tid, int) or tid <= tb:
                        continue
                    if drop_frac and rng.random() < drop_frac:
                        continue          # deliberately unanswered -> SKIP
                    link.send_nowait({"type": "report", "tid": tid, "cat": cat})
                time.sleep(0.002)
            if not sweep:
                link.send_nowait({"type": "trig_phantom_pulse"}); fired += 1
            i += 1
            if i % 50 == 0:
                g = _gate_of(link)
                if ((g.get("accept") or 0) - (g0.get("accept") or 0)) >= parts_per_point:
                    break
        span = time.time() - t0
        # let the tail land before reading the counters
        t_end = time.time() + 1.5
        while time.time() < t_end:
            for _, m in link.drain_async():
                if m.get("type") == "cam_trig" and not m.get("sync") \
                   and isinstance(m.get("tid"), int) and m["tid"] > tb:
                    if drop_frac and rng.random() < drop_frac:
                        continue
                    link.send_nowait({"type": "report", "tid": m["tid"], "cat": cat})
            time.sleep(0.005)

        g1 = _gate_of(link)
        dg = lambda k: (g1.get(k) or 0) - (g0.get(k) or 0)
        st1 = link.send({"type": "get_running_stat"}, timeout=3.0) or {}
        c1 = st1.get("count") or {}
        adm  = (g1.get("accept") or 0) - (g0.get("accept") or 0)
        skip = (c1.get("SKIP") or 0) - (c0.get("SKIP") or 0)
        judged = sum((c1.get(k) or 0) - (c0.get(k) or 0)
                     for k in ("SEL1", "SEL2", "SEL3", "NA"))
        unans = (c1.get("UNANSWERED") or 0) - (c0.get("UNANSWERED") or 0)
        # TWO different rates, and they are not the same number.
        #
        # p_lost is the throughput cost: every admitted part that reached the
        # selector without a verdict, however the firmware labelled it.
        # p_skip is what auto-rate actually reacts to -- autoRateBackoff() is
        # called from the SKIP case only. A part left UNSET (nobody ever spoke
        # for it) increments UNANSWERED_Count and the controller never sees it,
        # so a machine can be losing parts steadily while auto-rate reads a
        # clean 0% and holds full speed. The dry run showed exactly that: 3% of
        # parts dropped, SKIP_Count 0.
        p = ((adm - judged) / adm) if adm else 0.0
        p_skip = (skip / adm) if adm else 0.0
        # relative standard error of a proportion
        se = (math.sqrt(p * (1 - p) / adm) / p) if (adm and p > 0) else float("nan")
        good = judged / span if span else 0.0
        errs = _errors_of(st1)
        note = ""
        if errs:
            note = "FAULT " + ",".join(ERR_NAME.get(e, str(e)) for e in sorted(set(errs)))
        elif adm < parts_per_point * 0.5:
            note = "thin sample"
        label = ("f%d" % freq_here) if sweep else ("%.0f/s" % hz)
        print(f"  {label:>6} {adm:>9} {judged:>8} {p*100:>7.2f}% "
              f"{p_skip*100:>7.2f}% {unans:>7} "
              f"{('%.0f%%' % (se*100)) if se == se else '   -':>6} {good:>8.1f}  {note}")
        rows.append(dict(hz=(adm/span if (sweep and span) else hz),
                         freq=freq_here, admitted=adm, judged=judged, skipped=skip,
                         unans=unans, p=p, p_skip=p_skip, se=se, good=good,
                         span=span, errs=errs,
                         edges=dg("edges"), rej_width=dg("rej_width"),
                         rej_unstable=dg("rej_unstable"),
                         rej_blocked=dg("rej_blocked"), rej_dist=dg("rej_dist"),
                         rej_rate=dg("rej_rate"), rej_busy=dg("rej_busy")))
        if errs:
            break
        if not sweep:
            hz += step_hz

    # --- restore before saying anything about the numbers ---
    _quiesce(link, need_freq, timeout=4.0)
    link.send({"type": "set_setup", "plate_freq": 0,
               "gate": orig_gate,
               "stage_pulse_offset": orig_spo}, timeout=3.0)
    if orig_pol:
        link.send({"type": "set_setup", "skip_policy": {"mode": orig_pol}}, timeout=3.0)
    link.send({"type": "clear_error"}, timeout=2.0)
    print(f"\n  restored plate_freq={orig_freq} min_detect_sep_us={orig_sep} "
          f"skip_policy={orig_pol}")

    if not rows:
        rep.add("P.1", "measured a throughput curve", False, "no points completed")
        return

    # The whole ladder, sensor edge -> verdict. `accept` alone cannot be traced
    # back to what the sensor saw, which is what these counters are for.
    print(f"\n  {'asked':>6} {'edges':>7} {'width':>6} {'unstab':>7} {'block':>6} "
          f"{'dist':>6} {'rate':>6} {'busy':>5} {'admit':>7} {'judged':>7} {'end2end':>8}")
    print("  " + "-" * 84)
    # end-to-end only means something when the parts came through the SENSOR.
    # trig_phantom_pulse calls newPulseEvent directly and never touches
    # GateSensing, so in a phantom run `edges` and `admitted` count different
    # populations and their ratio is arithmetic, not a measurement.
    phantom_run = any(r["admitted"] > r["edges"] for r in rows)
    for r in rows:
        e2e = ("%6.1f%%" % (r["judged"] / r["edges"] * 100)) if (
            r["edges"] and not phantom_run) else "     --"
        lbl = ("f%d" % r["freq"]) if r.get("freq") and sweep else ("%.0f/s" % r["hz"])
        print(f"  {lbl:>6} {r['edges']:>7} {r['rej_width']:>6} "
              f"{r['rej_unstable']:>7} {r['rej_blocked']:>6} {r['rej_dist']:>6} "
              f"{r['rej_rate']:>6} {r['rej_busy']:>5} {r['admitted']:>7} "
              f"{r['judged']:>7} {e2e:>8}")
    if phantom_run:
        print("  end2end is blank: phantoms bypass GateSensing, so `edges` and "
              "`admitted`\n  are different populations. It fills in on a real "
              "run, fed by the sensor.")

    best = max(rows, key=lambda r: r["good"])
    rep.add("P.1", "measured a throughput curve", True,
            f"{len(rows)} points, {sum(r['admitted'] for r in rows)} parts total")

    # Is the peak interior, or is it the last point we dared to try?
    interior = best is not rows[-1] and best is not rows[0]
    rep.add("P.2", "peak throughput", None,
            f"{best['good']:.1f} good/s at "
            f"{('plate_freq %d' % best['freq']) if sweep else ('%.0f/s asked' % best['hz'])} "
            f"({best['hz']:.1f} parts/s measured) "
            f"({best['admitted']} admitted, p={best['p']*100:.2f}%)"
            + ("  -- INTERIOR peak: a hill-climb controller is the right shape"
               if interior else
               "  -- peak is at the edge of the sweep: either raise --max-hz, "
               "or this is a cliff and the law should be 'measure the edge, "
               "sit below it'"))

    # What the shipped controller would settle on, against what the curve says.
    tgt = 0.004762
    below = [r for r in rows if r["p_skip"] <= tgt]
    if below:
        at_setpoint = max(below, key=lambda r: r["hz"])
        loss = (1 - at_setpoint["good"] / best["good"]) * 100 if best["good"] else 0
        rep.add("P.3", "cost of the shipped auto-rate setpoint", None,
                f"auto-rate targets p<={tgt*100:.3f}%, which here is "
                f"{at_setpoint['hz']:.1f} parts/s "
                f"= {at_setpoint['good']:.1f} good/s, {loss:.0f}% below the peak")
    else:
        rep.add("P.3", "cost of the shipped auto-rate setpoint", None,
                f"no swept point reached p_skip<={tgt*100:.3f}% -- auto-rate "
                f"would ratchet to its floor here; start the sweep lower")
    blind = [r for r in rows if r["p"] > 0.01 and r["p_skip"] < 0.001]
    if blind:
        rep.add("P.5", "parts lost where auto-rate cannot see them", None,
                "at " + ", ".join(f"{r['hz']:.1f}/s ({r['p']*100:.1f}% lost, "
                                  f"{r['p_skip']*100:.2f}% skip)" for r in blind[:4])
                + " -- unjudged as UNSET, not SKIP, so the controller reads "
                  "clean and holds speed")

    thin = [r for r in rows if r["se"] == r["se"] and r["se"] > 0.30]
    if thin:
        rep.add("P.4", "sample precision", None,
                f"{len(thin)} of {len(rows)} points worse than +-30% on p "
                f"(raise --parts): " + ", ".join(f"{r['hz']}/s" for r in thin[:6]))


def stress(link, rep, start_hz, max_hz, step_hz, dwell, cat, do_report):
    """Ramp the phantom-object rate until the pipeline gives, and say what gave.

    The firmware rate-limits new objects two ways, both of which have to be
    opened up first or the ramp measures the limiter instead of the pipeline:
    a minimum time separation (SYS_MIN_PULSE_TIME_SEP_us, default ~67ms = 15/s)
    and a minimum travel distance (3.5mm, ~91 pulses) whose wall-clock cost
    depends on plate_freq.
    """
    print("\n\033[1m== Pipeline stress ==\033[0m")

    orig = link.send({"type": "get_setup"}, timeout=3.0) or {}
    orig_freq = orig.get("plate_freq")
    orig_sep = orig.get("min_detect_sep_us")
    orig_spo = _read_spo(link)
    if not orig_spo:
        rep.add("S.0", "read the station geometry before touching it", False,
                "get_setup never returned stage_pulse_offset -- refusing to "
                "run on a guessed geometry (which would then be 'restored' "
                "over the real one afterwards)")
        return

    # plate_freq needed so the 3.5mm distance gate clears fast enough for the
    # top rate we intend to ask for. ISR ticks at 2*plate_freq.
    need_freq = int(91 * max_hz / 2.0 * 1.5) + 500
    sep_us = max(200, int(1e6 / max_hz / 3))

    # Widen the selector window (see _widen_selector_window). Without this the
    # ramp measures how fast the host can answer inside the ~43-step gate, not
    # the pipeline's real ceiling; the first late verdict faults the machine and
    # the ramp stops at rate 1. Sized against need_freq, not as a step count:
    # the ramp raises plate_freq precisely to open the distance gate, and a
    # fixed step count would hand back the time it just bought. Restored in the
    # teardown below.
    _widen_selector_window(link, orig_spo, freq=need_freq)

    print(f"  opening the rate limiters: plate_freq={need_freq} "
          f"min_detect_sep_us={sep_us} "
          f"answer window {BENCH_WINDOW_S*1000:.0f}ms "
          f"({int(BENCH_WINDOW_S*2*need_freq)} steps)")
    print(f"  (defaults {orig_freq} / {orig_sep} cap objects at "
          f"{1e6/orig_sep:.1f}/s)" if orig_sep else "")

    link.send({"type": "set_setup", "plate_freq": need_freq,
               "min_detect_sep_us": sep_us}, timeout=3.0)
    link.send({"type": "enter_insp_mode"}, timeout=3.0)
    _wait_at_speed(link)

    st, _ = _state(link)
    if st != ST_READY:
        rep.add("S.0", "reached READY before ramping", False,
                f"state={st} ({ST_NAME.get(st, '?')})")
        return
    rep.add("S.0", "reached READY before ramping", True, f"plate_freq={need_freq}")

    # How long a part takes to reach the camera at the ramp's plate speed --
    # every "did it arrive?" wait below is derived from this, not guessed.
    _spo = _read_spo(link) or {}
    _cam_on = max((v for k, v in _spo.items()
                   if k.startswith("CAM") and k.endswith("_on")), default=0)
    tail_transit = (_cam_on / (2.0 * need_freq)) if need_freq else 0.0
    print(f"  camera transit {tail_transit:.2f}s "
          f"(CAM_on {_cam_on} at plate_freq {need_freq})")

    # What the plate delivers on its own. Real material on a spinning plate is
    # real objects; without this number "fired 60, seen 124" looks like the
    # firmware inventing parts.
    bg_hz = _background_rate(link)
    print(f"  plate delivers {bg_hz:.1f} obj/s on its own "
          f"({'real material on the plate' if bg_hz > 1 else 'plate is empty'})"
          f" -- counted into the expected total below")

    print(f"\n  {'rate':>6} {'fired':>7} {'admitted':>9} {'seen':>7} "
          f"{'ratio':>7} {'maxQs':>6}  result")
    print("  " + "-" * 72)

    results = []
    best = 0
    broke_at = None
    broke_why = ""

    hz = start_hz
    while hz <= max_hz:
        # Flush the previous rate's in-flight parts before touching anything --
        # see _quiesce. Their announcements would otherwise be answered here,
        # for objects the mode re-entry has already dropped.
        _quiesce(link, need_freq, timeout=tail_transit * 1.5 + 1.0)
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
        # CAL's sync pulses are objects too, and they are still walking to the
        # camera when this rate starts. Answering one of them here is a report
        # for a part that already passed SWITCH -- an instant tid desync that
        # used to end the whole ramp at the first rate tried. Everything above
        # tid_base is ours.
        tid_base = _settle_cal(link, need_freq, cat=cat)
        link.drain_async()

        period = 1.0 / hz
        n = max(1, int(dwell * hz))
        # The gate publishes exactly how many objects it ADMITTED. That is the
        # only honest denominator: what we fired is not it (the gate rejects on
        # purpose, and rejecting is the machine working, not the pipeline
        # failing), and neither is fired+background (an estimate). Measuring
        # against `fired` printed "0/s clean" for a ramp that survived every
        # rate to 80/s without a single fault.
        acc0 = _gate_of(link).get("accept") or 0
        fired = 0
        seen = set()          # unique object tids (each announces twice)
        reported = set()      # tids answered once -- a 2nd report desyncs
        qs_max = 0
        t0 = time.time()

        for i in range(n):
            deadline = t0 + i * period
            # Drain continuously while pacing to the deadline: a cam_trig left
            # sitting until the next fire would be answered up to a full period
            # late and miss the selector even with the window widened.
            while time.time() < deadline:
                for _, msg in link.drain_async():
                    if msg.get("type") == "cam_trig":
                        tid = msg.get("tid")
                        if isinstance(tid, int) and tid <= tid_base:
                            continue          # a calibration object, not ours
                        seen.add(tid)
                        qs_max = max(qs_max, msg.get("Qs", 0) or 0)
                        if do_report and tid not in reported:
                            reported.add(tid)
                            link.send_nowait({"type": "report", "tid": tid,
                                              "cat": cat})
                time.sleep(0.002)
            # No round trip: waiting on the ack would itself become the limit.
            link.send_nowait({"type": "trig_phantom_pulse"})
            fired += 1

        # Let the tail drain -- finely (2ms), the same cadence as the fire
        # loop. A 50ms poll here can leave the last part's verdict later than
        # its ~window-sized runway to the selector and fault an otherwise clean
        # run on the final object alone.
        # ... and long enough for the LAST part fired to reach the camera.
        # A flat 1.5s is a guess about a journey whose length is published:
        # CAM*_on / (2*plate_freq). At this suite's geometry that is 0.8s, but
        # at a slower plate it is seconds, and every part still in transit when
        # the window closed counted as "dropped at the rate gate".
        t_drain = time.time() + max(1.5, tail_transit * 1.4 + 0.5)
        while time.time() < t_drain:
            for _, msg in link.drain_async():
                if msg.get("type") == "cam_trig":
                    tid = msg.get("tid")
                    if isinstance(tid, int) and tid <= tid_base:
                        continue
                    seen.add(tid)
                    qs_max = max(qs_max, msg.get("Qs", 0) or 0)
                    if do_report and tid not in reported:
                        reported.add(tid)
                        link.send_nowait({"type": "report", "tid": tid, "cat": cat})
            time.sleep(0.002)

        st, stat = _state(link)
        errs = _errors_of(stat)
        g = _gate_of(link)
        admitted = max(0, (g.get("accept") or 0) - acc0)
        ratio = len(seen) / admitted if admitted else 0.0

        if st == ST_ERROR or errs:
            why = ", ".join(ERR_NAME.get(e, f"code {e}") for e in sorted(set(errs)))
            verdict = f"\033[31mFAULT\033[0m {why}"
            broke_at, broke_why = hz, why
        elif ratio < 0.98:
            verdict = (f"\033[33mlost {admitted - len(seen)} of {admitted} "
                       f"admitted\033[0m")
        else:
            verdict = "\033[32mok\033[0m"
            best = hz

        print(f"  {hz:>5}/s {fired:>7} {admitted:>9} {len(seen):>7} "
              f"{ratio:>6.0%} {qs_max:>6}  {verdict}")
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
                  f" object announces cam_trig twice (CAM1 and CAM2).")
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
        restore["plate_freq"] = orig_freq
    if isinstance(orig_sep, (int, float)):
        restore["min_detect_sep_us"] = orig_sep
    if orig_spo:
        restore["stage_pulse_offset"] = orig_spo
    link.send(restore, timeout=3.0)
    print(f"\n  restored plate_freq={orig_freq} min_detect_sep_us={orig_sep}")


def stall(link, rep, hz, stall_s, cat):
    """Stop answering mid-run and confirm the machine degrades the way it
    should: it must fault (OBJECT_HAS_NO_INSP_RESULT), not sort parts by stale
    or guessed verdicts."""
    print("\n\033[1m== Host stall ==\033[0m")
    print(f"  Reporting normally, then going silent for {stall_s}s.")
    print("  Expect OBJECT_HAS_NO_INSP_RESULT(2) -- a part reaching the")
    print("  selector with no verdict must stop the line, not guess.\n")

    orig = link.send({"type": "get_setup"}, timeout=3.0) or {}
    orig_spo = _read_spo(link) or {}
    # Widen the selector window so the host can answer inside it while healthy.
    # The silent phase still faults -- those parts get no verdict at all, window
    # or no window -- which is exactly what T.2 is checking for.
    _widen_selector_window(link, orig_spo)
    link.send({"type": "set_setup", "plate_freq": 1000}, timeout=3.0)
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
            if msg.get("type") == "cam_trig":
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
        link.send_nowait({"type": "trig_phantom_pulse"})
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
        link.send_nowait({"type": "trig_phantom_pulse"})
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


# --- chaos: randomized rate + plate speed + offset churn ------------------

def chaos(link, rep, seconds, min_hz, max_hz, seed, persist=False,
          verify=False, burst=False, burst_every=5.0, burst_count=8,
          report_delay_ms=0, report_shuffle=False, expect_fault=False):
    """Adversarial randomized stress. Fire objects at a rate that jitters every
    pulse across [min_hz, max_hz], while under the run we ALSO randomly, and
    concurrently: change the plate speed (mid-stream ramps); shove the
    selector-offset window around (hammers the double-buffered publish path
    with objects in flight); move the min_detect_sep_us rate gate; flood the
    link with read commands (PING/get_setup/get_running_stat) to contend for
    serial bandwidth; and churn the SEL1 batch countdown.

    With --persist-churn it also attempts NVS saves mid-run and asserts the
    firmware REFUSES them (C.5): a flash write with the timer ISR live is
    unsafe (onTimer is IRAM but its callees are not), so the firmware only
    permits a save with the plate stopped. Because the guard blocks the write,
    this costs no flash -- it verifies the guard, it does not exercise the
    hazard.

    With --verify-timing it periodically quiesces, fires ONE object at the
    current (churned) offset, and dumps io_trace to confirm SWITCH/SEL landed on
    that offset (C.6) -- the edges the publish path reads at dispatch. That
    turns a persistent torn/stale offset read from invisible (still judged, tid
    still contiguous, no fault) into a hard failure. It cannot see a purely
    transient torn read during the write instant -- that needs a firmware-side
    assertion -- but it catches a publish path that leaves a wrong value behind.

    Nothing here should fault, desync the tid sequence, overflow the queue, or
    stop answering -- the machine must simply survive the churn. Only NEW
    objects pick up a changed offset (a task's target is fixed when the object
    registers), so an in-flight part never has its window yanked out from under
    it; that is what makes the churn safe to demand zero faults from.
    """
    import random
    rng = random.Random(seed)
    print("\n\033[1m== Chaos: randomized rate + plate speed + offset churn =="
          "\033[0m")
    print(f"  {seconds}s, {min_hz}-{max_hz} obj/s, seed {seed}\n")

    # Capture the baseline to restore later. Retry until the fields we must put
    # back are all present -- a single partial read would drop them from the
    # teardown and leave the board on a stray churn value.
    orig = {}
    for _ in range(5):
        orig = link.send({"type": "get_setup"}, timeout=3.0) or {}
        if all(k in orig for k in ("plate_freq", "min_detect_sep_us",
                                   "stage_pulse_offset")):
            break
    orig_freq = orig.get("plate_freq")
    orig_sep = orig.get("min_detect_sep_us")
    orig_spo = _read_spo(link) or {}
    orig_nvs = bool(orig.get("cfg_from_nvs"))
    # No default station. Every offset below is built from l1a, so a guessed
    # l1a builds a guessed machine: with the real L1A_on at 9314, the 654
    # fallback put SWITCH ~6200 -- BEFORE the camera at 9315. Parts are then
    # sorted before they are announced, which the firmware correctly refuses to
    # do, and the churn is blamed for the fault it did not cause.
    if orig_spo.get("L1A_on") is None:
        rep.add("C.0", "read the station geometry before churning it", False,
                "get_setup never returned stage_pulse_offset")
        return
    l1a = int(orig_spo["L1A_on"])

    # min_detect_sep_us must stay below 1e6/max_hz or it caps the rate below
    # the target; jitter it within a band that always admits max_hz.
    sep_lo, sep_hi = 500, max(700, int(1e6 / max_hz / 2))
    sep_us = max(500, int(1e6 / max_hz / 3))   # setup value; let max_hz through
    FLOOD = ("ping", "get_setup", "get_running_stat")

    # plate_freq floor so max_hz still clears the 91-step (3.5mm) distance gate:
    # the wall gap 1/hz must exceed 91/(2*freq), i.e. freq > 91*hz/2. Keep the
    # random speed band above that floor so acceptance never starves the rate.
    freq_floor = int(91 * max_hz / 2 * 1.3) + 500
    freq_ceil = freq_floor * 2

    # The window (SWITCH offset past the camera trigger) is in plate steps, but
    # the host's answer budget is wall-time = steps/(2*plate_freq). plate_freq
    # ranges up to freq_ceil here, so size the window off the CEILING to keep a
    # comfortable ~300ms budget even at top speed -- a 600-step window that is
    # fine at plate_freq 1000 shrinks to ~100ms at these rates and drops parts.
    win_lo = int(0.30 * 2 * freq_ceil)     # ~300ms of runway at the ceiling
    win_hi = int(0.45 * 2 * freq_ceil)

    def _win(base):
        return {"SWITCH": l1a + base,
                "SEL1_on": l1a + base + 3,  "SEL1_off": l1a + base + 4,
                "SEL2_on": l1a + base + 13, "SEL2_off": l1a + base + 14,
                "SEL3_on": l1a + base + 23, "SEL3_off": l1a + base + 24}

    def _enter_ready():
        """Put the board into READY at the churn baseline. Idempotent, so it
        both starts the run and re-establishes state after a reconnect (a USB
        drop reboots the board back to IDLE)."""
        link.send({"type": "clear_error"}, timeout=2.0)
        link.send({"type": "clear_error_history"}, timeout=2.0)
        link.send({"type": "set_setup", "plate_freq": freq_floor,
                   "min_detect_sep_us": sep_us,
                   "stage_pulse_offset": _win(win_lo)}, timeout=3.0)
        link.send({"type": "enter_insp_mode"}, timeout=3.0)
        _wait_at_speed(link)
        return _state(link)[0]

    try:
        try:
            st = _enter_ready()
        except LinkReset:
            st = _enter_ready()      # dropped during setup; board rebooted, retry
        except LinkDead:
            st = None
        rep.add("C.0", "reached READY with the rate limiters opened",
                st == ST_READY, f"plate_freq={freq_floor}..{freq_ceil} "
                f"min_detect_sep_us={sep_us}")
        if st != ST_READY:
            return

        link.drain_async()
        tids, reported = [], set()
        qs_max = 0
        cats = (CAT_NA, 1, 2)
        events = []
        fault, unresponsive = None, False
        persist_total, persist_ok, persist_allowed = 0, 0, 0
        cur_base = win_lo
        checks_done, checks_ok, check_fails = 0, 0, []
        checks_skipped = 0        # inconclusive spot-checks (link glitch)
        report_delay = report_delay_ms / 1000.0
        pending_rep = []          # (due_time, tid, cat) FIFO for delayed reports
        last_due = [0.0]          # keeps due times monotonic -> in-order sends
        bursts_done = 0
        # A reconnect reboots the board, so the tid counter restarts: the +1
        # chain only holds WITHIN a segment between drops. Track objects and
        # gaps across all segments so C.2 stays meaningful across USB drops.
        seg_objs = [0]            # objects counted in already-closed segments
        seg_count = [1]           # segments seen (starts at 1)
        seg_gaps = []

        def _pump():
            nonlocal qs_max, fault
            now = time.time()
            for _, m in link.drain_async():
                t = m.get("type")
                if t == "cam_trig":
                    tid = m.get("tid")
                    if tid not in reported:
                        tids.append(tid)
                        reported.add(tid)
                        qs_max = max(qs_max, m.get("Qs", 0) or 0)
                        cat = rng.choice(cats)
                        if report_delay > 0:
                            due = now + rng.uniform(0, report_delay)
                            if not report_shuffle:
                                # Keep due times monotonic -> reports leave in
                                # tid order (a real host answers FIFO). With
                                # --report-shuffle we DROP this, letting reports
                                # reorder within the delay window to probe the
                                # firmware's out-of-order tolerance.
                                due = max(due, last_due[0])
                                last_due[0] = due
                            pending_rep.append((due, tid, cat))
                        else:
                            link.send_nowait({"type": "report", "tid": tid,
                                              "cat": cat})
                elif t == "system_info" and m.get("state") == ST_ERROR:
                    # Detect a fault the instant the firmware announces it, from
                    # the async stream -- no blocking poll that could stall
                    # reporting and self-inflict OBJECT_HAS_NO_INSP_RESULT.
                    if fault is None:
                        fault = _errors_of(m) or ["state=ERROR"]

        def _flush_rep():
            now = time.time()
            if report_shuffle:
                # Send any due entry -- due times are independent, so order is
                # shuffled relative to tid (deliberately, to test reordering).
                keep = []
                for e in pending_rep:
                    if e[0] <= now:
                        link.send_nowait({"type": "report", "tid": e[1],
                                          "cat": e[2]})
                    else:
                        keep.append(e)
                pending_rep[:] = keep
            else:
                # Send from the FRONT while due -> reports leave in tid order
                # (due times are monotonic; never send a later tid first).
                while pending_rep and pending_rep[0][0] <= now:
                    _due, tid, cat = pending_rep.pop(0)
                    link.send_nowait({"type": "report", "tid": tid, "cat": cat})

        def _spotcheck(base):
            """Fire ONE object at the CURRENT (churned) offset and confirm the
            firmware routes SWITCH/SEL to it -- the edges the publish path reads
            at dispatch. A torn/stale offset read shows up as SWITCH or SEL off
            its expected pulse. Quiesce first so the trace holds just our part
            (SEL edges log tid 0, so they are only attributable in isolation).
            Returns (ok, detail)."""
            link.send({"type": "set_sel1_cd", "count": -1}, timeout=2.0)
            # Quiesce: stop firing but KEEP reporting (via _pump/_flush_rep) so
            # the parts already in flight get a verdict before SWITCH -- a bare
            # wait would fault them (OBJECT_HAS_NO_INSP_RESULT). 1.3s > the
            # widest window traversal, so the pipeline empties and freq settles.
            tq = time.time() + 1.3
            while time.time() < tq:
                _pump()
                _flush_rep()
                time.sleep(0.003)
            # Force out any still-delayed reports so all prior tids are answered
            # before we fire the probe -- else the probe's (higher) tid reports
            # ahead of them and the SKIP scan desyncs the machine.
            for _due, tid, cat in pending_rep:
                link.send_nowait({"type": "report", "tid": tid, "cat": cat})
            pending_rep.clear()
            last_due[0] = 0.0
            time.sleep(0.05)
            link.send({"type": "io_trace_arm"}, timeout=2.0)
            link.drain_async()
            link.send({"type": "trig_phantom_pulse"}, timeout=2.0)
            te = time.time() + 1.3
            while time.time() < te:
                # Fold the probe object into the shared tid stream (report cat=1
                # so SEL1 fires) -- otherwise its tid is a phantom gap in C.2.
                for _, m in link.drain_async():
                    if m.get("type") == "cam_trig":
                        t = m.get("tid")
                        if t not in reported:
                            reported.add(t)
                            tids.append(t)
                            link.send_nowait({"type": "report", "tid": t,
                                              "cat": 1})
                time.sleep(0.003)
            dump = link.send({"type": "io_trace_dump"}, timeout=3.0) or {}
            link.send({"type": "io_trace_stop"}, timeout=2.0)
            named = [(IOT_PIN.get(p, p), v, pl)
                     for pl, p, v, _ in (dump.get("ev") or [])]

            def _last(nm, vv):       # our object is the most recent in the ring
                return next((pl for n, v, pl in reversed(named)
                             if n == nm and v == vv), None)
            l1a_on = _last("L1A", 1)
            sw = _last("SWITCH", 1)
            sw_val = next((v for n, v, pl in reversed(named)
                           if n == "SWITCH"), None)
            s1 = _last("SEL1", 1)
            if l1a_on is None or sw is None:
                # No usable trace came back. A torn/stale offset read shows WRONG
                # edge positions, not a MISSING trace -- an empty dump means the
                # io_trace reply didn't make it (link glitching / mid-drop). Mark
                # it inconclusive (ok=None) so a dying transport doesn't masquer-
                # ade as an offset-race failure; the caller skips it, not fails.
                return None, (f"base={base} incomplete trace "
                              f"L1A_on={l1a_on} SWITCH={sw} n={dump.get('n')}")
            # SWITCH offset (l1a+base) minus L1A_on offset (l1a) == base.
            d_sw = sw - l1a_on
            ok = abs(d_sw - base) <= 1 and sw_val == 1
            d_sel = None if s1 is None else s1 - l1a_on
            if s1 is not None:
                ok = ok and abs(d_sel - (base + 3)) <= 1
            return ok, (f"base={base} SWITCH-L1A={d_sw}(want {base}) "
                        f"sw_val={sw_val} SEL1-L1A={d_sel}(want {base + 3})")

        def _reestablish():
            """Recover from a reconnect (LinkReset). The board rebooted on the
            USB re-seat: back to IDLE with the tid counter and pipeline reset.
            Close the current tid segment, put the board back into READY at the
            baseline, and drop the volatile expectations a reboot invalidated."""
            nonlocal cur_base
            # Close the segment we were in: fold its objects + any in-segment
            # gaps into the running totals before the counter restarts.
            if tids:
                seg_gaps.extend((a, b) for a, b in zip(tids, tids[1:])
                                if b != a + 1)
                seg_objs[0] += len(tids)
                seg_count[0] += 1
            tids.clear()
            reported.clear()
            pending_rep.clear()
            last_due[0] = 0.0
            cur_base = win_lo
            # Re-arm READY; if it drops again mid-recovery, keep retrying.
            for _ in range(4):
                try:
                    if _enter_ready() == ST_READY:
                        link.drain_async()
                        return True
                except (LinkReset, LinkDead):
                    continue
            return False

        def _recover_silence():
            """The board stopped replying though writes never failed. If the
            port node re-enumerated it is a USB drop that kept our write side
            open (the write-failure path never fired) -- force a fresh handle
            and re-establish. If the node is INTACT the board went silent on its
            own (a real firmware hang/reboot) -- do NOT mask it; let C.1 fail."""
            if not link.port_reenumerated():
                return False
            print("  [board silent + port re-enumerated -- treating as a USB "
                  "drop, recovering]", flush=True)
            try:
                link._reopen_serial()
            except Exception:
                pass
            return _reestablish()

        now0 = time.time()
        t_end = now0 + seconds
        next_fire = now0
        next_freq = now0 + rng.uniform(1.5, 3.0)
        next_offs = now0 + rng.uniform(2.0, 4.0)
        next_sep = now0 + rng.uniform(2.0, 4.0)
        next_sel = now0 + rng.uniform(3.0, 5.0)
        next_flood = now0 + rng.uniform(0.08, 0.15)
        next_persist = now0 + rng.uniform(3.0, 5.0)
        next_check = now0 + rng.uniform(6.0, 10.0)
        next_burst = (now0 + rng.uniform(burst_every * 0.6, burst_every * 1.4)
                      if burst else float("inf"))
        next_poll = now0 + 2.0
        next_beat = now0 + 30.0
        fired = 0

        while time.time() < t_end:
          try:
            now = time.time()
            _pump()
            _flush_rep()
            if fault is not None:        # async system_info said ERROR
                break
            if now >= next_fire:
                link.send_nowait({"type": "trig_phantom_pulse"})
                fired += 1
                next_fire = now + 1.0 / rng.uniform(min_hz, max_hz)
            if burst and now >= next_burst:
                # A tight burst of M pulses 10ms apart -- most fall inside the
                # 3.5mm / time gates and are rejected; the point is that rapid
                # back-to-back triggers are shed cleanly, not desynced/faulted.
                for _ in range(burst_count):
                    link.send_nowait({"type": "trig_phantom_pulse"})
                    fired += 1
                    _pump()
                    _flush_rep()
                    time.sleep(0.010)
                bursts_done += 1
                events.append(f"burst{burst_count}")
                next_burst = now + rng.uniform(burst_every * 0.6,
                                               burst_every * 1.4)
            if now >= next_flood:      # contend for serial bandwidth
                link.send_nowait({"type": rng.choice(FLOOD)})
                next_flood = now + rng.uniform(0.08, 0.15)
            # Churn commands are fire-and-forget (send_nowait): a blocking round
            # trip here could stall reporting past an in-flight part's window and
            # self-inflict OBJECT_HAS_NO_INSP_RESULT (a host artifact, not a
            # firmware fault). We don't need their acks.
            if now >= next_freq:
                f = rng.randint(freq_floor, freq_ceil)
                link.send_nowait({"type": "set_setup", "plate_freq": f})
                events.append(f"freq={f}")
                next_freq = now + rng.uniform(1.5, 3.0)
            if now >= next_offs:
                base = rng.randint(win_lo, win_hi)   # stay generous, but move it
                link.send_nowait({"type": "set_setup",
                                  "stage_pulse_offset": _win(base)})
                cur_base = base
                events.append(f"win={base}")
                next_offs = now + rng.uniform(2.0, 4.0)
            if verify and now >= next_check:
                ok, detail = _spotcheck(cur_base)
                if ok is None:
                    # Inconclusive (no trace came back -- link glitch), not a
                    # verdict on the offset: skip it rather than fail C.6.
                    checks_skipped += 1
                    events.append("check-skip")
                else:
                    checks_done += 1
                    if ok:
                        checks_ok += 1
                    else:
                        check_fails.append(detail)
                    events.append("check" + ("" if ok else "-FAIL"))
                next_fire = time.time()          # resume firing immediately
                next_check = time.time() + rng.uniform(6.0, 10.0)
            if now >= next_sep:
                sep = rng.randint(sep_lo, sep_hi)
                link.send_nowait({"type": "set_setup",
                                  "min_detect_sep_us": sep})
                events.append(f"sep={sep}")
                next_sep = now + rng.uniform(2.0, 4.0)
            if now >= next_sel:
                cd = rng.choice([-1, rng.randint(1, 20)])
                link.send_nowait({"type": "set_sel1_cd", "count": cd})
                events.append(f"sel1cd={cd}")
                next_sel = now + rng.uniform(3.0, 5.0)
            if persist and now >= next_persist:
                # Attempt an NVS save mid-run; the firmware must refuse it
                # (plate is spinning -> flash write would be unsafe).
                r = link.send({"type": "set_setup", "persist": True},
                              timeout=3.0)
                persist_total += 1
                if r is None:
                    unresponsive = True
                    events.append("persist->NO REPLY")
                    break
                if r.get("persisted") is False:
                    persist_ok += 1                 # correctly refused
                else:
                    persist_allowed += 1            # guard let it through
                    events.append("persist-ALLOWED!")
                next_persist = now + rng.uniform(3.0, 5.0)
            if now >= next_poll:
                # Backup only: faults are caught live via async system_info in
                # _pump. This infrequent blocking poll just catches a hang (no
                # reply) and a missed fault; pump right after so the round trip
                # never leaves an in-flight part unreported.
                st, stat = _state(link)
                _pump()
                _flush_rep()
                if stat is None:
                    # No reply. A USB drop that kept the write side open won't
                    # have tripped the write-failure reconnect -- recover it if
                    # the port re-enumerated. A board silent with the node intact
                    # is a real hang: _recover_silence returns False -> C.1 fails.
                    if _recover_silence():
                        next_fire = next_poll = time.time()
                        continue
                    unresponsive = True
                    break
                errs = _errors_of(stat)
                if (st == ST_ERROR or errs) and fault is None:
                    fault = errs or ["state=ERROR"]
                if fault is not None:
                    break
                next_poll = time.time() + 2.0
            if now >= next_beat:
                # Flushed heartbeat so a long run is observable and a kill still
                # leaves a trail (plain prints are buffered when piped to a file).
                el = int(now - now0)
                print(f"  [{el:>4}s] fired={fired} objs={len(tids)} "
                      f"peakQs={qs_max} perturb={len(events)}"
                      + (f" bursts={bursts_done}" if burst else "")
                      + (f" checks={checks_ok}/{checks_done}" if verify else "")
                      + (f" persist_refused={persist_ok}" if persist else "")
                      + (f" pendRep={len(pending_rep)}" if report_delay else "")
                      + (f" reconnects={link.reconnects}"
                         if link.reconnects else ""),
                      flush=True)
                next_beat = now + 30.0
            time.sleep(0.001)
          except LinkReset:
            # The link dropped and was reopened mid-tick. The board rebooted, so
            # re-establish READY + reset the segment and carry on -- a USB drop
            # is a hiccup, not the end of the soak.
            if not _reestablish():
                unresponsive = True
                break
            next_fire = next_poll = time.time()
            continue
          except LinkDead:
            unresponsive = True     # port never came back within the timeout
            break

        # Flush any delayed reports immediately so nothing is stranded, then
        # answer the tail. A drop right at the finish is still just a reconnect.
        try:
            for _due, tid, cat in pending_rep:
                link.send_nowait({"type": "report", "tid": tid, "cat": cat})
            pending_rep.clear()
            t_s = time.time() + 1.0
            while time.time() < t_s:
                _pump()
                _flush_rep()
                time.sleep(0.003)
            st, stat = _state(link)
        except LinkReset:
            _reestablish()
            st, stat = _state(link)
        except LinkDead:
            unresponsive = True
            st, stat = None, None
        errs = _errors_of(stat)
        rate = fired / seconds if seconds else 0
        all_errs = (fault or []) + errs

        if expect_fault:
            # We deliberately delayed results past the window -- the machine
            # MUST error-stop with OBJECT_HAS_NO_INSP_RESULT (a slow inspection
            # is not allowed to let an unjudged part reach the selector), and
            # must stop cleanly (still responsive), not hang or silently pass.
            OBJ_NO_RESULT = 2
            rep.add("C.1", "a too-slow result error-stopped "
                    "(OBJECT_HAS_NO_INSP_RESULT), board still responsive",
                    OBJ_NO_RESULT in all_errs and not unresponsive
                    and stat is not None,
                    f"fired={fired} objects={len(tids)} "
                    f"state={ST_NAME.get(st, st)} "
                    f"errors={[ERR_NAME.get(e, e) for e in sorted(set(all_errs))]}"
                    + (" UNRESPONSIVE" if unresponsive else ""))
        else:
            rep.add("C.1", "survived the churn without faulting",
                    fault is None and not unresponsive and st == ST_READY
                    and not errs,
                    f"fired={fired} (~{rate:.0f}/s) objects={len(tids)} "
                    f"state={ST_NAME.get(st, st)}"
                    + (f" over {link.reconnects} USB reconnect(s)"
                       if link.reconnects else "")
                    + (f" FAULT={[ERR_NAME.get(e, e) for e in fault]}"
                       if fault else "")
                    + (" UNRESPONSIVE" if unresponsive else ""))

        # Close the final segment. A reconnect reboots the tid counter, so the
        # +1 chain only holds WITHIN a segment; a gap INSIDE a segment is still a
        # real desync and still fails C.2.
        seg_gaps.extend((a, b) for a, b in zip(tids, tids[1:]) if b != a + 1)
        total_objs = seg_objs[0] + len(tids)
        if link.reconnects:
            rep.add("C.2", "accepted tids stayed strictly +1 within each link "
                    "segment", total_objs > 0 and not seg_gaps,
                    f"{total_objs} objs across {seg_count[0]} segment(s), "
                    f"{link.reconnects} reconnect(s)"
                    + (f" gaps:{seg_gaps[:5]}" if seg_gaps else ""))
        else:
            rep.add("C.2", "accepted tids stayed strictly +1 through the churn",
                    bool(tids) and not seg_gaps,
                    (f"tid {tids[0]}..{tids[-1]}" if tids else "no objects")
                    + (f" gaps:{seg_gaps[:5]}" if seg_gaps else ""))

        # BOTH queues. This check has been passing on `Qs` (RBuf) every run
        # while the machine stopped with INSP_CAM_TRIG_INFO_CANNOT_BE_SENT --
        # which is the OTHER queue, ISRTrigQ, 32 entries, 2 pushed per object.
        # A green light on the queue that does not overflow is worse than no
        # light at all: it is an alibi.
        p = _poll(link)
        tq_hwm, tq_cap = p.get("tqhwm"), p.get("tqcap")
        tq_ovf = p.get("tqovf")
        rep.add("C.3", "both firmware queues stayed bounded under load",
                qs_max < PIPE_INFO_LEN and not tq_ovf,
                f"RBuf peak Qs={qs_max} of {PIPE_INFO_LEN}; "
                f"cam-trig queue peak={tq_hwm} of {tq_cap}, overflows={tq_ovf}")

        rep.add("C.4", "board still responsive after the run",
                stat is not None, "no reply = hang/reboot")

        if persist:
            rep.add("C.5", "mid-run NVS persist refused while the plate runs",
                    persist_total > 0 and persist_ok == persist_total
                    and persist_allowed == 0,
                    f"{persist_ok}/{persist_total} refused, "
                    f"{persist_allowed} wrongly allowed "
                    f"(a save with the timer ISR live is the flash-cache hazard)")

        if verify:
            # None (MANUAL) when nothing was sampled: a run that faulted before
            # the first quiesce has not disproved anything, and reporting that
            # as a failure buries the real one under a second red line.
            rep.add("C.6", "actuator edges matched the published offset in "
                    "every spot-check",
                    (checks_ok == checks_done) if checks_done > 0 else None,
                    f"{checks_ok}/{checks_done} spot-checks matched"
                    + (f", {checks_skipped} inconclusive (link glitch)"
                       if checks_skipped else "")
                    + (f"; mismatches: {check_fails[:4]}" if check_fails
                       else " (SWITCH/SEL landed on the current offset -- no "
                            "torn/stale offset read observed)"))

        print(f"\n  injected {len(events)} perturbations mid-run: "
              f"{', '.join(events[:14])}{' ...' if len(events) > 14 else ''}")

    finally:
        # Best-effort restore. If the link drops during teardown the board
        # reboots to a safe IDLE anyway, so a failed restore is harmless.
        try:
            link.send({"type": "clear_error"}, timeout=3.0)
            link.send({"type": "exit_insp_mode"}, timeout=3.0)
            link.send({"type": "set_sel1_cd", "count": -1}, timeout=3.0)
            restore = {"type": "set_setup"}
            if isinstance(orig_freq, (int, float)):
                restore["plate_freq"] = orig_freq
            if isinstance(orig_sep, (int, float)):
                restore["min_detect_sep_us"] = orig_sep
            if orig_spo:
                restore["stage_pulse_offset"] = orig_spo
            link.send(restore, timeout=3.0)
            link.send({"type": "clear_error_history"}, timeout=2.0)
            # The guard should have blocked every mid-run save, so NVS is
            # normally untouched. Only clean up if one slipped through.
            if persist and persist_allowed:
                if orig_nvs:
                    link.send({"type": "set_setup", "persist": True}, timeout=4.0)
                else:
                    link.send({"type": "clear_saved_setup"}, timeout=4.0)
        except (LinkReset, LinkDead):
            pass


# --- grill: the whole machine at REAL parameters --------------------------

def grill_plan(params):
    """Derive everything grill needs from the machine parameter file.

    Pure function so the derivation is unit-testable. 'tick' = one
    SYS_STEP_COUNT increment (the stage_pulse_offset unit); the ISR ticks at
    2x plate_freq, hence plate_freq = ticks_per_rev / sec_per_rev / 2.

    When the file gives no explicit stage_pulse_offset, stations are laid out
    along the real gate->last-station span: cameras at 5%, SWITCH (report
    deadline) at 90%, and the three selector exits at 92/96/100% with a
    sel_pulse_ms-wide actuation window.
    """
    tpr = float(params["ticks_per_rev"])
    span = float(params["gate_to_last_station_ticks"])
    spr = float(params["sec_per_rev"])
    tick_hz = tpr / spr                     # SYS_STEP_COUNT ticks per second
    freq = tick_hz / 2.0                    # plate_freq
    sel_w = max(2, int(round(params.get("sel_pulse_ms", 10.0) / 1000.0 * tick_hz)))

    spo = params.get("stage_pulse_offset")
    if not spo:
        cam = int(round(span * 0.05))
        switch = int(round(span * 0.90))
        spo = {
            "L1A_on": cam, "L1A_off": cam + 12,
            "CAM1_on": cam, "CAM1_off": cam + 2,
            "L2A_on": cam, "L2A_off": cam + 12,
            "CAM2_on": cam, "CAM2_off": cam + 2,
            "SWITCH": switch,
            "SEL1_on": int(round(span * 0.92)),
            "SEL2_on": int(round(span * 0.96)),
            "SEL3_on": int(round(span * 1.00)),
        }
        for i in (1, 2, 3):
            spo["SEL%d_off" % i] = spo["SEL%d_on" % i] + sel_w

    pat = params.get("pattern") or {"ok": 1, "ng": 0, "na": 0}
    cats = ([int(params.get("cat_ok", 1))] * int(pat.get("ok", 0))
            + [int(params.get("cat_ng", 2))] * int(pat.get("ng", 0))
            + [int(params.get("cat_na", CAT_NA))] * int(pat.get("na", 0)))

    window_ticks = spo["SWITCH"] - spo["CAM1_on"]

    # Real part geometry -> gate-domain numbers. mm_per_tick comes from the
    # rim circumference; the minimum inter-part gap becomes a hard ceiling on
    # min_detect_sep_us (a larger value MERGES legitimate parts), and
    # object size sets where the pulse width filter must sit.
    dia = float(params.get("plate_diameter_mm", 0))
    mm_per_tick = (3.14159265 * dia / tpr) if dia else 0
    min_obj_mm = float(params.get("min_object_mm", 0))
    min_gap_mm = float(params.get("min_gap_mm", 0))
    rim_mm_s = (3.14159265 * dia / spr) if dia else 0
    # Gate-filter settings derived from part geometry: sep = half the
    # min-gap flight time; width window = [half the smallest part, the
    # largest part with margin].
    max_obj_mm = float(params.get("max_object_mm", 0))
    return {
        "sep_us": int(min_gap_mm / rim_mm_s * 1e6 / 2) if rim_mm_s and min_gap_mm else None,
        "min_w": int(min_obj_mm / mm_per_tick / 2) if mm_per_tick and min_obj_mm else None,
        "max_w": int(max_obj_mm / mm_per_tick) if mm_per_tick and max_obj_mm else None,
        "mm_per_tick": mm_per_tick,
        "min_obj_ticks": (min_obj_mm / mm_per_tick) if mm_per_tick else 0,
        "gap_us": (min_gap_mm / rim_mm_s * 1e6) if rim_mm_s else 0,
        "pitch_rate_hz": (rim_mm_s / (min_obj_mm + min_gap_mm)
                          if (min_obj_mm + min_gap_mm) and rim_mm_s else 0),
        "freq": freq,
        "accel": float(params.get("plate_accel_hz_s", 4 * freq)),
        "tick_hz": tick_hz,
        "spo": spo,
        "cats": cats or [int(params.get("cat_ok", 1))],
        "rate": float(params.get("parts_per_second", 5.0)),
        "window_ticks": window_ticks,
        "window_ms": window_ticks / tick_hz * 1000.0,
        "transit_s": span / tick_hz,
        "speed_tol": float(params.get("speed_tolerance_pct", 3.0)),
        "diameter_mm": float(params.get("plate_diameter_mm", 0)),
        "sec_per_rev": spr,
        "ticks_per_rev": tpr,
    }


def _symbolize_backtrace(lines):
    """If the captured noise contains an ESP32 panic backtrace, resolve the
    PC addresses to source lines with the toolchain's addr2line and the
    build's ELF -- the log then names the guilty code, not just addresses."""
    import re
    import glob
    import subprocess
    addrs = []
    for t in lines:
        if "Backtrace" in t:
            # "Backtrace:0xPC:0xSP 0xPC:0xSP ..." -- PCs live in 0x40xxxxxx
            addrs += re.findall(r"0x40[0-9a-fA-F]{6}", t)
    if not addrs:
        return
    here = os.path.dirname(os.path.abspath(__file__))
    elf = os.path.join(here, "..", ".pio", "build", "esp32dev", "firmware.elf")
    tools = glob.glob(os.path.expanduser(
        "~/.platformio/packages/toolchain-xtensa-esp32*/bin/"
        "xtensa-esp32-elf-addr2line"))
    if not (os.path.exists(elf) and tools):
        print("  [backtrace addrs: %s -- symbolize manually with"
              " xtensa-esp32-elf-addr2line -pfiaC -e firmware.elf]"
              % " ".join(addrs), flush=True)
        return
    try:
        out = subprocess.run([tools[0], "-pfiaC", "-e", elf] + addrs,
                             capture_output=True, text=True, timeout=10).stdout
        print("  [-- backtrace symbolized (current build's ELF) --]",
              flush=True)
        for line in out.strip().splitlines():
            print("    " + line, flush=True)
    except Exception as exc:
        print("  [addr2line failed: %s]" % exc, flush=True)


def _dump_raw_tail(link, label, n=30):
    """Dump the link's raw receive log (frames AND out-of-frame noise --
    panic backtraces, boot banners) so a dead/rebooted board explains
    itself in the soak log."""
    tail = list(link._raw_log)[-n:]
    print("  [-- raw serial tail: %s --]" % label, flush=True)
    for ts, txt in tail:
        print("    %s %s" % (time.strftime("%H:%M:%S", time.localtime(ts)),
                             txt[:220]), flush=True)
    _symbolize_backtrace([txt for _, txt in tail])


def grill(link, rep, params, seconds, min_rate=None, max_rate=None,
          burst_every=0.0, burst_count=5, burst_hz=100.0, seed=None):
    """Run the sorter at the REAL machine's speed and geometry.

    bench/chaos prove the firmware at synthetic settings; grill asserts the
    actual deployment point: true plate speed and station positions, a
    sustained part stream with the real in-flight depth (transit is a full
    second, so many parts ride the plate at once), the M/N/O verdict pattern
    landing exactly, the firmware's own latency bookkeeping staying inside
    the answer window, and every actuator resting at its logical OFF level
    after the run.
    """
    plan = grill_plan(params)
    freq, tick_hz = plan["freq"], plan["tick_hz"]
    print("\n\033[1m== Grill: real-machine parameters ==\033[0m")
    print("  %.0f ticks/rev, %.1fs/rev -> plate_freq %.0f (tick rate %.0f/s)"
          % (plan["ticks_per_rev"], plan["sec_per_rev"], freq, tick_hz))
    print("  gate->last station %.0f ticks = %.2fs transit; answer window"
          " %.0f ticks = %.0fms" % (plan["spo"]["SEL3_on"], plan["transit_s"],
                                    plan["window_ticks"], plan["window_ms"]))

    orig = link.send({"type": "get_setup"}, timeout=3.0) or {}
    orig_freq = orig.get("plate_freq")
    orig_accel = orig.get("plate_accel")
    orig_spo = _read_spo(link) or {}
    link.send({"type": "clear_error"}, timeout=2.0)
    link.send({"type": "clear_error_history"}, timeout=2.0)
    link.send({"type": "reset_running_stat"}, timeout=2.0)

    cfg = {"type": "set_setup", "plate_freq": freq,
           "plate_accel": plan["accel"],
           "stage_pulse_offset": plan["spo"]}
    # Push the geometry-derived gate filter too: a config-blob version bump
    # silently reverts NVS to compiled defaults, and a 66ms separation gate
    # at a 20ms part pitch swallows most of the stream.
    if plan["sep_us"]:
        cfg["min_detect_sep_us"] = plan["sep_us"]
    if plan["min_w"] and plan["min_w"] > 20:
        cfg["pulse_min_width"] = 0   # phantoms are 20 ticks; do not filter them
    if plan["max_w"]:
        cfg["pulse_max_width"] = plan["max_w"]
    r = link.send(cfg, timeout=3.0)
    back = (link.send({"type": "get_setup"}, timeout=3.0) or {})
    ok = (bool(r) and back.get("plate_freq") == freq
          and (back.get("stage_pulse_offset") or {}).get("SWITCH")
          == plan["spo"]["SWITCH"])
    rep.add("G.0", "apply real-machine plan (freq/accel/offsets)", ok,
            f"plate_freq={freq:g} accel={plan['accel']:g}"
            f" SWITCH={plan['spo']['SWITCH']}")

    # -- G.1 spin-up time follows plate_accel -------------------------------
    t0 = time.time()
    link.send({"type": "enter_insp_mode"}, timeout=3.0)
    running, detail = _wait_at_speed(link)
    ramp_s = time.time() - t0
    expect_ramp = freq / plan["accel"] if plan["accel"] > 0 else 0
    rep.add("G.1", "reached speed; ramp consistent with plate_accel",
            running and ramp_s < expect_ramp + 2.5,
            f"ramp {ramp_s:.2f}s (accel predicts {expect_ramp:.2f}s"
            f" + measurement settle); {detail}")

    # -- G.2 the plate actually turns at sec_per_rev -----------------------
    def ssc():
        return (link.send({"type": "get_setup"}, timeout=3.0) or {}).get(
            "step_count")
    # Measure only while the machine is actually running. An error-stop
    # decelerates the plate to a standstill over several seconds, and averaging
    # across that reported "6273 ticks/s = 9.564s/rev, err 79.1%" for a plate
    # that had just been measured hitting 30800 ticks/s dead on -- a fault
    # dressed up as a speed error, pointing at the stepper instead of at
    # whatever stopped the machine.
    c0, t0 = ssc(), time.time()
    stalled = None
    for _ in range(40):
        time.sleep(0.1)
        stt, _ = _state(link)
        if stt != ST_READY:
            stalled = stt
            break
    c1, t1 = ssc(), time.time()
    meas_hz = (c1 - c0) / (t1 - t0) if isinstance(c0, int) and isinstance(c1, int) else 0
    err_pct = abs(meas_hz - tick_hz) / tick_hz * 100 if tick_hz else 999
    if stalled is not None:
        rep.add("G.2", f"plate speed within {plan['speed_tol']:g}% of"
                       f" {plan['sec_per_rev']:g}s/rev", None,
                f"left READY for {ST_NAME.get(stalled, stalled)} after "
                f"{t1-t0:.1f}s -- speed not measurable through a stop; "
                f"partial {meas_hz:.0f} ticks/s of {tick_hz:.0f} expected")
    else:
        rev_s = plan["ticks_per_rev"] / meas_hz if meas_hz else 0
        rim = ""
        if plan["diameter_mm"]:
            rim = " (rim %.0f mm/s)" % (3.14159265 * plan["diameter_mm"] / rev_s) \
                  if rev_s else ""
        rep.add("G.2", f"plate speed within {plan['speed_tol']:g}% of"
                       f" {plan['sec_per_rev']:g}s/rev",
                err_pct <= plan["speed_tol"],
                f"measured {meas_hz:.0f} ticks/s = {rev_s:.3f}s/rev"
                f" (err {err_pct:.1f}%){rim}")

    # -- G.3..G.5 sustained stream at the real rate ------------------------
    # seconds <= 0 means run FOREVER (soak): heartbeat every 30s, survive USB
    # drops by re-establishing the board, and stop only when the machine
    # error-stops (the firmware log then names the tid and how late it was).
    endless = seconds <= 0
    import random
    rng = random.Random(seed)
    churn = bool(min_rate and max_rate)
    rate = rng.uniform(min_rate, max_rate) if churn else plan["rate"]
    interval = 1.0 / rate
    if churn or burst_every:
        print("  rate %s, burst %s (seed=%s)"
              % ("%g..%g/s churning" % (min_rate, max_rate) if churn
                 else "%g/s fixed" % rate,
                 "%d parts @%gHz every ~%gs" % (burst_count, burst_hz,
                                                burst_every)
                 if burst_every else "off", seed), flush=True)
    cats = plan["cats"]
    base_counts, _ = _counts(link)
    link.drain_async()
    seen, answered, reported_cats = [], set(), []
    max_reg = 0
    seg_objs, segments, reconnects = 0, 0, 0
    err_log = [None]
    t_start = time.time()
    deadline = float("inf") if endless else time.time() + seconds
    next_fire = time.time()
    next_peek = time.time() + 1.0
    next_beat = time.time() + 30.0
    next_rate = time.time() + (rng.uniform(3, 8) if churn else 1e18)
    next_burst = time.time() + (burst_every * rng.uniform(0.6, 1.4)
                                if burst_every else 1e18)
    fired = 0
    bursts = 0
    noreply = 0
    reset_tried = False
    last_lat = {}
    fault = False

    def _pump():
        for _, msg in link.drain_async():
            mtype = msg.get("type")
            if mtype == "system_info" and msg.get("state") == ST_ERROR:
                err_log[0] = msg.get("log")
            if mtype != "cam_trig":
                continue
            tid = msg.get("tid")
            seen.append(tid)
            if tid not in answered:
                cat = cats[len(answered) % len(cats)]
                link.send_nowait({"type": "report", "tid": tid, "cat": cat})
                answered.add(tid)
                reported_cats.append(cat)

    def _reestablish():
        # The board rebooted with the USB drop: RAM config and tids are gone.
        # Close the tid segment, re-push the plan and spin back up.
        nonlocal seg_objs, segments
        segments += 1
        seg_objs += len(set(seen))
        seen.clear()
        answered.clear()
        # The protocol may be LATCHED by garbage bytes from the drop/sleep
        # transition -- while latched everything except literal RESET is
        # eaten. Fire the escape hatch first, always.
        try:
            link.ser.write(b'{"type":"RESET"}')
            time.sleep(0.4)
            link.drain_async()
        except Exception:
            pass
        g = link.send({"type": "get_setup"}, timeout=3.0) or {}
        if "reset_reason" in g:
            print("  [board reset reason: %s (%s)]"
                  % (g.get("reset_reason"), g.get("reset_reason_name")),
                  flush=True)
        link.send({"type": "clear_error"}, timeout=3.0)
        link.send({"type": "set_setup", "plate_freq": freq,
                   "plate_accel": plan["accel"],
                   "stage_pulse_offset": plan["spo"]}, timeout=3.0)
        link.send({"type": "enter_insp_mode"}, timeout=3.0)
        _wait_at_speed(link)
        link.drain_async()

    while time.time() < deadline:
        try:
            now = time.time()
            if churn and now >= next_rate:
                rate = rng.uniform(min_rate, max_rate)
                interval = 1.0 / rate
                next_rate = now + rng.uniform(3, 8)
            if now >= next_burst:
                # A short burst at burst_hz: spacing sits right on top of the
                # minDetectTimeSep / 3.5mm de-dup limits -- the point.
                for _ in range(burst_count):
                    link.send_nowait({"type": "trig_phantom_pulse"})
                    fired += 1
                    t_next = time.time() + 1.0 / burst_hz
                    while time.time() < t_next:
                        _pump()
                        time.sleep(0.001)
                bursts += 1
                next_burst = now + burst_every * rng.uniform(0.6, 1.4)
            if now >= next_fire:
                link.send_nowait({"type": "trig_phantom_pulse"})
                fired += 1
                next_fire = now + interval
            if now >= next_peek:
                next_peek = now + 1.0
                st = link.send({"type": "get_running_stat"}, timeout=1.0) or {}
                if not st:
                    # No reply. Three in a row is either a silent USB death
                    # (the OS keeps the fd alive while the device is gone --
                    # writes "succeed", reads are empty) or a real firmware
                    # hang. The port node tells them apart: a re-enumeration
                    # destroys it, a hung board leaves it intact.
                    noreply += 1
                    if noreply >= 3:
                        if endless and link.port_reenumerated():
                            print("  [silent USB drop -- waiting for the"
                                  " port to re-appear]", flush=True)
                            if link._reopen_serial():
                                reconnects += 1
                                _reestablish()
                                noreply = 0
                                next_fire = next_peek = time.time()
                                continue
                        if endless and link.read_errors >= 3:
                            # Node exists but reads die: zombie bridge. Only a
                            # TRUE re-enumeration (node vanishes, then returns
                            # -- usually a physical replug) can revive it.
                            print("  [ZOMBIE USB BRIDGE: node present, reads"
                                  " dead -- REPLUG THE USB CABLE; waiting for"
                                  " a true re-enumeration]", flush=True)
                            sig0 = link._port_sig()
                            while not (link._stop or
                                       link._port_sig() in (None,)):
                                time.sleep(1.0)
                            while link._port_sig() is None:
                                time.sleep(1.0)
                            if link._reopen_serial():
                                reconnects += 1
                                _reestablish()
                                noreply = 0
                                next_fire = next_peek = time.time()
                                continue
                        if endless and not reset_tried:
                            # A latched protocol also looks like silence.
                            # One RESET + retry before calling it a hang.
                            print("  [no reply -- firing the RESET escape"
                                  " hatch once before judging]", flush=True)
                            reset_tried = True
                            try:
                                link.ser.write(b'{"type":"RESET"}')
                                time.sleep(0.5)
                                link.drain_async()
                            except Exception:
                                pass
                            _reestablish()
                            noreply = 0
                            next_fire = next_peek = time.time()
                            continue
                        print("  [board unresponsive, port node intact --"
                              " treating as firmware hang]", flush=True)
                        _dump_raw_tail(link, "before hang")
                        fault = True
                        break
                    continue
                noreply = 0
                reset_tried = False
                reg = ((st.get("pipe") or {}).get("registered")) or 0
                max_reg = max(max_reg, reg)
                last_lat = st.get("report_latency") or last_lat
                if endless and st.get("state") == ST_ERROR:
                    _dump_raw_tail(link, "around the error stop", n=15)
                    fault = True
                    break
                if endless and st.get("state") == ST_IDLE:
                    # The board rebooted under us (brownout/WDT) without a
                    # USB drop: the port stayed up but RAM state is gone.
                    print("  [board rebooted to IDLE -- re-establishing]",
                          flush=True)
                    _dump_raw_tail(link, "around the reboot")
                    reconnects += 1
                    _reestablish()
                    next_fire = next_peek = time.time()
                    continue
            if endless and now >= next_beat:
                next_beat = now + 30.0
                c, _ = _counts(link)
                dc = {k: c.get(k, 0) - base_counts.get(k, 0)
                      for k in ("SEL1", "SEL2", "SEL3", "NA")}
                print("  [%5.0fs] fired=%d objs=%d peak=%d rate=%.0f/s"
                      " bursts=%d sorted=%s lat avg/max=%.0f/%.0fms reconn=%d"
                      " crcfail=%d gaps=%d"
                      % (now - t_start, fired, seg_objs + len(set(seen)),
                         max_reg, rate, bursts, dc,
                         last_lat.get("avg_us", 0) / 1000,
                         last_lat.get("max_us", 0) / 1000, reconnects,
                         link.rx_crc_fail, link.event_gaps),
                      flush=True)
            _pump()
            time.sleep(0.002)
        except LinkReset:
            reconnects += 1
            print("  [link reset -- re-establishing the board]", flush=True)
            try:
                _reestablish()
            except (LinkReset, LinkDead):
                pass
            next_fire = next_peek = time.time()
        except LinkDead:
            print("  [link dead -- port never came back]", flush=True)
            fault = True
            break
    drain_until = time.time() + plan["transit_s"] + 1.0
    while time.time() < drain_until:
        try:
            _pump()
        except (LinkReset, LinkDead):
            break
        time.sleep(0.005)
    seg_objs += len(set(t for t in seen if t is not None))

    objs = sorted(set(t for t in seen if t is not None))
    ran_s = time.time() - t_start
    st, stat = _state(link)
    stream_ok = st == ST_READY and not _errors_of(stat) and not fault
    rep.add("G.3", f"{'endless' if endless else '%gs' % seconds} stream at"
                   f" {rate:g}/s survives at real speed", stream_ok,
            f"ran {ran_s:.0f}s fired={fired} objects={seg_objs}"
            f" segments={segments + 1} reconnects={reconnects} state={st}"
            f" ({ST_NAME.get(st, '?')}) ERROR_HIST={_errors_of(stat)}"
            + (f"  firmware: {err_log[0]}" if err_log[0] else ""))

    gaps = [(a, b) for a, b in zip(objs, objs[1:]) if b != a + 1]
    rep.add("G.4", "tid strictly increasing by 1 (final segment)",
            bool(objs) and not gaps,
            f"tid {objs[0]}..{objs[-1]}" if objs and not gaps
            else f"gaps: {gaps[:5]}")

    counts, stat = _counts(link)
    dc = {k: counts.get(k, 0) - base_counts.get(k, 0) for k in
          ("SEL1", "SEL2", "SEL3", "NA")}
    from collections import Counter
    want = Counter()
    for c in reported_cats:
        want[{1: "SEL1", 2: "SEL2", 3: "SEL3", CAT_NA: "NA"}.get(c, "?")] += 1
    ok = all(dc.get(k, 0) == want.get(k, 0) for k in
             ("SEL1", "SEL2", "SEL3", "NA"))
    if segments:
        # A reboot mid-run zeroed the on-board counters; exact accounting is
        # impossible, so report what we have rather than a false failure.
        rep.add("G.5", "verdict pattern vs exit counters (reboot mid-run --"
                       " informational)", None,
                f"final-segment sorted {dc}, total reported {dict(want)}")
    else:
        rep.add("G.5", "verdict pattern lands exactly on the exit counters",
                ok, f"sorted {dc} expected {dict(want)}")

    # -- G.6 in-flight depth matches the real transit ----------------------
    peak_rate = max_rate if churn else rate
    expect_depth = peak_rate * plan["transit_s"] + (burst_count if burst_every
                                                    else 0)
    rep.add("G.6", "in-flight depth ~= rate x transit (RBuf holds a real"
                   " plate-load)", 1 <= max_reg <= expect_depth + 4,
            f"max registered={max_reg}, rate x transit = {expect_depth:.1f},"
            f" RBuf cap 100")

    # -- G.7 firmware's own latency stat inside the window -----------------
    lat = (stat or {}).get("report_latency") or {}
    lat_ok = (lat.get("n", 0) == len(answered)
              and lat.get("max_us", 0) / 1000.0 < plan["window_ms"])
    rep.add("G.7", "gate->report latency tracked for every part, max inside"
                   " the answer window", lat_ok,
            f"n={lat.get('n')} (answered {len(answered)}),"
            f" avg={lat.get('avg_us', 0) / 1000:.0f}ms"
            f" max={lat.get('max_us', 0) / 1000:.0f}ms"
            f" window={plan['window_ms']:.0f}ms")

    # -- G.8 stop; every actuator rests at logical OFF ---------------------
    link.send({"type": "exit_insp_mode"}, timeout=3.0)
    time.sleep(max(2.0, freq / plan["accel"] + 1.0))
    io_on = (link.send({"type": "get_setup"}, timeout=3.0) or {}).get(
        "io_on_level") or {}
    pins = {"L1A": 16, "CAM1": 17, "L2A": 18, "CAM2": 19,
            "SEL1": 25, "SEL2": 26, "SEL3": 32, "FEEDER": 21}
    names = list(pins)
    rd = link.send({"type": "pin_read", "pins": [pins[n] for n in names]},
                   timeout=2.0) or {}
    vals = rd.get("vals") or []
    bad = []
    for n, v in zip(names, vals):
        on_level = io_on.get(n, 1)
        if v == on_level:            # resting at its ON level = energised
            bad.append(f"{n}=phys{v}")
    rep.add("G.8", "after stop every actuator rests at logical OFF",
            bool(vals) and not bad,
            "all OFF" if vals and not bad else f"energised: {bad} "
            f"(vals={vals})")

    # -- G.9 the board's detect separation cannot exceed the real gap ------
    if plan["gap_us"]:
        cfg_sep = (link.send({"type": "get_setup"}, timeout=3.0)
                   or {}).get("min_detect_sep_us")
        ok = isinstance(cfg_sep, int) and cfg_sep < plan["gap_us"]
        rep.add("G.9", "configured min_detect_sep_us fits the real"
                       f" {params.get('min_gap_mm')}mm part gap", ok,
                f"board={cfg_sep}us, gap at speed = {plan['gap_us']:.0f}us"
                f" ({params.get('min_gap_mm')}mm at rim); a larger separation"
                f" MERGES adjacent parts. Pitch limit"
                f" ~{plan['pitch_rate_hz']:.0f} parts/s")

    # -- G.10 the REAL gate path: debounce + width filter ------------------
    # Phantoms bypass GateSensing entirely (trig_phantom_pulse calls
    # newPulseEvent directly), so the width filter is invisible to them.
    # Drive the gate pin itself instead: set it OUTPUT and pull it LOW
    # (sense is inverted -- LOW = beam blocked = part present) for a
    # controlled time. That walks debounce -> width filter -> registration
    # exactly as a real part does. A back-to-back LOW blip (~ms) must be
    # rejected by a 30ms minimum-width filter; an 80ms LOW must register,
    # announce once and sort cleanly.
    gate_pin = 27
    try:
        _grill_gate_test(link, rep, plan, orig, freq, tick_hz)
    except (LinkReset, LinkDead):
        rep.add("G.10", "real gate path (link dropped mid-test)", None,
                "USB link reset during the gate-pin drill; inconclusive")


def _grill_gate_test(link, rep, plan, orig, freq, tick_hz):
    gate_pin = 27
    minw = int(0.030 * tick_hz)
    maxw = int(0.500 * tick_hz)
    link.send({"type": "set_setup", "pulse_min_width": minw,
               "pulse_max_width": maxw}, timeout=2.0)
    link.send({"type": "pin_mode", "pin": gate_pin, "mode": "OUTPUT"},
              timeout=2.0)
    link.send({"type": "pin_on", "pin": gate_pin}, timeout=2.0)  # idle
    link.send({"type": "enter_insp_mode"}, timeout=3.0)
    _wait_at_speed(link)
    link.drain_async()

    # thin blip: two writes back-to-back, LOW for well under 30ms
    link.send_nowait({"type": "pin_off", "pin": gate_pin})
    link.send_nowait({"type": "pin_on", "pin": gate_pin})
    time.sleep(plan["transit_s"] * 0.2 + 0.5)
    ghosts = [m for _, m in link.drain_async() if m.get("type") == "cam_trig"]

    # real part: LOW for ~80ms (2400 ticks here), then answer it on announce
    link.send({"type": "pin_off", "pin": gate_pin}, timeout=2.0)
    time.sleep(0.08)
    link.send({"type": "pin_on", "pin": gate_pin}, timeout=2.0)
    real_tids = set()
    t_end = time.time() + plan["transit_s"] + 1.0
    while time.time() < t_end:
        for _, msg in link.drain_async():
            if msg.get("type") == "cam_trig":
                tid = msg.get("tid")
                if tid not in real_tids:
                    real_tids.add(tid)
                    link.send_nowait({"type": "report", "tid": tid,
                                      "cat": CAT_NA})
        time.sleep(0.005)
    st2, stat2 = _state(link)
    rep.add("G.10", "real gate path: width filter rejects a blip, accepts a"
                    " part, no error",
            not ghosts and len(real_tids) == 1 and st2 == ST_READY
            and not _errors_of(stat2),
            f"minWidth={minw} ticks ({minw * plan['mm_per_tick']:.1f}mm-eq):"
            f" blip -> {len(ghosts)} announcements; 80ms part ->"
            f" {len(real_tids)} object(s); state={st2}"
            f" ERROR_HIST={_errors_of(stat2)}")

    link.send({"type": "exit_insp_mode"}, timeout=3.0)
    link.send({"type": "pin_mode", "pin": gate_pin, "mode": "INPUT_PULLUP"},
              timeout=2.0)
    link.send({"type": "set_setup",
               "pulse_min_width": orig.get("pulse_min_width", 0),
               "pulse_max_width": orig.get("pulse_max_width", 1000)},
              timeout=2.0)
    time.sleep(max(2.0, freq / plan["accel"] + 1.0))

    _grill_teardown(link, orig)


def _grill_teardown(link, orig):
    orig_freq = orig.get("plate_freq")
    orig_accel = orig.get("plate_accel")
    orig_spo = _read_spo(link) or {}
    # -- teardown: put the board back the way we found it ------------------
    restore = {}
    if orig_freq is not None:
        restore["plate_freq"] = orig_freq
    if orig_accel is not None:
        restore["plate_accel"] = orig_accel
    if orig_spo:
        restore["stage_pulse_offset"] = orig_spo
    if restore:
        link.send({"type": "set_setup", **restore}, timeout=3.0)




# --- resetchaos: hard-reset survival, via the EN<-D5 wire -----------------

def resetchaos(link, rep, params, rounds):
    """Hard-reset the board at random moments and prove it always comes back
    clean. The trigger is the adapter's own RTS->EN line (the same circuit
    esptool uses): EN goes low while RTS is asserted and DTR is not, so a
    dtr=0 / rts-pulse sequence is electrically the reset button -- no extra
    wiring, no serial command racing its own death.

    Round phases: idle plate, spin-up ramp, mid-stream sorting, and the
    nastiest -- mid-NVS-save. Every round must end with: board back within
    10s, reset_reason POWERON, config image identical (NVS blob atomic),
    every actuator at logical OFF, and a working sort afterwards.
    """
    import random as _r
    plan = grill_plan(params)
    print("\n\033[1m== Resetchaos: %d hard resets via EN<-D5 ==\033[0m"
          % rounds)

    def _revive(t=12.0):
        deadline = time.time() + t
        while time.time() < deadline:
            try:
                link.ser.write(b'{"type":"RESET"}')
            except Exception:
                pass
            time.sleep(0.4)
            link.drain_async()
            if link.send({"type": "ping"}, timeout=1.0):
                return True
        return False

    def _push_cfg():
        link.send({"type": "set_setup", "plate_freq": 0,
                   "plate_accel": plan["accel"],
                   "min_detect_sep_us": plan["sep_us"] or 4000,
                   "pulse_min_width": 0,
                   "pulse_max_width": plan["max_w"] or 1600,
                   "stage_pulse_offset": plan["spo"]}, timeout=3.0)

    def _yank():
        try:
            link.ser.rts = True      # RTS is wired to EN: chip in reset
            time.sleep(0.15)
            link.ser.rts = False     # EN released: chip boots
        except Exception:
            pass

    # Baseline: one clean boot, push+persist the reference config once.
    if not _revive():
        rep.add("R.0", "board alive to start", False, "no contact")
        return
    _push_cfg()
    time.sleep(0.8)
    link.send({"type": "save_setup"}, timeout=5.0)
    base = link.send({"type": "get_setup"}, timeout=3.0) or {}
    base_crc = base.get("cfg_crc")
    io_on = base.get("io_on_level") or {}
    rep.add("R.0", "baseline config persisted", base_crc is not None,
            f"cfg_crc={base_crc}")

    pins = {"L1A": 16, "CAM1": 17, "L2A": 18, "CAM2": 19,
            "SEL1": 25, "SEL2": 26, "SEL3": 32, "FEEDER": 21}
    phases = ["idle", "ramp", "stream", "save"]
    fails = []
    for i in range(rounds):
        phase = phases[i % len(phases)]
        # -- arrange the phase, then yank at its most awkward moment
        if phase in ("ramp", "stream"):
            link.send({"type": "set_setup", "plate_freq": plan["freq"]},
                      timeout=2.0)
            link.send({"type": "stepper_enable"}, timeout=2.0)
            link.send({"type": "enter_insp_mode"}, timeout=2.0)
            if phase == "ramp":
                time.sleep(0.2)          # mid-acceleration
            else:
                _wait_at_speed(link)
                link.drain_async()
                for _ in range(4):       # parts in flight, verdicts pending
                    link.send_nowait({"type": "trig_phantom_pulse"})
                    time.sleep(0.05)
        elif phase == "save":
            link.send({"type": "set_setup", "plate_freq": 0}, timeout=2.0)
            time.sleep(0.8)
            link.send_nowait({"type": "save_setup"})
            time.sleep(_r.uniform(0.0, 0.02))   # land inside the NVS write
        _yank()
        t0 = time.time()
        alive = _revive()
        dt = time.time() - t0
        g = link.send({"type": "get_setup"}, timeout=3.0) or {}
        rr = g.get("reset_reason_name")
        crc = g.get("cfg_crc")
        rd = link.send({"type": "pin_read",
                        "pins": [pins[n] for n in pins]}, timeout=2.0) or {}
        vals = rd.get("vals") or []
        hot = [n for n, v in zip(pins, vals) if v == io_on.get(n, 1)]
        st, _stat = _state(link)
        probs = []
        if not alive:
            probs.append("no revival")
        if rr != "POWERON":
            probs.append(f"reset_reason={rr}")
        if crc != base_crc:
            probs.append(f"cfg_crc {base_crc}->{crc}")
        if not vals or hot:
            probs.append(f"outputs energised: {hot or 'unreadable'}")
        if st != ST_IDLE:
            probs.append(f"state={st}")
        tag = "ok" if not probs else "; ".join(probs)
        print("  [%2d/%d] %-6s revive %.1fs  %s"
              % (i + 1, rounds, phase, dt, tag), flush=True)
        if probs:
            fails.append((i + 1, phase, tag))
    rep.add("R.1", f"{rounds} hard resets across all phases survive clean",
            not fails, f"fails={fails[:4]}" if fails else
            f"all {rounds} rounds: POWERON, cfg intact, outputs OFF, IDLE")

    # -- prove the machine still sorts after the ordeal
    link.send({"type": "set_setup", "plate_freq": plan["freq"]}, timeout=2.0)
    link.send({"type": "stepper_enable"}, timeout=2.0)
    link.send({"type": "enter_insp_mode"}, timeout=2.0)
    _wait_at_speed(link)
    link.drain_async()
    answered = set()
    for _ in range(5):
        link.send({"type": "trig_phantom_pulse"}, timeout=2.0)
        t_end = time.time() + 0.4
        while time.time() < t_end:
            for _ts, m in link.drain_async():
                if m.get("type") == "cam_trig" and m.get("tid") not in answered:
                    answered.add(m.get("tid"))
                    link.send_nowait({"type": "report", "tid": m["tid"],
                                      "cat": CAT_NA})
            time.sleep(0.005)
    time.sleep(plan["transit_s"] + 0.5)
    st, stat = _state(link)
    rep.add("R.2", "sorts normally after the last reset",
            st == ST_READY and not _errors_of(stat),
            f"answered={len(answered)} state={st}"
            f" ERROR_HIST={_errors_of(stat)}")
    link.send({"type": "exit_insp_mode"}, timeout=2.0)
    time.sleep(2.0)
    link.send({"type": "set_setup", "plate_freq": 0}, timeout=2.0)



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
    # get_version stores the peer version and answers with the
    # firmware's own -- the core leans on this reply to know it is talking to
    # uInsp firmware and not, say, the CNC image that was on this very board.
    # The reply carries a HARDCODED id (100446), not the command's, so it
    # arrives as an async message rather than a matched reply -- drain for it.
    link.drain_async()
    link.send_nowait({"type": "get_version", "version": "probe-tool"})
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

    # --- P.3: trig_cam_pulse -------------------------------------------------
    # The camera-trigger simulation, distinct from trig_phantom_pulse: it
    # announces ONE cam_trig carrying the caller's trigger_id and pulses the
    # CAM/light pins, but does NOT call newPulseEvent, so no pipeline object is
    # created (Qs stays where it was). This is the announce path stage 1 leans
    # on -- verifiable before any camera is attached. A phantom pulse, by
    # contrast, announces twice (CAM1+CAM2) and does enqueue an object.
    link.drain_async()
    marker = 424242
    r = link.send({"type": "trig_cam_pulse", "trigger_id": marker},
                  timeout=3.0)
    time.sleep(0.5)
    anns = [m for _, m in link.drain_async() if m.get("type") == "cam_trig"]
    mine = [m for m in anns if m.get("tid") == marker]
    qs = mine[0].get("Qs") if mine else None
    rep.add("P.3", "trig_cam_pulse announces once with the caller's trigger_id,"
            " enqueues no object",
            bool(r) and len(mine) == 1 and mine[0].get("cam") == 1
            and qs == 0,
            f"announcements for {marker}: {len(mine)} "
            f"(cam={mine[0].get('cam') if mine else None}, Qs={qs}); "
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
# objects settle at the cap. Paced off the SET plate_freq (ISR = 2*freq), not a
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
    orig_freq = orig.get("plate_freq")
    orig_sep = orig.get("min_detect_sep_us")
    orig_spo = _read_spo(link) or {}
    link.send({"type": "clear_error"}, timeout=2.0)
    link.send({"type": "clear_error_history"}, timeout=2.0)

    def _pump(answer_cat, seen, answered, qs=None):
        for _, msg in link.drain_async():
            if msg.get("type") != "cam_trig":
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
            link.send({"type": "trig_phantom_pulse"}, timeout=3.0)
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
        link.send({"type": "set_setup", "plate_freq": 600}, timeout=3.0)
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
            link.send({"type": "trig_phantom_pulse"}, timeout=3.0)
            link.send_nowait({"type": "trig_phantom_pulse"})   # inside both gates
            time.sleep(0.2)
            link.send({"type": "trig_phantom_pulse"}, timeout=3.0)
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
            r1 = link.send({"type": "ping"}, timeout=1.0)      # eaten silently
            link.ser.write(b'{"type":"RESET"}')   # recovers and auto-redeems
            time.sleep(0.4)
            link.drain_async()
            r2 = link.send({"type": "ping"}, timeout=1.5)
            st, stat = _state(link)
            errs = _errors_of(stat)
            rep.add("E.5", "garbage latches the link: silent until one RESET"
                    " recovers it",
                    r1 is None and bool(r2 and r2.get("type") == "pong"),
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
            # No default station here either -- see _read_spo.
            l1a = orig_spo.get("L1A_on")
            if l1a is None:
                rep.add("E.6", "saturate RBuf and drain clean", False,
                        "get_setup never returned stage_pulse_offset")
                return
            win2 = int(l1a) + E6_WINDOW
            link.send({"type": "set_setup", "plate_freq": E6_FREQ,
                       "min_detect_sep_us": 5000,
                       "stage_pulse_offset": {
                           "SWITCH": win2,
                           "SEL1_on": win2 + 3,  "SEL1_off": win2 + 4,
                           "SEL2_on": win2 + 13, "SEL2_off": win2 + 14,
                           "SEL3_on": win2 + 23, "SEL3_off": win2 + 24}},
                      timeout=3.0)
            _wait_at_speed(link)

            # ISR ticks at 2*plate_freq, so steps/s is known exactly -- pace off
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
                link.send_nowait({"type": "trig_phantom_pulse"})
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
            restore["plate_freq"] = orig_freq
        if isinstance(orig_sep, (int, float)):
            restore["min_detect_sep_us"] = orig_sep
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
    WITHOUT widening the window. A low plate_freq is the trick: the real ~43-step
    camera-to-selector gap becomes tens of ms in wall time, long enough that the
    host verdict still lands in the true window, so SWITCH dispatches the part
    and the SEL edges fire on schedule too.
    """
    print("\n\033[1m== IO trace: actuator sequence at real geometry ==\033[0m")

    orig = link.send({"type": "get_setup"}, timeout=3.0) or {}
    orig_freq = orig.get("plate_freq")
    spo = dict(orig.get("stage_pulse_offset") or {})
    link.send({"type": "clear_error"}, timeout=2.0)
    link.send({"type": "clear_error_history"}, timeout=2.0)

    try:
        # Deliberately NOT widening the window -- the whole point is real
        # offsets. Low plate_freq stretches the 43-step gap into answerable ms.
        link.send({"type": "set_setup", "plate_freq": plate_freq}, timeout=3.0)
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
        link.send({"type": "trig_phantom_pulse"}, timeout=3.0)
        t_end = time.time() + 3.0
        while time.time() < t_end:
            for _, m in link.drain_async():
                if m.get("type") == "cam_trig":
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
            link.send({"type": "set_setup", "plate_freq": orig_freq},
                      timeout=3.0)
        st, _ = _state(link)
        rep.add("I.8", "returned to IDLE, plate_freq restored",
                st == ST_IDLE, f"state={ST_NAME.get(st, st)} "
                f"plate_freq={orig_freq}")


# --- publish path: does a set_setup offset reach the ISR? -----------------

def pubcheck(link, rep, plate_freq):
    """Prove STAGE_PULSE_OFFSET_publish() propagates a set_setup change all the
    way to the timer ISR.

    The firmware double-buffers the pulse offsets: set_setup edits a main-loop
    working copy, and a publish step hands it to the ISR by an atomic pointer
    swap (docs/CONCURRENCY_ANALYSIS.md 5.2). The one regression a missing
    publish would cause is invisible to iotrace, which only reads the offsets
    already in effect: get_setup would report the new value while the ISR kept
    firing at the old one.

    So change an offset, then read the ACTUAL edge back from io_trace and check
    it moved. If publish() were dropped from setMachineSetup, SEL1 would still
    fire at the old offset and I.P would fail even though get_setup looks right.
    """
    print("\n\033[1m== Publish check: set_setup offset reaches the ISR ==\033[0m")

    orig = link.send({"type": "get_setup"}, timeout=3.0) or {}
    orig_freq = orig.get("plate_freq")
    spo = dict(orig.get("stage_pulse_offset") or {})
    base_sel1_on = spo.get("SEL1_on")
    base_sel1_off = spo.get("SEL1_off")
    if not isinstance(base_sel1_on, int):
        rep.add("I.P", "read SEL1_on from get_setup", False, "no SEL1_on")
        return

    # Move SEL1 a few steps out and back within the same safe window. Small
    # enough that the verdict still lands before it, distinct enough that a
    # stale (un-published) offset is unambiguous.
    new_sel1_on = base_sel1_on + 5
    new_sel1_off = new_sel1_on + 1

    link.send({"type": "clear_error"}, timeout=2.0)
    link.send({"type": "clear_error_history"}, timeout=2.0)
    try:
        link.send({"type": "set_setup", "plate_freq": plate_freq}, timeout=3.0)
        # The change under test.
        link.send({"type": "set_setup", "stage_pulse_offset": {
            "SEL1_on": new_sel1_on, "SEL1_off": new_sel1_off}}, timeout=3.0)

        # Confirm the working copy took it (necessary but not sufficient -- this
        # is the part that was always fine).
        chk = (link.send({"type": "get_setup"}, timeout=3.0) or {}) \
            .get("stage_pulse_offset") or {}
        rep.add("I.P0", "set_setup updated the working copy",
                chk.get("SEL1_on") == new_sel1_on,
                f"SEL1_on now {chk.get('SEL1_on')} (was {base_sel1_on})")

        link.send({"type": "enter_insp_mode"}, timeout=3.0)
        _wait_at_speed(link)
        link.send({"type": "io_trace_arm"}, timeout=3.0)
        link.drain_async()

        answered = set()
        link.send({"type": "trig_phantom_pulse"}, timeout=3.0)
        t_end = time.time() + 3.0
        while time.time() < t_end:
            for _, m in link.drain_async():
                if m.get("type") == "cam_trig":
                    tid = m.get("tid")
                    if tid not in answered:
                        answered.add(tid)
                        link.send_nowait({"type": "report", "tid": tid, "cat": 1})
            time.sleep(0.002)

        dump = link.send({"type": "io_trace_dump"}, timeout=3.0) or {}
        ev = dump.get("ev") or []
        named = [(IOT_PIN.get(p, p), v, pulse, tid) for pulse, p, v, tid in ev]

        anchor = next((pulse for nm, v, pulse, _ in named
                       if nm == "L1A" and v == 1), None)
        gate = (anchor - spo.get("L1A_on", 0)) if anchor is not None else None
        sel1_on_pulse = next((pl for nm, v, pl, _ in named
                              if nm == "SEL1" and v == 1), None)
        actual = (sel1_on_pulse - gate) if (sel1_on_pulse is not None
                                            and gate is not None) else None

        # The real test: the ISR fired SEL1 at the NEW offset, not the old one.
        rep.add("I.P", "ISR fired SEL1 at the newly-set offset (publish works)",
                actual is not None and abs(actual - new_sel1_on) <= 1,
                f"SEL1_on edge at offset {actual}; set {new_sel1_on}, "
                f"old was {base_sel1_on}"
                + ("  <-- stale offset: STAGE_PULSE_OFFSET_publish() not "
                   "reaching the ISR" if actual == base_sel1_on else ""))

    finally:
        link.send({"type": "io_trace_stop"}, timeout=3.0)
        link.send({"type": "clear_error"}, timeout=3.0)
        link.send({"type": "exit_insp_mode"}, timeout=3.0)
        restore = {"type": "set_setup", "stage_pulse_offset": {
            "SEL1_on": base_sel1_on, "SEL1_off": base_sel1_off}}
        if isinstance(orig_freq, (int, float)):
            restore["plate_freq"] = orig_freq
        link.send(restore, timeout=3.0)
        back = (link.send({"type": "get_setup"}, timeout=3.0) or {}) \
            .get("stage_pulse_offset") or {}
        rep.add("I.P9", "restored SEL1 offset",
                back.get("SEL1_on") == base_sel1_on,
                f"SEL1_on back to {back.get('SEL1_on')}")


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
    ap.add_argument("--baud", type=int, default=230400)
    ap.add_argument("-v", "--verbose", action="store_true", help="log every frame")
    ap.add_argument("-o", "--out", default="uinsp_verify_report.md")
    sub = ap.add_subparsers(dest="cmd", required=True)

    sub.add_parser("ports", help="list serial ports")
    sub.add_parser("stage0", help="firmware-alone checks incl. NVS persistence")
    sub.add_parser("errorpath", help="stage 0.7 inspection-error behaviour")
    sub.add_parser("selectors", help="stage 3.1/3.2 outlet mapping")
    m = sub.add_parser("monitor", help="watch cam_trig / tid continuity")
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
                        "reset_running_stat, trig_cam_pulse -- board only, no rig")
    sub.add_parser("edge",
                   help="deep firmware paths: NA verdict, SKIP absorption, "
                        "pulse-gate rejection, SEL1 countdown, protocol-error "
                        "latch, RBuf saturation -- board only, no rig")
    it = sub.add_parser("iotrace",
                        help="dump the firmware's own record of the actuator "
                             "edge sequence and check it against the real "
                             "stage_pulse_offset -- board only, no scope")
    it.add_argument("--freq", type=float, default=200,
                    help="plate_freq; low stretches the real 43-step window "
                         "into answerable ms so SWITCH/SEL fire in-window")
    it.add_argument("--cat", type=int, default=1, help="1=SEL1 2=SEL2")
    pc = sub.add_parser("pubcheck",
                        help="prove a set_setup offset change reaches the ISR "
                             "(the STAGE_PULSE_OFFSET double-buffer) -- board only")
    pc.add_argument("--freq", type=float, default=200)
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

    pf = sub.add_parser("profile",
                        help="measure skip rate against object rate and find "
                             "where good throughput peaks -- the curve auto-rate "
                             "is currently guessing at")
    pf.add_argument("--start-hz", type=float, default=10.0)
    pf.add_argument("--max-hz", type=float, default=45.0)
    pf.add_argument("--step-hz", type=float, default=5.0)
    pf.add_argument("--parts", type=int, default=2500,
                    help="admitted parts per point; p to +-30%% needs ~2200 at "
                         "p=0.5%%, ~5000 for +-20%%")
    pf.add_argument("--max-seconds", type=float, default=180.0,
                    help="cap per point, in case the rate cannot be reached")
    pf.add_argument("--drop-frac", type=float, default=0.0,
                    help="leave this fraction of parts unanswered on purpose: "
                         "manufactures skips the way the machine really makes "
                         "them, so the instrument can be checked without a "
                         "camera (dry run only)")
    pf.add_argument("--cat", type=int, default=1)
    pf.add_argument("--plate-sweep", default=None,
                    help="comma-separated plate_freq values: spin the plate and "
                         "let the REAL parts set the rate instead of injecting "
                         "phantoms. The only mode where the end-to-end ratio "
                         "means anything, because a part has to pass the sensor "
                         "to be counted at `edges`.")

    ch = sub.add_parser("chaos",
                        help="randomized rate + plate speed + offset churn; "
                             "must survive without faulting -- board only")
    ch.add_argument("--seconds", type=float, default=20.0)
    ch.add_argument("--min-hz", type=float, default=30.0)
    ch.add_argument("--max-hz", type=float, default=40.0)
    ch.add_argument("--seed", type=int, default=None,
                    help="RNG seed; default random, printed so a run repeats")
    ch.add_argument("--persist-churn", action="store_true",
                    help="also attempt NVS saves mid-run and assert they are "
                         "refused (the plate is spinning); wears no flash")
    ch.add_argument("--verify-timing", action="store_true",
                    help="periodically spot-check that SWITCH/SEL land on the "
                         "current offset (catches a publish-path race); adds "
                         "brief low-load gaps")
    ch.add_argument("--burst", action="store_true",
                    help="periodic bursts of pulses 10ms apart (probes the "
                         "gate/queue under rapid back-to-back triggers)")
    ch.add_argument("--burst-every", type=float, default=5.0,
                    help="seconds between bursts (jittered +-40%%)")
    ch.add_argument("--burst-count", type=int, default=8,
                    help="pulses per burst, 10ms apart")
    ch.add_argument("--report-delay-ms", type=int, default=0,
                    help="report each verdict after a random 0..N ms delay "
                         "(simulate host latency; keep well under the window)")
    ch.add_argument("--report-shuffle", action="store_true",
                    help="let delayed reports reorder within the delay window "
                         "(out-of-order results; probes FIFO-assumption limits)")
    ch.add_argument("--expect-fault", action="store_true",
                    help="delay results PAST the window and assert the machine "
                         "error-stops (OBJECT_HAS_NO_INSP_RESULT) cleanly -- the "
                         "too-slow-inspection safety stop, under churn")
    ch.add_argument("--auto-reconnect", action=argparse.BooleanOptionalAction,
                    default=True,
                    help="survive a USB drop: reopen the port when it re-appears "
                         "and re-establish the board, instead of ending the run "
                         "(on by default; --no-auto-reconnect to disable). Lets a "
                         "soak run unattended for days across USB re-enumerations")

    gr = sub.add_parser("grill",
                        help="run the sorter at the REAL machine's speed and "
                             "geometry from a parameter file -- speed accuracy, "
                             "sustained stream with true in-flight depth, "
                             "verdict pattern, latency vs answer window, "
                             "part-gap / width-filter feasibility, fail-safe "
                             "outputs")
    gr.add_argument("--params", default="machine_params.json",
                    help="machine parameter file (JSON)")
    gr.add_argument("--min-rate", type=float, default=None,
                    help="with --max-rate: churn the part rate in this range")
    gr.add_argument("--max-rate", type=float, default=None)
    gr.add_argument("--burst-every", type=float, default=0.0,
                    help="every ~N s (jittered) fire a short burst; 0 = off")
    gr.add_argument("--burst-count", type=int, default=5)
    gr.add_argument("--burst-hz", type=float, default=100.0,
                    help="spacing inside a burst (100 = 10ms apart)")
    gr.add_argument("--seed", type=int, default=None)
    rc = sub.add_parser("resetchaos",
                        help="hard-reset the board at random moments via the "
                             "EN<-D5 wire; must always come back clean "
                             "(POWERON, config intact, outputs OFF, sorts)")
    rc.add_argument("--params", default="machine_params.json")
    rc.add_argument("--rounds", type=int, default=8)

    gr.add_argument("--seconds", type=float, default=30.0,
                    help="length of the sustained stream phase; 0 = run "
                         "FOREVER (soak: 30s heartbeat, survives USB drops, "
                         "stops when the machine error-stops)")

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
    # Opening the port resets the board through the modem-line bounce; give
    # the boot ROM + app startup a clear runway before first contact.
    time.sleep(2.5)
    link.ser.write(b'{"type":"RESET"}')
    time.sleep(0.4)
    link.drain_async()
    # Do not start a subcommand mid-boot. Wait until it
    # actually answers (RESET again each try: the escape hatch is idempotent).
    for _ in range(12):
        if link.send({"type": "ping"}, timeout=0.8):
            break
        try:
            link.ser.write(b'{"type":"RESET"}')
        except Exception:
            pass
        time.sleep(0.5)
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
        elif args.cmd == "pubcheck":
            pubcheck(link, rep, args.freq)
        elif args.cmd == "stress":
            stress(link, rep, args.start_hz, args.max_hz, args.step_hz,
                   args.dwell, args.cat, not args.no_report)
        elif args.cmd == "stall":
            stall(link, rep, args.hz, args.stall_seconds, args.cat)
        elif args.cmd == "profile":
            profile(link, rep, args.start_hz, args.max_hz, args.step_hz,
                    args.parts, args.max_seconds, args.drop_frac, args.cat,
                    [int(x) for x in args.plate_sweep.split(",")]
                    if args.plate_sweep else None)

        elif args.cmd == "chaos":
            link.auto_reconnect = args.auto_reconnect
            seed = (args.seed if args.seed is not None
                    else int(time.time() * 1000) & 0xFFFFFFFF)
            # Shuffling needs a delay window to reorder within; expecting a
            # fault needs a delay PAST the window to induce it.
            delay_ms = args.report_delay_ms
            if args.expect_fault and delay_ms == 0:
                delay_ms = 1200        # > the ~600ms worst-case window
            elif args.report_shuffle and delay_ms == 0:
                delay_ms = 60
            chaos(link, rep, args.seconds, args.min_hz, args.max_hz, seed,
                  persist=args.persist_churn, verify=args.verify_timing,
                  burst=args.burst, burst_every=args.burst_every,
                  burst_count=args.burst_count,
                  report_delay_ms=delay_ms, report_shuffle=args.report_shuffle,
                  expect_fault=args.expect_fault)
        elif args.cmd == "resetchaos":
            link.auto_reconnect = True
            pdir = os.path.dirname(os.path.abspath(__file__))
            ppath = args.params if os.path.exists(args.params) \
                else os.path.join(pdir, args.params)
            with open(ppath) as f:
                params = json.load(f)
            resetchaos(link, rep, params, args.rounds)
        elif args.cmd == "grill":
            link.auto_reconnect = True
            pdir = os.path.dirname(os.path.abspath(__file__))
            ppath = args.params if os.path.exists(args.params) \
                else os.path.join(pdir, args.params)
            with open(ppath) as f:
                params = json.load(f)
            seed = (args.seed if args.seed is not None
                    else int(time.time() * 1000) & 0xFFFFFFFF)
            grill(link, rep, params, args.seconds,
                  min_rate=args.min_rate, max_rate=args.max_rate,
                  burst_every=args.burst_every, burst_count=args.burst_count,
                  burst_hz=args.burst_hz, seed=seed)
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
