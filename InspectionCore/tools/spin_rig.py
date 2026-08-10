#!/usr/bin/env python3
"""spin_rig -- reproduce the core's THREAD SHAPE without the core.

Why this exists
---------------
The report path shows a latency tail that no single stage explains. Measured in
the real core over 98k frames (2026-08-10, plate freq 13000, budget 792ms):

    queue    avg  0.16ms  max 106.9ms
    inspect  avg 11.60ms  max 137.8ms
    wait     avg  0.43ms  max 309.4ms   <- report enqueued, thread not popping
    write    avg  0.15ms  max 182.6ms   <- the USB-serial segment
    e2e      avg 12.34ms  max 326.6ms

The wire is not the problem (write is under 0.5ms in 98.6% of frames), and the
tail lives mostly in `wait`: the send thread has work and is not running. That
is a statement about the machine and the scheduler as much as about our code --
but it was measured inside 200k lines of C++ that also touch a camera, a serial
port, cJSON, a log ring and a WebSocket. Any of those could be the real cause.

This rig strips all of it away and keeps only the shape:

    acquire  -> inspQueue(10)  -> inspect (spin) -> sendQueue(256) -> send
             -> viewQueue(10)  -> preview (compress)

Same rates, same queue depths, same drop-oldest policy, same five histograms
with the same bucket edges, so the numbers are directly comparable to the
core's. If the `wait` tail shows up here too, it is the platform and no amount
of restructuring the core removes it. If it does not, it is ours.

A negative result is worth as much as a positive one and costs the same run.

Deliberate constraints
----------------------
* stdlib only. Aravis lives in Homebrew's python, which is externally managed
  and cannot pip-install numpy or Pillow. Anything that needs a wheel is out.
* The spin must RELEASE THE GIL or the whole experiment is void: Python threads
  that hold the GIL serialise, which would model the core's thread pool as a
  single thread and produce a `wait` tail by construction. hashlib releases the
  GIL for buffers over 2047 bytes, and it also touches the frame memory the way
  a real inspection does, so it is the spin -- not time.sleep, not a counter
  loop.
* The send thread asks for QOS_CLASS_USER_INTERACTIVE through ctypes, because
  the core's does (wiringPanel.cpp). Without it the comparison is unfair in the
  direction that would manufacture the result we are looking for.

Usage
-----
    # No camera, no board: tests the scheduler/queue shape alone. Safe to run
    # any time EXCEPT during a soak -- it competes for the same cores.
    python3 spin_rig.py --source synthetic --minutes 30

    # Real camera at whatever ROI the core last set. Needs the core STOPPED;
    # the camera is exclusive.
    /opt/homebrew/bin/python3.13 spin_rig.py --source camera --minutes 30

    # Add a real tty write to the send path without touching the board.
    ... --sink pty

Do not run this while a soak is running. It burns the same cores the soak is
being measured on, and the 2026-08-10 investigation already lost one result to
exactly that kind of contamination.

Related, and NOT a duplicate
----------------------------
`Peripheral/uInspESP32/tools/uinsp_vision_loop.py` is the other Python rig that
drives this camera. It answers a different question -- gate -> trigger -> frame
-> verdict with the REAL board and the real timestamp pairing, i.e. "does the
verdict land on the right part". This one has no board and no pairing at all;
it asks "where does the latency tail come from". The camera setup here is
copied from there because that path is already proven.
"""

import argparse
import collections
import ctypes
import ctypes.util
import hashlib
import json
import os
import queue
import sys
import threading
import time
import zlib

# ---------------------------------------------------------------------------
# Histograms -- identical edges to wiringPanel.cpp's PERIF_HIST_EDGES_MS, so a
# reading here can be put next to a reading there without rescaling. Dense
# around the deadline on purpose: at plate freq 13000 the CAM->SWITCH budget is
# 792ms, so the count at or above 800 is the number of parts that went unjudged.
EDGES_MS = [0.5, 1, 2, 5, 10, 20, 50, 100, 200, 300, 400, 600, 800, 1200, 2000]


class Hist:
    """Single-writer by construction: each stage's histogram is fed by exactly
    one thread, so no lock. A reader may see a torn count; a bucket read one
    sample stale changes no conclusion."""

    __slots__ = ("bucket", "n", "sum_ms", "max_ms", "name")

    def __init__(self, name):
        self.name = name
        self.bucket = [0] * (len(EDGES_MS) + 1)
        self.n = 0
        self.sum_ms = 0.0
        self.max_ms = 0.0

    def add(self, ms):
        self.n += 1
        self.sum_ms += ms
        if ms > self.max_ms:
            self.max_ms = ms
        i = 0
        while i < len(EDGES_MS) and ms >= EDGES_MS[i]:
            i += 1
        self.bucket[i] += 1

    def label(self, i):
        if i == 0:
            return "<%g" % EDGES_MS[0]
        if i >= len(EDGES_MS):
            return ">=%g" % EDGES_MS[-1]
        return "%g-%g" % (EDGES_MS[i - 1], EDGES_MS[i])

    def line(self):
        nz = " ".join("%sms:%d" % (self.label(i), c)
                      for i, c in enumerate(self.bucket) if c)
        return ("[lat] %-7s n=%d avg=%.2fms max=%.1fms | %s"
                % (self.name, self.n,
                   self.sum_ms / self.n if self.n else 0.0, self.max_ms, nz))

    def as_dict(self):
        return {"n": self.n, "max_ms": round(self.max_ms, 3),
                "avg_ms": round(self.sum_ms / self.n, 4) if self.n else 0.0,
                "bucket": list(self.bucket)}


H = {k: Hist(k) for k in ("queue", "inspect", "wait", "write", "e2e")}


def now_us():
    # Monotonic, like the core's perif_now_us. Wall clock would let an NTP step
    # write a negative latency into a bucket.
    return time.monotonic_ns() // 1000


# ---------------------------------------------------------------------------
# Thread QoS. The core's PerifSendThread raises itself to USER_INTERACTIVE
# because the work is tiny and latency-critical -- and on Apple Silicon that
# also keeps it off the efficiency cores. A rig that skips this would find a
# `wait` tail caused by its own omission and call it a platform property.
QOS_CLASS_USER_INTERACTIVE = 0x21


def raise_qos():
    if sys.platform != "darwin":
        return "n/a (not darwin)"
    try:
        libc = ctypes.CDLL(ctypes.util.find_library("c"), use_errno=True)
        fn = libc.pthread_set_qos_class_self_np
        fn.restype = ctypes.c_int
        fn.argtypes = [ctypes.c_uint, ctypes.c_int]
        rc = fn(QOS_CLASS_USER_INTERACTIVE, 0)
        return "USER_INTERACTIVE" if rc == 0 else "FAILED rc=%d" % rc
    except Exception as e:                      # noqa: BLE001
        return "unavailable: %s" % e


# ---------------------------------------------------------------------------
class Frame:
    __slots__ = ("buf", "rx_us", "insp_us", "enq_us", "seq")

    def __init__(self, buf, seq):
        self.buf = buf
        self.seq = seq
        self.rx_us = now_us()
        self.insp_us = 0
        self.enq_us = 0


def drop_oldest_put(q, item, counter, name):
    """The core's policy everywhere: a full queue drops the OLDEST, because the
    freshest frame is the one whose part has not reached the ejector yet. Never
    block the producer -- blocking here is what turns a slow consumer into a
    stalled camera."""
    try:
        q.put_nowait(item)
    except queue.Full:
        try:
            q.get_nowait()
            counter[name] += 1
            q.put_nowait(item)
        except (queue.Empty, queue.Full):
            counter[name] += 1


# ---------------------------------------------------------------------------
def calibrate_spin(buf, target_ms):
    """How many hash rounds over this frame come to target_ms.

    Measured rather than assumed: the answer depends on the ROI size, the CPU
    and whether the thread lands on a P or E core, and a spin that is actually
    2ms when it claims 12ms would quietly remove the very load being tested."""
    rounds, t0 = 0, time.monotonic()
    while time.monotonic() - t0 < 0.20:
        hashlib.sha256(buf).digest()
        rounds += 1
    per_ms = (time.monotonic() - t0) * 1000.0 / rounds
    n = max(1, int(round(target_ms / per_ms)))
    return n, per_ms


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", choices=("synthetic", "camera"),
                    default="synthetic")
    ap.add_argument("--sink", choices=("null", "pty", "serial"), default="null")
    ap.add_argument("--serial-dev", default=None,
                    help="only with --sink serial. NOTE: opening the board's "
                         "port RESETS the ESP32 -- never while the machine runs")
    ap.add_argument("--minutes", type=float, default=30.0)
    ap.add_argument("--rate", type=float, default=36.5,
                    help="frames/s (production virtual-object rate)")
    ap.add_argument("--inspect-ms", type=float, default=11.6,
                    help="spin per frame; default = the core's measured avg")
    ap.add_argument("--workers", type=int, default=1,
                    help="inspection threads")
    ap.add_argument("--preview-fps", type=float, default=8.0,
                    help="0 disables the preview stage")
    ap.add_argument("--roi", default="318x424",
                    help="WxH used only by --source synthetic; camera mode "
                         "reads the region the core actually set")
    ap.add_argument("--report-s", type=float, default=60.0)
    ap.add_argument("--out", default=None, help="append JSON snapshots here")
    a = ap.parse_args()

    drops = collections.Counter()
    stop = threading.Event()

    # -- source -------------------------------------------------------------
    if a.source == "camera":
        import gi
        gi.require_version("Aravis", "0.8")
        from gi.repository import Aravis
        Aravis.update_device_list()
        cam = Aravis.Camera.new(None)
        dev = cam.get_device()
        # Both of these come from uinsp_vision_loop.py, which already proved
        # this camera out. The cache policy is not optional: with it enabled,
        # reads come back from a stale shadow copy and the geometry printed
        # below can disagree with the geometry in force.
        dev.set_register_cache_policy(Aravis.RegisterCachePolicy.DISABLE)
        cam.set_acquisition_mode(Aravis.AcquisitionMode.CONTINUOUS)
        # Free-run. The board is not in this experiment -- the trigger line is
        # the machine's, and asking for it here would need the plate running,
        # which is the thing being kept out of the measurement.
        cam.set_trigger(None)
        # Read the geometry, do not set it. The core owns the operating point;
        # a rig that imposes its own measures a machine that does not exist.
        # (soak_sched.py carries the same rule for DeviceReset.)
        x, y, w, h = cam.get_region()
        try:
            cam.set_frame_rate(float(a.rate))
        except Exception:                       # noqa: BLE001
            pass
        try:
            max_fps = dev.get_float_feature_value("ResultingFrameRate")
        except Exception:                       # noqa: BLE001
            max_fps = None
        print("[rig] camera region from device: %dx%d at (%d,%d)  payload %.2f MB"
              % (w, h, x, y, cam.get_payload() / 1e6))
        print("[rig] readout ceiling: %s fps, asking for %.1f"
              % ("%.1f" % max_fps if max_fps else "unknown", a.rate))
        if max_fps and a.rate > max_fps:
            # Readout, not exposure, sets the ceiling, and it scales with ROI
            # height. Above it frames are silently not produced -- which would
            # show up here as a rate that quietly does not match the core's and
            # a load that is correspondingly lighter.
            print("[rig] WARNING: above the readout ceiling -- the rig will run"
                  " SLOWER than the core it is being compared against")
        stream = cam.create_stream(None, None)
        payload = cam.get_payload()
        for _ in range(20):
            stream.push_buffer(Aravis.Buffer.new_allocate(payload))
        cam.start_acquisition()
    else:
        w, h = (int(v) for v in a.roi.lower().split("x"))
        print("[rig] synthetic frames %dx%d" % (w, h))

    frame_bytes = w * h
    # Content that does not compress to nothing -- a zeroed buffer would make
    # the preview stage ~free and understate its cost.
    template = bytes(bytearray((i * 37 + (i >> 8) * 11) & 0xFF
                               for i in range(frame_bytes)))

    rounds, per_ms = calibrate_spin(template, a.inspect_ms)
    print("[rig] spin: %d sha256 rounds over %d bytes = %.2fms target (%.4fms/round)"
          % (rounds, frame_bytes, rounds * per_ms, per_ms))

    inspQ = queue.Queue(10)
    viewQ = queue.Queue(10)
    sendQ = queue.Queue(256)

    # -- sink ---------------------------------------------------------------
    if a.sink == "pty":
        # A real tty write path -- termios, line discipline, the kernel side of
        # a serial port -- without the USB device or the board behind it. It
        # separates "the tty layer stalls" from "the USB-serial link stalls".
        master, slave = os.openpty()
        sink_fd = slave
        drain_fd = master

        def _drain():
            while not stop.is_set():
                try:
                    if not os.read(drain_fd, 65536):
                        break
                except OSError:
                    break
        threading.Thread(target=_drain, daemon=True).start()
        print("[rig] sink: pty (real tty write, no hardware)")
    elif a.sink == "serial":
        if not a.serial_dev:
            sys.exit("--sink serial needs --serial-dev")
        sink_fd = os.open(a.serial_dev, os.O_WRONLY | os.O_NOCTTY)
        print("[rig] sink: %s -- the board has just been RESET by this open"
              % a.serial_dev)
    else:
        sink_fd = os.open(os.devnull, os.O_WRONLY)
        print("[rig] sink: /dev/null (no tty, no hardware)")

    # -- threads ------------------------------------------------------------
    def acquire():
        seq = 0
        if a.source == "camera":
            while not stop.is_set():
                buf = stream.timeout_pop_buffer(1000000)
                if buf is None:
                    continue
                # Copy out before returning the buffer to the pool, exactly as
                # the core must: the pool reuses the memory immediately.
                data = buf.get_data()
                stream.push_buffer(buf)
                seq += 1
                f = Frame(data, seq)
                drop_oldest_put(inspQ, f, drops, "insp")
                if a.preview_fps:
                    drop_oldest_put(viewQ, f, drops, "view")
        else:
            period = 1.0 / a.rate
            nxt = time.monotonic()
            while not stop.is_set():
                nxt += period
                d = nxt - time.monotonic()
                if d > 0:
                    time.sleep(d)
                else:
                    # Do not try to catch up: a burst after a late wakeup would
                    # inject a load spike the real camera never produces.
                    nxt = time.monotonic()
                seq += 1
                f = Frame(template, seq)
                drop_oldest_put(inspQ, f, drops, "insp")
                if a.preview_fps:
                    drop_oldest_put(viewQ, f, drops, "view")

    def inspect():
        while not stop.is_set():
            try:
                f = inspQ.get(timeout=0.5)
            except queue.Empty:
                continue
            f.insp_us = now_us()
            H["queue"].add((f.insp_us - f.rx_us) / 1000.0)
            for _ in range(rounds):
                hashlib.sha256(f.buf).digest()
            f.enq_us = now_us()
            H["inspect"].add((f.enq_us - f.insp_us) / 1000.0)
            drop_oldest_put(sendQ, f, drops, "send")

    def preview():
        period = 1.0 / a.preview_fps
        last = 0.0
        while not stop.is_set():
            try:
                f = viewQ.get(timeout=0.5)
            except queue.Empty:
                continue
            t = time.monotonic()
            if t - last < period:       # the production per-verdict fps cap
                continue
            last = t
            # zlib stands in for the JPEG encode: same order of magnitude of
            # CPU over the same bytes, and it releases the GIL.
            zlib.compress(f.buf, 1)

    def send():
        qos = raise_qos()
        print("[rig] send thread qos: %s" % qos)
        while not stop.is_set():
            try:
                f = sendQ.get(timeout=0.5)
            except queue.Empty:
                continue
            pop = now_us()
            H["wait"].add((pop - f.enq_us) / 1000.0)
            # 24 bytes is the order of a verdict report on this link.
            payload = b"R%019d\n" % f.seq
            t0 = now_us()
            try:
                os.write(sink_fd, payload)
            except OSError:
                pass
            t1 = now_us()
            H["write"].add((t1 - t0) / 1000.0)
            H["e2e"].add((t1 - f.rx_us) / 1000.0)

    threads = [threading.Thread(target=acquire, daemon=True, name="acquire"),
               threading.Thread(target=send, daemon=True, name="send")]
    for i in range(a.workers):
        threads.append(threading.Thread(target=inspect, daemon=True,
                                        name="inspect%d" % i))
    if a.preview_fps:
        threads.append(threading.Thread(target=preview, daemon=True,
                                        name="preview"))
    for t in threads:
        t.start()

    # -- report -------------------------------------------------------------
    t_end = time.monotonic() + a.minutes * 60
    try:
        while time.monotonic() < t_end:
            time.sleep(a.report_s)
            for k in ("queue", "inspect", "wait", "write", "e2e"):
                print(H[k].line(), flush=True)
            print("[rig] drops: %s  q: insp=%d view=%d send=%d"
                  % (dict(drops), inspQ.qsize(), viewQ.qsize(), sendQ.qsize()),
                  flush=True)
            if a.out:
                with open(a.out, "a") as fh:
                    fh.write(json.dumps({
                        "t": time.strftime("%Y-%m-%dT%H:%M:%S"),
                        "edges_ms": EDGES_MS,
                        "hist": {k: H[k].as_dict() for k in H},
                        "drops": dict(drops)}) + "\n")
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        time.sleep(0.5)
        if a.source == "camera":
            try:
                cam.stop_acquisition()
            except Exception:               # noqa: BLE001
                pass
    print("[rig] done")


if __name__ == "__main__":
    main()
