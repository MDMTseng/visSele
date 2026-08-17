#!/usr/bin/env python3
"""Where is the exposure window, relative to the trigger the board fires?

The timestamp pairing assumes the image belongs to the object that was under
the camera when the trigger went out. That holds only if the exposure happens
promptly after the trigger -- if the camera deferred it, the frame would show a
different part while every timestamp still matched, and no residual would ever
reveal it. Nothing measured so far can see that.

The backlight is the probe, and it needs no extra hardware. Parts are imaged
against a backlight, so the sensor only collects light while the light is ON.
Fire a SHORT light pulse and sweep its delay against the trigger: mean image
brightness rises when the pulse starts falling inside the exposure window and
falls when it leaves. The two edges of that curve ARE the exposure window,
measured in the board's own clock -- the same clock cam_us is stamped from.

  python3 expose.py --pulse 60 --step 40 --max 1600
"""
import argparse, json, sys, time
import gi
gi.require_version('Aravis', '0.8')
from gi.repository import Aravis
import serial

PORT = '/dev/cu.usbserial-0001'


def board(cmd, ser, wait=1.0):
    ser.write((json.dumps(cmd) + "\n").encode())
    t0, buf = time.time(), b''
    while time.time() - t0 < wait:
        buf += ser.read(4096)
    out = []
    for line in buf.decode('utf8', 'replace').splitlines():
        line = line.strip()
        if line.startswith('{'):
            try: out.append(json.loads(line.split('*')[0]))
            except Exception: pass
    return out


def main(a):
    cam = Aravis.Camera.new(None)
    dev = cam.get_device()
    cam.set_region(a.x, a.y, a.w, a.h)
    cam.set_exposure_time(a.exposure)
    dev.set_string_feature_value("TriggerSelector", "FrameBurstStart")
    dev.set_string_feature_value("TriggerMode", "On")
    dev.set_string_feature_value("TriggerSource", "Line0")
    print("region %dx%d exposure %.0fus | light pulse %dus, delay 0..%dus step %d"
          % (a.w, a.h, a.exposure, a.pulse, a.max, a.step))
    stream = cam.create_stream(None, None)
    for _ in range(30):
        stream.push_buffer(Aravis.Buffer.new_allocate(cam.get_payload()))
    cam.start_acquisition()
    time.sleep(0.4)
    ser = serial.Serial(PORT, 230400, timeout=0.2)
    time.sleep(2.5); ser.reset_input_buffer()
    board({"type": "clear_error"}, ser)

    print("%8s %10s %8s" % ("delay_us", "brightness", "frames"))
    rows = []
    for d in range(0, a.max + 1, a.step):
        while stream.try_pop_buffer() is not None: pass
        board({"type": "trig_cam_burst", "count": a.n, "period_us": 60000,
               "light_delay": d, "light_duration": a.pulse}, ser,
              wait=a.n * 0.06 + 2.0)
        vals = []
        t_end = time.time() + 2.0
        while time.time() < t_end:
            buf = stream.timeout_pop_buffer(200000)
            if buf is None: continue
            if buf.get_status() == Aravis.BufferStatus.SUCCESS:
                data = bytes(buf.get_data())
                vals.append(sum(data[::997]) / len(data[::997]))
            stream.push_buffer(buf)
            t_end = max(t_end, time.time() + 0.3)
        b = sum(vals) / len(vals) if vals else 0.0
        rows.append((d, b, len(vals)))
        print("%8d %10.1f %8d" % (d, b, len(vals)), flush=True)

    cam.stop_acquisition(); ser.close()
    # Guard against the degenerate case. The first version took "> 50% of
    # peak" with a peak of 1.0 on an all-black sweep and duly reported an
    # exposure window of 0..1560us -- a confident answer computed from no
    # signal at all. Require real contrast before believing any edge.
    peak = max(r[1] for r in rows)
    floor = min(r[1] for r in rows)
    if peak - floor < 5.0:
        print("\nNO CONTRAST (peak %.1f floor %.1f): the light never reached the "
              "sensor at any delay, or never fired. Nothing can be concluded "
              "about the exposure window from this sweep." % (peak, floor))
        return 1
    on = [r[0] for r in rows if r[1] > (peak + floor) / 2.0]
    if on:
        print("\nexposure window (brightness > 50%% of peak): %d..%d us after trigger"
              % (min(on), max(on) + a.pulse))
        print("=> trigger->exposure start ~%d us; window ~%d us"
              % (min(on), max(on) + a.pulse - min(on)))
    return 0


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument("--pulse", type=int, default=60)
    ap.add_argument("--step", type=int, default=40)
    ap.add_argument("--max", type=int, default=1600)
    ap.add_argument("--n", type=int, default=6)
    ap.add_argument("--exposure", type=float, default=1000.0)
    ap.add_argument("--x", type=int, default=1248)
    ap.add_argument("--y", type=int, default=428)
    ap.add_argument("--w", type=int, default=560)
    ap.add_argument("--h", type=int, default=452)
    sys.exit(main(ap.parse_args()))
