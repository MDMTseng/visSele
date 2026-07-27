#!/usr/bin/env python3
"""Offline tests for uinsp_test.py's framing and reply matching.

Runs without hardware by substituting a fake serial port that behaves like the
firmware: brace-framed JSON in, replies echoing the command id out, plus
unsolicited bTrigInfo traffic interleaved mid-frame to make sure the reader
splits the two streams correctly.

    python test_uinsp_test.py
"""

import json
import sys
import threading
import time
import types
import unittest

# uinsp_test hard-requires pyserial at import; stub enough of it to import the
# module under test on a machine that has not got it.
if "serial" not in sys.modules:
    try:
        import serial  # noqa: F401
    except ImportError:
        fake = types.ModuleType("serial")
        fake.Serial = object
        tools = types.ModuleType("serial.tools")
        lp = types.ModuleType("serial.tools.list_ports")
        lp.comports = lambda: []
        tools.list_ports = lp
        fake.tools = tools
        sys.modules["serial"] = fake
        sys.modules["serial.tools"] = tools
        sys.modules["serial.tools.list_ports"] = lp

import uinsp_test


class FakeSerial:
    """Minimal duck-type of serial.Serial that answers like the firmware."""

    def __init__(self, auto_reply=True):
        self.auto_reply = auto_reply
        self._out = bytearray()          # bytes the tool will read
        self._in = ""                    # bytes the tool has written
        self._lock = threading.Lock()
        self.written = []

    # -- tool-facing API ---------------------------------------------------
    def read(self, n):
        with self._lock:
            if not self._out:
                return b""
            chunk = bytes(self._out[:n])
            del self._out[:n]
        return chunk

    def write(self, data):
        text = data.decode()
        self.written.append(text)
        if self.auto_reply:
            try:
                msg = json.loads(text)
            except json.JSONDecodeError:
                return len(data)
            self.reply_to(msg)
        return len(data)

    def flush(self):
        pass

    def close(self):
        pass

    # -- test-facing API ---------------------------------------------------
    def feed(self, text):
        """Push raw bytes toward the tool, in arbitrary fragments."""
        with self._lock:
            self._out.extend(text.encode())

    def reply_to(self, msg):
        t = msg.get("type")
        rep = {"id": msg.get("id"), "ack": True}
        if t == "PING":
            rep["type"] = "PONG"
        elif t == "get_setup":
            rep.update({"type": "get_setup", "machine_id": "M1",
                        "cfg_from_nvs": True, "stage_pulse_offset": {"CAM1_on": 654},
                        "pulse_minWidth": 0, "pulse_maxWidth": 1000})
        else:
            rep["type"] = t
        self.feed(json.dumps(rep))


def make_link(fake):
    link = uinsp_test.UInspLink.__new__(uinsp_test.UInspLink)
    link.ser = fake
    link.verbose = False
    link._id = 1000
    link._pending = {}
    link._lock = threading.Lock()
    from collections import deque
    link._async = deque(maxlen=1000)
    link._async_ev = threading.Event()
    link._raw_log = deque(maxlen=1000)
    link._stop = False
    link._rx = threading.Thread(target=link._reader, daemon=True)
    link._rx.start()
    return link


class TestFraming(unittest.TestCase):

    def setUp(self):
        self.fake = FakeSerial(auto_reply=False)
        self.link = make_link(self.fake)

    def tearDown(self):
        self.link._stop = True
        time.sleep(0.08)

    def _wait_async(self, count, timeout=2.0):
        end = time.time() + timeout
        got = []
        while time.time() < end and len(got) < count:
            got += [m for _, m in self.link.drain_async()]
            time.sleep(0.02)
        return got

    def test_single_object(self):
        self.fake.feed('{"type":"bTrigInfo","tid":1}')
        got = self._wait_async(1)
        self.assertEqual(got[0]["tid"], 1)

    def test_split_across_reads(self):
        # The reader must not assume a message arrives in one chunk.
        for piece in ['{"type":"bTrig', 'Info","tid"', ':7}']:
            self.fake.feed(piece)
            time.sleep(0.03)
        got = self._wait_async(1)
        self.assertEqual(got[0]["tid"], 7)

    def test_back_to_back(self):
        self.fake.feed('{"type":"a","tid":1}{"type":"b","tid":2}')
        got = self._wait_async(2)
        self.assertEqual([m["tid"] for m in got], [1, 2])

    def test_nested_objects(self):
        self.fake.feed('{"type":"get_setup","stage_pulse_offset":{"a":1,"b":{"c":2}}}')
        got = self._wait_async(1)
        self.assertEqual(got[0]["stage_pulse_offset"]["b"]["c"], 2)

    def test_braces_inside_strings_do_not_confuse_depth(self):
        self.fake.feed('{"dbg":"a{b}c","tid":3}')
        got = self._wait_async(1)
        self.assertEqual(got[0]["tid"], 3)

    def test_escaped_quote_inside_string(self):
        self.fake.feed('{"dbg":"say \\"hi\\" {x}","tid":4}')
        got = self._wait_async(1)
        self.assertEqual(got[0]["tid"], 4)

    def test_garbage_between_messages_is_skipped(self):
        # Attaching mid-stream lands in the middle of noise; the tool resyncs
        # rather than latching an error the way the firmware would.
        self.fake.feed('garbage\r\n{"type":"x","tid":9}')
        got = self._wait_async(1)
        self.assertEqual(got[0]["tid"], 9)

    def test_poisoned_frame_resyncs(self):
        # The firmware's recv_ERROR dbg embeds RAW received bytes; a stray
        # quote in there flips the string parity and, before the resync guard,
        # silenced reception permanently (hardware-reproduced: every reply
        # after a protocol-error test vanished while commands kept executing).
        self.fake.feed('{"dbg":"recv_ERROR:1 dat:@@"@"}')     # parity broken
        time.sleep(1.3)                    # stale half-frame gets dropped
        self.fake.feed('{"type":"x","tid":5}')
        got = self._wait_async(1)
        self.assertTrue(got and got[0].get("tid") == 5,
                        "reader must recover from an unbalanced frame")

    def test_array_framing(self):
        self.fake.feed('[1,2,[3,4]]')
        end = time.time() + 1.0
        seen = []
        while time.time() < end and not seen:
            seen += [m for _, m in self.link.drain_async()]
            time.sleep(0.02)
        self.assertEqual(seen[0], [1, 2, [3, 4]])


class TestReplyMatching(unittest.TestCase):

    def setUp(self):
        self.fake = FakeSerial(auto_reply=True)
        self.link = make_link(self.fake)

    def tearDown(self):
        self.link._stop = True
        time.sleep(0.08)

    def test_ping_reply_returned(self):
        r = self.link.send({"type": "PING"}, timeout=2.0)
        self.assertIsNotNone(r, "reply was not routed back to send()")
        self.assertEqual(r["type"], "PONG")

    def test_id_is_injected_and_unique(self):
        self.link.send({"type": "PING"}, timeout=2.0)
        self.link.send({"type": "PING"}, timeout=2.0)
        ids = [json.loads(w)["id"] for w in self.fake.written]
        self.assertEqual(len(set(ids)), 2)

    def test_timeout_returns_none(self):
        self.fake.auto_reply = False
        self.assertIsNone(self.link.send({"type": "PING"}, timeout=0.3))

    def test_async_traffic_does_not_steal_a_reply(self):
        # bTrigInfo arriving between command and reply must not be mistaken for
        # the reply, and must still be visible to the monitor.
        self.fake.auto_reply = False

        def responder():
            time.sleep(0.1)
            self.fake.feed('{"type":"bTrigInfo","tid":42,"Qs":3}')
            time.sleep(0.05)
            sent = json.loads(self.fake.written[-1])
            self.fake.reply_to(sent)

        threading.Thread(target=responder, daemon=True).start()
        r = self.link.send({"type": "PING"}, timeout=3.0)
        self.assertIsNotNone(r)
        self.assertEqual(r["type"], "PONG")
        asyncs = [m for _, m in self.link.drain_async()]
        self.assertTrue(any(m.get("tid") == 42 for m in asyncs),
                        "bTrigInfo was swallowed instead of queued")

    def test_reply_without_id_goes_to_async(self):
        self.fake.auto_reply = False
        self.fake.feed('{"type":"systemInfo","state":112}')
        end = time.time() + 1.0
        seen = []
        while time.time() < end and not seen:
            seen += [m for _, m in self.link.drain_async()]
            time.sleep(0.02)
        self.assertEqual(seen[0]["type"], "systemInfo")


class FakeFirmware(FakeSerial):
    """Models enough of LegacyFirmware.cpp to exercise bench() offline.

    Reproduces the parts the bench actually asserts on: tids are issued per
    phantom pulse and announced via bTrigInfo, report{tid} only matches an
    outstanding object, an unmatched tid faults with
    INSP_RESULT_MATCHES_NO_OBJECT(1), and a part that reaches the selector with
    no verdict faults with OBJECT_HAS_NO_INSP_RESULT(2).
    """

    ST_IDLE, ST_READY, ST_ERROR = 100, 101, 112
    E_NO_OBJECT, E_NO_RESULT, E_PROTOCOL = 1, 2, 11
    RBUF_LEN = 100

    def __init__(self, judge_deadline=1.0, drop_announce=False,
                 min_sep_s=0.067):
        super().__init__(auto_reply=True)
        self.state = self.ST_IDLE
        self.plate_freq = 0
        self.step_count = 0
        self.tid = 0
        self.spo = {"L1A_on": 654, "CAM1_on": 654, "CAM2_on": 654,
                    "SWITCH": 697, "SEL1_on": 700, "SEL1_off": 701,
                    "SEL2_on": 710, "SEL2_off": 711,
                    "SEL3_on": 720, "SEL3_off": 721}
        self.pending = {}          # tid -> issued_at (UNSET, still judgeable)
        self.transit = {}          # tid -> (release_time, cat) judged/skipped,
                                   # still occupying RBuf until the selector
        self.counts = {"SEL1": 0, "SEL2": 0, "SEL3": 0, "NA": 0}
        self.errors = []
        self.sel1_cd = -1
        self.data_latched = False    # cleared by a RESET (substring or parsed)
        self._errbuf = ""
        self.judge_deadline = judge_deadline
        self.drop_announce = drop_announce
        self.min_sep_s = min_sep_s
        self._last_pulse_t = 0.0
        self._t = threading.Thread(target=self._tick, daemon=True)
        self._run = True
        self._t.start()

    def stop(self):
        self._run = False

    def _travel_s(self):
        """Announce-to-selector time at the current window and plate speed."""
        pf = max(1, self.plate_freq)
        return (self.spo["SWITCH"] - self.spo["L1A_on"] + 78) / (2.0 * pf)

    def _qdepth(self):
        return len(self.pending) + len(self.transit)

    def _release(self, cat):
        """A judged object reaching the selector (real: ACT_SWITCH dispatch)."""
        if cat == "SKIP":
            return
        if cat == 1:
            if self.sel1_cd == 0:      # countdown spent: quiet, no count
                return
            if self.sel1_cd > 0:
                self.sel1_cd -= 1
            self.counts["SEL1"] += 1
        elif cat == 2:
            self.counts["SEL2"] += 1
        elif cat == 3:
            self.counts["SEL3"] += 1
        elif cat == 0xFFFF:
            self.counts["NA"] += 1

    def _tick(self):
        while self._run:
            time.sleep(0.05)
            if self.plate_freq > 0 and self.state in (self.ST_READY,):
                self.step_count += int(self.plate_freq * 2 * 0.05)
            now = time.time()
            for tid, (release, cat) in list(self.transit.items()):
                if now >= release:
                    del self.transit[tid]
                    self._release(cat)
            # A pending object that reached the selector unjudged.
            for tid, born in list(self.pending.items()):
                if now - born > self.judge_deadline:
                    del self.pending[tid]
                    if self.state == self.ST_READY:
                        self.state = self.ST_ERROR
                        self.errors.append(self.E_NO_RESULT)

    # -- the protocol-error latch (write-level, like the firmware's data
    #    layer). Garbage latches + faults; while latched everything is eaten
    #    during a scan for RESET; the recovery routes through
    #    handleResetCommand, so one RESET clears the latch AND redeems. ------
    def write(self, data):
        text = data.decode()
        self.written.append(text)
        if self.data_latched:
            self._errbuf += text
            if '"type":"RESET"' in self._errbuf:
                self._on_framing_reset()
            return len(data)
        try:
            msg = json.loads(text)
        except json.JSONDecodeError:
            self.data_latched = True
            self._errbuf = ""
            if self.state == self.ST_READY:
                self.state = self.ST_ERROR
                self.errors.append(self.E_PROTOCOL)
            return len(data)
        if self.auto_reply:
            self.reply_to(msg)
        return len(data)

    def _on_framing_reset(self):
        """recv_RESET calls handleResetCommand: framing, the command lock and
        a latch-caused fault all recover on the FIRST reset."""
        self.data_latched = False
        self._errbuf = ""
        if self.state == self.ST_ERROR and self.E_PROTOCOL in self.errors:
            self.state = self.ST_READY
        self.feed('{"type":"RESET_OK"}')

    def reply_to(self, msg):
        t = msg.get("type")
        if t == "RESET":
            self.feed('{"type":"RESET_OK"}')
            return

        rep = {"id": msg.get("id"), "ack": True}

        if t == "PING":
            rep["type"] = "PONG"
        elif t == "get_setup":
            rep.update({"type": "get_setup", "machine_id": "BENCH",
                        "cfg_from_nvs": True, "plateFreq": self.plate_freq,
                        "SYS_STEP_COUNT": self.step_count,
                        "stage_pulse_offset": dict(self.spo),
                        "pulse_minWidth": 0, "pulse_maxWidth": 1000})
        elif t == "set_setup":
            if "plateFreq" in msg:
                self.plate_freq = msg["plateFreq"]
            if "minDetectTimeSep_us" in msg:
                self.min_sep_s = msg["minDetectTimeSep_us"] / 1e6
            if "stage_pulse_offset" in msg:
                self.spo.update(msg["stage_pulse_offset"])
            rep["type"] = "set_setup"
        elif t == "enter_insp_mode":
            self.state = self.ST_READY
            rep["type"] = "enter_insp_mode"
        elif t == "exit_insp_mode":
            self.state = self.ST_IDLE
            self.pending.clear()
            self.transit.clear()
            rep["type"] = "exit_insp_mode"
        elif t == "clear_error":
            self.state = self.ST_IDLE
            self.pending.clear()
            self.transit.clear()
            rep["type"] = "clear_error"
        elif t == "clear_error_history":
            self.errors.clear()
            rep["type"] = "clear_error_history"
        elif t == "set_sel1_cd":
            self.sel1_cd = msg.get("count", 0)
            rep["type"] = "set_sel1_cd"
        elif t == "get_sel1_cd":
            rep.update({"type": "get_sel1_cd", "sel1_cd": self.sel1_cd})
        elif t == "get_running_stat":
            rep.update({"type": "get_running_stat", "state": self.state,
                        "count": dict(self.counts), "ERROR_HIST": list(self.errors),
                        "plateFreq": self.plate_freq, "sel1_cd": self.sel1_cd})
        elif t == "trig_phamton_pulse":
            now = time.time()
            if (self.state == self.ST_READY and
                    now - self._last_pulse_t >= self.min_sep_s):
                self._last_pulse_t = now
                if self._qdepth() < self.RBUF_LEN:   # RBuf full: silent reject
                    self.tid += 1
                    self.pending[self.tid] = now
                    if not self.drop_announce:
                        # Real firmware announces each object twice: CAM1
                        # (tidx=1) and CAM2 (tidx=2), same tid, same offset.
                        for tidx in (1, 2):
                            self.feed(json.dumps({"type": "bTrigInfo",
                                                  "tidx": tidx, "usH": 0,
                                                  "usL": int(now * 1e6) & 0xFFFFFFFF,
                                                  "tid": self.tid,
                                                  "Qs": self._qdepth()}))
            rep["type"] = "trig_phamton_pulse"
        elif t == "report":
            self._report(msg.get("tid"), msg.get("cat"))
            return   # firmware sets doRsp=false for report
        else:
            rep["type"] = t
        self.feed(json.dumps(rep))

    def _report(self, tid, cat):
        if tid in self.pending:
            travel = self._travel_s()
            # Older UNSET objects are marked SKIP by the scan toward the
            # reported tid -- the FIFO's desync absorber.
            for k in list(self.pending):
                if k < tid:
                    self.transit[k] = (self.pending.pop(k) + travel, "SKIP")
            born = self.pending.pop(tid)
            self.transit[tid] = (born + travel, cat)
        else:
            if self.state == self.ST_READY:
                self.state = self.ST_ERROR
                self.errors.append(self.E_NO_OBJECT)


class TestBench(unittest.TestCase):

    def _run_bench(self, fw, **kw):
        link = make_link(fw)
        rep = uinsp_test.Report()
        try:
            uinsp_test.bench(link, rep,
                             kw.get("count", 4), kw.get("freq", 1000),
                             kw.get("interval_ms", 120), kw.get("cat", 1))
        finally:
            link._stop = True
            fw.stop()
            time.sleep(0.08)
        return {r[0]: r for r in rep.rows}

    def test_happy_path(self):
        fw = FakeFirmware(judge_deadline=5.0)
        rows = self._run_bench(fw, count=4)
        self.assertTrue(rows["B.3"][2], "timer-running check should pass")
        self.assertTrue(rows["B.4"][2], "should reach READY")
        self.assertTrue(rows["B.5"][2], f"1:1 pulses->bTrigInfo: {rows['B.5'][3]}")
        self.assertTrue(rows["B.6"][2], "tid should be contiguous")
        self.assertTrue(rows["B.8"][2], f"SEL1 count: {rows['B.8'][3]}")

    def test_unknown_tid_is_detected(self):
        fw = FakeFirmware(judge_deadline=5.0)
        rows = self._run_bench(fw, count=3)
        self.assertTrue(rows["B.9"][2],
                        "reporting a bogus tid must be seen to fault the machine")

    def test_unjudged_part_is_detected(self):
        fw = FakeFirmware(judge_deadline=1.0)
        rows = self._run_bench(fw, count=3)
        self.assertTrue(rows["B.11"][2], "board must still answer")
        self.assertTrue(rows["B.12"][2],
                        f"unjudged part should fault: {rows['B.12'][3]}")

    def test_missing_announcement_is_caught(self):
        # If bTrigInfo never arrives the bench must fail B.5 rather than
        # quietly reporting success on zero parts.
        fw = FakeFirmware(judge_deadline=5.0, drop_announce=True)
        rows = self._run_bench(fw, count=3)
        self.assertFalse(rows["B.5"][2],
                         "missing bTrigInfo must be reported as a failure")

    def test_too_fast_pulses_are_reported_not_hidden(self):
        # Firing faster than SYS_MIN_PULSE_TIME_SEP_us makes the firmware drop
        # pulses; the bench should surface that as a 1:1 failure with a hint.
        fw = FakeFirmware(judge_deadline=5.0, min_sep_s=0.5)
        rows = self._run_bench(fw, count=4, interval_ms=60)
        self.assertFalse(rows["B.5"][2])
        self.assertIn("interval-ms", rows["B.5"][3])


class FakeFirmwareRateLimited(FakeFirmware):
    """Adds the throughput ceiling: a 20-deep comm queue drained at a fixed
    rate, which faults with INSP_CAM_TRIG_INFO_CANNOT_BE_SENT(10) when it
    overflows -- the real firmware's behaviour under load."""

    E_CANNOT_SEND = 10

    def __init__(self, drain_hz=40, **kw):
        self.commq = 0
        self.drain_hz = drain_hz
        super().__init__(**kw)

    def _tick(self):
        while self._run:
            time.sleep(0.05)
            if self.plate_freq > 0 and self.state == self.ST_READY:
                self.step_count += int(self.plate_freq * 2 * 0.05)
            drained = min(self.commq, max(1, int(self.drain_hz * 0.05)))
            self.commq -= drained
            now = time.time()
            for tid, (release, cat) in list(self.transit.items()):
                if now >= release:
                    del self.transit[tid]
                    self._release(cat)
            for tid, born in list(self.pending.items()):
                if now - born > self.judge_deadline:
                    del self.pending[tid]
                    if self.state == self.ST_READY:
                        self.state = self.ST_ERROR
                        self.errors.append(self.E_NO_RESULT)

    def reply_to(self, msg):
        if msg.get("type") == "trig_phamton_pulse" and self.state == self.ST_READY:
            now = time.time()
            if now - self._last_pulse_t >= self.min_sep_s:
                if self.commq >= 20:
                    self.state = self.ST_ERROR
                    self.errors.append(self.E_CANNOT_SEND)
                    self.feed(json.dumps({"id": msg.get("id"), "ack": True,
                                          "type": "trig_phamton_pulse"}))
                    return
                self.commq += 1
        super().reply_to(msg)


class TestStress(unittest.TestCase):

    def _run(self, fw, fn, *a):
        link = make_link(fw)
        rep = uinsp_test.Report()
        try:
            fn(link, rep, *a)
        finally:
            link._stop = True
            fw.stop()
            time.sleep(0.08)
        return {r[0]: r for r in rep.rows}

    def test_ramp_finds_a_ceiling(self):
        # Drain slower than the offered rate, so the queue must overflow.
        fw = FakeFirmwareRateLimited(drain_hz=25, judge_deadline=30.0,
                                     min_sep_s=0.0)
        rows = self._run(fw, uinsp_test.stress, 10, 60, 20, 1.0,
                         uinsp_test.CAT_NA, True)
        self.assertIn("S.1", rows)
        self.assertIn("S.2", rows)
        self.assertIn("comm queue overflow", rows["S.2"][3],
                      f"should name the failure mode, got: {rows['S.2'][3]}")

    def test_ramp_reports_a_clean_rate_when_nothing_breaks(self):
        fw = FakeFirmwareRateLimited(drain_hz=10000, judge_deadline=30.0,
                                     min_sep_s=0.0)
        rows = self._run(fw, uinsp_test.stress, 10, 30, 10, 0.6,
                         uinsp_test.CAT_NA, True)
        self.assertTrue(rows["S.1"][2], f"expected a clean rate: {rows['S.1'][3]}")

    def test_stall_is_detected_as_a_fault(self):
        fw = FakeFirmware(judge_deadline=1.0, min_sep_s=0.0)
        rows = self._run(fw, uinsp_test.stall, 10, 3.0, uinsp_test.CAT_NA)
        self.assertTrue(rows["T.2"][2],
                        f"unanswered parts must fault: {rows['T.2'][3]}")
        self.assertTrue(rows["T.3"][2], "board must still answer")


class FakeFirmwareNoSkip(FakeFirmware):
    """Regression model: a report matches its own tid but does NOT mark older
    unanswered objects SKIP -- they ride to the selector unjudged and fault."""

    def _report(self, tid, cat):
        if tid in self.pending:
            born = self.pending.pop(tid)
            self.transit[tid] = (born + self._travel_s(), cat)
        else:
            if self.state == self.ST_READY:
                self.state = self.ST_ERROR
                self.errors.append(self.E_NO_OBJECT)


class FakeFirmwareNoLatch(FakeFirmware):
    """Regression model: garbage is shrugged off instead of latching -- the
    board keeps answering as if nothing happened."""

    def write(self, data):
        text = data.decode()
        self.written.append(text)
        try:
            msg = json.loads(text)
        except json.JSONDecodeError:
            return len(data)     # ignore garbage: no latch, no fault
        if self.auto_reply:
            self.reply_to(msg)
        return len(data)


class TestEdge(unittest.TestCase):

    def _run_edge(self, fw, only=None):
        link = make_link(fw)
        rep = uinsp_test.Report()
        try:
            uinsp_test.edge(link, rep, only=only)
        finally:
            link._stop = True
            fw.stop()
            time.sleep(0.08)
        return {r[0]: r for r in rep.rows}

    def test_edge_happy_path(self):
        fw = FakeFirmware(judge_deadline=1.5)
        rows = self._run_edge(fw)
        for ref in ("E.1", "E.2", "E.3", "E.4", "E.5", "E.5b", "E.6", "E.7"):
            self.assertIn(ref, rows)
            self.assertTrue(rows[ref][2], f"{ref}: {rows[ref][3]}")

    def test_missing_skip_absorption_is_detected(self):
        # If a newer report stops SKIPping older unanswered objects, E.2 must
        # go red -- that silent absorber is what keeps a desynced FIFO from
        # stopping the line on every recoverable hiccup.
        fw = FakeFirmwareNoSkip(judge_deadline=1.5)
        rows = self._run_edge(fw, only={"E.2"})
        self.assertFalse(rows["E.2"][2],
                         f"missing SKIP absorption must fail E.2: {rows['E.2'][3]}")

    def test_missing_protocol_latch_is_detected(self):
        # If garbage stops latching the link, commands after a corrupted
        # stream would execute on whatever half-frame the parser salvages --
        # E.5 must notice the board kept answering.
        fw = FakeFirmwareNoLatch(judge_deadline=1.5)
        rows = self._run_edge(fw, only={"E.5"})
        self.assertFalse(rows["E.5"][2],
                         f"a board that shrugs off garbage must fail E.5: "
                         f"{rows['E.5'][3]}")


class TestReport(unittest.TestCase):

    def test_summary_counts(self):
        rep = uinsp_test.Report()
        rep.add("1.1", "a", True)
        rep.add("1.2", "b", False)
        rep.add("1.3", "c", None)
        self.assertFalse(rep.summary())
        self.assertEqual(len(rep.rows), 3)

    def test_markdown_escapes_pipes(self):
        import tempfile, os
        rep = uinsp_test.Report()
        rep.add("1.1", "a", True, "x|y")
        path = os.path.join(tempfile.mkdtemp(), "r.md")
        rep.write_markdown(path)
        with open(path, encoding="utf-8") as fh:
            body = fh.read()
        self.assertIn("x\\|y", body)


if __name__ == "__main__":
    unittest.main(verbosity=2)
