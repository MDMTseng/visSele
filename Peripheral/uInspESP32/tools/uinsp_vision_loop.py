#!/usr/bin/env python3
"""uinsp_vision_loop -- close the loop: gate -> light/trigger -> image -> verdict.

The panel's auto-report answers every part from a canned pattern; this answers
from the actual picture. One object on the plate produces:

    gate pulse -> firmware schedules the stations -> L1A fires at its offset
      -> that same edge triggers the camera (this rig wires the trigger to the
         backlight, see docs) -> a frame lands over USB3
      -> cam_trig arrives over serial carrying the tid
      -> this loop pairs the two, thresholds the silhouette, and reports a
         verdict before the SWITCH deadline

It is the smallest honest vertical slice of the real machine: the timing
budget, the frame<->tid pairing and the fail-to-answer path are all the
production ones. The verdict rule itself is deliberately trivial (dark-pixel
fraction of a backlit silhouette) -- Core0_1 owns the real inspection.

Needs the serial port to itself: stop uinsp_panel.py first.

    /opt/homebrew/bin/python3.13 uinsp_vision_loop.py --port /dev/cu.usbserial-0001 \
        --parts 20 --rate 2 --plate-freq 2000

Runs on Homebrew's python (Aravis binding). No numpy there, so the image
statistic is computed with bytes.translate + count, which is C-speed.
"""

import argparse
import json
import sys
import threading
import time

import gi
gi.require_version("Aravis", "0.8")
from gi.repository import Aravis  # noqa: E402

from uinsp_test import UInspLink  # noqa: E402

CAT_OK = 1
CAT_NG = 2
CAT_NA = 0xFFFF


_DARK_TABLES = {}


def dark_fraction(data, thresh):
    """Fraction of pixels darker than thresh.

    A backlit part is a hole in a bright field, so this is the silhouette area
    normalised by the frame -- enough to tell 'part present and the right size'
    from 'nothing there' or 'two stuck together'.
    """
    table = _DARK_TABLES.get(thresh)
    if table is None:
        table = _DARK_TABLES[thresh] = bytes(1 if i < thresh else 0 for i in range(256))
    return data.translate(table).count(1) / len(data)


class VisionLoop:
    def __init__(self, args):
        self.args = args
        self.link = None
        self.cam = None
        self.stream = None
        self.answered = set()
        self.stop = False
        self.stats = {"parts": 0, "ok": 0, "ng": 0, "na": 0, "no_frame": 0,
                      "unlit": 0, "fid_gaps": 0, "flushed": 0,
                      "lat_sum": 0.0, "lat_max": 0.0}
        self.last_fid = None
        self.max_fps = None
        self.rows = []

    # -- setup ---------------------------------------------------------------

    def open_camera(self):
        cam = Aravis.Camera.new(self.args.camera)
        dev = cam.get_device()
        dev.set_register_cache_policy(Aravis.RegisterCachePolicy.DISABLE)
        cam.set_acquisition_mode(Aravis.AcquisitionMode.CONTINUOUS)
        cam.set_trigger(self.args.source)
        try:
            dev.set_string_feature_value("TriggerActivation", self.args.edge)
        except Exception:
            pass
        if self.args.expo:
            cam.set_exposure_time(float(self.args.expo))
        # Readout, not exposure, sets the ceiling: this sensor does 7.5 fps at
        # full height and scales linearly with it (512 rows -> 30 fps). Parts
        # arriving faster than ResultingFrameRate are silently lost, so the ROI
        # is a throughput decision, not a cosmetic one.
        if self.args.roi_height:
            full_w, full_h = cam.get_sensor_size()
            dev.set_integer_feature_value("OffsetY", 0)
            dev.set_integer_feature_value("Height", self.args.roi_height)
            dev.set_integer_feature_value(
                "OffsetY", max(0, (full_h - self.args.roi_height) // 2))
        try:
            self.max_fps = dev.get_float_feature_value("ResultingFrameRate")
        except Exception:
            self.max_fps = None
        stream = cam.create_stream(None, None)
        for _ in range(self.args.buffers):
            stream.push_buffer(Aravis.Buffer.new_allocate(cam.get_payload()))
        cam.start_acquisition()
        self.cam, self.stream = cam, stream
        print("camera: %s %s  exposure %.0f us  trigger %s/%s"
              % (cam.get_vendor_name(), cam.get_model_name(),
                 cam.get_exposure_time(), self.args.source, self.args.edge))
        print("frame: %dx%d  %.1f MB  max %.1f fps (readout limit)"
              % (cam.get_region()[2], cam.get_region()[3], cam.get_payload() / 1e6,
                 self.max_fps or 0))
        if self.max_fps and self.args.rate > self.max_fps:
            print("WARNING: asking for %.1f parts/s but the camera tops out at"
                  " %.1f fps -- triggers above that are lost. Lower --roi-height."
                  % (self.args.rate, self.max_fps))

    def configure_board(self):
        lk = self.link
        setup = lk.send({"type": "get_setup"}, timeout=3.0) or {}
        self.orig_freq = setup.get("plate_freq")
        spo = dict(setup.get("stage_pulse_offset") or {})
        self.orig_spo = dict(spo)

        # Trigger window: the camera fires on the L1A rising edge, so only the
        # ON edge matters for timing -- the width just has to outlast the
        # exposure (measured floor is ~100us; see cam_grab.py pulsetest).
        if self.args.light_ticks:
            spo["L1A_off"] = spo["L1A_on"] + self.args.light_ticks
        if self.args.sel1_on:
            # The blow station's real distance from the gate. SEL1_off keeps its
            # width, so the jet duration is unchanged.
            width = spo["SEL1_off"] - spo["SEL1_on"]
            spo["SEL1_on"] = self.args.sel1_on
            spo["SEL1_off"] = self.args.sel1_on + width
        if self.args.switch:
            spo["SWITCH"] = self.args.switch
        r = lk.send({"type": "set_setup", "stage_pulse_offset": spo,
                     "plate_freq": self.args.plate_freq}, timeout=3.0)
        if not (r and r.get("ack")):
            raise SystemExit("set_setup failed: %r" % (r,))

        window = spo["SWITCH"] - spo["L1A_on"]
        ticks_s = 2 * self.args.plate_freq
        self.window_ms = window / ticks_s * 1000.0
        print("trigger at L1A_on=%d (+%d ticks of light), SWITCH=%d"
              % (spo["L1A_on"], spo["L1A_off"] - spo["L1A_on"], spo["SWITCH"]))
        print("answer window = %d ticks = %.0f ms at plate_freq %d (%d ticks/s)"
              % (window, self.window_ms, self.args.plate_freq, ticks_s))

    # -- the loop ------------------------------------------------------------

    def grab(self, timeout_s):
        """Pop the frame this object's light pulse produced.

        Returns (data, frame_id) or (None, None). Also tracks frame_id gaps:
        the camera drops triggers it cannot keep up with, and a dropped frame
        would otherwise shift every later frame onto the wrong tid.
        """
        t0 = time.time()
        while time.time() - t0 < timeout_s:
            buf = self.stream.timeout_pop_buffer(int(0.05 * 1e6))
            if buf is None:
                continue
            ok = buf.get_status().value_nick == "success"
            data = buf.get_data() if ok else None
            fid = buf.get_frame_id()
            self.stream.push_buffer(buf)
            if ok:
                if self.last_fid is not None and fid != self.last_fid + 1:
                    self.stats["fid_gaps"] += 1
                self.last_fid = fid
                return data, fid
        return None, None

    def flush(self):
        """Drop everything queued -- used after a miss, so one lost frame does
        not pair every subsequent frame with the wrong object."""
        n = 0
        while self.stream.try_pop_buffer():
            n += 1
        if n:
            self.stats["flushed"] += n
        self.last_fid = None

    def on_cam_trig(self, obj):
        tid = obj.get("tid")
        # Each object announces at CAM1 and CAM2; only the first is ours (the
        # trigger wire rides L1A, so exactly one frame exists per object).
        if tid is None or tid in self.answered:
            return
        self.answered.add(tid)
        t_evt = time.time()

        data, _fid = self.grab(self.args.frame_timeout)
        if data is None:
            # Fail-to-answer is the dangerous path: never leave the object
            # unanswered (that stops the line), send NA and count it.
            self.stats["no_frame"] += 1
            self.flush()
            cat, verdict, frac = CAT_NA, "NO-FRAME", None
        else:
            frac = dark_fraction(data, self.args.thresh)
            if frac > 0.99:
                # Not a silhouette -- an unlit frame. Happens when the camera is
                # triggered faster than it can read out; the picture is useless,
                # so say so instead of calling a black frame an oversized part.
                self.stats["unlit"] += 1
                self.flush()
                cat, verdict = CAT_NA, "UNLIT"
            elif frac < self.args.min_area:
                cat, verdict = CAT_NA, "EMPTY"
            elif frac > self.args.max_area:
                cat, verdict = CAT_NG, "TOO-BIG"
            else:
                cat, verdict = CAT_OK, "OK"

        self.link.send_nowait({"type": "report", "tid": tid, "cat": cat})
        lat = (time.time() - t_evt) * 1000.0
        self.stats["parts"] += 1
        self.stats["lat_sum"] += lat
        self.stats["lat_max"] = max(self.stats["lat_max"], lat)
        self.stats["ok" if cat == CAT_OK else "ng" if cat == CAT_NG else "na"] += 1
        self.rows.append((tid, verdict, frac, lat, obj.get("gate_pulse")))
        print("  tid %-6s %-8s dark %-7s answered in %5.1f ms  (%.0f%% of window)"
              % (tid, verdict, "%.3f%%" % (frac * 100) if frac is not None else "-",
                 lat, 100.0 * lat / self.window_ms))

    def pump(self):
        while not self.stop:
            self.link._async_ev.wait(0.1)
            self.link._async_ev.clear()
            for msg in self.link.drain_async():
                obj = msg[1] if isinstance(msg, (list, tuple)) and len(msg) == 2 else msg
                if not isinstance(obj, dict):
                    continue
                if obj.get("type") in ("cam_trig", "cam_trig_tagged"):
                    self.on_cam_trig(obj)
                elif obj.get("type") == "system_info":
                    print("  [device] state=%s %s"
                          % (obj.get("state"), obj.get("log", "")))

    # -- run -----------------------------------------------------------------

    def run(self):
        self.link = UInspLink(self.args.port)
        self.link.auto_reconnect = True
        # Opening the port resets the board (line transients on this adapter),
        # so the first ping is always into the void. Give it a boot runway.
        for _ in range(12):
            if self.link.send({"type": "ping"}, timeout=1.5):
                break
            time.sleep(0.5)
        else:
            raise SystemExit("board did not answer ping")
        self.configure_board()
        self.open_camera()
        while self.stream.try_pop_buffer():      # drop anything stale
            pass

        threading.Thread(target=self.pump, daemon=True).start()

        r = self.link.send({"type": "enter_insp_mode"}, timeout=3.0)
        print("enter_insp_mode:", r)
        time.sleep(self.args.spin_up)

        print("\nfiring %d part(s) at %.1f/s\n" % (self.args.parts, self.args.rate))
        try:
            for _ in range(self.args.parts):
                self.link.send_nowait({"type": "trig_phantom_pulse"})
                time.sleep(1.0 / self.args.rate)
            # Parts answered at the camera are still travelling to their
            # selector. Exiting now would abort them mid-flight and the ejection
            # counters would read zero for a run that actually worked.
            # NOT "registered == 0": an object leaves the pipeline at SWITCH,
            # but its jet fires later, at the selector's own offset. Waiting on
            # the pipeline alone exits mid-flight and the ejection counters read
            # zero for a run that worked. Wait for the counts instead.
            t0 = time.time()
            while time.time() - t0 < self.args.drain:
                st = self.link.send({"type": "get_running_stat"}, timeout=2.0) or {}
                c = st.get("count") or {}
                done = sum(v for k, v in c.items() if k != "UNANSWERED")
                if done >= self.stats["parts"]:
                    break
                time.sleep(0.25)
        except KeyboardInterrupt:
            pass
        finally:
            self.stop = True
            self.teardown()
        return 0

    def teardown(self):
        try:
            st = self.link.send({"type": "get_running_stat"}, timeout=2.0) or {}
            self.link.send({"type": "exit_insp_mode"}, timeout=3.0)
            restore = {"stage_pulse_offset": self.orig_spo}
            if self.orig_freq is not None:
                restore["plate_freq"] = self.orig_freq
            self.link.send({"type": "set_setup", **restore}, timeout=3.0)
        except Exception as exc:
            print("teardown: %s" % exc)
            st = {}
        try:
            self.cam.stop_acquisition()
        except Exception:
            pass

        s = self.stats
        n = max(1, s["parts"])
        print("\n--- vision loop ---")
        print("answered %d part(s): ok %d  ng %d  na %d" % (s["parts"], s["ok"], s["ng"], s["na"]))
        print("camera misses: %d no-frame, %d unlit, %d frame-id gap(s), %d flushed"
              % (s["no_frame"], s["unlit"], s["fid_gaps"], s["flushed"]))
        print("report latency avg %.1f ms  max %.1f ms  (window %.0f ms)"
              % (s["lat_sum"] / n, s["lat_max"], self.window_ms))
        print("device counters:", json.dumps(st.get("count", {})))
        print("device state:", st.get("state"), " pipe:", json.dumps(st.get("pipe", {})))
        if s["no_frame"] or s["unlit"] or s["fid_gaps"]:
            # Two distinct causes, and they need different fixes -- say which.
            gaps = []
            prev = None
            for row in self.rows:
                gp = row[4]
                if gp is not None and prev is not None and row[1] == "NO-FRAME":
                    gaps.append(gp - prev)
                if gp is not None:
                    prev = gp
            close = [g for g in gaps if g is not None and g <= self.args.light_ticks]
            print("WARNING: %d object(s) got no picture." % (s["no_frame"] + s["unlit"]))
            if close:
                print("  %d of them arrived within the %d-tick light window of the"
                      " previous object. With the trigger wired to the backlight,"
                      " an object landing while the light is already ON produces no"
                      " rising edge and therefore no frame. Narrow --light-ticks (the"
                      " measured floor is ~100us of light) or drive the camera from"
                      " its own CAM pin." % (len(close), self.args.light_ticks))
            if s["unlit"]:
                print("  %d frame(s) came back unlit -- that IS the camera being"
                      " triggered faster than %.1f fps."
                      % (s["unlit"], self.max_fps or 0))
            if gaps and not close and not s["unlit"]:
                print("  spacing to the previous object: %s ticks (light window is %d)"
                      % (gaps, self.args.light_ticks))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True)
    ap.add_argument("--camera", default=None)
    ap.add_argument("--parts", type=int, default=20)
    ap.add_argument("--rate", type=float, default=2.0, help="phantom parts per second")
    ap.add_argument("--plate-freq", type=int, default=2000)
    ap.add_argument("--light-ticks", type=int, default=18,
                    help="L1A window width in ticks (measured: 600us of light)")
    ap.add_argument("--switch", type=int, default=None, help="SWITCH offset override")
    ap.add_argument("--sel1-on", type=int, default=None,
                    help="blow station distance from the gate, in pulses")
    ap.add_argument("--spin-up", type=float, default=2.0, help="s to let the plate settle")
    ap.add_argument("--drain", type=float, default=15.0,
                    help="max s to wait for answered parts to reach their selector")
    ap.add_argument("--expo", type=float, default=None, help="exposure us (default: camera preset)")
    ap.add_argument("--source", default="Line0")
    ap.add_argument("--edge", default="RisingEdge")
    ap.add_argument("--buffers", type=int, default=20)
    ap.add_argument("--roi-height", type=int, default=None,
                    help="sensor rows to read out; fps scales inversely (512 -> ~30 fps)")
    ap.add_argument("--frame-timeout", type=float, default=0.5,
                    help="s to wait for the frame belonging to a cam_trig")
    ap.add_argument("--thresh", type=int, default=100, help="dark threshold, 0-255")
    ap.add_argument("--min-area", type=float, default=0.0005,
                    help="dark fraction below this = nothing there")
    ap.add_argument("--max-area", type=float, default=0.5,
                    help="dark fraction above this = oversize/merged")
    return VisionLoop(ap.parse_args()).run()


if __name__ == "__main__":
    sys.exit(main())
