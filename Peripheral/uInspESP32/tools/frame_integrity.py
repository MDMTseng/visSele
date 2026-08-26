#!/usr/bin/env python3
"""Every reply the board sends must be a WHOLE object, or an explicit error.

    python frame_integrity.py --port COM3

THIS REBOOTS THE BOARD (opening the port asserts DTR/RTS -- see board_query.py).
Use it on an idle bench board, never on a running machine.

What it is for
--------------
serializeJson(doc, buf, size) truncates when the result does not fit and reports
only how many bytes it wrote, so passing that length ships half an object. Seven
of the eight reply sites did exactly that, and dbg_printf/msg_printf advanced
their write pointer by vsnprintf's return value -- the length it WOULD have
written -- which walked past the end of a 500 byte buffer.

The reachable trigger is not exotic. recv_ERROR logs the offending bytes:

    dbg_printf("recv_ERROR:%d %s dat:%s", errorcode, dataBuff, hex)

dataBuff is 2048 bytes. So ANY garbage on the line longer than the debug buffer
went through the overflow -- line noise, a half-sent frame, a wrong baud rate.
That is why this probe sends garbage on purpose.

A check that cannot fail is not a check
---------------------------------------
Every assertion here requires replies to have actually arrived. An earlier
version of this file passed against a silent board, because "no malformed
frames" is trivially true when there are no frames -- which is the same trap
that produced three confident zeroes elsewhere in this project.
"""
import argparse
import json
import sys
import time

import serial

BAUD = 230400          # platformio.ini monitor_speed; 921600 is the UPLOAD speed
DBG_BUFF = 500         # dbgBuff in LegacyFirmware.cpp
REPLY_CAP = 3584       # the largest reply buffer
NL = bytes([10])       # every escape written through a heredoc in this project
                       # has arrived with the backslash eaten at least once


def crc16_ccitt(data):
    """Must match Data_JsonRaw_Layer::crc16_ccitt."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


class Probe:
    def __init__(self, port):
        self.ser = serial.Serial(port, BAUD, timeout=0.3)
        time.sleep(3.0)                    # the open reset the board; let it boot
        self.ser.reset_input_buffer()
        self.fails = 0

    def ok(self, cond, msg, detail=""):
        print(("PASS  " if cond else "FAIL  ") + msg + (("  -- " + detail) if detail else ""))
        if not cond:
            self.fails += 1

    def pump(self, seconds):
        """Collect whole top-level objects by counting braces, as the board does.

        Returns (frames, leftover). A non-empty leftover IS the failure this
        probe exists to catch: bytes that opened an object and never closed it.
        """
        buf, out, depth, instr, esc = "", [], 0, False, False
        t0 = time.time()
        while time.time() - t0 < seconds:
            for ch in self.ser.read(1024).decode("utf-8", "replace"):
                if depth == 0 and ch != "{":
                    continue
                buf += ch
                if esc:
                    esc = False
                    continue
                if ch == "\\":
                    esc = True
                    continue
                if ch == '"':
                    instr = not instr
                    continue
                if instr:
                    continue
                if ch == "{":
                    depth += 1
                elif ch == "}":
                    depth -= 1
                    if depth == 0:
                        out.append(buf)
                        buf = ""
        return out, buf

    def send(self, obj):
        # The real host's framing: JSON, then *CRC16 and a NEWLINE. The newline
        # is not decoration -- after a protocol error the layer goes to RESYNC,
        # which discards everything up to the next newline. A probe that omits
        # it sees the board answer CLEAR_ERROR_OK and then ignore every command
        # forever, and would report that as the board failing to recover. It is
        # the probe failing to speak the protocol.
        raw = json.dumps(obj, separators=(",", ":")).encode()
        raw += b"*%04X" % crc16_ccitt(raw) + NL
        self.ser.write(raw)
        self.ser.flush()

    def check_frames(self, frames, leftover, label):
        bad = []
        for f in frames:
            try:
                json.loads(f)
            except Exception as e:
                bad.append((str(e), f[:100]))
        if leftover.strip():
            bad.append(("unterminated object", leftover[:100]))
        self.ok(not bad, "%s: every frame is well-formed JSON" % label,
                "" if not bad else repr(bad[:1]))
        return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    args = ap.parse_args()

    p = Probe(args.port)
    p.pump(2.0)                            # drain boot chatter

    # --- the reply that is closest to its cap -----------------------------
    sizes = []
    guards = []
    bad = []
    for i in range(8):
        p.send({"type": "get_running_stat", "id": 9100 + i})
        frames, leftover = p.pump(1.5)
        bad += p_check(p, frames, leftover, sizes, guards)
    p.ok(len(sizes) >= 6, "get_running_stat actually replied (a silent board must not pass)",
         "n=%d" % len(sizes))
    p.ok(len(sizes) >= 6 and not bad, "no reply was a fragment",
         "" if not bad else repr(bad[:1]))
    if sizes:
        biggest = max(sizes)
        p.ok(biggest <= REPLY_CAP, "largest reply is within the buffer",
             "%d of %d bytes" % (biggest, REPLY_CAP))
        if biggest > REPLY_CAP * 0.8:
            print("  NOTE: within 20%% of the cap (%d/%d). The guard will start"
                  " firing before this is a fragment, but the payload is growing."
                  % (biggest, REPLY_CAP))
    for g in guards:
        print("  the overflow guard fired -- that is the designed outcome: %s" % g[:120])

    # --- the reachable overflow trigger -----------------------------------
    for n in (600, 1500, 2000):
        p.ser.write(b"G" * n)              # not JSON: goes through recv_ERROR
        p.ser.flush()
        frames, leftover = p.pump(2.5)
        label = "%d garbage bytes" % n
        p.ok(len(frames) > 0, "%s: the board still said something" % label,
             "frames=%d" % len(frames))
        p.check_frames(frames, leftover, label)
        # Only the debug frames: an unrelated get_running_stat reply can land in
        # this window and it is legitimately allowed to be bigger.
        dbgs = [f for f in frames if '"dbg"' in f[:16] or '"type":"dbg"' in f[:24]]
        longest = max((len(f) for f in dbgs), default=0)
        p.ok(dbgs, "%s: the error path actually logged something" % label,
             "dbg frames=%d of %d" % (len(dbgs), len(frames)))
        p.ok(longest <= DBG_BUFF + 20, "%s: no debug frame exceeds the debug buffer" % label,
             "longest=%d, buffer=%d" % (longest, DBG_BUFF))
        # clear_error, NOT RESET. Both escape the parser latch, but RESET runs
        # handleResetCommand and tears the link down -- the firmware's own
        # comment says it: "a clear_error is a request to continue, not to tear
        # the link down". Using RESET here made the board look dead afterwards,
        # which is the probe being wrong about the board, not the other way up.
        p.send({"type": "clear_error"})
        p.pump(1.5)

    # --- and it is still a working board ----------------------------------
    p.pump(1.0)
    alive = False
    for i in range(6):
        p.send({"type": "PING", "id": 8100 + i})
        frames, _ = p.pump(2.0)
        if any('"pong"' in f for f in frames):
            alive = True
            break
    p.ok(alive, "the board still answers PING after all of that")

    p.ser.close()
    print(("%d FAILURES" % p.fails) if p.fails else "--- all pass ---")
    return 1 if p.fails else 0


def p_check(p, frames, leftover, sizes, guards):
    bad = []
    for f in frames:
        try:
            j = json.loads(f)
            sizes.append(len(f))
            if isinstance(j, dict) and j.get("err") in ("buf_overflow", "doc_overflow"):
                guards.append(f)
        except Exception as e:
            bad.append((str(e), f[:90]))
    if leftover.strip():
        bad.append(("unterminated object", leftover[:90]))
    return bad


if __name__ == "__main__":
    sys.exit(main())
