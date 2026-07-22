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
