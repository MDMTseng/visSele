"""cam_hwtrig_win -- board fires, camera counts. The wiring verdict, on Windows.

The Windows/HikRobot sibling of cam_grab.py's `hwtrig` (that one needs Aravis,
which this bench does not have). It closes the same loop without an
oscilloscope: this process ARMS the camera on its hardware trigger and COUNTS
what arrives, while driving the ESP32 over COM to fire the pulses.

    pulses fired vs frames received IS the answer:
      N fired, N received  -> trigger path is good
      N fired, 0 received  -> the pulse is not reaching the sensor: wiring, opto,
                              TriggerSource pointing at the wrong line, or a
                              camera wedged by a force-killed core
      N fired, some lost   -> over-triggering (the camera DISCARDS rather than
                              queues) or exposure/readout overlap

Written 2026-08-20, after the board acked `cam_trig` ten times, the core logged
"grabbing STARTED", and zero frames arrived -- with every layer reporting
success and nothing able to say which side was lying.

TWO PRECONDITIONS, both hard:
  * THE CORE MUST BE STOPPED. It holds the camera exclusively and holds COM3
    open. Stop it with InspectionCore/tools/stop_core.ps1 (NOT taskkill /F,
    which wedges the camera -- docs/README.md).
  * OPENING COM3 RESETS THE BOARD (DTR; see board_query.py -- de-asserting
    beforehand does not help on this rig). So this is for a machine that is
    already stopped, and --allow-reset is required so it is never a surprise.

    python cam_hwtrig_win.py --allow-reset            # 10 pulses
    python cam_hwtrig_win.py --allow-reset -n 30 --gap 0.2
"""
import argparse, json, os, sys, time

SDK = r"C:\Program Files (x86)\MVS\Development\Samples\Python"
if not os.path.isdir(SDK):
    sys.exit("MVS Python SDK not found at: " + SDK)
sys.path.append(SDK)
from MvImport.MvCameraControl_class import *  # noqa: E402,F403
import serial  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("--port", default="COM3")
ap.add_argument("--baud", type=int, default=230400)
ap.add_argument("-n", "--pulses", type=int, default=10)
ap.add_argument("--gap", type=float, default=0.3, help="seconds between pulses")
ap.add_argument("--timeout", type=int, default=1500, help="ms to wait per frame")
ap.add_argument("--allow-reset", action="store_true",
                help="acknowledge that opening COM resets the board")
# Drive the trigger pin DIRECTLY instead of going through trig_cam_pulse.
#
# trig_cam_pulse emits a 100us pulse -- and 100us is the documented trigger
# FLOOR, i.e. the narrowest the rig was ever shown to work with. If the camera
# answers a wide manual pulse but not the 100us one, the fault is width/edge,
# not wiring, and that is a completely different repair. pin_on/pin_off take the
# firmware's own pin numbers: CAM1 is GPIO 17 (HardwareConfig.hpp).
ap.add_argument("--pin", action="store_true",
                help="drive the pin directly with pin_on/pin_off instead of trig_cam_pulse")
ap.add_argument("--pin-no", type=int, default=17, help="GPIO to drive (CAM1=17, CAM2=19)")
ap.add_argument("--width", type=float, default=0.05,
                help="seconds the pin is held asserted in --pin mode (default 50ms = 500x the 100us pulse)")
# CAM1 idles HIGH (pin_read val=1 with nothing driving it), so the trigger is
# asserted by pulling LOW. Getting this backwards produces a test that drives
# the line to the level it is already at and concludes "no frames" -- which is
# how the first run of this went.
ap.add_argument("--active-high", action="store_true",
                help="assert by driving HIGH (default: LOW, matching CAM1's idle-high line)")
a = ap.parse_args()

if not a.allow_reset:
    sys.exit("opening %s RESETS the board. Re-run with --allow-reset once the "
             "machine is stopped." % a.port)

# ---- camera: arm on hardware trigger ---------------------------------------
devs = MV_CC_DEVICE_INFO_LIST()
if MvCamera.MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, devs) != 0 or devs.nDeviceNum == 0:
    sys.exit("no camera enumerated")
info = cast(devs.pDeviceInfo[0], POINTER(MV_CC_DEVICE_INFO)).contents
cam = MvCamera()
if cam.MV_CC_CreateHandle(info) != 0:
    sys.exit("CreateHandle failed")
ret = cam.MV_CC_OpenDevice(MV_ACCESS_Exclusive, 0)
if ret != 0:
    cam.MV_CC_DestroyHandle()
    sys.exit("OpenDevice failed 0x%x -- is the core still running?" % ret)

def enum_of(key):
    v = MVCC_ENUMVALUE(); memset(byref(v), 0, sizeof(MVCC_ENUMVALUE))
    return v.nCurValue if cam.MV_CC_GetEnumValue(key, v) == 0 else None

saved = (enum_of("TriggerMode"), enum_of("TriggerSource"))
cam.MV_CC_SetEnumValueByString("TriggerMode", "On")
# "Anyway" accepts an edge on ANY input line, so this does not assume the wire
# is on Line0 -- which is the assumption that makes the Line0 variant of this
# test lie when the trigger is actually on Line1/Line2.
if cam.MV_CC_SetEnumValueByString("TriggerSource", "Anyway") != 0:
    cam.MV_CC_SetEnumValueByString("TriggerSource", "Line0")
r = cam.MV_CC_StartGrabbing()
print("camera armed: TriggerMode=On Source=%s StartGrabbing=0x%x"
      % (enum_of("TriggerSource"), r))
if r != 0:
    cam.MV_CC_CloseDevice(); cam.MV_CC_DestroyHandle()
    sys.exit("StartGrabbing failed -- camera wedged? run InspectionCore/tools/cam_device_reset.py")

# ---- board: fire the pulses -------------------------------------------------
print("opening %s (this resets the board) ..." % a.port)
s = serial.Serial()
s.port, s.baudrate, s.timeout = a.port, a.baud, 0.2
s.open()
time.sleep(2.0)          # let the firmware boot before it is spoken to
s.reset_input_buffer()

fired = acked = frames = 0
for i in range(a.pulses):
    s.write(b'{"type":"trig_cam_pulse"}\n')
    s.flush()
    fired += 1
    deadline = time.time() + a.gap
    while time.time() < deadline:
        line = s.readline()
        if line and b'cam_trig' in line:
            acked += 1
            break
    f = MV_FRAME_OUT(); memset(byref(f), 0, sizeof(f))
    if cam.MV_CC_GetImageBuffer(f, a.timeout) == 0:
        frames += 1
        print("  pulse %2d -> frame %dx%d frameNum=%d"
              % (i + 1, f.stFrameInfo.nWidth, f.stFrameInfo.nHeight, f.stFrameInfo.nFrameNum))
        cam.MV_CC_FreeImageBuffer(f)
    else:
        print("  pulse %2d -> NO FRAME (%dms)" % (i + 1, a.timeout))

s.close()
cam.MV_CC_StopGrabbing()
if saved[0] is not None: cam.MV_CC_SetEnumValue("TriggerMode", saved[0])
if saved[1] is not None: cam.MV_CC_SetEnumValue("TriggerSource", saved[1])
cam.MV_CC_CloseDevice(); cam.MV_CC_DestroyHandle()

print("\n== verdict ==")
print("pulses fired      : %d" % fired)
print("board acked       : %d  (cam_trig replies -- the board's own claim)" % acked)
print("frames received   : %d" % frames)
if frames == fired:
    print("TRIGGER PATH GOOD.")
elif frames == 0 and acked:
    print("The board fires and the camera sees NOTHING: the pulse is not reaching")
    print("the sensor. Check the wire/opto between CAM pin and the camera's trigger")
    print("input, and that the camera is not wedged (cam_device_reset.py).")
elif frames == 0:
    print("No frames AND no board acks -- suspect the board/serial side first.")
else:
    print("Partial: %d of %d. Over-triggering discards frames (the camera drops"
          % (frames, fired))
    print("rather than queues), as does a trigger landing during readout.")
sys.exit(0 if frames == fired else 2)
