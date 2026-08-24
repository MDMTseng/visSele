"""clean_region_probe -- what do the clean regions ACTUALLY see?

    python clean_region_probe.py [--allow-reset] [-n 3] [--out clean_probe.png]

The core's clean-area gate skips inspection when a clean region is dirtier than
its threshold, and on this bench it fires on nearly every frame. Reading that
from the core tells you the VERDICT ("DIRTY") and the ratio, but not the thing
that decides what to do about it: whether the region is looking at a few specks
of debris or at nothing at all.

So take the picture and measure it the same way the core does:

    grey < dark_thresh  ->  dark pixel
    dark_area  = dark_px * mmpp^2      vs  dark_area_max
    dark_ratio = dark_px / region_px   vs  dark_ratio_max

and then say what the region actually contains -- min/max/mean grey, and a
histogram coarse enough to read at a glance. A region that is 100% dark with a
max grey of 3 is not dirty, it is unlit or mis-placed, and no threshold will
fix it.

Regions and mmpp are read from the SAME files the core reads, so this cannot
drift from what the machine believes:
    Core0_1/data/machine_setting.json   clean_regions, inspection_region
The camera's own ROI offset is read off the camera, because the region
coordinates are full-sensor and the crop is not.

PRECONDITIONS: the core must be stopped (it holds the camera and COM3), and
the board must be able to trigger the camera -- this uses the real hardware
trigger, which is the whole point: it measures the frames the machine sees.
"""
import argparse, json, os, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
MSET = os.path.join(REPO, 'InspectionCore', 'Core0_1', 'data', 'machine_setting.json')

SDK = r"C:\Program Files (x86)\MVS\Development\Samples\Python"
if not os.path.isdir(SDK):
    sys.exit("MVS Python SDK not found at: " + SDK)
sys.path.append(SDK)
from MvImport.MvCameraControl_class import *  # noqa: E402,F403

ap = argparse.ArgumentParser()
ap.add_argument("-n", type=int, default=3, help="frames to grab")
ap.add_argument("--port", default="COM3")
ap.add_argument("--baud", type=int, default=230400)
ap.add_argument("--allow-reset", action="store_true",
                help="acknowledge that opening COM3 may reboot the board")
ap.add_argument("--mmpp", type=float, default=0.01388594314,
                help="mm per pixel from the core's calibration (UIData.instrument_mmpp)")
ap.add_argument("--light", default="L1A", help="backlight channel to hold on (L1A/L2A)")
ap.add_argument("--out", default=os.path.join(HERE, "clean_probe.png"))
args = ap.parse_args()
if not args.allow_reset:
    ap.error("opening COM3 can reboot the board; pass --allow-reset")

# No numpy, no OpenCV. Neither is installed on the bench machine and this is a
# diagnostic -- adding a dependency to a machine that is running production is a
# bad trade for arithmetic this simple. Counting bytes under a threshold inside
# a rectangle is exactly what the work is, and 36k pixels is nothing.
try:
    import cv2                      # only for the annotated PNG, optional
except ImportError:
    cv2 = None

# ---- what the machine believes -------------------------------------------
cfg = json.load(open(MSET, encoding='utf-8'))
regions = cfg.get('clean_regions') or []
if not regions:
    sys.exit("no clean_regions in " + MSET)
insp = cfg.get('inspection_region')


def ck(rc, what):
    if rc != 0:
        sys.exit("%s failed: 0x%08x" % (what, rc & 0xFFFFFFFF))


def get_int(cam, node):
    v = MVCC_INTVALUE()
    memset(byref(v), 0, sizeof(MVCC_INTVALUE))
    return v.nCurValue if cam.MV_CC_GetIntValue(node, v) == 0 else None


def get_float(cam, node):
    v = MVCC_FLOATVALUE()
    memset(byref(v), 0, sizeof(MVCC_FLOATVALUE))
    return v.fCurValue if cam.MV_CC_GetFloatValue(node, v) == 0 else None


dl = MV_CC_DEVICE_INFO_LIST()
ck(MvCamera.MV_CC_EnumDevices(MV_USB_DEVICE | MV_GIGE_DEVICE, dl), "EnumDevices")
if dl.nDeviceNum == 0:
    sys.exit("no camera -- is the core still running?")
cam = MvCamera()
ck(cam.MV_CC_CreateHandle(cast(dl.pDeviceInfo[0], POINTER(MV_CC_DEVICE_INFO)).contents),
   "CreateHandle")
ck(cam.MV_CC_OpenDevice(MV_ACCESS_Exclusive, 0), "OpenDevice")

ser = None
try:
    w = get_int(cam, "Width"); h = get_int(cam, "Height")
    ox = get_int(cam, "OffsetX") or 0; oy = get_int(cam, "OffsetY") or 0
    expo = get_float(cam, "ExposureTime")
    print("camera ROI: %dx%d at offset (%d,%d)   exposure %.0fus" % (w, h, ox, oy, expo or -1))

    # Hardware trigger, because the question is what the MACHINE sees. A
    # free-run frame is a different exposure at a different plate position.
    cam.MV_CC_SetEnumValue("TriggerMode", 1)
    ck(cam.MV_CC_StartGrabbing(), "StartGrabbing")

    import serial
    ser = serial.Serial()
    ser.port = args.port; ser.baudrate = args.baud; ser.timeout = 0.2
    ser.dtr = False; ser.rts = False       # RTS is wired to EN on this adapter
    ser.open()
    time.sleep(0.4)

    def crc16(data):
        crc = 0xFFFF
        for byte in data:
            crc ^= byte << 8
            for _ in range(8):
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
        return crc

    def send(obj):
        raw = json.dumps(obj, separators=(",", ":")).encode()
        ser.write(raw + b"*%04X\n" % crc16(raw))

    # THE LIGHT MATTERS MORE THAN THE TRIGGER HERE.
    #
    # This is a BACKLIT station: the field is bright and the part is the dark
    # thing in it, so `dark_thresh` is not a dust threshold -- it asks "is
    # something blocking the backlight in this region". Trigger the camera
    # without the light and every pixel is below the threshold, every clean
    # region reads 100% dark, and the measurement says "filthy" about a scene
    # that simply was not lit. That is exactly the wrong answer this probe
    # exists to avoid, and it is the first one it gave.
    #
    # In a real cycle the stage strobes the light in step with the camera. Held
    # on for the duration here, which is the static equivalent.
    send({"type": "light", "ch": args.light, "on": True, "timeout_ms": 60000, "id": 9000})
    time.sleep(0.5)
    print("light %s held on" % args.light)

    def fire():
        send({"type": "trig_cam_pulse", "id": 9001})

    frame = MV_FRAME_OUT()
    memset(byref(frame), 0, sizeof(frame))
    got = []
    for i in range(args.n):
        fire()
        rc = cam.MV_CC_GetImageBuffer(frame, 2000)
        if rc != 0:
            print("  frame %d: no image (0x%08x)" % (i + 1, rc & 0xFFFFFFFF))
            continue
        fw, fh = frame.stFrameInfo.nWidth, frame.stFrameInfo.nHeight
        buf = (c_ubyte * frame.stFrameInfo.nFrameLen).from_address(
            addressof(frame.pBufAddr.contents))
        raw = bytes(buf)
        if len(raw) >= fw * fh:
            got.append((raw[:fw * fh], fw, fh))
            print("  frame %d: %dx%d" % (i + 1, fw, fh))
        cam.MV_CC_FreeImageBuffer(frame)

    if not got:
        sys.exit("no frames arrived -- the hardware trigger is not reaching the camera")

    # Every frame, not just the last. A threshold picked from one frame is a
    # threshold picked from one sample of the noise; the spread across frames is
    # what says how much margin a setting needs.
    if len(got) > 1:
        print("")
        print("dark-pixel count per frame (thr from each region's dark_thresh):")
        for idx, c in enumerate(regions):
            nm = c.get('name') or ('clean%d' % (idx + 1))
            th = c.get('dark_thresh')
            counts = []
            for (fimg, fw_, fh_) in got:
                rx, ry = int(round(c['x'] - ox)), int(round(c['y'] - oy))
                x0, y0 = max(0, rx), max(0, ry)
                x1, y1 = min(fw_, rx + int(c['w'])), min(fh_, ry + int(c['h']))
                d = 0
                for yy in range(y0, y1):
                    for v in fimg[yy * fw_ + x0: yy * fw_ + x1]:
                        if v < th: d += 1
                counts.append(d)
            mm = args.mmpp * args.mmpp
            print("  %-8s %s px   -> %.4f .. %.4f mm2   (limit %s)"
                  % (nm, counts, min(counts) * mm, max(counts) * mm, c.get('dark_area_max')))
        print("")

    img, IW, IH = got[-1]
    # mm-per-pixel comes from the CALIBRATION, which lives in the core, not in
    # machine_setting.json -- so it is an argument here. The default is what the
    # running core reported through the UI (UIData.instrument_mmpp) on
    # 2026-08-21; if the lens or the calibration changes, pass the new one.
    mmpp = args.mmpp

    print("")
    print("clean regions, measured the way the core measures them:")
    print("")
    for idx, c in enumerate(regions):
        name = c.get('name') or ('clean%d' % (idx + 1))
        rx, ry = int(round(c['x'] - ox)), int(round(c['y'] - oy))
        rw, rh = int(round(c['w'])), int(round(c['h']))
        x0, y0 = max(0, rx), max(0, ry)
        x1, y1 = min(IW, rx + rw), min(IH, ry + rh)
        if x1 - x0 < 2 or y1 - y0 < 2:
            print("  %-8s OFF-IMAGE after the ROI offset -- the core skips it" % name)
            continue
        thr = c.get('dark_thresh')
        dark = 0; total = 0
        gmin, gmax, gsum = 255, 0, 0
        hist = [0] * 8
        for yy in range(y0, y1):
            row = img[yy * IW + x0: yy * IW + x1]
            total += len(row)
            for v in row:
                if v < gmin: gmin = v
                if v > gmax: gmax = v
                gsum += v
                hist[v >> 5] += 1
                if thr is not None and v < thr: dark += 1
        ratio = dark / float(total)
        area = dark * mmpp * mmpp
        print("  %-8s box (%d,%d) %dx%d  ->  crop (%d,%d) %dx%d" %
              (name, c['x'], c['y'], c['w'], c['h'], x0, y0, x1 - x0, y1 - y0))
        print("           grey  min=%3d  max=%3d  mean=%6.1f" % (gmin, gmax, gsum / float(total)))
        print("           dark(<%s) = %d / %d px = %.4f" % (thr, dark, total, ratio))
        print("           dark_area = %.4f mm2   (limit %s)" % (area, c.get('dark_area_max')))
        bars = "  ".join("%d-%d:%5.1f%%" % (b * 32, b * 32 + 31, 100.0 * hist[b] / total)
                         for b in range(8) if hist[b])
        print("           grey spread  %s" % bars)
        # WHERE the dark pixels are decides what to do about them, and the
        # totals cannot say. A gradient hugging one edge means the box includes
        # something it should not and wants moving; a blob in the middle of an
        # otherwise clean field is a real object and wants the threshold left
        # alone. Same number, opposite fix.
        if dark:
            dx0, dy0, dx1, dy1 = 1 << 30, 1 << 30, -1, -1
            GX, GY = 6, 6
            grid = [[0] * GX for _ in range(GY)]
            for yy in range(y0, y1):
                row = img[yy * IW + x0: yy * IW + x1]
                for xi, v in enumerate(row):
                    if v < thr:
                        if xi < dx0: dx0 = xi
                        if xi > dx1: dx1 = xi
                        ry_ = yy - y0
                        if ry_ < dy0: dy0 = ry_
                        if ry_ > dy1: dy1 = ry_
                        grid[min(GY - 1, ry_ * GY // (y1 - y0))][min(GX - 1, xi * GX // (x1 - x0))] += 1
            print("           dark px bbox  x %d..%d of %d,  y %d..%d of %d"
                  % (dx0, dx1, x1 - x0, dy0, dy1, y1 - y0))
            print("           where (6x6 cells, count of dark px):")
            for r in grid:
                print("             " + " ".join("%5d" % v for v in r))
        over = []
        if c.get('dark_area_max') is not None and area > c['dark_area_max']:
            over.append("over dark_area_max (%.4f > %s)" % (area, c['dark_area_max']))
        if c.get('dark_ratio_max') is not None and ratio > c['dark_ratio_max']:
            over.append("over dark_ratio_max")
        print("           -> %s" % ("DIRTY: " + ", ".join(over) if over else "CLEAN"))
        print("")

    # What would a smaller box see?
    #
    # The useful answer is not "raise the threshold until it passes" -- that
    # blunts the only thing the gate is for. If the darkness is structural and
    # sits against one edge, the box is simply looking at something it was not
    # meant to watch, and trimming that edge keeps the tight threshold AND makes
    # it mean something again. This sweep says exactly how far to trim.
    print("edge trim sweep -- dark px remaining after cutting N px off each side:")
    print("")
    for idx, c in enumerate(regions):
        nm = c.get('name') or ('clean%d' % (idx + 1))
        th = c.get('dark_thresh')
        rx, ry = int(round(c['x'] - ox)), int(round(c['y'] - oy))
        bx0, by0 = max(0, rx), max(0, ry)
        bx1, by1 = min(IW, rx + int(c['w'])), min(IH, ry + int(c['h']))
        lim_px = c.get('dark_area_max', 0) / (mmpp * mmpp) if c.get('dark_area_max') else 0
        print("  %s   (limit is %.1f px)" % (nm, lim_px))
        for side in ('left', 'right', 'top', 'bottom'):
            row = []
            for cut in (0, 10, 20, 30, 40, 50, 60):
                ax0, ay0, ax1, ay1 = bx0, by0, bx1, by1
                if side == 'left':   ax0 = bx0 + cut
                if side == 'right':  ax1 = bx1 - cut
                if side == 'top':    ay0 = by0 + cut
                if side == 'bottom': ay1 = by1 - cut
                if ax1 - ax0 < 4 or ay1 - ay0 < 4: row.append('--'); continue
                d = 0
                for yy in range(ay0, ay1):
                    for v in img[yy * IW + ax0: yy * IW + ax1]:
                        if v < th: d += 1
                row.append(str(d))
            print("    cut %-7s %s" % (side + ':', "  ".join("%5s" % v for v in row)))
        print("            %s" % "  ".join("%5s" % ("+%d" % c2) for c2 in (0,10,20,30,40,50,60)))
        print("")

    if cv2 is not None:   # optional, absent on the bench
        vis = cv2.cvtColor(img, cv2.COLOR_GRAY2BGR)
        for idx, c in enumerate(regions):
            rx, ry = int(round(c['x'] - ox)), int(round(c['y'] - oy))
            cv2.rectangle(vis, (rx, ry), (rx + int(c['w']), ry + int(c['h'])), (0, 0, 255), 2)
            cv2.putText(vis, c.get('name') or ('clean%d' % (idx + 1)), (rx, max(14, ry - 6)),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1)
        if insp:
            ix, iy = int(round(insp['x'] - ox)), int(round(insp['y'] - oy))
            cv2.rectangle(vis, (ix, iy), (ix + int(insp['w']), iy + int(insp['h'])),
                          (0, 255, 0), 2)
            cv2.putText(vis, "inspection_region", (ix, max(14, iy - 6)),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
        cv2.imwrite(args.out, vis)
        print("annotated frame -> %s" % args.out)
finally:
    try:
        cam.MV_CC_StopGrabbing()
    except Exception:
        pass
    cam.MV_CC_CloseDevice()
    cam.MV_CC_DestroyHandle()
    if ser is not None:
        try: ser.close()
        except Exception: pass
