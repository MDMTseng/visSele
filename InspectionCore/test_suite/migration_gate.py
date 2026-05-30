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


# Deep walk of every numeric leaf in the report tree, keyed by a JSON-pointer-
# ish path so drift can be localized.  Skips the documented non-deterministic
# `confidence` field for back-compat with pre-fix baselines (the engine fix
# made it deterministic, but tolerating its absence in older baselines is fine).
_SKIP_KEYS = set()    # could put e.g. "confidence" here if we ever regenerate
                      # the baseline before that field stabilises

def deep_numeric(out_bytes):
    """Returns {path -> number} over every numeric leaf in the report JSON."""
    j = json.loads(out_bytes)
    out = {}
    def walk(o, path):
        if isinstance(o, dict):
            for k, v in o.items():
                if k in _SKIP_KEYS: continue
                walk(v, f"{path}.{k}")
        elif isinstance(o, list):
            for i, v in enumerate(o):
                walk(v, f"{path}[{i}]")
        elif isinstance(o, bool):
            pass  # bool is int in Python; skip
        elif isinstance(o, (int, float)):
            out[path] = o
    walk(j, "$")
    return out

def run():
    env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD)
    p = subprocess.run([VIS, "--insp", IMG, DEF, "/tmp/_mg.json"],
                       cwd=CORE, env=env, capture_output=True, timeout=120)
    if p.returncode != 0:
        print(f"FAIL: --insp returncode={p.returncode}")
        sys.exit(2)
    return open("/tmp/_mg.json", "rb").read()

def main():
    base_bytes = open(BASE, "rb").read()
    cur_bytes  = run()

    # ---- Layer 1: judges only (back-compat with the old "0.000 mm table") ----
    base = judges(base_bytes); cur = judges(cur_bytes)
    ids = sorted(set(base) | set(cur))
    print(f"{'id':>4}  {'status':>14}  {'baseline':>12}  {'current':>12}  {'drift':>10}")
    judge_fails = 0
    for jid in ids:
        bs, bv = base.get(jid, (None, None))
        cs, cv = cur.get(jid, (None, None))
        status = f"{bs}->{cs}" if bs is not None else f"   ->{cs}"
        bvs = f"{bv:12.6f}" if isinstance(bv, (int, float)) else f"{str(bv):>12}"
        cvs = f"{cv:12.6f}" if isinstance(cv, (int, float)) else f"{str(cv):>12}"
        drift = ""
        if isinstance(bv, (int, float)) and isinstance(cv, (int, float)):
            d = abs(cv - bv); drift = f"{d:10.6f}"
            if d > MAX_DRIFT: drift += "  <-- OVER"; judge_fails += 1
        elif bv is None and cv is None: pass
        elif bv is None or cv is None: drift = "NA-MISMATCH"; judge_fails += 1
        print(f"{jid:>4}  {status:>14}  {bvs}  {cvs}  {drift}")
    print(f"\n  judges: {len(ids) - judge_fails}/{len(ids)} within {MAX_DRIFT} mm")

    # ---- Layer 2: every numeric leaf in the report (line/arc/sp/aux + judges) ----
    # Tighter tolerance for low-level geometry (px-domain values like cx, vx,
    # matching_pts, s should match much closer than the 0.01 mm judge budget).
    DEEP_TOL = 1e-4
    base_d = deep_numeric(base_bytes); cur_d = deep_numeric(cur_bytes)
    all_paths = sorted(set(base_d) | set(cur_d))
    drifted = []
    only_base, only_cur = [], []
    for p in all_paths:
        if p not in cur_d:  only_base.append(p); continue
        if p not in base_d: only_cur.append(p); continue
        bv, cv = base_d[p], cur_d[p]
        if isinstance(bv, float) and isinstance(cv, float):
            d = abs(cv - bv)
        else:
            d = 0 if cv == bv else float("inf")
        if d > DEEP_TOL: drifted.append((p, bv, cv, d))
    drifted.sort(key=lambda r: -r[3])
    print(f"\n  deep-walk: {len(all_paths)} numeric leaves checked, "
          f"{len(drifted)} drifted, {len(only_base)} present only in baseline, "
          f"{len(only_cur)} only in current")
    if drifted or only_base or only_cur:
        for p, bv, cv, d in drifted[:20]:
            print(f"    DRIFT {p:<70} base={bv} cur={cv} d={d:.6g}")
        for p in only_base[:5]: print(f"    GONE  {p:<70} base={base_d[p]}")
        for p in only_cur[:5]:  print(f"    NEW   {p:<70} cur={cur_d[p]}")
        if len(drifted) > 20: print(f"    ... +{len(drifted)-20} more drifted paths")

    deep_fails = len(drifted) + len(only_base) + len(only_cur)
    fails = judge_fails + deep_fails
    if fails:
        print(f"\nFAIL: {judge_fails} judge(s) over {MAX_DRIFT} mm, "
              f"{deep_fails} report-tree paths drifted/missing")
        return 1
    print(f"\nPASS: judges within {MAX_DRIFT} mm AND every numeric report "
          f"leaf within {DEEP_TOL} of committed baseline")
    return 0

if __name__ == "__main__":
    sys.exit(main())
