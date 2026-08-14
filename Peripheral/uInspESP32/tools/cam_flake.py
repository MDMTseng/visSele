#!/usr/bin/env python3
"""How often does the camera fail to start, and does anything change the rate?

The camera intermittently refuses AcquisitionStart -- "USB3Vision write_memory
error (invalid-parameter)" -- while still enumerating and still accepting
configuration. The core does not check the failure, so it runs with no frames
at all: the board fires calibration pulses into a void and halts on
CAM_CLOCK_CAL_FAILED, which from the board is indistinguishable from a clock
fault.

Three hypotheses about the trigger were each proposed and then killed by their
own experiment on 2026-08-09 (SIGTERM escalation, externally applied geometry,
payload mismatch). Every one of them rested on a SINGLE run. That is the actual
mistake being corrected here: an intermittent fault cannot be attributed from
one observation, and it will happily confirm whatever was tried most recently.

So measure a RATE. Each trial is identical apart from the condition under test,
and a condition only means something if its rate differs across enough trials
to be more than noise.

Conditions:
  plain     start the core, enter inspection, record whether calibration
            converged. Nothing touched beforehand.
  reset     DeviceReset first, wait for re-enumeration, then as above.
  roi       DeviceReset, then apply an ROI from outside, then as above.
  probe     DeviceReset, then start+stop acquisition from outside (proving the
            camera works), then as above. Separates "the camera is broken" from
            "the core breaks it".
  stepper   DeviceReset, then stepper_enable with the plate held at 0 -- the
            driver energised but nothing turning.
  spin      DeviceReset, stepper_enable AND plate 15000 -- the driver actually
            stepping. Against `stepper` this separates "energised" from
            "moving"; against `reset` it isolates the one command that the
            passing conditions never send and the failing tool always does.

Each trial also records whether the camera could stream BEFORE the core started
and AFTER it stopped, so a failure can be placed on one side of the core.

  python3 cam_flake.py --trials 5 --conditions plain,reset,roi,probe
"""
import argparse, json, os, socket, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from soak_sched import (CONN, PORT, CORE_BIN, CORE_DIR, CAM_PY,
                        camera_ok, core_start, core_stop)

CAM_RESET_ONLY = """
import gi
gi.require_version('Aravis','0.8')
from gi.repository import Aravis
Aravis.update_device_list()
Aravis.Camera.new(None).get_device().execute_command('DeviceReset')
print('reset')
"""

CAM_SET_ROI = """
import gi
gi.require_version('Aravis','0.8')
from gi.repository import Aravis
Aravis.update_device_list()
cam = Aravis.Camera.new(None)
cam.set_region(1248, 428, 560, 452)
cam.set_exposure_time(50.0)
print('roi set')
"""


def sh(script, timeout=90):
    try:
        p = subprocess.run([CAM_PY, "-c", script], capture_output=True,
                           text=True, timeout=timeout)
        return p.returncode == 0
    except Exception:
        return False


def device_reset_and_wait():
    sh(CAM_RESET_ONLY)
    # Re-enumeration takes tens of seconds and the device disappears meanwhile.
    for _ in range(30):
        time.sleep(5)
        ok, _ = camera_ok()
        if ok:
            return True
    return False


def try_calibrate(seconds=45, plate_freq=15000, stepper=False, poll=False,
                  gap=0.6):
    """Enter inspection once and report what the clock calibration did.

    Returns (converged, sync_pulses, learned, err_list) or None if the board
    could not be reached at all.
    """
    try:
        s = socket.create_connection(("127.0.0.1", PORT), timeout=5)
    except OSError:
        return None
    s.settimeout(0.4)
    s.sendall(("!pd " + json.dumps(CONN) + "\n").encode())
    time.sleep(3.5)
    try:
        s.recv(65536)
    except socket.timeout:
        pass
    s.sendall(b'{"type":"clear_error"}\n')
    time.sleep(gap)
    s.sendall(('{"type":"set_setup","plate":{"freq":%d}}\n' % plate_freq).encode())
    time.sleep(gap)
    # stepper_enable is the one command the passing conditions never send, and
    # soak_real -- which fails every time -- does. Energising the driver (and
    # then turning the plate) is a plausible physical disturbance for a USB3
    # camera, and it would explain the whole pattern: still-plate phantom runs
    # pass, real-parts runs fail, the 3-hour regress at plate 3500 survived, and
    # once disturbed the camera stays wedged until a DeviceReset.
    if stepper:
        s.sendall(b'{"type":"stepper_enable"}\n')
        time.sleep(gap)
    s.sendall(b'{"type":"enter_insp_mode"}\n')

    # poll: soak_real watches for READY by asking get_running_stat about once a
    # second while calibration is running. That reply is ~1.7kB of JSON built on
    # the same MCU that is timing the sync pulses, so the watcher is not free.
    # Everything else about the two sequences matches, and soak_real fails every
    # time on a machine that passes 30/30 without it -- this is the last
    # difference left, and if it reproduces, the monitor is breaking the thing
    # it monitors.
    buf, t0, last_poll = b"", time.time(), 0.0
    while time.time() - t0 < seconds:
        if poll and time.time() - last_poll > 1.0:
            last_poll = time.time()
            try:
                s.sendall(b'{"type":"get_running_stat"}\n')
            except OSError:
                pass
        try:
            buf += s.recv(65536)
        except socket.timeout:
            continue
        if b"CAMSYNC CAL ok" in buf or b"CAMSYNC CAL FAILED" in buf:
            break
    s.sendall(b'{"type":"get_running_stat"}\n')
    time.sleep(2.5)
    t0 = time.time()
    while time.time() - t0 < 4:
        try:
            buf += s.recv(65536)
        except socket.timeout:
            pass
    # Always leave the machine stopped, whatever happened.
    s.sendall(b'{"type":"exit_insp_mode"}\n')
    time.sleep(1.2)
    s.sendall(b'{"type":"set_setup","plate":{"freq":0}}\n')
    time.sleep(1.2)
    s.close()

    text = buf.decode("utf8", "replace")
    converged = "CAMSYNC CAL ok" in text
    pulses = learned = None
    errs = []
    for line in text.splitlines():
        if "cam_sync" in line:
            try:
                j = json.loads(line.split("*")[0])
                pulses = j["cam_sync"].get("sync_pulses")
                learned = j["cam_sync"].get("learned")
                errs = j.get("error_hist", [])
            except Exception:
                pass
    return converged, pulses, learned, errs


def trial(cond, idx, logf):
    line = {"cond": cond, "i": idx}

    if cond in ("reset", "roi", "probe", "stepper", "spin", "spin10k", "poll", "fastgap"):
        line["reset_ok"] = device_reset_and_wait()
    if cond == "roi":
        line["roi_set"] = sh(CAM_SET_ROI)
    if cond == "probe":
        ok, _ = camera_ok()
        line["probe_before"] = ok

    ok, why = camera_ok()
    line["cam_before"] = ok
    if not ok:
        line["cam_before_why"] = (why or [""])[0][:60]

    p, cfh, err = core_start({}, os.path.join(logf, "%s_%d.core.log" % (cond, idx)))
    if err:
        core_stop(p, cfh)
        line["core"] = err
        return line

    # spin10k is soak_real's plate speed. spin passes 3/3 at 15000 and
    # soak_real fails every time at 10000 with everything else the same, so the
    # speed is the last difference left standing.
    freq = {"stepper": 0, "spin10k": 10000}.get(cond, 15000)
    # fastgap is soak_real's 0.3s spacing between the four setup commands.
    # Every other difference between the two sequences has been eliminated --
    # CONNECT payload identical, plate speed, stepper_enable, polling during
    # calibration -- and 36 trials of this harness pass while soak_real fails
    # every time on a machine verified clean beforehand. This is what is left.
    r = try_calibrate(plate_freq=freq,
                      stepper=cond in ("stepper", "spin", "spin10k", "poll",
                                       "fastgap"),
                      poll=(cond in ("poll", "fastgap")),
                      gap=(0.3 if cond == "fastgap" else 0.6))
    if r is None:
        line["cal"] = "unreachable"
    else:
        conv, pulses, learned, errs = r
        line["cal"] = "ok" if conv else "FAILED"
        line["pulses"] = pulses
        line["learned"] = learned
        line["err"] = errs

    core_stop(p, cfh)
    time.sleep(3)
    ok, why = camera_ok()
    line["cam_after"] = ok
    return line


def main(a):
    outdir = os.path.join(HERE, "soak_runs", "cam_flake")
    os.makedirs(outdir, exist_ok=True)
    conds = [c.strip() for c in a.conditions.split(",") if c.strip()]
    rows = []
    for cond in conds:
        for i in range(a.trials):
            row = trial(cond, i, outdir)
            rows.append(row)
            print(json.dumps(row, ensure_ascii=False), flush=True)

    print("\n===== rate by condition =====")
    for cond in conds:
        sub = [r for r in rows if r["cond"] == cond]
        ok = sum(1 for r in sub if r.get("cal") == "ok")
        cb = sum(1 for r in sub if r.get("cam_before"))
        ca = sum(1 for r in sub if r.get("cam_after"))
        print("  %-6s cal ok %d/%d   camera ok before %d/%d  after %d/%d"
              % (cond, ok, len(sub), cb, len(sub), ca, len(sub)))
    with open(os.path.join(outdir, "rows.json"), "w") as f:
        json.dump(rows, f, indent=1)
    return 0


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--trials", type=int, default=5)
    ap.add_argument("--conditions", default="plain,reset,roi,probe")
    sys.exit(main(ap.parse_args()))
