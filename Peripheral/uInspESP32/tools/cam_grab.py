#!/usr/bin/env python3
"""cam_grab -- grab frames from a GenICam camera via Aravis, from Python.

Companion to uinsp_panel's camera card: the panel drives the ESP32's CAM/light
pins, this side proves the camera actually answered. Together they close the
loop without an oscilloscope.

    list      enumerate cameras and their trigger sources
    grab      free-run: take N frames (no trigger wiring needed)
    hwtrig    arm the camera on a hardware trigger line and count what arrives
              -- run the panel's Free-run / Burst at the same time; frames
              received vs pulses fired is the wiring verdict

Needs the Aravis GObject binding, which lives in Homebrew's python, NOT pyenv's:

    /opt/homebrew/bin/python3.13 cam_grab.py list

PNG writing is hand-rolled (zlib only) so the script has no pip dependencies --
Homebrew's python is externally managed and cannot pip-install numpy/Pillow.
"""

import argparse
import struct
import sys
import time
import zlib

import gi
gi.require_version("Aravis", "0.8")
from gi.repository import Aravis  # noqa: E402


def write_png_gray(path, data, width, height):
    """Minimal 8-bit grayscale PNG writer (data = width*height bytes)."""
    raw = bytearray()
    for y in range(height):
        raw.append(0)                                  # filter type 0
        raw += data[y * width:(y + 1) * width]

    def chunk(tag, payload):
        return (struct.pack(">I", len(payload)) + tag + payload
                + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 6)))
        f.write(chunk(b"IEND", b""))


def stats(data):
    """Cheap brightness summary -- enough to tell 'lens cap on' from 'lit'."""
    sample = data[::997] or data
    return max(data), sum(sample) / len(sample)


def open_camera(name):
    cam = Aravis.Camera.new(name)
    print("camera: %s %s  sensor %dx%d  %s"
          % (cam.get_vendor_name(), cam.get_model_name(),
             *cam.get_sensor_size(), cam.get_pixel_format_as_string()))
    # Feature values are cached by default. A live input level read back through
    # the cache never changes, which reads exactly like a dead trigger wire.
    try:
        cam.get_device().set_register_cache_policy(Aravis.RegisterCachePolicy.DISABLE)
    except Exception:
        pass
    return cam


def feat_str(dev, name):
    """Read a GenICam feature whatever its type (or 'n/a' if absent)."""
    for getter in (dev.get_string_feature_value, dev.get_integer_feature_value,
                   dev.get_boolean_feature_value, dev.get_float_feature_value):
        try:
            return str(getter(name))
        except Exception:
            pass
    return "n/a"


def cmd_lines(args):
    """Watch the camera's input lines -- the wiring test with no acquisition.

    Toggle the ESP32 pin (panel hold buttons, or its camera card) and watch for
    a change here. No change at all means the signal is not reaching the camera:
    the ESP32 side can be proven separately with pin_read.
    """
    cam = open_camera(args.camera)
    dev = cam.get_device()
    for f in ("TriggerMode", "TriggerSource", "TriggerActivation"):
        print("  %s = %s" % (f, feat_str(dev, f)))
    try:
        lines = dev.dup_available_enumeration_feature_values_as_strings("LineSelector")
    except Exception:
        lines = ["Line0"]
    for ln in lines:
        try:
            dev.set_string_feature_value("LineSelector", ln)
            print("  %s mode=%s" % (ln, feat_str(dev, "LineMode")))
        except Exception:
            pass

    print("watching LineStatusAll for %.0fs -- toggle the pin now" % args.seconds)
    t0 = time.time()
    last = None
    changes = 0
    while time.time() - t0 < args.seconds:
        try:
            v = dev.get_integer_feature_value("LineStatusAll")
        except Exception as exc:
            print("  LineStatusAll unreadable: %s" % exc)
            return 1
        if v != last:
            if last is not None:
                changes += 1
            print("  [%6.2fs] LineStatusAll = 0x%X  %s"
                  % (time.time() - t0, v, format(v, "04b")))
            last = v
        time.sleep(0.05)
    print("%d change(s) seen" % changes)
    if changes == 0:
        print("Line levels never moved. The signal is not reaching the camera --"
              " check the common ground between the ESP32 and the camera I/O"
              " connector, and note that opto-isolated inputs (Line0 on most"
              " Hikrobot bodies) want 5-24V: a 3.3V GPIO may sit below the"
              " opto's threshold even when the wire is correct.")
    return 0 if changes else 1


def cmd_list(args):
    Aravis.update_device_list()
    n = Aravis.get_n_devices()
    print("%d device(s)" % n)
    for i in range(n):
        print("  [%d] %s | %s %s | %s"
              % (i, Aravis.get_device_id(i), Aravis.get_device_vendor(i),
                 Aravis.get_device_model(i), Aravis.get_device_address(i)))
    if n:
        cam = Aravis.Camera.new(Aravis.get_device_id(0))
        print("  exposure range: %.0f .. %.0f us" % cam.get_exposure_time_bounds())
        print("  gain range:     %.2f .. %.2f" % cam.get_gain_bounds())
        try:
            print("  trigger sources:", cam.dup_available_trigger_sources())
        except Exception:
            pass
    return 0


def apply_exposure(cam, args):
    if args.expo:
        cam.set_exposure_time(float(args.expo))
    if args.gain is not None:
        cam.set_gain(float(args.gain))


def cmd_grab(args):
    cam = open_camera(args.camera)
    cam.clear_triggers()                 # free-run
    apply_exposure(cam, args)
    ok = 0
    for i in range(args.count):
        cam.set_acquisition_mode(Aravis.AcquisitionMode.SINGLE_FRAME)
        buf = cam.acquisition(int(args.timeout * 1e6))
        if buf is None or buf.get_status().value_nick != "success":
            print("  frame %d: FAILED (%s)"
                  % (i, buf.get_status().value_nick if buf else "timeout"))
            continue
        ok += 1
        w, h = buf.get_image_width(), buf.get_image_height()
        d = buf.get_data()
        mx, mean = stats(d)
        out = "%s_%02d.png" % (args.out, i)
        write_png_gray(out, d, w, h)
        print("  frame %d: %dx%d  max %d  mean %.1f  -> %s" % (i, w, h, mx, mean, out))
    print("%d/%d frames" % (ok, args.count))
    if ok and mx < 16:
        print("NOTE: frame is essentially black -- lens cap, closed aperture, or"
              " no light. Hold the machine light on from the panel and retry.")
    return 0 if ok else 1


def cmd_hwtrig(args):
    """Arm on a hardware trigger line and report what actually arrives.

    This is the real wiring test: the ESP32's CAM pin drives the camera's
    trigger input, so 'frames received' here should track 'pulses fired' in the
    panel. A silent run means the trigger line, its polarity, or the pin choice
    is wrong -- the camera itself is fine if `grab` worked.
    """
    cam = open_camera(args.camera)
    apply_exposure(cam, args)
    cam.set_acquisition_mode(Aravis.AcquisitionMode.CONTINUOUS)
    cam.set_trigger(args.source)
    # No set_trigger_activation() in this binding -- go through the feature.
    try:
        cam.get_device().set_string_feature_value("TriggerActivation", args.edge)
    except Exception as exc:
        print("  (trigger activation %s not settable: %s)" % (args.edge, exc))
    print("armed on %s (%s edge), waiting %.0fs -- fire pulses from the panel now"
          % (args.source, args.edge, args.seconds))

    stream = cam.create_stream(None, None)
    for _ in range(20):
        stream.push_buffer(Aravis.Buffer.new_allocate(cam.get_payload()))
    cam.start_acquisition()
    t0 = time.time()
    got = 0
    try:
        while time.time() - t0 < args.seconds:
            buf = stream.timeout_pop_buffer(200000)   # 200 ms
            if buf is None:
                continue
            if buf.get_status().value_nick == "success":
                got += 1
                d = buf.get_data()
                mx, mean = stats(d)
                print("  [%6.2fs] frame %d  max %d  mean %.1f"
                      % (time.time() - t0, got, mx, mean))
                if got == 1 and args.out:
                    write_png_gray("%s_hwtrig.png" % args.out, d,
                                   buf.get_image_width(), buf.get_image_height())
            stream.push_buffer(buf)
    except KeyboardInterrupt:
        pass
    finally:
        cam.stop_acquisition()
    print("%d triggered frame(s) in %.0fs" % (got, args.seconds))
    if got == 0:
        print("No frames: check the trigger wire (ESP32 CAM pin -> camera %s),"
              " its polarity (io_on_level / --edge), and that the panel was"
              " firing during the window." % args.source)
    return 0 if got else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--camera", default=None, help="device id (default: first found)")
    ap.add_argument("--expo", type=float, default=None, help="exposure time, us")
    ap.add_argument("--gain", type=float, default=None)
    sub = ap.add_subparsers(dest="cmd", required=True)

    sub.add_parser("list").set_defaults(fn=cmd_list)

    g = sub.add_parser("grab")
    g.add_argument("--count", type=int, default=1)
    g.add_argument("--timeout", type=float, default=3.0, help="per-frame, seconds")
    g.add_argument("--out", default="grab")
    g.set_defaults(fn=cmd_grab)

    ln = sub.add_parser("lines")
    ln.add_argument("--seconds", type=float, default=20.0)
    ln.set_defaults(fn=cmd_lines)

    h = sub.add_parser("hwtrig")
    h.add_argument("--source", default="Line0", help="camera trigger source")
    h.add_argument("--edge", default="RisingEdge",
                   choices=["RisingEdge", "FallingEdge", "LevelHigh", "LevelLow"])
    h.add_argument("--seconds", type=float, default=15.0)
    h.add_argument("--out", default="cam")
    h.set_defaults(fn=cmd_hwtrig)

    args = ap.parse_args()
    return args.fn(args)


if __name__ == "__main__":
    sys.exit(main())
