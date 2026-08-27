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


def arc_from_3pts(p1, p2, p3):
    """Circumcentre, radius and swept angle of the arc p1->p2->p3.

    The chord approximation this replaces (|p1p2| + |p2p3|) understates a real
    sweep, and the understatement grows with curvature -- so the tightest arcs,
    which already have the fewest calipers, got the fewest of all. On this
    corpus the two 0.3mm arcs were the worst converted features by an order of
    magnitude.
    """
    ax, ay = p1["x"], p1["y"]
    bx, by = p2["x"], p2["y"]
    cx_, cy_ = p3["x"], p3["y"]
    d = 2.0 * (ax * (by - cy_) + bx * (cy_ - ay) + cx_ * (ay - by))
    if abs(d) < 1e-12:
        return None            # collinear: no circle, caller falls back
    ux = ((ax * ax + ay * ay) * (by - cy_) + (bx * bx + by * by) * (cy_ - ay)
          + (cx_ * cx_ + cy_ * cy_) * (ay - by)) / d
    uy = ((ax * ax + ay * ay) * (cx_ - bx) + (bx * bx + by * by) * (ax - cx_)
          + (cx_ * cx_ + cy_ * cy_) * (bx - ax)) / d
    r = math.hypot(ax - ux, ay - uy)
    a1 = math.atan2(ay - uy, ax - ux)
    a2 = math.atan2(by - uy, bx - ux)
    a3 = math.atan2(cy_ - uy, cx_ - ux)
    # The sweep is the one that PASSES THROUGH p2, which is the only thing that
    # distinguishes the arc from its complement -- the same mistake the WebUI's
    # arc auto-width made (MathTools arcSweep, fixed 2026-08-26): three copies
    # of a p1->p3 span that never consulted p2, seeding 11x the intended width.
    def norm(a):
        while a <= -math.pi:
            a += 2 * math.pi
        while a > math.pi:
            a -= 2 * math.pi
        return a
    d13 = norm(a3 - a1)
    d12 = norm(a2 - a1)
    # p2 must lie between p1 and p3 along the swept direction; if it does not,
    # the arc is the long way round.
    if d13 >= 0:
        sweep = d13 if (0 <= d12 <= d13) else d13 - 2 * math.pi
    else:
        sweep = d13 if (d13 <= d12 <= 0) else d13 + 2 * math.pi
    return ux, uy, r, abs(sweep)


def feature_length_mm(f):
    """Length along the feature, in def units (mm).

    A line is its two endpoints. An arc is r * sweep -- the true arc length,
    through the middle point. See arc_from_3pts for why the chord sum it
    replaces was not good enough.
    """
    t = f.get("type")
    try:
        if t == "line":
            return dist(f["pt1"], f["pt2"])
        if t == "arc":
            a = arc_from_3pts(f["pt1"], f["pt2"], f["pt3"])
            if a is None:
                return dist(f["pt1"], f["pt2"]) + dist(f["pt2"], f["pt3"])
            _, _, r, sweep = a
            return r * sweep
    except (KeyError, TypeError):
        return None
    return None


def convert_feature(f, spacing, min_strength=0, polarity=None, nth=None, arc_polarity=None,
                    mmpp=None, min_sagitta_px=3.0):
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

    # A FLAT-TAUGHT ARC POINTS THE CALIPER SEARCH THE WRONG WAY.
    #
    # The two modes use the taught geometry for different things. Contour treats
    # the def arc as a SEARCH REGION -- it finds the real contour near it and
    # fits that, so a sloppily taught arc still recovers the true bend. Caliper
    # takes it LITERALLY: it lays calipers along the taught arc and searches
    # RADIALLY from the taught centre.
    #
    # So what matters is where that centre lands. Measured on 93020
    # BCG-20X40X53 feature [13]: the three taught points are nearly collinear,
    # putting their circumcentre 129px away with r=129px, while the real bend
    # has r=20px. The ten search rays fan over 15.9 degrees -- nearly parallel
    # -- across a corner that turns almost 90. Each ray therefore crosses the
    # wire at a different oblique angle, and one running nearly ALONG the wire
    # finds its "first edge" anywhere at all. Probing the returned hits shows
    # exactly that: grey levels along the sequence run 222, 69, 35, 50, 201, 235
    # instead of sitting on a transition, and the last one is buried inside the
    # wire with a local contrast of 29.
    #
    # That is why neither polarity nor nth moves the result toward contour: the
    # search DIRECTION is wrong, so which edge it selects is beside the point.
    # (Two rounds were spent on polarity and caliper count before measuring
    # this. The sagitta test below would have said so immediately.)
    #
    # Sagitta is the cheap proxy: small sagitta <=> distant centre <=> parallel
    # rays. It is a property of the def alone -- no engine, no image. These need
    # RE-TEACHING by whoever set the recipe, not converting. 12 of the 14 arcs
    # in the corpus have sagittas of 8.6-51px and convert to within ~0.01mm.
    if t == "arc":
        a = arc_from_3pts(f["pt1"], f["pt2"], f["pt3"])
        if a is not None:
            _, _, r_mm, _ = a
            chord = dist(f["pt1"], f["pt3"])
            if 2 * r_mm > chord:
                sag_mm = r_mm - math.sqrt(max(r_mm * r_mm - (chord / 2.0) ** 2, 0.0))
                sag_px = sag_mm / mmpp if mmpp else 0
                if sag_px < min_sagitta_px:
                    return False, ("SKIPPED -- taught sagitta %.1fpx (< %.1f): the taught "
                                   "points are nearly collinear, so the caliper's radial "
                                   "search would not run perpendicular to the edge. Have the "
                                   "recipe's arc re-taught; do not convert."
                                   % (sag_px, min_sagitta_px))

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
    # Per-type default, and it is not cosmetic. A backlit wire has TWO edges;
    # `falling` takes the outer one, which is right for a silhouette boundary
    # and wrong for the inner radius an arc usually measures. Measured on this
    # corpus: falling put every R1.0 out by -0.11mm (about half a wire), rising
    # brought it to -0.01mm.
    _pol = (arc_polarity if (t == "arc" and arc_polarity) else polarity)
    edge.setdefault("polarity", _pol or ("rising" if t == "arc" else "falling"))
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
    ap.add_argument("--min-sagitta-px", type=float, default=3.0,
                    help="arcs whose taught sagitta is below this are left in contour "
                         "mode and reported (default 3.0)")
    ap.add_argument("--arc-polarity", default=None, choices=["falling", "rising", "any"],
                    help="override the arc default (rising); lines are unaffected")
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
        changed = 0; skipped = 0
        print("== " + base)
        for feat in fs.get("features") or []:
            ok, note = convert_feature(feat, a.spacing, a.min_strength, a.polarity, a.nth,
                                        a.arc_polarity, fs.get('mmpp'), a.min_sagitta_px)
            if note:
                print("   %-18s %s" % (str(feat.get("name"))[:18], note))
            if ok:
                changed += 1
            elif note and note.startswith("SKIPPED"):
                skipped += 1
        if a.engine:
            fs["locating_engine"] = a.engine
            print("   locating_engine -> " + a.engine)
        total += changed
        print("   %d converted%s" % (changed, (", %d SKIPPED (needs re-teaching)" % skipped) if skipped else ""))

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
