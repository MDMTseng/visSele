"""cam_line_probe -- what IO lines does this camera have, and can it DRIVE one?

Written 2026-08-20, after the forward test went dead: the board's GPIO17 toggles
(pin_read 1->0->1), the camera grabs fine in free-run (5/5 @ 816x528), yet the
camera's LineStatus on Line0/1/2 does not move when GPIO17 moves. The forward
direction tells us "no signal arrived" but not WHERE it died.

So run the wire BACKWARDS. HikRobot cameras expose an opto-isolated OUTPUT
(usually Line1) and often a bidirectional GPIO (Line2). Drive it from the
camera, read it on the ESP32:

    reverse works  -> the conductor is fine; the break is on the camera's INPUT
                      side (dead Line0 opto, or the trigger is not landing on
                      the line TriggerSource names)
    reverse dead   -> the wire/connector/opto is open in BOTH directions, which
                      is the thing a person on site can find with a meter

Modes:
    --list                    enumerate LineSelector entries + each line's
                              LineMode / LineStatus / LineSource. Read-only.
    --drive LineN             set LineN to Output/UserOutput and toggle it,
                              printing what the ESP32 reads back on --pin.

PRECONDITION: the core must be stopped -- it holds the camera exclusively and
holds COM3 open. Use InspectionCore/tools/stop_core.ps1 (NOT taskkill /F).
Opening COM3 resets the board, so --pin work requires --allow-reset.

    python cam_line_probe.py --list
    python cam_line_probe.py --drive Line1 --pin 16 --allow-reset
"""
import argparse, os, sys, time

SDK = r"C:\Program Files (x86)\MVS\Development\Samples\Python"
if not os.path.isdir(SDK):
    sys.exit("MVS Python SDK not found at: " + SDK)
sys.path.append(SDK)
from MvImport.MvCameraControl_class import *  # noqa: E402,F403

ap = argparse.ArgumentParser()
ap.add_argument("--list", action="store_true", help="enumerate lines, read-only")
ap.add_argument("--drive", metavar="LINE", help="e.g. Line1 -- set to output and toggle")
ap.add_argument("--pin", type=int, default=None,
                help="ESP32 GPIO to read back while driving (needs --allow-reset)")
ap.add_argument("--port", default="COM3")
ap.add_argument("--baud", type=int, default=230400)
ap.add_argument("--allow-reset", action="store_true",
                help="acknowledge that opening COM3 reboots the board")
ap.add_argument("-n", type=int, default=6, help="toggles when driving")
ap.add_argument("--gap", type=float, default=0.5)
args = ap.parse_args()

if not args.list and not args.drive:
    ap.error("pick one: --list or --drive LineN")
if args.pin is not None and not args.allow_reset:
    ap.error("--pin opens COM3 which RESETS the board; pass --allow-reset")


# On these HikRobot models the output mode is spelled "Strobe" -- there is no
# entry literally called "Output". Matching on "Out" reports NONE and hides the
# only two lines that can drive.
OUT_MODES = ("Output", "Strobe")


def is_out(mode):
    return any(m in mode for m in OUT_MODES)


def ck(rc, what):
    if rc != 0:
        sys.exit("%s failed: 0x%08x" % (what, rc & 0xFFFFFFFF))


def get_enum_entries(cam, node):
    """Symbolic names this enum will actually accept on this model."""
    val = MVCC_ENUMVALUE()
    memset(byref(val), 0, sizeof(MVCC_ENUMVALUE))
    if cam.MV_CC_GetEnumValue(node, val) != 0:
        return None, []
    names = []
    for i in range(val.nSupportedNum):
        s = MVCC_ENUMENTRY()
        memset(byref(s), 0, sizeof(MVCC_ENUMENTRY))
        s.nValue = val.nSupportValue[i]
        if cam.MV_CC_GetEnumEntrySymbolic(node, s) == 0:
            names.append(s.chSymbolic.decode("ascii", "replace").strip("\x00"))
        else:
            names.append("<%d>" % val.nSupportValue[i])
    cur = None
    for i, nm in enumerate(names):
        if val.nSupportValue[i] == val.nCurValue:
            cur = nm
    return cur, names


def get_str_enum(cam, node):
    cur, _ = get_enum_entries(cam, node)
    return cur


def get_bool(cam, node):
    v = c_bool(False)
    if cam.MV_CC_GetBoolValue(node, v) != 0:
        return None
    return bool(v.value)


# ---- open the camera -------------------------------------------------------
dev_list = MV_CC_DEVICE_INFO_LIST()
ck(MvCamera.MV_CC_EnumDevices(MV_USB_DEVICE | MV_GIGE_DEVICE, dev_list), "EnumDevices")
if dev_list.nDeviceNum == 0:
    sys.exit("no camera found. Is the core still running? Try "
             "InspectionCore/tools/cam_device_reset.py")

info = cast(dev_list.pDeviceInfo[0], POINTER(MV_CC_DEVICE_INFO)).contents
cam = MvCamera()
ck(cam.MV_CC_CreateHandle(info), "CreateHandle")
ck(cam.MV_CC_OpenDevice(MV_ACCESS_Exclusive, 0), "OpenDevice")

try:
    sel_cur, lines = get_enum_entries(cam, "LineSelector")
    if not lines:
        sys.exit("this camera exposes no LineSelector -- no digital IO at all")

    print("LineSelector entries: %s" % ", ".join(lines))
    print("TriggerSource is currently: %s" % get_str_enum(cam, "TriggerSource"))
    print("TriggerMode   is currently: %s" % get_str_enum(cam, "TriggerMode"))
    print("")

    # ---- inventory ---------------------------------------------------------
    caps = {}
    for ln in lines:
        if cam.MV_CC_SetEnumValueByString("LineSelector", ln) != 0:
            print("%-8s <cannot select>" % ln)
            continue
        mode, modes = get_enum_entries(cam, "LineMode")
        status = get_bool(cam, "LineStatus")
        src = get_str_enum(cam, "LineSource")
        caps[ln] = modes
        print("%-8s mode=%-8s status=%-5s source=%-20s can_be=[%s]" % (
            ln, mode, status, src, ",".join(modes) or "?"))
    print("")

    if args.list:
        outs = [l for l, m in caps.items() if any(is_out(x) for x in m)]
        print("lines that can OUTPUT: %s" % (", ".join(outs) if outs else "NONE"))
        if outs:
            print("reverse test:  python cam_line_probe.py --drive %s --pin 16 "
                  "--allow-reset" % outs[0])
        sys.exit(0)

    # ---- drive -------------------------------------------------------------
    ln = args.drive
    if ln not in caps:
        sys.exit("no such line %r; camera has: %s" % (ln, ", ".join(lines)))
    if not any(is_out(m) for m in caps[ln]):
        sys.exit("%s cannot be an output on this model (modes: %s)"
                 % (ln, ", ".join(caps[ln])))

    ck(cam.MV_CC_SetEnumValueByString("LineSelector", ln), "LineSelector=" + ln)
    # Remember what this line was, so a diagnostic never leaves the camera
    # reconfigured for the next run of the core.
    restore = (ln, get_str_enum(cam, "LineMode"), get_str_enum(cam, "LineSource"))
    # LineMode is read-only while the line is armed as a trigger input:
    # SetEnumValue returns 0x80000106 (wrong access). Disarm first.
    trig_restore = get_str_enum(cam, "TriggerMode")
    if trig_restore and trig_restore != "Off":
        if cam.MV_CC_SetEnumValueByString("TriggerMode", "Off") == 0:
            print("TriggerMode Off (was %s) so LineMode becomes writable" % trig_restore)
    out_mode = next(m for m in caps[ln] if is_out(m))
    ck(cam.MV_CC_SetEnumValueByString("LineMode", out_mode), "LineMode=" + out_mode)

    # Point the line at a user-controlled bit rather than an exposure signal, so
    # nothing has to be grabbing for us to move it.
    _, srcs = get_enum_entries(cam, "LineSource")
    user_src = next((s for s in srcs if s.startswith("UserOutput")), None)
    if user_src is None:
        sys.exit("no UserOutput* LineSource on this model; sources: %s" % ", ".join(srcs))
    ck(cam.MV_CC_SetEnumValueByString("LineSource", user_src), "LineSource=" + user_src)

    _, usels = get_enum_entries(cam, "UserOutputSelector")
    if usels:
        pick = user_src if user_src in usels else usels[0]
        cam.MV_CC_SetEnumValueByString("UserOutputSelector", pick)

    print("driving %s as %s via %s" % (ln, out_mode, user_src))

    ser = None
    if args.pin is not None:
        import serial
        ser = serial.Serial(args.port, args.baud, timeout=1.0)
        time.sleep(2.0)          # the DTR reset above; let the board come back
        ser.reset_input_buffer()
        # uppercase only -- "input" is silently ignored and leaves the old mode
        ser.write(("pin_mode %d INPUT\n" % args.pin).encode())
        time.sleep(0.2)
        ser.reset_input_buffer()
        print("ESP32 GPIO%d set to INPUT on %s" % (args.pin, args.port))

    def read_pin():
        if ser is None:
            return "?"
        ser.reset_input_buffer()
        ser.write(("pin_read %d\n" % args.pin).encode())
        deadline = time.time() + 1.0
        while time.time() < deadline:
            line = ser.readline().decode("ascii", "replace").strip()
            if "val" in line:
                return line
        return "<no reply>"

    seen = set()
    for i in range(args.n):
        lvl = i % 2
        ck(cam.MV_CC_SetBoolValue("UserOutputValue", bool(lvl)), "UserOutputValue")
        time.sleep(args.gap)
        cam_reads = get_bool(cam, "LineStatus")
        esp = read_pin()
        seen.add(str(esp))
        print("  drive=%d   camera LineStatus=%-5s   esp32: %s" % (lvl, cam_reads, esp))

    print("")
    if ser is None:
        print("no --pin given: this only proves the camera moved its own line.")
        print("re-run with --pin <gpio> --allow-reset to see if it reaches the board.")
    elif len(seen) > 1:
        print("VERDICT: the ESP32 side FOLLOWED the camera -> the conductor is good.")
        print("         The break is on the camera's INPUT side (Line0 opto, or")
        print("         TriggerSource is naming a line the wire is not on).")
    else:
        print("VERDICT: the ESP32 side never moved -> open circuit in BOTH")
        print("         directions. Wire / connector / opto. Needs a meter on site.")
finally:
    try:
        if trig_restore:
            cam.MV_CC_SetEnumValueByString("TriggerMode", trig_restore)
    except NameError:
        pass
    try:
        rln, rmode, rsrc = restore
        cam.MV_CC_SetEnumValueByString("LineSelector", rln)
        if rsrc:
            cam.MV_CC_SetEnumValueByString("LineSource", rsrc)
        if rmode:
            cam.MV_CC_SetEnumValueByString("LineMode", rmode)
        print("restored %s to mode=%s source=%s" % (rln, rmode, rsrc))
    except NameError:
        pass                      # never got as far as touching a line
    cam.MV_CC_CloseDevice()
    cam.MV_CC_DestroyHandle()
