#!/usr/bin/env python3
"""Old def files must keep measuring what they measured.

The measurement path is edited often -- caliper internals, the search-point
scanner, the report shape -- and every one of those edits reaches defs written
years earlier. Nothing checked them. On 2026-08-27 two real defects were fixed
inside the caliper (a fabricated edge on the frame boundary, and a truncated
scan window reporting SUCCESS), and the only way to know whether that moved a
number on an OLD def was to run one and look.

This runs a corpus of def+image pairs through the shipped binary's --insp and
compares every judge value against a committed baseline. It answers one
question: did this build change what an old recipe measures?

    python3 def_compat.py --corpus <dir>              # check against baseline
    python3 def_compat.py --corpus <dir> --bless      # record a new baseline

To measure a MIGRATION rather than a build, convert a corpus first and compare
it against the pre-conversion baseline:

    node UI/WebUI/tools/def_convert.mjs --in <corpus> --out <converted>
    python3 def_compat.py --corpus <converted> --ignore-def-sha1

The converter lives in the WebUI because rewriting a recipe is the editor's job,
and it imports the editor's own seeding rule -- so what it produces is what a
person switching `locating` to 'caliper' by hand would get.

The corpus is a directory of <name>.hydef next to <name>.png. It is NOT in the
repo: these are production recipes with customer part numbers, and the images
are a megabyte each. The BASELINE is in the repo (small, and it is the thing
with the information in it), and it records each def's sha1 so a corpus that
has drifted from the one the baseline was taken on is reported rather than
silently compared.

WHY --insp AND NOT THE LIVE PIPELINE: --insp is one frame through the whole
engine with no camera, no board and no UI, so a difference it reports is a
difference in the measurement path and nothing else. That is the entire point.

WHAT MAKES A BASELINE PORTABLE, and what it therefore cannot see:

  the SCALE comes from the DEF, not the bench. --insp calls
  apply_def_cam_param, so each def is measured with its own embedded cam_param.
  Verified rather than assumed: the six corpus defs report mmpp 0.0100631 and
  0.0088416 -- each matching its OWN def -- while this bench's own recipe uses
  0.0138859, which appears in no report. A baseline recorded on one machine
  therefore reproduces on another to the digit.

  the STATION REGION does not, so INSP_AREA_BYPASS=1 is set for every run. It
  lives in machine_setting.json and belongs to the MACHINE; without the bypass a
  corpus recorded on one bench fails on another because the part centre falls
  outside a box that has nothing to do with the recipe.

  the COST of both is that this harness cannot see a calibration problem. It
  compares def + image + engine, and the bench's own calibration is isolated
  out. That is the point -- a difference it reports is a difference in the
  measurement path -- but it means a green run says nothing about whether THIS
  machine is calibrated.

EXPECTED FAILURES ARE PART OF THE BASELINE. One def in the original corpus
(93013 5G2545060B) locates nothing, because three defs in that family share one
signature: they were copied and the signature was never regenerated, so two of
them match their photo and the third cannot. That is a data defect, not a build
defect, and the baseline records it as the current truth. If a later build
suddenly locates it, that shows up as drift and somebody gets to find out why.
"""

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile

# Judge values are floats out of a deterministic pipeline: the same build on the
# same image reproduces them exactly. The tolerance is here for the compiler,
# not for the measurement -- a different optimiser or libm can move the last
# bits. Anything a person would care about is orders of magnitude above it.
DEFAULT_TOL = 1e-6


def find_exe(explicit):
    if explicit:
        return explicit
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(os.path.dirname(here))
    for rel in (
        # The deployed trees first: that is the binary a machine actually runs,
        # and checking the one in build/ can pass while the shipped one differs.
        "export_v2/app/1.1.105/Core/visSele.exe",
        "export_v2/1.1.104/Core/visSele.exe",
        "InspectionCore/build/win-mingw-msys/visSele.exe",
        "InspectionCore/build/win-mingw-msys/visSele",
    ):
        p = os.path.join(repo, rel)
        if os.path.exists(p):
            return p
    return None


def sha1_of(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def run_one(exe, workdir, hydef, png, out_json):
    env = dict(os.environ)
    env["INSP_AREA_BYPASS"] = "1"
    # The camera SDK's DLLs sit next to the executable, so the process has to
    # run there; chdir= is what tells the core which machine's data/ to use.
    # They are two different things and both are needed -- see docs/SYSTEM_OVERVIEW.md.
    cwd = os.path.dirname(exe)
    cmd = [exe, "chdir=" + workdir, "--insp", png, hydef, out_json]
    p = subprocess.run(cmd, cwd=cwd, env=env,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return p.returncode, p.stdout.decode("utf-8", "replace")


def extract(out_json):
    """{judge_id: [name, value]} plus a located flag. Nothing else.

    Deliberately narrow. The full report carries per-hit debug arrays that move
    with any drawing or diagnostic change, and a baseline that fails on those
    would be turned off within a week. Judge values are what the machine sorts
    on."""
    with open(out_json, encoding="utf-8") as f:
        d = json.load(f)
    reps = d.get("reports") or []
    inner = (reps[0].get("reports") if reps else None) or []
    if not inner:
        return {"located": False, "judges": {}}
    js = inner[0].get("judgeReports") or []
    out = {}
    for j in js:
        v = j.get("value")
        if isinstance(v, (int, float)):
            out[str(j.get("id"))] = [j.get("name"), v, j.get("status")]
    return {"located": True, "judges": out}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--corpus", required=True, help="dir of <name>.hydef + <name>.png")
    ap.add_argument("--baseline", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "def_compat_baseline.json"))
    ap.add_argument("--exe", default=None)
    ap.add_argument("--workdir", default=None,
                    help="the core's working dir (parent of data/)")
    ap.add_argument("--bless", action="store_true", help="record a new baseline")
    ap.add_argument("--tol", type=float, default=DEFAULT_TOL)
    # The sha1 guard exists for a corpus that drifted by ACCIDENT. Comparing
    # across a deliberate def edit -- a contour->caliper conversion, say -- is
    # exactly what you want to measure, so it gets an explicit opt-out rather
    # than a weaker guard.
    ap.add_argument("--ignore-def-sha1", action="store_true",
                    help="compare even though the def files changed (for measuring "
                         "an intentional edit against a pre-edit baseline)")
    a = ap.parse_args()

    exe = find_exe(a.exe)
    if not exe:
        print("no visSele binary found; pass --exe", file=sys.stderr)
        return 2
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(os.path.dirname(here))
    workdir = a.workdir or os.path.join(repo, "InspectionCore", "Core0_1")

    defs = sorted(f for f in os.listdir(a.corpus) if f.endswith(".hydef"))
    if not defs:
        print("no .hydef in " + a.corpus, file=sys.stderr)
        return 2

    print("exe     : " + exe)
    print("workdir : " + workdir)
    print("corpus  : %s (%d defs)" % (a.corpus, len(defs)))
    print("")

    cur = {}
    tmp = tempfile.mkdtemp(prefix="defcompat_")
    for name in defs:
        base = name[:-6]
        hydef = os.path.join(a.corpus, name)
        png = os.path.join(a.corpus, base + ".png")
        if not os.path.exists(png):
            print("  SKIP %-34s (no .png beside it)" % base[:34])
            continue
        out_json = os.path.join(tmp, base.replace(" ", "_") + ".json")
        rc, log = run_one(exe, workdir, hydef, png, out_json)
        if rc != 0 or not os.path.exists(out_json):
            cur[base] = {"error": "insp rc=%d" % rc, "def_sha1": sha1_of(hydef)}
            print("  FAIL %-34s --insp rc=%d" % (base[:34], rc))
            continue
        e = extract(out_json)
        e["def_sha1"] = sha1_of(hydef)
        cur[base] = e

    if a.bless:
        with open(a.baseline, "w", encoding="utf-8") as f:
            json.dump(cur, f, ensure_ascii=False, indent=1, sort_keys=True)
        n = sum(len(v.get("judges", {})) for v in cur.values())
        print("baseline written: %s (%d defs, %d judge values)" % (a.baseline, len(cur), n))
        return 0

    if not os.path.exists(a.baseline):
        print("no baseline at %s -- run with --bless first" % a.baseline, file=sys.stderr)
        return 2
    with open(a.baseline, encoding="utf-8") as f:
        old = json.load(f)

    bad = 0
    checked = 0
    for base in sorted(set(list(old.keys()) + list(cur.keys()))):
        o, c = old.get(base), cur.get(base)
        if o is None:
            print("  NEW  %-34s not in baseline (bless to accept)" % base[:34]); continue
        if c is None:
            print("  GONE %-34s in baseline, not in corpus" % base[:34]); bad += 1; continue
        # A def that changed on disk cannot be compared: say so instead of
        # reporting every one of its numbers as drift.
        if (not a.ignore_def_sha1) and o.get("def_sha1") and c.get("def_sha1")                 and o["def_sha1"] != c["def_sha1"]:
            print("  ---- %-34s DEF FILE CHANGED since baseline; not compared" % base[:34])
            continue
        if o.get("located") != c.get("located"):
            print("  DIFF %-34s located %s -> %s" % (base[:34], o.get("located"), c.get("located")))
            bad += 1
            continue
        for jid, ov in sorted((o.get("judges") or {}).items()):
            cv = (c.get("judges") or {}).get(jid)
            checked += 1
            if cv is None:
                print("  DIFF %-34s judge %s (%s) DISAPPEARED" % (base[:34], jid, ov[0]))
                bad += 1
                continue
            d = cv[1] - ov[1]
            if abs(d) > a.tol:
                print("  DIFF %-34s %-24s %.6f -> %.6f  (%+.6f)"
                      % (base[:34], str(ov[0])[:24], ov[1], cv[1], d))
                bad += 1
            elif len(ov) > 2 and len(cv) > 2 and ov[2] != cv[2]:
                print("  DIFF %-34s %-24s status %s -> %s"
                      % (base[:34], str(ov[0])[:24], ov[2], cv[2]))
                bad += 1
        for jid, cv in sorted((c.get("judges") or {}).items()):
            if jid not in (o.get("judges") or {}):
                print("  DIFF %-34s judge %s (%s) APPEARED" % (base[:34], jid, cv[0]))
                bad += 1

    print("")
    print("%s -- %d judge values checked, %d difference%s"
          % ("FAIL" if bad else "ALL MATCH", checked, bad, "" if bad == 1 else "s"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
