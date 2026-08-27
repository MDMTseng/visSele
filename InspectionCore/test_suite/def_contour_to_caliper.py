#!/usr/bin/env python3
"""Convert a def's contour-mode line/arc features to caliper mode.

WHY THIS EXISTS. Contour-mode features (`locating` absent or 0 -- the default,
so every def written before caliper mode) are measured by walking the
labeled-contour grid. The shape locator never builds that grid: FeatureMatching
returns at `if (locating_engine == 1 && shape_ready) return
FeatureMatching_shape();` long before it is filled, and edge_grid is a class
member so the call still runs, against an empty grid. Those features then
produce nothing, silently. So a def cannot be moved to SBM until its contour
features are moved to caliper.

WHAT IS DERIVED AND WHAT IS NOT. Almost nothing has to be invented, which was a
surprise -- the first reading of this said min_strength had no source and
someone would have to pick it. That was wrong, and the code says so:

  cal_length      LEFT UNSET. Its sentinel already falls back to
                  initMatchingMargin, which IS the contour search depth. The
                  old defs carry 0.2-0.5mm there and it keeps working untouched.
  cal_step        LEFT UNSET -> 1px.
  cal_min_inliers LEFT UNSET -> engine default (2 for a line, 3 for an arc).
  cal_max_error   LEFT UNSET -> no cap.
  edge.min_strength  0, and that is a TRANSLATION, not a guess.
                  contourGridGrayLevelRefine computes a Sobel at every contour
                  point and then sets `edgeRsp = 1` unconditionally -- the
                  contour path has no gradient floor at all, it takes every
                  point on the 128 threshold crossing. "No floor" is 0. It is
                  also the parser's own default for a caliper line.
  edge.polarity   falling -- the parser's default, and the right one for a
                  backlit dark-object-on-bright silhouette.
  edge.method     strongest -- the parser's default.

  cal_count       DERIVED from the feature's length. The only real decision.
  cal_width       length / count, so the calipers tile the feature exactly.

WHAT THIS DOES NOT PROMISE. Caliper and contour do not measure identically even
with matched parameters: the caliper's edge search carries a relative peak gate
(peakFrac = 0.40 of the strongest peak in the profile) that contour has no
equivalent of. How much that moves a number is a question for measurement, not
argument -- convert a corpus with this, then run def_compat.py against the
pre-conversion baseline and read the differences.

    python3 def_contour_to_caliper.py --in <dir> --out <dir> [--spacing 0.1]
    python3 def_contour_to_caliper.py --in <dir> --out <dir> --engine shape_based

--engine also flips locating_engine, for producing the def you would actually
run on SBM. Left alone by default so the conversion can be measured on its own,
without the localizer change confounding it.

Originals are never modified.
"""

import argparse
import json
import math
import os
import shutil
import sys

# Calipers per millimetre of feature length, and the floor.
#
# 0.1mm on this corpus gives 3 calipers on the shortest feature (0.296mm) and 32
# on the longest (3.153mm). The floor matters more than the spacing: a line fit
# needs cal_min_inliers (2 by default) and an arc needs 3, so a feature that
# produces only two or three usable calipers has no margin for a single bad one.
DEFAULT_SPACING_MM = 0.1
MIN_COUNT_LINE = 4
MIN_COUNT_ARC = 5


def dist(a, b):
    return math.hypot(a["x"] - b["x"], a["y"] - b["y"])


def feature_length_mm(f):
    """Length along the feature, in def units (mm).

    A line is its two endpoints. An arc is given as three points, so the
    two-chord path pt1->pt2->pt3 is used: it understates a wide sweep, which
    errs toward MORE calipers rather than fewer, and the count is what it feeds.
    """
    t = f.get("type")
    try:
        if t == "line":
            return dist(f["pt1"], f["pt2"])
        if t == "arc":
            return dist(f["pt1"], f["pt2"]) + dist(f["pt2"], f["pt3"])
    except (KeyError, TypeError):
        return None
    return None


def convert_feature(f, spacing, min_strength=0, polarity=None, nth=None):
    """Returns (changed, note). Mutates f."""
    t = f.get("type")
    if t not in ("line", "arc"):
        return False, None
    # "caliper" is what the UI writes; the core reads the numeric locating.
    # Either spelling already in caliper mode is left alone.
    loc = f.get("locating")
    if loc == "caliper" or loc == 1:
        return False, None

    L = feature_length_mm(f)
    if not L or L <= 0:
        return False, "no usable geometry -- left in contour mode"

    floor = MIN_COUNT_ARC if t == "arc" else MIN_COUNT_LINE
    count = max(floor, int(round(L / spacing)))
    width = L / count

    f["locating"] = "caliper"
    f["cal_count"] = count
    f["cal_width"] = round(width, 6)
    # cal_length / cal_step / cal_min_inliers / cal_max_error are deliberately
    # NOT written -- their sentinels are the behaviour we want, and writing a
    # number here would freeze today's engine defaults into the file.
    edge = f.get("edge")
    if not isinstance(edge, dict):
        edge = {}
    edge.setdefault("method", "strongest")
    edge.setdefault("polarity", polarity or "falling")
    edge.setdefault("nth", 0 if nth is None else nth)
    edge.setdefault("min_strength", min_strength)
    f["edge"] = edge
    return True, "%s len=%.3fmm -> count=%d width=%.4fmm" % (t, L, count, width)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="src", required=True)
    ap.add_argument("--out", dest="dst", required=True)
    ap.add_argument("--spacing", type=float, default=DEFAULT_SPACING_MM,
                    help="mm between calipers (default %.2f)" % DEFAULT_SPACING_MM)
    # Edge-selection knobs, for measuring rather than assuming. The defaults are
    # the faithful translation of contour semantics (no gradient floor, falling
    # edge for a backlit silhouette); everything else is a hypothesis to test
    # against def_compat.py, which is the only thing that can settle it.
    ap.add_argument("--min-strength", type=float, default=0)
    ap.add_argument("--polarity", default=None, choices=["falling", "rising", "any"])
    ap.add_argument("--nth", type=int, default=None)
    ap.add_argument("--engine", default=None, choices=["shape_based", "sig360"],
                    help="also set locating_engine (default: leave alone)")
    a = ap.parse_args()

    os.makedirs(a.dst, exist_ok=True)
    names = sorted(f for f in os.listdir(a.src) if f.endswith(".hydef"))
    if not names:
        print("no .hydef in " + a.src, file=sys.stderr)
        return 2

    total = 0
    for name in names:
        base = name[:-6]
        with open(os.path.join(a.src, name), encoding="utf-8") as f:
            d = json.load(f)
        fs = (d.get("featureSet") or [{}])[0]
        changed = 0
        print("== " + base)
        for feat in fs.get("features") or []:
            ok, note = convert_feature(feat, a.spacing, a.min_strength, a.polarity, a.nth)
            if note:
                print("   %-18s %s" % (str(feat.get("name"))[:18], note))
            if ok:
                changed += 1
        if a.engine:
            fs["locating_engine"] = a.engine
            print("   locating_engine -> " + a.engine)
        total += changed
        print("   %d feature(s) converted" % changed)

        with open(os.path.join(a.dst, name), "w", encoding="utf-8") as f:
            json.dump(d, f, ensure_ascii=False)
        # The image travels with the def: --insp needs the pair, and so does
        # the shape trainer.
        png = os.path.join(a.src, base + ".png")
        if os.path.exists(png):
            shutil.copy(png, os.path.join(a.dst, base + ".png"))

    print("")
    print("%d feature(s) converted across %d def(s) -> %s" % (total, len(names), a.dst))
    print("originals untouched. Next: def_compat.py --corpus <out> and compare "
          "against the pre-conversion baseline.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
