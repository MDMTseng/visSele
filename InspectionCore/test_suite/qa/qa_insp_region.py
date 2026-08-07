#!/usr/bin/env python3
"""Prove the inspection region: it selects, it does not disturb.

Two claims, and they pull in opposite directions, which is why both are needed:

  A. NO REGION == BEFORE. With no inspection_region configured, the report must
     be byte-identical to the binary that had no such feature. A filter that
     changes anything when it is switched off is not a filter.

  B. THE REGION ACTUALLY SELECTS. A region containing the object keeps it and
     the result stays byte-identical to (A) -- filtering is not allowed to
     perturb the measurement of what it keeps. A region NOT containing the
     object drops it, and the report comes back with no located object at all.

The region is machine-level, so it is injected the way the real thing is: a
data/machine_setting.json in an isolated cwd. That exercises the real load path
(load_insp_region), not a test hook. The cwd is a scratch dir with data/ symlinked
to the real one, so the live machine's own settings are never touched.
"""
import json, os, shutil, subprocess, sys

WT    = "/Users/mdm/workspace/visSele/.claude/worktrees/uinsp-mini-compact/InspectionCore"
BUILD = WT + "/build/mac-arm64"
VIS   = BUILD + "/visSele"
REAL_CORE = WT + "/Core0_1"
OLD_VIS = "/Users/mdm/workspace/visSele/InspectionCore/build/mac-arm64/visSele"   # pre-region
TEST  = "/Users/mdm/workspace/HY_sync/DEV/test"
IMG   = TEST + "/10221 BOS-LT12BH4211 SORTING_bk.png"
GDEF  = TEST + "/10221 BOS-LT12BH4211 SORTING_bk.hydef"
TMP   = "/Users/mdm/.claude/jobs/823502fe/tmp/region"

fails = []
def check(name, cond, detail=""):
    print("  %-46s %s%s" % (name, "PASS" if cond else "FAIL", "  " + detail if detail else ""))
    if not cond: fails.append(name)


def sandbox(region):
    """A cwd with its own data/machine_setting.json and everything else symlinked."""
    shutil.rmtree(TMP, ignore_errors=True)
    os.makedirs(TMP + "/data")
    for e in os.listdir(REAL_CORE + "/data"):
        if e == "machine_setting.json":
            continue
        try: os.symlink(REAL_CORE + "/data/" + e, TMP + "/data/" + e)
        except OSError: pass
    ms = {}
    real_ms = REAL_CORE + "/data/machine_setting.json"
    if os.path.exists(real_ms):
        ms = json.load(open(real_ms))
    ms.pop("inspection_region", None)
    if region is not None:
        ms["inspection_region"] = region
    json.dump(ms, open(TMP + "/data/machine_setting.json", "w"), indent=1)
    return TMP


def run(vis, region, tag):
    cwd = sandbox(region)
    out = "%s/%s.json" % (TMP, tag)
    env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD)
    p = subprocess.run([vis, "--insp", IMG, GDEF, out], cwd=cwd, env=env,
                       capture_output=True, timeout=180)
    body = open(out, "rb").read() if os.path.exists(out) else None
    return body, (p.stderr or b"").decode(errors="replace")


def located(body):
    """How many objects the report contains, and the first one's centre."""
    if body is None: return None, None
    d = json.loads(body)
    def find(o, k):
        if isinstance(o, dict):
            if k in o: return o[k]
            for v in o.values():
                r = find(v, k)
                if r is not None: return r
        elif isinstance(o, list):
            for v in o:
                r = find(v, k)
                if r is not None: return r
        return None
    inner = find(d, "reports")
    while isinstance(inner, list) and inner and isinstance(inner[0], dict) and "reports" in inner[0]:
        inner = inner[0]["reports"]
    n = len(inner) if isinstance(inner, list) else 0
    c = None
    if n:
        c = (inner[0].get("Center") or inner[0].get("center") or
             {"x": inner[0].get("x"), "y": inner[0].get("y")})
    return n, c


print("A. with no region, the new binary must match the old one exactly")
old_body, _ = run(OLD_VIS, None, "old_noregion")
new_body, err = run(VIS, None, "new_noregion")
check("old binary produced a report", old_body is not None, err[-200:] if old_body is None else "")
check("new binary produced a report", new_body is not None, err[-200:] if new_body is None else "")
check("byte-identical with no region configured", old_body == new_body,
      "" if old_body == new_body else "old %s B / new %s B" % (
          len(old_body or b""), len(new_body or b"")))

n, c = located(new_body)
print("\n   located objects: %s  first centre: %s" % (n, c))
if not n:
    print("   cannot continue: the golden def located nothing to aim a region at")
    sys.exit(1)

print("\nB. the region selects, and does not perturb what it keeps")
# Full-frame region covering everything -> must keep the object AND be identical.
big, _ = run(VIS, {"x": 0, "y": 0, "w": 10000, "h": 10000}, "region_all")
check("region covering the whole frame -> identical to no region", big == new_body,
      "" if big == new_body else "%s B vs %s B" % (len(big or b""), len(new_body or b"")))

# A region far away from anything -> the object must be dropped.
far, _ = run(VIS, {"x": 9000, "y": 9000, "w": 100, "h": 100}, "region_far")
nf, _ = located(far)
check("region far from the object -> nothing located", nf == 0, "located=%s" % nf)
check("region far from the object -> report still valid JSON", far is not None)
check("dropping the object changed the output", far != new_body)

# A 1-px region: still off the object, same as far away.
tiny, _ = run(VIS, {"x": 0, "y": 0, "w": 1, "h": 1}, "region_corner")
nt, _ = located(tiny)
check("1px region at the origin -> nothing located", nt == 0, "located=%s" % nt)

# w/h of 0 must read as "not configured", not as "a region of zero size that
# rejects everything" -- an off switch that silently rejects is a trap.
zero, _ = run(VIS, {"x": 0, "y": 0, "w": 0, "h": 0}, "region_zero")
check("w=h=0 means OFF, not 'reject everything'", zero == new_body,
      "" if zero == new_body else "differs -> zero-size region is rejecting")

# --- C. the decisive case: a SMALL region placed ON the object ---------------
#
# A whole-frame region proves the filter does not break things; it does not
# prove the filter can aim. The object's centre is at ~(1355, 856) full-frame px
# (cx/cy 12.00/7.58 mm at mmpp 0.008858). A 300px box there must keep it; the
# same box shifted a plate-width away must not.
print("\nC. a small region aimed at the object")
OX, OY = 1355, 856
on,  _ = run(VIS, {"x": OX - 150, "y": OY - 150, "w": 300, "h": 300, "fit": "center"}, "region_on")
off, _ = run(VIS, {"x": OX + 600, "y": OY - 150, "w": 300, "h": 300, "fit": "center"}, "region_off")
n_on,  _ = located(on)
n_off, _ = located(off)
check("centre mode: 300px box on the object -> kept", n_on == 1, "located=%s" % n_on)
check("kept object is measured identically",     on == new_body,
      "" if on == new_body else "%s B vs %s B" % (len(on or b""), len(new_body or b"")))
check("centre mode: same box 600px away -> dropped",  n_off == 0, "located=%s" % n_off)


# --- D. fit: contain vs centre ----------------------------------------------
#
# The point of contain-mode is that the natural gesture works: a box drawn
# comfortably around a part keeps it, while a box that merely covers the part's
# CENTRE does not. Centre-mode behaves the other way round, which is why
# selecting one part used to mean drawing a box smaller than the part.
print("\nD. fit mode")
# The object's real bounding box, read from the core's own diagnostic:
#   label 3 area 84328 raw(1355,856) bbox[852,203 982x1143]
# It is 982x1143 px -- far bigger than the 300px "aim" box in section C, which
# is exactly why contain-mode and centre-mode disagree so visibly here.
tiny_on_centre = {"x": OX - 40, "y": OY - 40, "w": 80, "h": 80}
roomy          = {"x": 800, "y": 150, "w": 1100, "h": 1250}   # holds bbox with margin

r1, _ = run(VIS, dict(tiny_on_centre, fit="center"),  "fit_c_tiny")
r2, _ = run(VIS, dict(tiny_on_centre, fit="contain"), "fit_o_tiny")
r3, _ = run(VIS, dict(roomy,          fit="contain"), "fit_o_roomy")
r4, _ = run(VIS, tiny_on_centre,                      "fit_default_tiny")
n1, n2, n3, n4 = (located(x)[0] for x in (r1, r2, r3, r4))
check("centre mode: 80px box on the centre -> kept", n1 == 1, "located=%s" % n1)
check("contain mode: same 80px box -> dropped",      n2 == 0, "located=%s" % n2)
check("contain mode: roomy box -> kept",             n3 == 1, "located=%s" % n3)
check("contain mode does not perturb what it keeps", r3 == new_body,
      "" if r3 == new_body else "%s B vs %s B" % (len(r3 or b""), len(new_body or b"")))
check("default fit is contain, not centre",          n4 == 0, "located=%s" % n4)

print("\n=> %s" % ("ALL PASS" if not fails else "%d FAILED: %s" % (len(fails), fails)))
sys.exit(1 if fails else 0)
