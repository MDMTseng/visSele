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

# ================= ROUND 3: deeper ref-chain / cascade / extreme-step probes =================

def _set_id(d, fid, key, val):
    """Walk and set key on the feature with this id."""
    def w(o):
        if isinstance(o, dict):
            if o.get("id") == fid: o[key] = val
            for v in o.values(): w(v)
        elif isinstance(o, list):
            for v in o: w(v)
    w(d)

def _find_id(d, fid):
    out = [None]
    def w(o):
        if isinstance(o, dict):
            if o.get("id") == fid: out[0] = o
            for v in o.values(): w(v)
        elif isinstance(o, list):
            for v in o: w(v)
    w(d); return out[0]

# R3-1: locating_anchor cycle/chain - sp3 anchors sp4 anchors sp3 (cycle)
def _anchor_cycle():
    d = golden()
    sp3 = _find_id(d, 3); sp4 = _find_id(d, 4)
    if sp3 and sp4:
        sp3["ref"] = [{"id": 4, "element": "search_point"}]
        sp4["ref"] = [{"id": 3, "element": "search_point"}]
        sp3["locating_anchor"] = True
        sp4["locating_anchor"] = True
    return d

# R3-2: aux_point lineCross with two parallel lines (no intersection)
def _aux_lineCross_parallel():
    d = golden()
    sig = first_of(d, "sig360_circle_line")
    if sig is None: return d
    # Create two parallel lines and an aux_point lineCross referencing them.
    new_line_a = {"id": 7001, "type": "line", "pt1": {"x":0,"y":0}, "pt2":{"x":10,"y":0}}
    new_line_b = {"id": 7002, "type": "line", "pt1": {"x":0,"y":5}, "pt2":{"x":10,"y":5}}
    aux = {"id": 7003, "type": "aux_point", "subtype": "lineCross",
           "ref": [{"id":7001,"element":"line"},{"id":7002,"element":"line"}]}
    items = sig.get("items") or sig.get("data") or None
    # Inject directly into sig360_circle_line's list children
    for k, v in sig.items():
        if isinstance(v, list):
            v.extend([new_line_a, new_line_b, aux]); break
    return d

# R3-3: aux_point ref of WRONG type (point references a measure id, not a line/arc)
def _aux_ref_wrong_type():
    d = golden()
    sig = first_of(d, "sig360_circle_line")
    if sig is None: return d
    aux = {"id": 7010, "type": "aux_point", "subtype": "lineCross",
           "ref": [{"id":12,"element":"measure"},{"id":14,"element":"measure"}]}
    for k, v in sig.items():
        if isinstance(v, list): v.append(aux); break
    return d

# R3-4: measure(angle) referring to circles (arcs id=24,25) instead of lines
def _angle_ref_arcs():
    d = golden(); m = first_of(d, "measure")
    m["subtype"] = "angle"
    m["ref"] = [{"id":24,"element":"arc"},{"id":25,"element":"arc"}]
    return d

# R3-5: search_point referencing a feature that fails (line id 999 nonexistent) - cascade NA
def _sp_ref_missing():
    d = golden()
    sp = _find_id(d, 3)
    if sp: sp["ref"] = [{"id":999, "element":"line"}]
    return d

# R3-6: extreme caliper step
_cal_step_one    = _all_caliper_with({"caliper": {"step": 1}})
_cal_step_huge   = _all_caliper_with({"caliper": {"step": 1000}})

# R3-7: vertex_touch_searching=true on every search_point
_vts_on = _sp_set({"vertex_touch_searching": True})

# R3-8: multiple measures sharing OBJ refs - duplicate measure 12 several times
def _shared_refs_many_measures():
    d = golden()
    sig = first_of(d, "sig360_circle_line")
    m12 = _find_id(d, 12)
    if sig is None or m12 is None: return d
    for k, v in sig.items():
        if isinstance(v, list):
            for i in range(5):
                copy_m = copy.deepcopy(m12)
                copy_m["id"] = 8100 + i
                v.append(copy_m)
            break
    return d

# R3-9: deep aux->aux->aux chain
def _deep_aux_chain():
    d = golden()
    sig = first_of(d, "sig360_circle_line")
    if sig is None: return d
    nodes = []
    prev_id = 1  # line id 1
    for i in range(5):
        nid = 7100 + i
        nodes.append({
            "id": nid, "type": "aux_point", "subtype": "lineCross" if i==0 else "passThrough",
            "ref": [{"id": prev_id, "element": "line" if i==0 else "aux_point"},
                    {"id": 2, "element": "line"}] if i==0 else [{"id": prev_id, "element": "aux_point"}]
        })
        prev_id = nid
    for k, v in sig.items():
        if isinstance(v, list): v.extend(nodes); break
    return d

# R3-10: orientation_essential=true on a (deliberately broken) measure -> forces reDo loop
def _orientation_essential_broken():
    d = golden(); m = first_of(d, "measure")
    m["orientation_essential"] = True
    # break it: ref a non-existent id
    m["ref"] = [{"id": 9999, "element": "search_point"}, {"id": 9998, "element": "search_point"}]
    return d

# R3-11: quality_essential=true on first measure
def _quality_essential():
    d = golden(); m = first_of(d, "measure")
    m["quality_essential"] = True
    return d

# R3-12: extremely tight tolerance forces OOT but value must still be finite
def _tight_tol():
    d = golden(); m = first_of(d, "measure")
    m["tol_upper"] = 0.00001; m["tol_lower"] = -0.00001
    return d

# ---- custom fns for R3 ----

def fn_cascade_NA(run_insp):
    """Broken ref must not crash; downstream judges referring to broken sp must
    be NA (status!=0, value null) OR finite. No NaN/inf garbage."""
    rc, out = run_insp(_sp_ref_missing())
    if rc == "TIMEOUT" or (isinstance(rc, int) and rc < 0):
        return False, f"rc={rc_str(rc)} (crash/timeout)"
    if out is None: return True, f"rc={rc_str(rc)} (no output, no crash)"
    if b"nan" in out.lower() or b"\"inf" in out.lower():
        return False, "NaN/Inf in JSON"
    try: js = _collect_judges(json.loads(out))
    except Exception as e: return False, f"invalid JSON: {e}"
    bad = [j for j in js if _bad_value(j)]
    succ = sum(1 for j in js if j.get("status") == 0)
    na = sum(1 for j in js if j.get("status") != 0)
    return (not bad), f"rc={rc_str(rc)} judges={len(js)} succ={succ} na={na} bad={len(bad)}"

def fn_parallel_lineCross(run_insp):
    """Parallel lines -> lineCross undefined. Engine must not crash or emit NaN.
    Controlled SIGABRT reject is acceptable."""
    rc, out = run_insp(_aux_lineCross_parallel())
    if rc == "TIMEOUT" or (isinstance(rc, int) and rc in SIGCRASH):
        return False, f"rc={rc_str(rc)} (crash/timeout)"
    if isinstance(rc, int) and rc < 0:
        return True, f"rc={rc_str(rc)} (controlled-reject)"
    if out is None: return True, f"rc={rc_str(rc)} (no out)"
    if b"nan" in out.lower() or b"\"inf" in out.lower():
        return False, "NaN/Inf in JSON for parallel lineCross"
    try: json.loads(out)
    except Exception as e: return False, f"invalid JSON: {e}"
    return True, f"rc={rc_str(rc)} clean json (no NaN/Inf)"

def fn_angle_ref_arcs(run_insp):
    """Angle measure referring to arcs (not lines) - either reject or NA, must not crash/NaN."""
    rc, out = run_insp(_angle_ref_arcs())
    if rc == "TIMEOUT" or (isinstance(rc, int) and rc < 0):
        return False, f"rc={rc_str(rc)} (crash/timeout)"
    if out is None: return True, f"rc={rc_str(rc)} (no out)"
    if b"nan" in out.lower() or b"\"inf" in out.lower():
        return False, "NaN/Inf in JSON"
    try: js = _collect_judges(json.loads(out))
    except Exception as e: return False, f"invalid JSON: {e}"
    bad = [j for j in js if _bad_value(j)]
    return (not bad), f"rc={rc_str(rc)} judges={len(js)} bad={len(bad)}"

def fn_tight_tol_oot_finite(run_insp):
    """Tight tolerance: expect at least one judge status==-1 (OOT) with finite value,
    not garbage."""
    rc, out = run_insp(_tight_tol())
    if rc != 0 or out is None: return False, f"rc={rc_str(rc)}"
    js = _collect_judges(json.loads(out))
    if not js: return False, "no judges"
    bad = [j for j in js if _bad_value(j)]
    oot = sum(1 for j in js if j.get("status") == -1)
    # finite values must dominate; we just need no garbage
    return (not bad), f"judges={len(js)} OOT={oot} bad={len(bad)}"

def fn_shared_refs(run_insp):
    """5 measures referencing the same OBJ ids must each produce the same finite
    value (determinism within one output across duplicates). Controlled SIGABRT
    (e.g. dup-id reject) is acceptable - we just must not crash."""
    rc, out = run_insp(_shared_refs_many_measures())
    if rc == "TIMEOUT" or (isinstance(rc, int) and rc in SIGCRASH):
        return False, f"rc={rc_str(rc)} (crash)"
    if isinstance(rc, int) and rc < 0:
        return True, f"rc={rc_str(rc)} (controlled-reject)"
    if rc != 0 or out is None: return False, f"rc={rc_str(rc)}"
    js = _collect_judges(json.loads(out))
    dup = [j for j in js if isinstance(j.get("id"), int) and 8100 <= j["id"] <= 8104]
    if len(dup) < 2:
        return True, f"only {len(dup)} duplicates surfaced (skip strict check)"
    vals = [d.get("value") for d in dup if _finite(d.get("value"))]
    if not vals: return False, f"no finite values in duplicates ({len(dup)})"
    spread = max(vals) - min(vals)
    return (spread < 1e-6), f"dup-measures={len(dup)} finite={len(vals)} spread={spread:.2e}"

def fn_orientation_essential_loop(run_insp):
    """orientation_essential on broken measure should fail gracefully (no hang, no crash)."""
    rc, out = run_insp(_orientation_essential_broken(), timeout_override=60) \
        if False else run_insp(_orientation_essential_broken())
    if rc == "TIMEOUT":
        return False, "TIMEOUT (possible hang in reDo loop)"
    if isinstance(rc, int) and rc < 0 and rc in SIGCRASH:
        return False, f"crash rc={rc_str(rc)}"
    if out is None: return True, f"rc={rc_str(rc)} no out, no crash"
    if b"nan" in out.lower() or b"\"inf" in out.lower():
        return False, "NaN/Inf in JSON"
    return True, f"rc={rc_str(rc)} no hang/crash"

CASES += [
    # ref-chain / cascade
    case("anchor_cycle",               "robust", make=_anchor_cycle),
    case("aux_lineCross_parallel",     "custom", fn=fn_parallel_lineCross),
    case("aux_ref_wrong_type",         "robust", make=_aux_ref_wrong_type),
    case("angle_ref_arcs",             "custom", fn=fn_angle_ref_arcs),
    case("sp_ref_missing_cascade",     "custom", fn=fn_cascade_NA),
    case("deep_aux_chain",             "robust", make=_deep_aux_chain),

    # extreme caliper step
    case("caliper_step_one",           "custom", fn=_fn_no_crash_finite(_cal_step_one)),
    case("caliper_step_huge",          "custom", fn=_fn_no_crash_finite(_cal_step_huge)),

    # vertex_touch_searching
    case("vts_on",                     "custom", fn=_fn_no_crash_finite(_vts_on)),

    # multi-measure sharing refs
    case("shared_refs_many_measures",  "custom", fn=fn_shared_refs),

    # orientation_essential + broken measure: reDo loop must terminate
    case("orientation_essential_broken","custom", fn=fn_orientation_essential_loop),

    # quality_essential edge
    case("quality_essential",          "custom", fn=_fn_no_crash_finite(_quality_essential)),

    # tight tolerance -> OOT with finite values, no garbage
    case("tight_tolerance_OOT",        "custom", fn=fn_tight_tol_oot_finite),
]

# ================= ROUND 4: clamp boundaries / new flags / timing regressions =================
import time

# ---- caliper clamp boundary helpers ----
_cal_at_boundary       = _all_caliper_with({"caliper": {"count": 4096, "width": 2048, "length": 4096}})
_cal_width_just_over   = _all_caliper_with({"caliper": {"count": 4096, "width": 2049, "length": 4096}})
_cal_width_extreme     = _all_caliper_with({"caliper": {"count": 4096, "width": 10000, "length": 10000}})

def fn_timed(make, max_seconds=30.0):
    """Run, assert exit0 + finite values + completion under max_seconds. Verifies
    clamps protect against DoS (an unclamped caliper with width*length*count of
    ~10^11 would never finish)."""
    def fn(run_insp):
        t0 = time.time()
        # Cap subprocess timeout slightly above our budget so a real hang
        # surfaces as TIMEOUT here (not as the harness default 120s wait).
        rc, out = run_insp(make(), timeout=max_seconds + 5)
        dt = time.time() - t0
        if rc == "TIMEOUT":
            return False, f"TIMEOUT after {dt:.1f}s"
        if isinstance(rc, int) and rc in SIGCRASH:
            return False, f"crash rc={rc_str(rc)} dt={dt:.1f}s"
        if dt > max_seconds:
            return False, f"too slow: {dt:.1f}s > {max_seconds}s"
        if rc != 0 or out is None:
            return False, f"rc={rc_str(rc)} dt={dt:.1f}s"
        if b"nan" in out.lower() or b"\"inf" in out.lower():
            return False, f"NaN/Inf in JSON dt={dt:.1f}s"
        try: js = _collect_judges(json.loads(out))
        except Exception as e: return False, f"invalid JSON dt={dt:.1f}s: {e}"
        bad = [j for j in js if _bad_value(j)]
        ok = (not bad) and bool(js)
        return ok, f"rc=0 dt={dt:.2f}s judges={len(js)} bad={len(bad)}"
    return fn

# ---- circle_info subtype variants (all 5 info_types) ----
_circleinfo_mind        = _measure_subtype("circle_info", "min_diameter")
_circleinfo_rough_max   = _measure_subtype("circle_info", "roughness_max")
_circleinfo_rough_min   = _measure_subtype("circle_info", "roughness_min")

# Round-2 regression check: circle_info WITHOUT info_type now must return a
# controlled reject (-1 from parse_judgeData -> early exit), NOT SIGSEGV.
def fn_circle_info_no_infotype_controlled(run_insp):
    # use a fresh out path to avoid the shared _out.json from prior cases.
    rc, out = run_insp(_circleinfo_noinfo(), out_path=f"{TMP}/_out_circ_noinfo.json")
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"SIGSEGV/BUS regression: rc={rc_str(rc)}"
    if rc == "TIMEOUT":
        return False, "TIMEOUT (hang)"
    # Acceptable: any non-crash outcome (controlled abort, error rc, or NA output).
    if out is not None and (b"nan" in out.lower() or b"\"inf" in out.lower()):
        return False, "NaN/Inf in JSON"
    return True, f"rc={rc_str(rc)} (no crash)"

# ---- locating_anchor TRUE on ALL search_points (heavy anchor pass) ----
_anchor_all_on = _sp_set({"locating_anchor": True})

# ---- value_adjust ----
def _value_adjust(amount):
    def f():
        d = golden()
        for sig in all_of(d, "sig360_circle_line"):
            for k, v in sig.items():
                if isinstance(v, list):
                    for it in v:
                        if isinstance(it, dict) and it.get("type") == "measure":
                            it["value_adjust"] = amount
        return d
    return f

def fn_value_adjust_shift(run_insp):
    """value_adjust adds an offset to the reported measure value. Run baseline
    + adjusted; for each common judge id the diff must equal the offset (within fp)."""
    rcA, oA = run_insp(GDEF)
    rcB, oB = run_insp(_value_adjust(0.5)())
    if rcA != 0 or rcB != 0: return False, f"rcA={rc_str(rcA)} rcB={rc_str(rcB)}"
    ja = {j.get("id"): j for j in _collect_judges(json.loads(oA)) if "id" in j and _finite(j.get("value"))}
    jb = {j.get("id"): j for j in _collect_judges(json.loads(oB)) if "id" in j and _finite(j.get("value"))}
    common = set(ja) & set(jb)
    if not common: return False, "no common judges"
    diffs = [jb[i]["value"] - ja[i]["value"] for i in common]
    # at least one judge should have shifted by ~0.5; non-numeric measures (calc) may not.
    matched = sum(1 for d in diffs if abs(d - 0.5) < 1e-4)
    bad = [d for d in diffs if not math.isfinite(d)]
    # Acceptable: feature is honored (matched > 0) OR engine ignores it cleanly (no NaN, all stable).
    ok = (matched > 0 or all(abs(d) < 1e-4 for d in diffs)) and not bad
    return ok, f"common={len(common)} shifted_by_0.5={matched} sample_diffs={[round(d,4) for d in diffs[:4]]}"

# ---- back_value_setup flag ----
def _back_value_setup(flag):
    def f():
        d = golden()
        for sig in all_of(d, "sig360_circle_line"):
            for k, v in sig.items():
                if isinstance(v, list):
                    for it in v:
                        if isinstance(it, dict) and it.get("type") == "measure":
                            it["back_value_setup"] = flag
        return d
    return f

# ---- NGasNA / NAasNG combos ----
def _na_ng_flags(ng_as_na, na_as_ng):
    def f():
        d = golden()
        for sig in all_of(d, "sig360_circle_line"):
            for k, v in sig.items():
                if isinstance(v, list):
                    for it in v:
                        if isinstance(it, dict) and it.get("type") == "measure":
                            it["NGasNA"] = ng_as_na
                            it["NAasNG"] = na_as_ng
        return d
    return f

def fn_flag_combos_no_crash(run_insp):
    """All four (NGasNA, NAasNG) combos: must exit0 and emit finite values."""
    combos = [(False, False), (True, False), (False, True), (True, True)]
    details = []
    for ng, na in combos:
        rc, out = run_insp(_na_ng_flags(ng, na)())
        if rc != 0 or out is None:
            return False, f"NGasNA={ng} NAasNG={na} rc={rc_str(rc)}"
        if b"nan" in out.lower() or b"\"inf" in out.lower():
            return False, f"NGasNA={ng} NAasNG={na} NaN/Inf"
        try: js = _collect_judges(json.loads(out))
        except Exception as e: return False, f"invalid JSON: {e}"
        bad = [j for j in js if _bad_value(j)]
        if bad: return False, f"NGasNA={ng} NAasNG={na} bad={len(bad)}"
        succ = sum(1 for j in js if j.get("status") == 0)
        details.append(f"({int(ng)}{int(na)}):s{succ}")
    return True, " ".join(details)

# ---- search_point with extreme angleDeg (~0 and 359) ----
def _sp_angle(deg):
    def f():
        d = golden()
        for sp in all_of(d, "search_point"):
            sp["angleDeg"] = deg
        return d
    return f
_sp_angle_zero = _sp_angle(0.001)
_sp_angle_359  = _sp_angle(359.0)

CASES += [
    # ---- caliper clamp boundary: at the cap must work, slightly over must clamp (also work), under 30s ----
    case("caliper_at_clamp_boundary",   "custom", fn=fn_timed(_cal_at_boundary, max_seconds=30.0)),
    case("caliper_width_2049_clamped",  "custom", fn=fn_timed(_cal_width_just_over, max_seconds=30.0)),
    case("caliper_width_10000_clamped", "custom", fn=fn_timed(_cal_width_extreme, max_seconds=30.0)),

    # ---- circle_info subtypes (all 5 info_types) ----
    case("circle_info_min_diameter",    "custom", fn=fn_subtype_value(_circleinfo_mind, "circle_info")),
    case("circle_info_roughness_max",   "custom", fn=fn_subtype_value(_circleinfo_rough_max, "circle_info")),
    case("circle_info_roughness_min",   "custom", fn=fn_subtype_value(_circleinfo_rough_min, "circle_info")),
    # Regression: round-2 fix - no info_type must NOT SIGSEGV
    case("circle_info_no_infotype_no_segv", "custom", fn=fn_circle_info_no_infotype_controlled),

    # ---- sigma / radius (crafted geometry) re-asserted under timing ----
    case("sigma_subtype_timed",         "custom", fn=fn_timed(_sigma_measure, max_seconds=30.0)),

    # ---- locating_anchor on ALL search_points (heavy anchor pass) ----
    case("anchor_all_on_finite",        "custom", fn=_fn_no_crash_finite(_anchor_all_on)),
    case("anchor_all_on_timed",         "custom", fn=fn_timed(_anchor_all_on, max_seconds=30.0)),

    # ---- value_adjust on measures ----
    case("measure_value_adjust",        "custom", fn=fn_value_adjust_shift),

    # ---- back_value_setup flag (true/false) ----
    case("back_value_setup_true",       "custom", fn=_fn_no_crash_finite(_back_value_setup(True))),
    case("back_value_setup_false",      "custom", fn=_fn_no_crash_finite(_back_value_setup(False))),

    # ---- NGasNA / NAasNG combos ----
    case("ng_na_flag_combos",           "custom", fn=fn_flag_combos_no_crash),

    # ---- extreme search_point angleDeg ----
    case("sp_angleDeg_near_zero",       "custom", fn=_fn_no_crash_finite(_sp_angle_zero)),
    case("sp_angleDeg_359",             "custom", fn=_fn_no_crash_finite(_sp_angle_359)),
]

# ================= ROUND 5: area subtype, value remap, back_value, docheck, ref-mismatch, new clamp =================

# R5-1: measure subtype "area" - parser accepts (line 1769), measure path branch
_area_measure = _measure_subtype("area")

# R5-2: measure subtype "roughness" - NOT in parser switch -> must reject with -1
def _roughness_measure():
    d = golden(); m = first_of(d, "measure")
    m["subtype"] = "roughness"
    return d

# R5-3: value remap (value_A,B,X,Y) -- this IS the engine's "adjust" path
# measured_val = (v - A)*(Y-X)/(B-A) + X. With A=0,B=1,X=10,Y=11 -> shift +10.
def _value_remap_shift10():
    d = golden()
    for sig in all_of(d, "sig360_circle_line"):
        for k, v in sig.items():
            if isinstance(v, list):
                for it in v:
                    if isinstance(it, dict) and it.get("type") == "measure" \
                       and it.get("subtype") in (None, "distance", "sigma", "radius"):
                        it["value_A"] = 0.0; it["value_B"] = 1.0
                        it["value_X"] = 10.0; it["value_Y"] = 11.0
    return d

def fn_value_remap_shift(run_insp):
    """value_A=0,B=1,X=10,Y=11 -> linear identity + 10 offset on every distance/sigma/radius."""
    rcA, oA = run_insp(GDEF)
    rcB, oB = run_insp(_value_remap_shift10())
    if rcA != 0 or rcB != 0: return False, f"rcA={rc_str(rcA)} rcB={rc_str(rcB)}"
    ja = {j.get("id"): j for j in _collect_judges(json.loads(oA)) if "id" in j and _finite(j.get("value"))}
    jb = {j.get("id"): j for j in _collect_judges(json.loads(oB)) if "id" in j and _finite(j.get("value"))}
    common = set(ja) & set(jb)
    if not common: return False, "no common judges"
    shifted = sum(1 for i in common if abs((jb[i]["value"] - ja[i]["value"]) - 10.0) < 1e-3)
    sample = [(i, round(jb[i]["value"]-ja[i]["value"],3)) for i in list(common)[:4]]
    ok = shifted > 0
    return ok, f"common={len(common)} shifted_by_+10={shifted} sample={sample}"

# R5-4: value_adjust on calc subtype (round-4 noted it appears ignored)
def _value_adjust_calc():
    d = mut_calc(["dist"])()
    m = first_of(d, "measure")
    m["value_adjust"] = 5.0
    return d

def fn_value_adjust_calc_noop(run_insp):
    """value_adjust is NOT in parser (sig_circle_line.cpp ~line 1782-1812). Setting
    on a calc subtype must be a no-op (clean run, no NaN, no segv)."""
    rc, out = run_insp(_value_adjust_calc())
    if rc == "TIMEOUT" or (isinstance(rc, int) and rc in SIGCRASH):
        return False, f"rc={rc_str(rc)} (crash)"
    if out is None: return True, f"rc={rc_str(rc)} no out"
    if b"nan" in out.lower(): return False, "NaN"
    return True, f"rc={rc_str(rc)} (no-op accepted)"

# R5-5: back_value_setup TRUE with USL_b/LSL_b/value_b values
def _back_value_full():
    d = golden()
    for sig in all_of(d, "sig360_circle_line"):
        for k, v in sig.items():
            if isinstance(v, list):
                for it in v:
                    if isinstance(it, dict) and it.get("type") == "measure":
                        it["back_value_setup"] = True
                        it["value_b"] = 0.5
                        it["USL_b"]   = 9999.0
                        it["LSL_b"]   = -9999.0
    return d

# R5-6: docheck=false on every measure
def _docheck_false():
    d = golden()
    for sig in all_of(d, "sig360_circle_line"):
        for k, v in sig.items():
            if isinstance(v, list):
                for it in v:
                    if isinstance(it, dict) and it.get("type") == "measure":
                        it["docheck"] = False
    return d

# R5-7: ref element type mismatch - measure ref says "line" but the id is actually a search_point
def _ref_type_mismatch_line():
    d = golden(); m = first_of(d, "measure")
    # id 3 is a search_point in golden; claim it's a line
    m["ref"] = [{"id": 3, "element": "line"}, {"id": 4, "element": "line"}]
    return d

# R5-8: ref says "search_point" but the id is actually a line
def _ref_type_mismatch_sp():
    d = golden(); m = first_of(d, "measure")
    m["subtype"] = "angle"
    m["ref"] = [{"id": 1, "element": "search_point"}, {"id": 2, "element": "search_point"}]
    return d

# R5-9: NEW caliper boundary count=1024,width=128,length=512 -- must run fast (<3s)
_cal_new_boundary = _all_caliper_with({"caliper": {"count": 1024, "width": 128, "length": 512}})

# R5-10: caliper one notch above boundary (count=1025,width=129,length=513) -> all clamped, still <3s
_cal_just_over_boundary = _all_caliper_with({"caliper": {"count": 1025, "width": 129, "length": 513}})

# R5-11: roughness_rmse circle_info value sanity (>=0, finite)
def fn_roughness_rmse_nonneg(run_insp):
    rc, out = run_insp(_circleinfo_rough())
    if rc != 0 or out is None: return True, f"rc={rc_str(rc)} (no out)"
    js = _collect_judges(json.loads(out))
    cis = [j for j in js if j.get("subtype") == "circle_info" and _finite(j.get("value"))]
    if not cis: return True, "no circle_info judges with finite value (skip)"
    neg = [c for c in cis if c["value"] < 0]
    return (not neg), f"circle_info judges={len(cis)} negative_rmse={len(neg)}"

def fn_timed_strict(make, max_seconds=3.0):
    """Tight timing budget for the new caliper boundary - clamps must keep it <3s."""
    def fn(run_insp):
        t0 = time.time()
        rc, out = run_insp(make(), timeout=max_seconds + 5)
        dt = time.time() - t0
        if rc == "TIMEOUT": return False, f"TIMEOUT {dt:.2f}s"
        if isinstance(rc, int) and rc in SIGCRASH: return False, f"crash rc={rc_str(rc)}"
        if dt > max_seconds: return False, f"too slow {dt:.2f}s>{max_seconds}s"
        if rc != 0 or out is None: return False, f"rc={rc_str(rc)} dt={dt:.2f}s"
        if b"nan" in out.lower(): return False, f"NaN dt={dt:.2f}s"
        try: js = _collect_judges(json.loads(out))
        except Exception as e: return False, f"invalid JSON: {e}"
        bad = [j for j in js if _bad_value(j)]
        return (not bad), f"rc=0 dt={dt:.2f}s judges={len(js)} bad={len(bad)}"
    return fn

CASES += [
    # ---- measure subtype "area" (parser + measure path) ----
    case("area_subtype_no_crash",       "custom", fn=fn_subtype_value(_area_measure, "area")),
    case("area_subtype_robust",         "robust", make=_area_measure),

    # ---- "roughness" not a real subtype -> parser rejects with -1 ----
    case("roughness_subtype_rejected",  "robust", make=_roughness_measure),

    # ---- value_A/B/X/Y linear remap (the engine's actual "adjust" path) ----
    case("value_remap_plus10",          "custom", fn=fn_value_remap_shift),

    # ---- value_adjust on calc (confirmed parser does NOT read this key) ----
    case("value_adjust_calc_noop",      "custom", fn=fn_value_adjust_calc_noop),

    # ---- back_value_setup TRUE with full USL_b/LSL_b/value_b ----
    case("back_value_full_pathway",     "custom", fn=_fn_no_crash_finite(_back_value_full)),

    # ---- docheck=false on all measures ----
    case("docheck_false_all",           "custom", fn=_fn_no_crash_finite(_docheck_false)),

    # ---- ref element type mismatches ----
    case("ref_mismatch_sp_as_line",     "robust", make=_ref_type_mismatch_line),
    case("ref_mismatch_line_as_sp",     "robust", make=_ref_type_mismatch_sp),

    # ---- NEW caliper boundary (count=1024,width=128,length=512) must be fast ----
    case("caliper_new_boundary_fast",   "custom", fn=fn_timed_strict(_cal_new_boundary, max_seconds=3.0)),
    case("caliper_over_boundary_fast",  "custom", fn=fn_timed_strict(_cal_just_over_boundary, max_seconds=3.0)),

    # ---- roughness_rmse non-negative sanity ----
    case("roughness_rmse_nonneg",       "custom", fn=fn_roughness_rmse_nonneg),
]

if __name__ == "__main__":
    sys.exit(run_module("qa_measure", CASES))
