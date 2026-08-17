#!/usr/bin/env python3
"""Prove the clean-space region: the dark measurement, and its effect on the part.

Everything runs through `visSele --insp`, so it is the real parse -> place ->
measure -> self-judge path, not a unit stub.

Two things are being checked and they are separate claims:

  A. dark_ratio/dark_area_mm2 measure what they say. A threshold sweep over the
     SAME image must be monotonic in dark_ratio: raise the grey level that counts
     as "dark" and the dark fraction can only grow. A stat that is not monotonic
     here is not counting pixels under a threshold, whatever else it is doing.

  B. a tripped bound reaches the part. Same region, same image, one bound moved
     across the measured value -> the region's status must flip, and it must flip
     to what on_fail asked for (NA by default, FAILURE only when on_fail:"ng").

The region is placed with ignore_translation+ignore_rotation so it sits at a
fixed image location regardless of where the part was found -- otherwise a pose
difference between runs would change the pixels underneath and A would be
measuring two different things.
"""
import json, os, subprocess, sys

ROOT  = "/Users/mdm/workspace/visSele/.claude/worktrees/uinsp-mini-compact/InspectionCore"
BUILD = ROOT + "/build/mac-arm64"
CORE  = ROOT + "/Core0_1"
VIS   = BUILD + "/visSele"
TEST  = "/Users/mdm/workspace/HY_sync/DEV/test"
IMG   = TEST + "/10221 BOS-LT12BH4211 SORTING_bk.png"
GDEF  = TEST + "/10221 BOS-LT12BH4211 SORTING_bk.hydef"
TMP   = "/Users/mdm/.claude/jobs/823502fe/tmp/od"
os.makedirs(TMP, exist_ok=True)

STATUS_SUCCESS, STATUS_FAILURE, STATUS_NA = 0, -1, -128


def run(defobj, tag):
    dp, op = f"{TMP}/{tag}.hydef", f"{TMP}/{tag}.json"
    json.dump(defobj, open(dp, "w"))
    env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD)
    p = subprocess.run([VIS, "--insp", IMG, dp, op], cwd=CORE, env=env,
                       capture_output=True, timeout=180)
    if not os.path.exists(op):
        return None, p.returncode, (p.stderr or b"")[-400:].decode(errors="replace")
    return json.load(open(op)), p.returncode, ""


def find(o, key):
    """First value under `key` anywhere in the tree (the report nests deeply)."""
    if isinstance(o, dict):
        if key in o:
            return o[key]
        for v in o.values():
            r = find(v, key)
            if r is not None:
                return r
    elif isinstance(o, list):
        for v in o:
            r = find(v, key)
            if r is not None:
                return r
    return None


def with_region(**over):
    """Golden def + one obj_detect region pinned to an absolute image position."""
    d = json.load(open(GDEF))
    reg = {"type": "obj_detect", "id": 90001, "name": "CLEANSPACE",
           "pt1": {"x": -3.0, "y": -3.0}, "pt2": {"x": 3.0, "y": 3.0},
           "ignore_rotation": True, "ignore_translation": True}
    reg.update(over)
    fs = d["featureSet"][0]
    fs["features"] = [f for f in fs["features"] if f.get("id") != 90001] + [reg]
    d.pop("featureSet_sha1", None)
    return d


def region_of(rep):
    ods = find(rep, "objDetects") or find(rep, "detectedObjDetects")
    if ods is None:
        return None
    for r in ods:
        if r.get("id") == 90001:
            return r
    return None


fails = []


def check(name, cond, detail=""):
    print("  %-52s %s%s" % (name, "PASS" if cond else "FAIL", "  " + detail if detail else ""))
    if not cond:
        fails.append(name)


# --- locate the array name the core actually emits ------------------------
rep, rc, err = run(with_region(dark_thresh=128), "probe")
if rep is None:
    print("harness: --insp produced no output (rc=%s) %s" % (rc, err))
    sys.exit(1)
r = region_of(rep)
if r is None:
    print("harness: no obj_detect report with id 90001 in output")
    print("top-level keys:", list(rep.keys()))
    sys.exit(1)
print("region report @thresh=128:", json.dumps(r, sort_keys=True)[:220])
print()

# --- A. the dark stat is a threshold count -------------------------------
print("A. dark_ratio monotonic in dark_thresh (same image, pinned region)")
prev, seen = -1.0, []
for th in (16, 64, 128, 192, 240):
    rp, _, _ = run(with_region(dark_thresh=th), f"th{th}")
    rr = region_of(rp) if rp else None
    if rr is None or "dark_ratio" not in rr:
        check(f"thresh {th} reported dark_ratio", False)
        continue
    v = rr["dark_ratio"]
    seen.append((th, round(v, 5), round(rr["dark_area_mm2"], 5)))
    check(f"thresh {th:3d}: dark_ratio {v:.5f} >= previous {prev:.5f}", v >= prev - 1e-9)
    prev = v
check("dark_ratio actually varies (not a constant)",
      len({s[1] for s in seen}) > 1, str(seen))
check("dark_area_mm2 tracks dark_ratio (both 0 or both >0)",
      all((s[1] > 0) == (s[2] > 0) for s in seen))

# --- dark_thresh absent = no dark measurement at all ---------------------
rp, _, _ = run(with_region(), "nothresh")
rr = region_of(rp) if rp else None
check("no dark_thresh -> no dark_ratio field emitted",
      rr is not None and "dark_ratio" not in rr,
      str(sorted(rr.keys())) if rr else "no report")

# --- B. a tripped bound reaches the part ---------------------------------
print("\nB. bound violation -> region status -> on_fail")
base, _, _ = run(with_region(dark_thresh=128), "base")
b = region_of(base)
measured = b["dark_ratio"]
loose = min(1.0, measured + 0.25)          # comfortably above  -> pass
tight = max(0.0, measured - 0.25)          # comfortably below  -> trip
check("baseline region passes with no bounds", b["status"] == STATUS_SUCCESS,
      "status=%s dark_ratio=%.5f" % (b["status"], measured))

rp, _, _ = run(with_region(dark_thresh=128, dark_ratio_max=loose), "loose")
rr = region_of(rp)
check("dark_ratio_max above measured -> SUCCESS", rr["status"] == STATUS_SUCCESS,
      "max=%.5f status=%s" % (loose, rr["status"]))

rp, _, _ = run(with_region(dark_thresh=128, dark_ratio_max=tight), "tight_default")
rr = region_of(rp)
check("dark_ratio_max below measured, default on_fail -> NA",
      rr["status"] == STATUS_NA, "max=%.5f status=%s" % (tight, rr["status"]))

rp, _, _ = run(with_region(dark_thresh=128, dark_ratio_max=tight, on_fail="na"), "tight_na")
rr = region_of(rp)
check('on_fail:"na" -> NA', rr["status"] == STATUS_NA, "status=%s" % rr["status"])

rp, _, _ = run(with_region(dark_thresh=128, dark_ratio_max=tight, on_fail="ng"), "tight_ng")
rr = region_of(rp)
check('on_fail:"ng" -> FAILURE', rr["status"] == STATUS_FAILURE, "status=%s" % rr["status"])

# area bound uses the same path but a different unit -- check it is wired
area = b["dark_area_mm2"]
rp, _, _ = run(with_region(dark_thresh=128, dark_area_max=max(0.0, area * 0.5),
                           on_fail="ng"), "area_tight")
rr = region_of(rp)
check("dark_area_max below measured -> FAILURE",
      rr["status"] == STATUS_FAILURE if area > 0 else True,
      "measured=%.5fmm2 status=%s" % (area, rr["status"]))

# each region carries its own threshold -- two regions, two thresholds, one def
d = with_region(dark_thresh=32, id=90001)
fs = d["featureSet"][0]
second = dict(fs["features"][-1])
second.update({"id": 90002, "name": "CLEANSPACE2", "dark_thresh": 240})
fs["features"].append(second)
rp, _, _ = run(d, "two_regions")
ods = find(rp, "objDetects") or find(rp, "detectedObjDetects") or []
byid = {o["id"]: o for o in ods if "id" in o}
check("two regions, per-region thresholds are independent",
      90001 in byid and 90002 in byid
      and "dark_ratio" in byid[90001] and "dark_ratio" in byid[90002]
      and byid[90002]["dark_ratio"] >= byid[90001]["dark_ratio"],
      "th32=%.5f th240=%.5f" % (byid.get(90001, {}).get("dark_ratio", -1),
                                byid.get(90002, {}).get("dark_ratio", -1)))

print("\n=> %s" % ("ALL PASS" if not fails else "%d FAILED: %s" % (len(fails), fails)))
sys.exit(1 if fails else 0)
