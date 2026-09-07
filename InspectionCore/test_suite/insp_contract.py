#!/usr/bin/env python3
"""What an inspection produces, end to end, asserted against the real core.

    python test_suite/insp_contract.py --exe build/win-mingw-msys/visSele.exe

WHY THIS EXISTS
---------------
Everything established on 2026-08-26 -- that the shape cache does not change
the answer, that localization holds over +-30 deg, that ROI refine is worth
~15x, that a caliper measures to a few microns -- was measured BY HAND, once,
and written into a document. A number in a document does not fail when
somebody edits the core. This does.

It drives the shipped binary through --insp, the only way to exercise the whole
chain from a shell: def parse -> feature cache -> localization -> caliper
search -> judgement.

TWO HALVES, and the split is not tidiness
-----------------------------------------
The CALIPER half runs against a committed fixture and always runs.

The SBM half needs a shape_based def with a trained cache and its 7 MB
reference image, and that data is machine-local and gitignored. It SKIPS,
loudly, rather than being quietly absent -- a suite that silently covers less
than it claims is worse than one that covers less and says so.

WHAT IT DOES NOT DO
-------------------
It asserts INVARIANTS, not a golden dump. A golden report has to be
regenerated for every legitimate change, and a file regenerated on every
change has stopped being evidence of anything.
"""
import argparse, copy, io, json, math, os, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
FIX = os.path.join(ROOT, os.pardir, "UI", "WebUI", "tools", "webctl", "fixtures")
CAL_DEF = os.path.join(FIX, "caliper_verify_tagged.hydef")
CAL_IMG = os.path.join(FIX, "caliper_verify_tagged.png")
SBM_DEF = os.path.join(ROOT, "Core0_1", "data", "test1.hydef")
SBM_IMG = os.path.join(ROOT, "Core0_1", "data", "test1.png")

fails, skips = [], []


def ok(cond, msg, detail=""):
    print(("PASS  " if cond else "FAIL  ") + msg + (("  -- " + detail) if detail else ""))
    if not cond:
        fails.append(msg)


def skip(msg, why):
    print("SKIP  " + msg + "  -- " + why)
    skips.append(msg)


def load(p):
    return json.load(io.open(p, encoding="utf-8"))


def run(exe, defjson, img, out, perturb=None, env_extra=None):
    """One --insp.

    The def is written BESIDE the image, because the template sidecar resolves
    as <defbase>.png -- a def in a temp directory silently fails to train, and
    two failures then agree with each other. That cost an hour once already.
    """
    d = os.path.dirname(img)
    dp = os.path.join(d, "_ic_tmp.hydef")
    ip = os.path.join(d, "_ic_tmp.png")
    io.open(dp, "w", encoding="utf-8").write(json.dumps(defjson))
    made_img = not os.path.exists(ip)
    if made_img:
        io.open(ip, "wb").write(io.open(img, "rb").read())
    env = dict(os.environ)
    # The MSYS2 runtime DLLs are not on a plain Windows PATH, and the failure
    # is silent: CreateProcess returns 0xC0000135 with no output at all.
    cand = os.path.join("C:", os.sep, "msys64", "mingw64", "bin")
    if os.path.isdir(cand) and cand.lower() not in env.get("PATH", "").lower():
        env["PATH"] = cand + os.pathsep + env.get("PATH", "")
    env.update(env_extra or {})
    args = [exe, "--insp", img, dp, out]
    if perturb:
        args.append(json.dumps(perturb))
    try:
        subprocess.run(args, cwd=ROOT, env=env, timeout=300,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    finally:
        for f in (dp, ip):
            try:
                os.remove(f)
            except OSError:
                pass
    return load(out) if os.path.exists(out) else None


def envelope(rep):
    r = (rep or {}).get("reports") or [{}]
    return r[0]


def objects(rep):
    return envelope(rep).get("reports") or []


def obj_frame_residuals(obj, defjson):
    """Each reported point, put back into the OBJECT frame with the run's OWN
    pose, against the def pt1 it belongs to.

    Deliberately NOT "un-rotate by the perturbation I applied". That version
    was wrong and betrayed itself by giving identical answers for runs whose
    reported angles differed by 2 deg -- a quantity that ignores a 2 deg
    difference in its own input is not measuring what it claims to.
    """
    want = {}
    for s in defjson["featureSet"][0]["features"]:
        if s.get("type") == "search_point" and s.get("pt1"):
            want[s["id"]] = (s["pt1"]["x"], s["pt1"]["y"])
    rot = obj.get("rotate", 0.0)
    ca, sa = math.cos(rot), math.sin(rot)
    fl = -1 if obj.get("isFlipped") else 1
    out = {}
    for x in obj.get("searchPoints", []):
        if x.get("status") != 0 or x["id"] not in want:
            continue
        dx, dy = x["x"] - obj["cx"], x["y"] - obj["cy"]
        ox, oy = dx * ca - dy * sa, (dx * sa + dy * ca) * fl
        gx, gy = want[x["id"]]
        out[x["id"]] = math.hypot(ox - gx, oy - gy)
    return out


def caliper_half(exe, tmp):
    print("=== the caliper chain (committed fixture) ===")
    cal = copy.deepcopy(load(CAL_DEF))
    # The fixture ships contour-mode. Forcing caliper is what puts the search
    # band, the edge gate and the sub-pixel fit under test at all.
    for s in cal["featureSet"][0]["features"]:
        if s.get("type") == "search_point":
            s["locating"] = "caliper"
            s["edge"] = {"method": "first", "polarity": "any", "nth": 0,
                         "min_strength": 0, "include_range": 0, "manual_offset": 0}
    rep = run(exe, cal, CAL_IMG, os.path.join(tmp, "cal.json"))
    ok(rep is not None, "the core produced a report at all")
    objs = objects(rep)
    ok(len(objs) >= 1, "the fixture locates", "objects=%d" % len(objs))
    if not objs:
        return
    sp = objs[0].get("searchPoints", [])
    good = [x for x in sp if x.get("status") == 0]
    ok(len(sp) >= 8, "every search point is reported", "%d points" % len(sp))
    ok(len(good) == len(sp), "and every one of them measured",
       "%d/%d" % (len(good), len(sp)))
    for x in sp:
        if x.get("status") != 0:
            print("        id %s status %s reason %r"
                  % (x["id"], x["status"], x.get("na_reason")))
    # A caliper reporting no hits is a caliper that did not run, and it would
    # still show status 0 with the point sitting exactly on pt1.
    hits = [len((x.get("extra") or {}).get("cal_hits") or []) for x in good]
    ok(bool(hits) and min(hits) > 0, "each measured point carries caliper hits",
       ("min %d max %d" % (min(hits), max(hits))) if hits else "none")

    # REQUIREDNESS. min_strength decides which edges exist at all, so a def
    # that omits it must go NA naming the knob rather than have one guessed.
    # This is the rule that made the 1.1.104 behaviour change safe.
    noreq = copy.deepcopy(cal)
    for s in noreq["featureSet"][0]["features"]:
        if s.get("type") == "search_point" and "edge" in s:
            s["edge"].pop("min_strength", None)
    rep2 = run(exe, noreq, CAL_IMG, os.path.join(tmp, "cal_noreq.json"))
    o2 = (objects(rep2) or [{}])[0]
    na = [x for x in o2.get("searchPoints", []) if x.get("status") != 0]
    ok(bool(na) and all("min_strength" in (x.get("na_reason") or "") for x in na),
       "a missing edge.min_strength is NA and names the knob",
       (na[0].get("na_reason") if na else "nothing went NA -- it was guessed"))


def sbm_half(exe, tmp):
    print("")
    print("=== the SBM chain (machine-local data) ===")
    if not (os.path.exists(SBM_DEF) and os.path.exists(SBM_IMG)):
        skip("the whole SBM half", "no Core0_1/data/test1.hydef + .png here")
        return
    d = load(SBM_DEF)
    if d["featureSet"][0].get("locating_engine") != "shape_based":
        skip("the whole SBM half", "the local def is not shape_based")
        return

    # INSP_AREA_BYPASS: --insp enforces the production station and the part in
    # a TRAINING image is not at it. Correct behaviour, not a fault -- an
    # earlier note called --insp broken for SBM defs over exactly this.
    E = {"INSP_AREA_BYPASS": "1"}
    EX = dict(E)
    EX["SBM_ALLOW_IMPLICIT_EXTRACT"] = "1"

    if not d["featureSet"][0].get("__shape_cache"):
        skip("cache vs fresh extraction", "the local def carries no __shape_cache")
    else:
        nc = copy.deepcopy(d)
        nc["featureSet"][0].pop("__shape_cache", None)
        a = run(exe, d, SBM_IMG, os.path.join(tmp, "s_cache.json"), env_extra=E)
        b = run(exe, nc, SBM_IMG, os.path.join(tmp, "s_fresh.json"), env_extra=EX)
        # DID RUN `a` ACTUALLY USE THE CACHE? If the def's cache is stale the
        # core refuses it and sig360 covers, so this would compare sig360
        # against SBM and call the difference a cache defect.
        #
        # That is not hypothetical: the first hand-run of this comparison
        # reported "0 leaves differ" and was meaningless, because BOTH sides had
        # been forced to extract. A comparison has to prove it compared the two
        # things it names.
        code = (envelope(a).get("locate") or {}).get("code")
        if code in ("untrained", "train_failed"):
            skip("cache vs fresh extraction",
                 "the local def's cache is stale (locate.code=%s), so the cached "
                 "run fell back to sig360 -- regenerate in the studio first" % code)
            return_early = True
        else:
            return_early = False
        la, lb = {}, {}
        leaves((a or {}).get("reports"), la)
        leaves((b or {}).get("reports"), lb)
        diff = sorted(k for k in set(la) | set(lb) if la.get(k) != lb.get(k))
        if return_early:
            pass
        else:
            ok(bool(la) and not diff,
               "a cached feature set and a fresh extraction give IDENTICAL measurements",
               "%d leaves, %d differ%s" % (len(la), len(diff),
                                           "" if not diff else " e.g. " + diff[0]))

    # Localization across rotation, and whether the MEASUREMENTS drift with it.
    nc = copy.deepcopy(d)
    nc["featureSet"][0].pop("__shape_cache", None)
    worst_res, worst_ang, lost, base_rot = 0.0, 0.0, [], None
    for deg in (0, 5, -8, 15, 25):
        rep = run(exe, nc, SBM_IMG, os.path.join(tmp, "r%d.json" % deg),
                  perturb={"rot_deg": deg, "seed": 1}, env_extra=EX)
        objs = objects(rep)
        if not objs:
            lost.append(deg)
            continue
        o = objs[0]
        r = obj_frame_residuals(o, d)
        if r:
            worst_res = max(worst_res, max(r.values()))
        rot = math.degrees(o["rotate"])
        if base_rot is None:
            base_rot = rot
        else:
            worst_ang = max(worst_ang, abs(rot - base_rot - deg))
    ok(not lost, "it locates at every rotation tested (0, +5, -8, +15, +25)",
       ("lost at %s" % lost) if lost else "5/5")
    # 0.5 deg: the line2Dup grid is 1.0 deg and ROI refine leaves about +-0.2 of
    # it. A regression that dropped refine would land at +-2 and trip this.
    ok(worst_ang < 0.5, "and the reported angle tracks the applied one",
       "worst residual %.4f deg" % worst_ang)
    # 50 um. Measured 2-8 um mean, 24 um worst on 2026-08-26 -- a REGRESSION
    # bound, not a spec: set where a real break shows and normal variation
    # does not.
    ok(worst_res < 0.050, "and the measured points do not drift with rotation",
       "worst object-frame residual %.1f um" % (worst_res * 1000))


def leaves(node, out, path=""):
    """Every scalar in the report tree, under a stable path.

    __shape_cache and locate are excluded: one is the thing under test and the
    other legitimately differs between a cache hit and an extraction."""
    if isinstance(node, dict):
        for k in sorted(node):
            if k in ("__shape_cache", "locate"):
                continue
            leaves(node[k], out, path + "." + k)
    elif isinstance(node, list):
        for i, v in enumerate(node):
            leaves(v, out, path + "[%d]" % i)
    else:
        out[path] = node


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    args = ap.parse_args()
    exe = os.path.abspath(args.exe)
    if not os.path.exists(exe):
        raise SystemExit("no exe at " + exe)
    with tempfile.TemporaryDirectory() as tmp:
        caliper_half(exe, tmp)
        sbm_half(exe, tmp)
    print("")
    if skips:
        print("%d skipped: %s" % (len(skips), "; ".join(skips)))
    if fails:
        print("%d FAILURES" % len(fails))
        return 1
    print("--- the inspection chain behaves as specified ---")
    return 0


if __name__ == "__main__":
    sys.exit(main())
