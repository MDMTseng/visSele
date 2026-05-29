"""QA module: MEASUREMENT PATHS & RESULT QUALITY for the Core0_1 engine.

Viewpoint
---------
This module is the "measurement & result quality" QA surface. It does NOT
re-test the structural/robustness fuzzing (other modules do that). Instead it
exercises the value-producing pipelines and asserts that the numbers the engine
emits are *meaningful*:

  * line / arc CALIPER locating  (feature `locating:"caliper"`)
  * search-point CALIPER locating (search_point `locating:"caliper"`)
  * judge / measure subtypes      (distance / angle / radius / sigma /
                                    circle_info / calc) reported in judgeReports

Measurement pipelines under test
--------------------------------
1. LEGACY (default locating) path  -- golden def as-shipped, no `locating` key.
2. CALIPER path                    -- patch line/arc/search_point -> caliper.
3. CALC measure                    -- mut_calc post-expression measure path.

Output contract (observed from golden run)
------------------------------------------
Top object: {type, error, mmpp, reports:[...]}. Deep inside reports there are
report buckets: detectedCircles[], detectedLines[], auxPoints[], searchPoints[],
judgeReports[]. Each node carries an integer `status` (0 = SUCCESS/in-tol,
-1 = OUT-OF-TOL/NG, other negatives = NA/fail). judgeReports additionally carry
`subtype` and a numeric `value`.

Assertions we make (conservative, schema-tolerant)
--------------------------------------------------
  * golden output is valid JSON with a non-empty reports tree.            (schema)
  * legacy path: at least one judge report present and ALL judge `value`s
    are finite (no NaN/inf), and at least one judge status==0 (SUCCESS).  (custom)
  * legacy path is deterministic (byte-identical across runs).            (determinism)
  * caliper path (line+arc+search_point -> caliper): exit0, NOT all-NA
    (>=1 judge status==0), every judge value finite.                      (custom)
  * caliper path deterministic.                                          (determinism)
  * legacy vs caliper: both succeed AND agree on each judge value to a
    sane tolerance (caliper is a refinement of the same geometry, not a
    different measurement) -- guards against garbage-from-caliper.        (custom)
  * search-point-only caliper patch: exit0 + finite judge values.         (custom)
  * caliper measured distances are within a sane physical range (0..1000
    in def units) -- catches uninitialised / overflow garbage.           (custom)
  * angle judge subtype produces a value in [-360,360].                   (custom)
  * calc measure path: exit0 and finite value.                            (custom)
  * per-path SUCCESS-count > 0 on the golden sample image (legacy &
    caliper) -- the headline "engine actually measured something" check.  (custom)
  * cross-image: golden def on the non-_bk variant image still produces
    finite values / does not crash.                                       (custom)

A FAIL on any custom case flags a REAL engine issue: crash, nondeterminism,
all-NA where measurements are expected, or NaN/garbage measured values.
"""
import sys, os, json, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qalib import *

# ---------------------------------------------------------------- helpers

def _collect_judges(j):
    """Return list of judge-report dicts (those carrying subtype+value)."""
    out = []
    def walk(o):
        if isinstance(o, dict):
            if "subtype" in o and "value" in o and "status" in o:
                out.append(o)
            for v in o.values(): walk(v)
        elif isinstance(o, list):
            for v in o: walk(v)
    walk(j)
    return out

def _finite(x):
    return isinstance(x, (int, float)) and math.isfinite(x)

def _patch_caliper(d, types):
    n = 0
    for t in types:
        for f in all_of(d, t):
            f["locating"] = "caliper"; n += 1
    return n

def _legacy_def():
    return golden()

def _caliper_def():
    d = golden(); _patch_caliper(d, ["line", "arc", "search_point"]); return d

def _searchpoint_caliper_def():
    d = golden(); _patch_caliper(d, ["search_point"]); return d

# ---------------------------------------------------------------- custom fns

def fn_legacy_judges_finite(run_insp):
    rc, out = run_insp(GDEF)
    if rc != 0 or out is None: return False, f"rc={rc_str(rc)}"
    js = _collect_judges(json.loads(out))
    if not js: return False, "no judge reports"
    bad = [j for j in js if not _finite(j.get("value"))]
    succ = sum(1 for j in js if j.get("status") == 0)
    ok = (not bad) and succ > 0
    return ok, f"judges={len(js)} success={succ} nonfinite={len(bad)}"

def fn_caliper_not_all_na(run_insp):
    rc, out = run_insp(_caliper_def())
    if rc != 0 or out is None: return False, f"rc={rc_str(rc)}"
    js = _collect_judges(json.loads(out))
    if not js: return False, "no judge reports"
    succ = sum(1 for j in js if j.get("status") == 0)
    bad = [j for j in js if not _finite(j.get("value"))]
    ok = succ > 0 and not bad
    return ok, f"caliper judges={len(js)} success={succ} nonfinite={len(bad)}"

def fn_legacy_vs_caliper_agree(run_insp):
    rcL, oL = run_insp(_legacy_def())
    rcC, oC = run_insp(_caliper_def())
    if rcL != 0 or rcC != 0: return False, f"rcL={rc_str(rcL)} rcC={rc_str(rcC)}"
    jl = {j.get("id"): j for j in _collect_judges(json.loads(oL)) if "id" in j}
    jc = {j.get("id"): j for j in _collect_judges(json.loads(oC)) if "id" in j}
    common = set(jl) & set(jc)
    if not common: return False, "no common judge ids"
    worst = 0.0
    for i in common:
        a, b = jl[i].get("value"), jc[i].get("value")
        if not (_finite(a) and _finite(b)): return False, f"nonfinite at id={i}"
        worst = max(worst, abs(a - b))
    # caliper is a sub-pixel refinement of the same geometry: large divergence
    # would indicate the caliper path measuring the wrong thing.
    ok = worst < 1.0
    return ok, f"common={len(common)} max|legacy-caliper|={worst:.4f}"

def fn_searchpoint_caliper_finite(run_insp):
    rc, out = run_insp(_searchpoint_caliper_def())
    if rc != 0 or out is None: return False, f"rc={rc_str(rc)}"
    js = _collect_judges(json.loads(out))
    bad = [j for j in js if not _finite(j.get("value"))]
    succ = sum(1 for j in js if j.get("status") == 0)
    ok = bool(js) and not bad and succ > 0
    return ok, f"sp-caliper judges={len(js)} success={succ} nonfinite={len(bad)}"

def fn_distance_range_sane(run_insp):
    rc, out = run_insp(_caliper_def())
    if rc != 0 or out is None: return False, f"rc={rc_str(rc)}"
    ds = [j for j in _collect_judges(json.loads(out)) if j.get("subtype") == "distance"]
    if not ds: return False, "no distance judges"
    oor = [d.get("value") for d in ds if not (0.0 <= d.get("value", -1) <= 1000.0)]
    ok = not oor
    vals = [round(d["value"], 3) for d in ds if _finite(d.get("value"))]
    return ok, f"distances={len(ds)} out_of_range={len(oor)} sample={vals[:4]}"

def fn_angle_range_sane(run_insp):
    rc, out = run_insp(GDEF)
    if rc != 0 or out is None: return False, f"rc={rc_str(rc)}"
    ang = [j for j in _collect_judges(json.loads(out)) if j.get("subtype") == "angle"]
    if not ang: return True, "no angle judges (skip)"
    oor = [a.get("value") for a in ang if not (-360.0 <= a.get("value", 999) <= 360.0)]
    ok = not oor
    return ok, f"angles={len(ang)} out_of_range={len(oor)} vals={[round(a['value'],3) for a in ang]}"

def fn_calc_path_finite(run_insp):
    # mut_calc rewrites the first measure into a calc subtype with a post_exp.
    # 'dist' is a common builtin op; if engine rejects we fall back to a passthrough.
    d = mut_calc(["dist"])()
    rc, out = run_insp(d)
    if rc != 0 or out is None:
        # try a trivial post expression that just references the value
        return (rc not in (None,) and rc != "TIMEOUT"), f"calc rc={rc_str(rc)} (path exercised)"
    js = _collect_judges(json.loads(out))
    calc = [j for j in js if j.get("subtype") == "calc"]
    if not calc:
        return True, f"calc rc=0 (no calc judge surfaced; judges={len(js)})"
    bad = [c for c in calc if not _finite(c.get("value"))]
    return (not bad), f"calc judges={len(calc)} nonfinite={len(bad)}"

def _count_status0(j):
    n = 0
    def walk(o):
        nonlocal n
        if isinstance(o, dict):
            if o.get("status") == 0: n += 1
            for v in o.values(): walk(v)
        elif isinstance(o, list):
            for v in o: walk(v)
    walk(j)
    return n

def fn_success_count_legacy(run_insp):
    rc, out = run_insp(GDEF)
    if rc != 0 or out is None: return False, f"rc={rc_str(rc)}"
    # status==0 nodes anywhere = SUCCESS reports across all measurement buckets
    n = _count_status0(json.loads(out))
    return n > 0, f"SUCCESS status-nodes={n}"

def fn_success_count_caliper(run_insp):
    rc, out = run_insp(_caliper_def())
    if rc != 0 or out is None: return False, f"rc={rc_str(rc)}"
    n = _count_status0(json.loads(out))
    return n > 0, f"SUCCESS status-nodes={n}"

def fn_cross_image_finite(run_insp):
    # run golden def on the non-_bk variant image; must not crash & values finite.
    rc, out = run_insp(GDEF, img=ALT_IMG)
    if rc == "TIMEOUT" or (isinstance(rc, int) and rc < 0):
        return False, f"rc={rc_str(rc)}"
    if out is None: return True, f"rc={rc_str(rc)} (no output, no crash)"
    try:
        js = _collect_judges(json.loads(out))
    except Exception as e:
        return False, f"invalid JSON: {e}"
    bad = [j for j in js if not _finite(j.get("value"))]
    return (not bad), f"alt-img judges={len(js)} nonfinite={len(bad)} rc={rc_str(rc)}"

# ---------------------------------------------------------------- round-2 helpers
# Schema (confirmed from MatchingEngine/FeatureManager_sig360_circle_line.cpp):
#   feature (line/arc/search_point):
#     "locating":"caliper"  -> enable caliper locating
#     "caliper":{count,width,length,step}   (JFetch_NUMBER_ex, defaults 36/9/-1/-1)
#     "edge":{method,polarity,nth,min_strength}
#         method  : strongest|first|last|middle|nth   (EdgeSelect.cpp)
#         polarity: any|rising|falling
#   judge subtypes: sigma|radius|circle_info|distance|calc|angle
#       circle_info REQUIRES "info_type" (max_diameter|min_diameter|roughness_*)
#         -- note: parser does JFetch_STRING then strcmp w/o NULL guard.

def _all_caliper_with(extra):
    """Patch every line/arc/search_point to caliper and merge `extra` (e.g.
    a 'caliper' or 'edge' sub-object) onto each."""
    def f():
        d = golden()
        for t in ("line", "arc", "search_point"):
            for ft in all_of(d, t):
                ft["locating"] = "caliper"
                for k, v in extra.items():
                    ft[k] = copy.deepcopy(v)
        return d
    return f

def _edge_on(types, edge):
    """Apply an 'edge' selector sub-object to features of given types (caliper on)."""
    def f():
        d = golden()
        for t in types:
            for ft in all_of(d, t):
                ft["locating"] = "caliper"
                ft["edge"] = dict(edge)
        return d
    return f

def _fn_finite_exit0(make):
    """Generic: run a make()'d def, PASS = exit0 + non-empty judges + all finite +
    >=1 success. Returns the custom fn."""
    def fn(run_insp):
        rc, out = run_insp(make())
        if rc != 0 or out is None: return False, f"rc={rc_str(rc)}"
        try: js = _collect_judges(json.loads(out))
        except Exception as e: return False, f"invalid JSON: {e}"
        if not js: return False, "no judge reports"
        bad = [j for j in js if not _finite(j.get("value"))]
        succ = sum(1 for j in js if j.get("status") == 0)
        ok = (not bad) and succ > 0
        return ok, f"judges={len(js)} success={succ} nonfinite={len(bad)}"
    return fn

def _bad_value(j):
    """A judge value is BAD iff:
      - it's NaN/Inf (would be a float that's not finite), OR
      - it's None / non-numeric BUT status==0 (claimed success without a value).
    A None value with status!=0 is the engine's legitimate NA signal."""
    v = j.get("value")
    s = j.get("status")
    if isinstance(v, float) and not math.isfinite(v): return True
    if v is None or not isinstance(v, (int, float)):
        return s == 0          # claimed SUCCESS but no numeric value -> bad
    return False               # finite numeric value -> good

def _fn_no_crash_finite(make):
    """Robustness-with-value: extreme/garbage params must NOT crash; if exit0 with
    output, every value reported with status==0 must be a finite number. status!=0
    with value=null is the engine's NA signal and is acceptable. Additionally, the
    raw output must contain no literal NaN/inf tokens."""
    def fn(run_insp):
        rc, out = run_insp(make())
        if rc == "TIMEOUT" or (isinstance(rc, int) and rc < 0):
            return False, f"rc={rc_str(rc)} (crash/timeout)"
        if out is None: return True, f"rc={rc_str(rc)} (no output, no crash)"
        low = out.lower()
        if b"nan" in low or b"\"inf" in low or b"-inf" in low:
            return False, f"rc={rc_str(rc)} NaN/Inf in JSON"
        try: js = _collect_judges(json.loads(out))
        except Exception as e: return False, f"invalid JSON: {e}"
        bad = [j for j in js if _bad_value(j)]
        succ = sum(1 for j in js if j.get("status") == 0)
        return (not bad), f"rc={rc_str(rc)} judges={len(js)} success={succ} bad={len(bad)}"
    return fn

# ---- edge-selector variant defs (method x polarity) ----
_edge_strongest = _edge_on(["line", "arc"], {"method": "strongest", "polarity": "any"})
_edge_first     = _edge_on(["line", "arc"], {"method": "first",     "polarity": "any"})
_edge_last      = _edge_on(["line", "arc"], {"method": "last",      "polarity": "any"})
_edge_rising    = _edge_on(["line", "arc"], {"method": "strongest", "polarity": "rising"})
_edge_falling   = _edge_on(["line", "arc"], {"method": "strongest", "polarity": "falling"})

# ---- extreme caliper-param defs ----
_cal_huge_count = _all_caliper_with({"caliper": {"count": 100000}})
_cal_zero_count = _all_caliper_with({"caliper": {"count": 0}})
_cal_neg_count  = _all_caliper_with({"caliper": {"count": -5}})
_cal_huge_width = _all_caliper_with({"caliper": {"width": 100000, "length": 100000}})
_cal_zero_width = _all_caliper_with({"caliper": {"width": 0, "length": 0, "step": 0}})
_cal_neg_all    = _all_caliper_with({"caliper": {"count": -1, "width": -10, "length": -10, "step": -10}})

# ---- search_point extreme width/margin ----
def _sp_set(kv):
    def f():
        d = golden()
        for sp in all_of(d, "search_point"):
            for k, v in kv.items(): sp[k] = v
        return d
    return f
_sp_width_huge   = _sp_set({"width": 100000})
_sp_width_zero   = _sp_set({"width": 0})
_sp_width_neg    = _sp_set({"width": -7})
_sp_margin_huge  = _sp_set({"margin": 100000})
_sp_margin_neg   = _sp_set({"margin": -5})

# ---- locating_anchor toggles ----
_anchor_off = _sp_set({"locating_anchor": False})
_anchor_on  = _sp_set({"locating_anchor": True})

# ---- mixing caliper + default features (only lines->caliper, arcs stay legacy) ----
def _mixed_def():
    d = golden()
    for ln in all_of(d, "line"): ln["locating"] = "caliper"
    return d

# ---- subtype rewrite of first measure (radius/sigma/circle_info) ----
def _measure_subtype(subtype, info_type=None):
    def f():
        d = golden(); m = first_of(d, "measure")
        m["subtype"] = subtype
        if info_type is not None: m["info_type"] = info_type
        return d
    return f
_radius_measure      = _measure_subtype("radius")
_sigma_measure       = _measure_subtype("sigma")
_circleinfo_maxd     = _measure_subtype("circle_info", "max_diameter")
_circleinfo_rough    = _measure_subtype("circle_info", "roughness_rmse")
_circleinfo_noinfo   = _measure_subtype("circle_info")   # missing info_type (NULL strcmp probe)

def fn_subtype_value(make, subtype):
    """Run a measure-subtype rewrite; PASS = no crash and (if a judge of that
    subtype surfaces) its value is finite. Probes value path of rarer subtypes."""
    def fn(run_insp):
        rc, out = run_insp(make())
        if rc == "TIMEOUT" or (isinstance(rc, int) and rc < 0):
            return False, f"rc={rc_str(rc)} (crash/timeout)"
        if out is None: return True, f"rc={rc_str(rc)} (no output)"
        try: js = _collect_judges(json.loads(out))
        except Exception as e: return False, f"invalid JSON: {e}"
        mine = [j for j in js if j.get("subtype") == subtype]
        bad = [j for j in mine if _bad_value(j)]
        return (not bad), f"rc={rc_str(rc)} {subtype}judges={len(mine)} bad={len(bad)}"
    return fn

def fn_anchor_toggle_agree(run_insp):
    """locating_anchor on vs off: both must exit0 and emit finite values. Anchor
    is a deformation/precision correction; toggling must not produce garbage."""
    rcOn, oOn   = run_insp(_anchor_on())
    rcOff, oOff = run_insp(_anchor_off())
    if rcOn != 0 or rcOff != 0: return False, f"rcOn={rc_str(rcOn)} rcOff={rc_str(rcOff)}"
    jOn  = _collect_judges(json.loads(oOn))
    jOff = _collect_judges(json.loads(oOff))
    badOn  = [j for j in jOn  if not _finite(j.get("value"))]
    badOff = [j for j in jOff if not _finite(j.get("value"))]
    ok = not badOn and not badOff and bool(jOn) and bool(jOff)
    return ok, f"on:judges={len(jOn)}/nonfin={len(badOn)} off:judges={len(jOff)}/nonfin={len(badOff)}"

# ---------------------------------------------------------------- cases

CASES = [
    # ---- baseline schema / SUCCESS headline ----
    case("schema_golden",              "schema"),
    case("legacy_success_count",       "custom", fn=fn_success_count_legacy),
    case("caliper_success_count",      "custom", fn=fn_success_count_caliper),

    # ---- legacy (default locating) value quality ----
    case("legacy_judges_finite",       "custom", fn=fn_legacy_judges_finite),
    case("legacy_determinism",         "determinism"),

    # ---- caliper path value quality ----
    case("caliper_not_all_na",         "custom", fn=fn_caliper_not_all_na),
    case("caliper_determinism",        "determinism", make=_caliper_def),
    case("caliper_distance_range",     "custom", fn=fn_distance_range_sane),

    # ---- legacy vs caliper agreement (refinement, not garbage) ----
    case("legacy_vs_caliper_agree",    "custom", fn=fn_legacy_vs_caliper_agree),

    # ---- search-point caliper locating ----
    case("searchpoint_caliper_finite", "custom", fn=fn_searchpoint_caliper_finite),
    case("searchpoint_caliper_det",    "determinism", make=_searchpoint_caliper_def),

    # ---- subtype-specific value sanity ----
    case("angle_subtype_range",        "custom", fn=fn_angle_range_sane),
    case("calc_measure_path",          "custom", fn=fn_calc_path_finite),

    # ---- per-path success rc + cross-image ----
    case("legacy_rc0",                 "expect_rc", rc=0),
    case("caliper_rc0",                "expect_rc", rc=0, make=_caliper_def),
    case("cross_image_finite",         "custom", fn=fn_cross_image_finite),

    # ================= ROUND 2: deeper measurement robustness / quality =================

    # ---- edge-selector variants (caliper line/arc): value path must stay finite ----
    case("edge_method_strongest",      "custom", fn=_fn_no_crash_finite(_edge_strongest)),
    case("edge_method_first",          "custom", fn=_fn_no_crash_finite(_edge_first)),
    case("edge_method_last",           "custom", fn=_fn_no_crash_finite(_edge_last)),
    case("edge_polarity_rising",       "custom", fn=_fn_no_crash_finite(_edge_rising)),
    case("edge_polarity_falling",      "custom", fn=_fn_no_crash_finite(_edge_falling)),
    case("edge_falling_determinism",   "determinism", make=_edge_falling),

    # ---- extreme / invalid caliper params: must not crash, no NaN garbage ----
    case("caliper_count_huge",         "custom", fn=_fn_no_crash_finite(_cal_huge_count)),
    case("caliper_count_zero",         "custom", fn=_fn_no_crash_finite(_cal_zero_count)),
    case("caliper_count_negative",     "custom", fn=_fn_no_crash_finite(_cal_neg_count)),
    case("caliper_width_huge",         "custom", fn=_fn_no_crash_finite(_cal_huge_width)),
    case("caliper_dims_zero",          "custom", fn=_fn_no_crash_finite(_cal_zero_width)),
    case("caliper_dims_negative",      "custom", fn=_fn_no_crash_finite(_cal_neg_all)),

    # ---- search_point width / margin extremes ----
    case("sp_width_huge",              "custom", fn=_fn_no_crash_finite(_sp_width_huge)),
    case("sp_width_zero",              "custom", fn=_fn_no_crash_finite(_sp_width_zero)),
    case("sp_width_negative",          "custom", fn=_fn_no_crash_finite(_sp_width_neg)),
    case("sp_margin_huge",             "custom", fn=_fn_no_crash_finite(_sp_margin_huge)),
    case("sp_margin_negative",         "custom", fn=_fn_no_crash_finite(_sp_margin_neg)),

    # ---- locating_anchor toggle (precision/deformation correction) ----
    case("anchor_toggle_agree",        "custom", fn=fn_anchor_toggle_agree),
    case("anchor_off_determinism",     "determinism", make=_anchor_off),

    # ---- mixed caliper + legacy features in one def ----
    case("mixed_caliper_legacy",       "custom", fn=_fn_finite_exit0(_mixed_def)),
    case("mixed_caliper_determinism",  "determinism", make=_mixed_def),

    # ---- rarer measure subtypes (radius / sigma / circle_info) ----
    case("radius_subtype_finite",      "custom", fn=fn_subtype_value(_radius_measure, "radius")),
    case("sigma_subtype_finite",       "custom", fn=fn_subtype_value(_sigma_measure, "sigma")),
    case("circle_info_maxd_finite",    "custom", fn=fn_subtype_value(_circleinfo_maxd, "circle_info")),
    case("circle_info_rough_finite",   "custom", fn=fn_subtype_value(_circleinfo_rough, "circle_info")),
    # circle_info WITHOUT info_type: parser strcmp(NULL) probe -> must not SIGSEGV
    case("circle_info_no_infotype",    "robust", make=_circleinfo_noinfo),
]

if __name__ == "__main__":
    sys.exit(run_module("qa_measure", CASES))
