#!/usr/bin/env python3
"""
Migration gate -- run the engine on the golden sample and report per-judge
drift against the committed baseline (`expected/10221.json`). Used as the
safety net for the acv -> cv migration: each step is expected to keep judge
values within MAX_DRIFT_MM (no bit-identical requirement).

Exit 0 if all judges drift <= tolerance OR a judge is NA in both runs.
Exit 1 if any drift exceeds tolerance OR a previously-finite judge went NA.
"""
import json, os, subprocess, sys, signal

ROOT  = "/Users/mdm/workspace/visSele/InspectionCore"
BUILD = ROOT + "/build/mac-arm64"
CORE  = ROOT + "/Core0_1"
VIS   = BUILD + "/visSele"
TEST  = "/Users/mdm/workspace/HY_sync/DEV/test"
IMG   = TEST + "/10221 BOS-LT12BH4211 SORTING_bk.png"
DEF   = TEST + "/10221 BOS-LT12BH4211 SORTING_bk.hydef"
BASE  = os.path.dirname(os.path.abspath(__file__)) + "/expected/10221.json"

# Tolerance: 0.01 mm on judge values. The golden sample's distance/angle
# measures are O(8-10 mm); 0.01 mm is ~0.1% which is well within the
# "super close" budget the user asked for, and *much* tighter than any
# sample-noise drift (qa_imgstress's heaviest noise was 0.087 mm).
MAX_DRIFT = 0.01

def judges(out_bytes):
    j = json.loads(out_bytes)
    found = {}
    def walk(o):
        if isinstance(o, dict):
            if "id" in o and "status" in o and "value" in o and "subtype" in o:
                found[o["id"]] = (o["status"], o.get("value"))
            for v in o.values(): walk(v)
        elif isinstance(o, list):
            for v in o: walk(v)
    walk(j)
    return found

def run():
    env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD)
    p = subprocess.run([VIS, "--insp", IMG, DEF, "/tmp/_mg.json"],
                       cwd=CORE, env=env, capture_output=True, timeout=120)
    if p.returncode != 0:
        print(f"FAIL: --insp returncode={p.returncode}")
        sys.exit(2)
    return open("/tmp/_mg.json", "rb").read()

def main():
    base = judges(open(BASE, "rb").read())
    cur  = judges(run())
    ids = sorted(set(base) | set(cur))
    print(f"{'id':>4}  {'status':>14}  {'baseline':>12}  {'current':>12}  {'drift':>10}")
    fails = 0
    for jid in ids:
        bs, bv = base.get(jid, (None, None))
        cs, cv = cur.get(jid, (None, None))
        status = f"{bs}->{cs}" if bs is not None else f"   ->{cs}"
        bvs = f"{bv:12.6f}" if isinstance(bv, (int, float)) else f"{str(bv):>12}"
        cvs = f"{cv:12.6f}" if isinstance(cv, (int, float)) else f"{str(cv):>12}"
        drift = ""
        if isinstance(bv, (int, float)) and isinstance(cv, (int, float)):
            d = abs(cv - bv)
            drift = f"{d:10.6f}"
            if d > MAX_DRIFT:
                drift += "  <-- OVER"
                fails += 1
        elif bv is None and cv is None:
            pass
        elif bv is None or cv is None:
            drift = "NA-MISMATCH"
            fails += 1
        print(f"{jid:>4}  {status:>14}  {bvs}  {cvs}  {drift}")
    if fails:
        print(f"\nFAIL: {fails} judge(s) exceeded drift tolerance ({MAX_DRIFT} mm)")
        return 1
    print(f"\nPASS: all judges within {MAX_DRIFT} mm of committed baseline")
    return 0

if __name__ == "__main__":
    sys.exit(main())
