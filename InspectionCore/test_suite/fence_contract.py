#!/usr/bin/env python3
"""Does the measurement fence actually bound where a caliper may pick up an edge?

    python test_suite/fence_contract.py --exe build/win-mingw-msys/visSele.exe

Two properties, and the FIRST one is the one that lets this ship:

  1. A def with NO fence is bit-identical to before the fence existed. The
     fence replaces a background mask that had been dead since 2026-05-29, so
     every def on all 15 machines is a no-fence def. If this ever fails, the
     change stopped being opt-in and became a fleet-wide behaviour change.

  2. A def WITH a fence gates at the SAMPLE level, not at the point level:
     points outside it go NA, points inside it are untouched to the digit, and
     points straddling the boundary keep fewer caliper hits. A mask that only
     rejected whole points would look like it worked here and be wrong.

Plus the thing that is easy to skip: a fenced-out point must SAY the fence did
it. "found nothing" and "was not allowed to look" are different problems and
they look identical on an operator's screen.

Measured on the fixture when this was written (left-half fence over a part
whose search points sit on both sides):
    inside  4 points  bit-identical
    outside 4 points  NA, "no edge inside the measure fence"
    astride 4 points  hits 2835->1525, 1855->924, 18->8, 18->10
"""
import argparse
import copy
import io
import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
FIXTURES = os.path.join(ROOT, os.pardir, "UI", "WebUI", "tools", "webctl", "fixtures")
DEF = os.path.join(FIXTURES, "caliper_verify_tagged.hydef")
IMG = os.path.join(FIXTURES, "caliper_verify_tagged.png")

# Keeps the left half of the part. The fixture's search points sit on both
# sides of x=0, which is why this fixture and not another.
LEFT_HALF = [[{"x": -20, "y": -20}, {"x": 0, "y": -20},
              {"x": 0, "y": 20}, {"x": -20, "y": 20}]]


def load(path):
    return json.load(io.open(path, encoding="utf-8"))


def caliper_mode(defjson):
    """The fixture's search points are contour-mode; the fence lives on the
    caliper path, so a contour-mode def would pass this file vacuously."""
    d = copy.deepcopy(defjson)
    for s in d["featureSet"][0]["features"]:
        if s.get("type") == "search_point":
            s["locating"] = "caliper"
            s["edge"] = {"method": "first", "polarity": "any", "nth": 0,
                         "min_strength": 0, "include_range": 0, "manual_offset": 0}
    return d


def run(exe, defjson, tmp, tag):
    dp = os.path.join(tmp, tag + ".hydef")
    op = os.path.join(tmp, tag + ".json")
    io.open(dp, "w", encoding="utf-8").write(json.dumps(defjson))
    # cwd=ROOT: the core writes crash_reports/ and log files relative to the
    # working directory and a read-only one takes the whole run down.
    # The MSYS2 runtime DLLs are not on a plain Windows PATH, and the failure
    # is silent: CreateProcess returns 0xC0000135 with no output at all, which
    # reads exactly like the core deciding not to inspect.
    env = dict(os.environ)
    for d in (os.path.join("C:", os.sep, "msys64", "mingw64", "bin"),):
        if os.path.isdir(d) and d.lower() not in env.get("PATH", "").lower():
            env["PATH"] = d + os.pathsep + env.get("PATH", "")
    r = subprocess.run([exe, "--insp", IMG, dp, op], cwd=ROOT, env=env,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=300)
    if not os.path.exists(op):
        raise SystemExit("the core produced no report for %s\n%s"
                         % (tag, r.stdout.decode("utf-8", "replace")[-2000:]))
    return load(op)


def points(report):
    r = report["reports"][0]["reports"][0]["searchPoints"]
    return {x["id"]: {"status": x.get("status"),
                      "hits": len((x.get("extra") or {}).get("cal_hits") or []),
                      "reason": x.get("na_reason"),
                      "xy": (x.get("x"), x.get("y"))} for x in r}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    args = ap.parse_args()
    args.exe = os.path.abspath(args.exe)

    fails = []

    def ok(cond, msg, detail=""):
        print(("PASS  " if cond else "FAIL  ") + msg + (("  -- " + detail) if detail else ""))
        if not cond:
            fails.append(msg)

    base = load(DEF)
    with tempfile.TemporaryDirectory() as tmp:
        # 1. the untouched fixture, exactly as it ships
        plain = run(args.exe, base, tmp, "plain")
        ok(plain.get("error") in (None, 0, "", "none") or "reports" in plain,
           "the fixture still inspects at all",
           "a broken fixture would make everything below vacuous")

        cal = caliper_mode(base)
        none = points(run(args.exe, cal, tmp, "nofence"))
        ok(len(none) >= 8, "the caliper-mode fixture reports search points",
           "n=%d" % len(none))
        ok(all(p["status"] == 0 for p in none.values()),
           "and every one of them succeeds without a fence",
           "so a later NA is the fence and not the fixture")

        fenced = copy.deepcopy(cal)
        fenced["featureSet"][0]["measure_fence_include"] = LEFT_HALF
        left = points(run(args.exe, fenced, tmp, "fenced"))

        gone = [i for i in none if left[i]["status"] != 0]
        same = [i for i in none if left[i] == none[i]]
        fewer = [i for i in none
                 if left[i]["status"] == 0 and left[i]["hits"] < none[i]["hits"]]

        ok(gone, "a fence puts the points outside it into NA", "ids %s" % gone)
        ok(same, "and leaves the points inside it bit-identical", "ids %s" % same)
        ok(fewer,
           "and thins the ones that STRADDLE it -- the gate is per sample, "
           "not per point",
           ", ".join("id%s %d->%d" % (i, none[i]["hits"], left[i]["hits"])
                     for i in fewer))
        ok(all("fence" in (left[i]["reason"] or "") for i in gone),
           "every fenced-out point says the FENCE did it",
           "reasons %s" % sorted({left[i]["reason"] for i in gone}))

        # A degenerate fence must mean NO fence, not an empty one. The UI can
        # emit [] (every polygon dropped for being under 3 points), and an
        # empty include list read as "allow nothing" would make every search
        # point NA on a def that looks unfenced on screen.
        empty = copy.deepcopy(cal)
        empty["featureSet"][0]["measure_fence_include"] = []
        empty["featureSet"][0]["measure_fence_exclude"] = [[{"x": 0, "y": 0},
                                                            {"x": 1, "y": 0}]]
        deg = points(run(args.exe, empty, tmp, "degenerate"))
        ok(deg == none,
           "a degenerate fence means NO fence, not a fence around nothing",
           "%d point(s) differ" % sum(deg[i] != none[i] for i in none))

        # 2. an exclude-only fence is a legal fence: the region starts as the
        # whole image and the polygon carves a hole in it.
        holed = copy.deepcopy(cal)
        holed["featureSet"][0]["measure_fence_exclude"] = LEFT_HALF
        hole = points(run(args.exe, holed, tmp, "holed"))
        ok(any(hole[i] != none[i] for i in none),
           "an exclude-only fence is honoured (no include list required)",
           "%d point(s) changed" % sum(hole[i] != none[i] for i in none))
        ok(set(i for i in none if hole[i]["status"] != 0) != set(gone),
           "and it carves the OPPOSITE side from the include above",
           "NA now %s, was %s" % (sorted(i for i in none if hole[i]["status"] != 0),
                                  sorted(gone)))

    print(("\n%d FAILURES" % len(fails)) if fails
          else "\n--- the fence bounds the measurement, and says so ---")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
