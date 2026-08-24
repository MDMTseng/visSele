"""clean_region_shot -- a picture of what the clean regions are actually sitting on.

    python clean_region_shot.py --allow-reset [--out clean_regions.png]

clean_region_probe.py answers "how many dark pixels" and "where in the box".
This answers the question those numbers keep pointing at and cannot show: what
IS that dark thing. Where to put a clean region is a decision about the physical
station -- which pocket, which neighbour -- and a number cannot make it. A
picture can.

Writes a PNG with no numpy and no OpenCV, because the bench machine has neither
and a production machine is a bad place to install a dependency for a
diagnostic. zlib and struct are in the standard library and a PNG is not hard.

    grey  the real frame, backlit: bright field, dark parts
    red   clean_regions from machine_setting.json
    cyan  pixels below that region's dark_thresh -- the ones that trip the gate
    green inspection_region
"""
import argparse, json, os, struct, sys, time, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
MSET = os.path.join(REPO, 'InspectionCore', 'Core0_1', 'data', 'machine_setting.json')

SDK = r"C:\Program Files (x86)\MVS\Development\Samples\Python"
if not os.path.isdir(SDK):
    sys.exit("MVS Python SDK not found at: " + SDK)
sys.path.append(SDK)
from MvImport.MvCameraControl_class import *  # noqa: E402,F403

ap = argparse.ArgumentParser()
ap.add_argument("--port", default="COM3")
ap.add_argument("--light", default="L1A")
ap.add_argument("--allow-reset", action="store_true")
ap.add_argument("--out", default=os.path.join(HERE, "clean_regions.png"))
args = ap.parse_args()
if not args.allow_reset:
    ap.error("opening COM3 can reboot the board; pass --allow-reset")

cfg = json.load(open(MSET, encoding='utf-8'))
regions = cfg.get('clean_regions') or []
insp = cfg.get('inspection_region')


def crc16(d):
    c = 0xFFFF
    for b in d:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


def write_png(path, w, h, rgb):
    """Truecolour PNG, one filter byte per row. rgb is a bytearray of w*h*3."""
    raw = bytearray()
    for y in range(h):
        raw.append(0)                       # filter: none
        raw += rgb[y * w * 3:(y + 1) * w * 3]

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    with open(path, 'wb') as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 6)))
        f.write(chunk(b"IEND", b""))


dl = MV_CC_DEVICE_INFO_LIST()
if MvCamera.MV_CC_EnumDevices(MV_USB_DEVICE | MV_GIGE_DEVICE, dl) != 0 or dl.nDeviceNum == 0:
    sys.exit("no camera -- is the core still running?")
cam = MvCamera()
cam.MV_CC_CreateHandle(cast(dl.pDeviceInfo[0], POINTER(MV_CC_DEVICE_INFO)).contents)
if cam.MV_CC_OpenDevice(MV_ACCESS_Exclusive, 0) != 0:
    sys.exit("cannot open the camera")

ser = None
try:
    def gi(n):
        v = MVCC_INTVALUE(); memset(byref(v), 0, sizeof(v))
        cam.MV_CC_GetIntValue(n, v); return v.nCurValue
    ox, oy = gi("OffsetX"), gi("OffsetY")

    cam.MV_CC_SetEnumValue("TriggerMode", 1)
    cam.MV_CC_StartGrabbing()

    import serial
    ser = serial.Serial()
    ser.port = args.port; ser.baudrate = 230400; ser.timeout = 0.2
    ser.dtr = False; ser.rts = False        # RTS is wired to EN on this adapter
    ser.open(); time.sleep(0.4)

    def send(o):
        raw = json.dumps(o, separators=(",", ":")).encode()
        ser.write(raw + b"*%04X\n" % crc16(raw))

    # Backlit station: without the light every pixel is under the threshold and
    # the picture says nothing except that nobody turned the lamp on.
    send({"type": "light", "ch": args.light, "on": True, "timeout_ms": 60000, "id": 1})
    time.sleep(0.5)

    fr = MV_FRAME_OUT(); memset(byref(fr), 0, sizeof(fr))
    send({"type": "trig_cam_pulse", "id": 2})
    if cam.MV_CC_GetImageBuffer(fr, 3000) != 0:
        sys.exit("no frame -- the hardware trigger did not reach the camera")
    W, H = fr.stFrameInfo.nWidth, fr.stFrameInfo.nHeight
    grey = bytes((c_ubyte * fr.stFrameInfo.nFrameLen).from_address(
        addressof(fr.pBufAddr.contents)))[:W * H]
    cam.MV_CC_FreeImageBuffer(fr)
    print("frame %dx%d at offset (%d,%d)" % (W, H, ox, oy))

    rgb = bytearray(W * H * 3)
    for i, v in enumerate(grey):
        rgb[i * 3] = rgb[i * 3 + 1] = rgb[i * 3 + 2] = v

    def px(x, y, c):
        if 0 <= x < W and 0 <= y < H:
            o = (y * W + x) * 3
            rgb[o], rgb[o + 1], rgb[o + 2] = c

    def box(x, y, w, h, c, t=2):
        for k in range(t):
            for xx in range(x, x + w):
                px(xx, y + k, c); px(xx, y + h - 1 - k, c)
            for yy in range(y, y + h):
                px(x + k, yy, c); px(x + w - 1 - k, yy, c)

    CYAN, RED, GREEN = (0, 255, 255), (255, 40, 40), (40, 220, 40)
    for idx, c in enumerate(regions):
        rx, ry = int(round(c['x'] - ox)), int(round(c['y'] - oy))
        rw, rh = int(round(c['w'])), int(round(c['h']))
        th = c.get('dark_thresh')
        n = 0
        # Paint the offending pixels FIRST so the box outline stays on top.
        for yy in range(max(0, ry), min(H, ry + rh)):
            base = yy * W
            for xx in range(max(0, rx), min(W, rx + rw)):
                if grey[base + xx] < th:
                    px(xx, yy, CYAN); n += 1
        box(rx, ry, rw, rh, RED)
        print("  %-8s (%d,%d) %dx%d  thr %s -> %d dark px marked cyan"
              % (c.get('name') or ('clean%d' % (idx + 1)), c['x'], c['y'], c['w'], c['h'], th, n))
    if insp:
        box(int(round(insp['x'] - ox)), int(round(insp['y'] - oy)),
            int(round(insp['w'])), int(round(insp['h'])), GREEN)
        print("  inspection_region (%d,%d) %dx%d -> green"
              % (insp['x'], insp['y'], insp['w'], insp['h']))

    write_png(args.out, W, H, rgb)
    print("wrote %s" % args.out)
finally:
    try:
        cam.MV_CC_StopGrabbing()
    except Exception:
        pass
    cam.MV_CC_CloseDevice(); cam.MV_CC_DestroyHandle()
    if ser is not None:
        try: ser.close()
        except Exception: pass
