"""DeviceReset a wedged HikRobot camera, without touching the USB cable.

When a core is killed before it releases the camera (taskkill /F, kill -9), the
camera survives in a state where it still ENUMERATES and still ACCEPTS SETTINGS
but delivers no frames on hardware trigger -- so every later core sees "camera
present, cam_status 0, zero frames", which reads exactly like a dead trigger
wire or a board fault. The board's own `cam_trig` reply proves the pulse went
out, and that is the tell.

docs/README.md states the rule this recovers from: never kill -9 the core.
InspectionCore/docs/REGRESSION_TESTS.md documents the lighter recovery to try
FIRST (roi_restore.mjs -> rc_once.mjs); this is the next step when that fails,
and it is what UINSP_CAVEATS.md's "never READY" section reaches for.

THE CORE MUST BE STOPPED: the camera can only be opened by one process, so this
exits rather than fighting for the handle.

    python cam_device_reset.py            # reset the first camera found
    python cam_device_reset.py --list     # enumerate only, change nothing
"""
import sys, time, os

SDK = r"C:\Program Files (x86)\MVS\Development\Samples\Python"
if not os.path.isdir(SDK):
    sys.exit("MVS Python SDK not found at: " + SDK)
sys.path.append(SDK)

from MvImport.MvCameraControl_class import *  # noqa: E402,F403

def main():
    list_only = "--list" in sys.argv
    dev_list = MV_CC_DEVICE_INFO_LIST()
    # USB3 and GigE both, so this works on either rig.
    tlayer = MV_GIGE_DEVICE | MV_USB_DEVICE
    ret = MvCamera.MV_CC_EnumDevices(tlayer, dev_list)
    if ret != 0:
        sys.exit("MV_CC_EnumDevices failed: 0x%x" % ret)
    print("cameras found: %d" % dev_list.nDeviceNum)
    if dev_list.nDeviceNum == 0:
        sys.exit("no camera -- is it powered/plugged? (a wedged camera still enumerates,"
                 " so ZERO here is a different fault)")

    for i in range(dev_list.nDeviceNum):
        info = cast(dev_list.pDeviceInfo[i], POINTER(MV_CC_DEVICE_INFO)).contents
        if info.nTLayerType == MV_USB_DEVICE:
            name = "".join(chr(c) for c in info.SpecialInfo.stUsb3VInfo.chModelName if c)
            sn = "".join(chr(c) for c in info.SpecialInfo.stUsb3VInfo.chSerialNumber if c)
        else:
            name = "".join(chr(c) for c in info.SpecialInfo.stGigEInfo.chModelName if c)
            sn = "".join(chr(c) for c in info.SpecialInfo.stGigEInfo.chSerialNumber if c)
        print("  [%d] %s  sn=%s" % (i, name, sn))

    if list_only:
        return 0

    info = cast(dev_list.pDeviceInfo[0], POINTER(MV_CC_DEVICE_INFO)).contents
    cam = MvCamera()
    ret = cam.MV_CC_CreateHandle(info)
    if ret != 0:
        sys.exit("CreateHandle failed: 0x%x" % ret)
    # Exclusive open. If the core is still running this is where it fails, and
    # that is the right place to stop -- resetting a camera out from under a
    # running core would take the machine down mid-part.
    ret = cam.MV_CC_OpenDevice(MV_ACCESS_Exclusive, 0)
    if ret != 0:
        cam.MV_CC_DestroyHandle()
        sys.exit("OpenDevice failed: 0x%x  -- is the core still running? stop it first." % ret)

    print("issuing DeviceReset ...")
    ret = cam.MV_CC_SetCommandValue("DeviceReset")
    print("DeviceReset ret: 0x%x %s" % (ret, "OK" if ret == 0 else "FAILED"))
    # The device drops off the bus on reset; Close/Destroy will complain and that
    # is expected, so their return codes are informational only.
    cam.MV_CC_CloseDevice()
    cam.MV_CC_DestroyHandle()
    if ret != 0:
        return 1

    print("waiting for the camera to come back ...")
    for attempt in range(1, 21):
        time.sleep(2)
        again = MV_CC_DEVICE_INFO_LIST()
        if MvCamera.MV_CC_EnumDevices(tlayer, again) == 0 and again.nDeviceNum > 0:
            print("camera back after %ds" % (attempt * 2))
            return 0
    print("camera did NOT re-enumerate within 40s -- a physical replug is the next step")
    return 2

if __name__ == "__main__":
    sys.exit(main())
