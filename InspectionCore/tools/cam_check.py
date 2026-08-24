"""Ask the camera directly what state it is in, and prove whether it can shoot.

Written after a long misdiagnosis on 2026-08-20: the board fired triggers
(`cam_trig` acked, q=1..10), the core opened the camera, configured it and
logged "grabbing STARTED" -- and not one frame arrived. Every layer reported
success. The only way to settle it is to bypass the core and ask the device.

THE CORE MUST BE STOPPED (the camera opens exclusively). Stop it with
tools/stop_core.ps1 -- NOT taskkill /F, which wedges the camera (docs/README.md).

    python cam_check.py              # status + soft-trigger test + free-run test
    python cam_check.py --status     # read-only: change nothing, shoot nothing

What the outcomes mean:
  soft OK, free-run OK   -> the sensor and the link are fine; the fault is in the
                            hardware trigger path (wiring/opto/TriggerSource) or
                            upstream of it.
  soft FAIL, free-run OK -> trigger configuration, not the sensor.
  both FAIL              -> the camera is not delivering at all: wedged by a
                            force-killed core (run cam_device_reset.py) or a
                            genuine hardware/USB fault.
"""
import sys, os, time

SDK = r"C:\Program Files (x86)\MVS\Development\Samples\Python"
if not os.path.isdir(SDK):
    sys.exit("MVS Python SDK not found at: " + SDK)
sys.path.append(SDK)
from MvImport.MvCameraControl_class import *  # noqa: E402,F403

def get_int(cam, key):
    v = MVCC_INTVALUE_EX()
    memset(byref(v), 0, sizeof(MVCC_INTVALUE_EX))
    return v.nCurValue if cam.MV_CC_GetIntValueEx(key, v) == 0 else None

def get_enum(cam, key):
    v = MVCC_ENUMVALUE()
    memset(byref(v), 0, sizeof(MVCC_ENUMVALUE))
    return v.nCurValue if cam.MV_CC_GetEnumValue(key, v) == 0 else None

def get_float(cam, key):
    v = MVCC_FLOATVALUE()
    memset(byref(v), 0, sizeof(MVCC_FLOATVALUE))
    return v.fCurValue if cam.MV_CC_GetFloatValue(key, v) == 0 else None

def get_bool(cam, key):
    v = c_bool(False)
    return v.value if cam.MV_CC_GetBoolValue(key, v) == 0 else None

def grab(cam, n, timeout_ms, label):
    """Count frames actually delivered. This is the only number that matters."""
    got = 0
    for _ in range(n):
        f = MV_FRAME_OUT()
        memset(byref(f), 0, sizeof(f))
        if cam.MV_CC_GetImageBuffer(f, timeout_ms) == 0:
            got += 1
            print("    frame %d: %dx%d  %d bytes  frameNum=%d"
                  % (got, f.stFrameInfo.nWidth, f.stFrameInfo.nHeight,
                     f.stFrameInfo.nFrameLen, f.stFrameInfo.nFrameNum))
            cam.MV_CC_FreeImageBuffer(f)
    print("  %s: %d/%d frames" % (label, got, n))
    return got

def main():
    status_only = "--status" in sys.argv
    devs = MV_CC_DEVICE_INFO_LIST()
    if MvCamera.MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, devs) != 0:
        sys.exit("EnumDevices failed")
    if devs.nDeviceNum == 0:
        sys.exit("no camera enumerated -- power/USB fault (a WEDGED camera still enumerates)")
    print("cameras: %d" % devs.nDeviceNum)

    info = cast(devs.pDeviceInfo[0], POINTER(MV_CC_DEVICE_INFO)).contents
    cam = MvCamera()
    if cam.MV_CC_CreateHandle(info) != 0:
        sys.exit("CreateHandle failed")
    ret = cam.MV_CC_OpenDevice(MV_ACCESS_Exclusive, 0)
    if ret != 0:
        cam.MV_CC_DestroyHandle()
        sys.exit("OpenDevice failed 0x%x -- the core is probably still running. "
                 "Stop it with tools/stop_core.ps1 first." % ret)

    print("\n-- status --")
    for k in ("Width", "Height", "OffsetX", "OffsetY", "WidthMax", "HeightMax", "PayloadSize"):
        print("  %-14s %s" % (k, get_int(cam, k)))
    for k in ("TriggerMode", "TriggerSource", "TriggerSelector", "TriggerActivation", "PixelFormat"):
        print("  %-14s %s" % (k, get_enum(cam, k)))
    print("  %-14s %s" % ("ExposureTime", get_float(cam, "ExposureTime")))
    print("  %-14s %s" % ("ResultingFrameRate", get_float(cam, "ResultingFrameRate")))
    print("  %-14s %s" % ("AcqFrameRateEn", get_bool(cam, "AcquisitionFrameRateEnable")))
    print("  %-14s %s" % ("BurstFrameCount", get_int(cam, "AcquisitionBurstFrameCount")))

    if status_only:
        cam.MV_CC_CloseDevice(); cam.MV_CC_DestroyHandle(); return 0

    saved_mode = get_enum(cam, "TriggerMode")
    saved_src = get_enum(cam, "TriggerSource")

    print("\n-- soft trigger test --")
    cam.MV_CC_SetEnumValueByString("TriggerMode", "On")
    cam.MV_CC_SetEnumValueByString("TriggerSource", "Software")
    r = cam.MV_CC_StartGrabbing()
    print("  StartGrabbing ret=0x%x" % r)
    soft = 0
    if r == 0:
        for _ in range(5):
            cam.MV_CC_SetCommandValue("TriggerSoftware")
            time.sleep(0.1)
        soft = grab(cam, 5, 2000, "soft trigger")
        cam.MV_CC_StopGrabbing()

    print("\n-- free-run test --")
    cam.MV_CC_SetEnumValueByString("TriggerMode", "Off")
    r = cam.MV_CC_StartGrabbing()
    print("  StartGrabbing ret=0x%x" % r)
    free = grab(cam, 5, 2000, "free run") if r == 0 else 0
    cam.MV_CC_StopGrabbing()

    # Put the trigger config back exactly as found: this is a diagnostic, and a
    # diagnostic that leaves the machine differently configured is a trap for
    # whoever runs it next.
    if saved_mode is not None:
        cam.MV_CC_SetEnumValue("TriggerMode", saved_mode)
    if saved_src is not None:
        cam.MV_CC_SetEnumValue("TriggerSource", saved_src)
    print("\nrestored TriggerMode=%s TriggerSource=%s" % (saved_mode, saved_src))

    cam.MV_CC_CloseDevice(); cam.MV_CC_DestroyHandle()

    print("\n== verdict ==")
    if soft and free:
        print("sensor and link OK -- the fault is in the HARDWARE TRIGGER path")
        print("(wiring/opto/TriggerSource), not in the camera itself.")
        return 0
    if free and not soft:
        print("free-run works, soft trigger does not -- trigger CONFIGURATION fault.")
        return 1
    print("NO frames by either route -- camera is not delivering.")
    print("If the last core was force-killed: python cam_device_reset.py")
    print("If that does not fix it: physical replug / power cycle.")
    return 2

if __name__ == "__main__":
    sys.exit(main())
