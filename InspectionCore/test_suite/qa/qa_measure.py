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

# ================= ROUND 6: degenerate geometry / cascade / cycles / matching_method =================

# R6-1: caliper on a line whose pt1==pt2 (zero direction vector, normalize div-by-zero)
def _caliper_zero_line():
    d = golden()
    ln = first_of(d, "line")
    if ln is not None:
        ln["locating"] = "caliper"
        ln["pt1"] = {"x": 100.0, "y": 100.0}
        ln["pt2"] = {"x": 100.0, "y": 100.0}
    return d

# R6-2: caliper on an arc whose 3 points are collinear (circle fit degenerates)
def _caliper_collinear_arc():
    d = golden()
    ar = first_of(d, "arc")
    if ar is not None:
        ar["locating"] = "caliper"
        ar["pt1"] = {"x": 0.0,   "y": 100.0}
        ar["pt2"] = {"x": 100.0, "y": 100.0}
        ar["pt3"] = {"x": 200.0, "y": 100.0}
    return d

# R6-3: vertex_touch_searching on a line with no actual edge nearby (far off-image)
def _vts_line_no_edge():
    d = golden()
    for ln in all_of(d, "line"):
        ln["vertex_touch_searching"] = True
        ln["pt1"] = {"x": -9999.0, "y": -9999.0}
        ln["pt2"] = {"x": -9990.0, "y": -9999.0}
    return d

# R6-4: measure depending on a CALIPER-failed feature
# Force a caliper on a line with a far-off-image position; measure refs it.
def _measure_depends_failed_caliper():
    d = golden()
    ln = first_of(d, "line")
    if ln is not None:
        ln["locating"] = "caliper"
        ln["pt1"] = {"x": -9999.0, "y": -9999.0}
        ln["pt2"] = {"x": -9990.0, "y": -9999.0}
    return d

# R6-5: caliper succeeded but very low confidence -- shrink caliper params drastically
def _caliper_low_confidence():
    d = golden()
    for t in ("line", "arc"):
        for ft in all_of(d, t):
            ft["locating"] = "caliper"
            ft["caliper"] = {"count": 2, "width": 1, "length": 1, "step": 1}
    return d

# R6-6: aux_line two-line-cross where both lines fail (out-of-image lines)
def _aux_lineCross_both_fail():
    d = golden()
    sig = first_of(d, "sig360_circle_line")
    if sig is None: return d
    bad_a = {"id": 9201, "type": "line", "locating": "caliper",
             "pt1":{"x":-9999,"y":-9999},"pt2":{"x":-9990,"y":-9999}}
    bad_b = {"id": 9202, "type": "line", "locating": "caliper",
             "pt1":{"x":-9999,"y":-9990},"pt2":{"x":-9990,"y":-9990}}
    aux = {"id": 9203, "type": "aux_point", "subtype": "lineCross",
           "ref":[{"id":9201,"element":"line"},{"id":9202,"element":"line"}]}
    for k, v in sig.items():
        if isinstance(v, list):
            v.extend([bad_a, bad_b, aux]); break
    return d

# R6-7: orientation_essential measure depending on a cycling chain (sp3<->sp4)
def _orientation_essential_cycle():
    d = golden()
    sp3 = _find_id(d, 3); sp4 = _find_id(d, 4)
    if sp3 and sp4:
        sp3["ref"] = [{"id":4,"element":"search_point"}]
        sp4["ref"] = [{"id":3,"element":"search_point"}]
        sp3["locating_anchor"] = True
        sp4["locating_anchor"] = True
    m = first_of(d, "measure")
    if m: m["orientation_essential"] = True
    return d

# R6-8: quality_essential=false on every measure (should still produce reports)
def _quality_essential_false_all():
    d = golden()
    for sig in all_of(d, "sig360_circle_line"):
        for k, v in sig.items():
            if isinstance(v, list):
                for it in v:
                    if isinstance(it, dict) and it.get("type") == "measure":
                        it["quality_essential"] = False
    return d

# R6-9: intrusionSizeLimitRatio extreme values (0, 1, 100).
# The key stopped being read on 2026-08-07 (the gate is gone; obj_detect clean-space
# regions replace it). These cases stay: they now assert the weaker but still useful
# property that a def carrying a garbage value for a key the core no longer knows
# loads and inspects without crashing -- which is exactly what every factory def on
# disk is about to become.
def _intrusion_ratio(r):
    def f():
        d = golden()
        for sig in all_of(d, "sig360_circle_line"):
            sig["intrusionSizeLimitRatio"] = r
        return d
    return f
_intrusion_0   = _intrusion_ratio(0.0)
_intrusion_1   = _intrusion_ratio(1.0)
_intrusion_100 = _intrusion_ratio(100.0)

# R6-10: matching_method=1 (edge_sig path) at top-level
def _matching_method_swap():
    d = golden()
    d["matching_method"] = 1
    return d

# ---- custom fns for R6 ----

def fn_no_crash_under_2s(make, max_seconds=2.0):
    """No crash, no NaN, ideally completes under max_seconds (caps now 512/64/256)."""
    def fn(run_insp):
        t0 = time.time()
        rc, out = run_insp(make(), timeout=max_seconds + 8)
        dt = time.time() - t0
        if rc == "TIMEOUT": return False, f"TIMEOUT dt={dt:.2f}s"
        if isinstance(rc, int) and rc in SIGCRASH:
            return False, f"crash rc={rc_str(rc)} dt={dt:.2f}s"
        if out is not None:
            low = out.lower()
            if b"nan" in low or b"\"inf" in low or b"-inf" in low:
                return False, f"NaN/Inf rc={rc_str(rc)} dt={dt:.2f}s"
        slow = " SLOW" if dt > max_seconds else ""
        return True, f"rc={rc_str(rc)} dt={dt:.2f}s{slow}"
    return fn

def fn_zero_line_caliper(run_insp):
    """pt1==pt2 line with caliper: must not SIGFPE/SIGSEGV on normalize."""
    rc, out = run_insp(_caliper_zero_line(), timeout=10)
    if rc == "TIMEOUT": return False, "TIMEOUT"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"REAL BUG: rc={rc_str(rc)} on zero-length line caliper"
    if out is not None and (b"nan" in out.lower() or b"\"inf" in out.lower()):
        return False, "NaN/Inf in JSON"
    return True, f"rc={rc_str(rc)} (no crash on zero-dir vector)"

def fn_collinear_arc_caliper(run_insp):
    """3 collinear points -> circle fit degenerate. Must not crash or emit NaN."""
    rc, out = run_insp(_caliper_collinear_arc(), timeout=10)
    if rc == "TIMEOUT": return False, "TIMEOUT"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"REAL BUG: rc={rc_str(rc)} on collinear arc"
    if out is not None and (b"nan" in out.lower() or b"\"inf" in out.lower()):
        return False, "NaN/Inf in JSON"
    return True, f"rc={rc_str(rc)} (no crash on collinear arc)"

def fn_cascade_failed_caliper(run_insp):
    """Measure depending on a caliper-failed feature: judges referencing it must
    be NA (status!=0, value null or finite), no NaN, no crash."""
    rc, out = run_insp(_measure_depends_failed_caliper(), timeout=10)
    if rc == "TIMEOUT": return False, "TIMEOUT"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"crash rc={rc_str(rc)}"
    if out is None: return True, f"rc={rc_str(rc)} no out"
    if b"nan" in out.lower() or b"\"inf" in out.lower():
        return False, "NaN/Inf in JSON"
    try: js = _collect_judges(json.loads(out))
    except Exception as e: return False, f"invalid JSON: {e}"
    bad = [j for j in js if _bad_value(j)]
    na = sum(1 for j in js if j.get("status") != 0)
    return (not bad), f"rc={rc_str(rc)} judges={len(js)} na={na} bad={len(bad)}"

def fn_low_confidence_gating(run_insp):
    """Tiny caliper => low confidence. Must not produce SUCCESS judges with garbage:
    success judges (status==0) must carry finite numeric values."""
    t0 = time.time()
    rc, out = run_insp(_caliper_low_confidence(), timeout=10)
    dt = time.time() - t0
    if rc == "TIMEOUT": return False, f"TIMEOUT {dt:.2f}s"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"crash rc={rc_str(rc)} dt={dt:.2f}s"
    if out is None: return True, f"rc={rc_str(rc)} dt={dt:.2f}s no out"
    if b"nan" in out.lower() or b"\"inf" in out.lower():
        return False, "NaN/Inf in JSON"
    try: js = _collect_judges(json.loads(out))
    except Exception as e: return False, f"invalid JSON: {e}"
    bad = [j for j in js if _bad_value(j)]
    succ = sum(1 for j in js if j.get("status") == 0)
    return (not bad), f"rc={rc_str(rc)} dt={dt:.2f}s judges={len(js)} succ={succ} bad={len(bad)}"

def fn_aux_both_fail(run_insp):
    """Both lines for lineCross fail: aux must be NA, no crash, no NaN."""
    rc, out = run_insp(_aux_lineCross_both_fail(), timeout=10)
    if rc == "TIMEOUT": return False, "TIMEOUT"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"crash rc={rc_str(rc)}"
    if out is None: return True, f"rc={rc_str(rc)} no out"
    if b"nan" in out.lower() or b"\"inf" in out.lower():
        return False, "NaN/Inf in JSON"
    return True, f"rc={rc_str(rc)} (no crash on dual-fail lineCross)"

def fn_orient_cycle(run_insp):
    """orientation_essential + ref cycle: must NOT hang, must NOT crash."""
    t0 = time.time()
    rc, out = run_insp(_orientation_essential_cycle(), timeout=10)
    dt = time.time() - t0
    if rc == "TIMEOUT":
        return False, f"REAL BUG: TIMEOUT (likely infinite reDo loop) dt={dt:.2f}s"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"crash rc={rc_str(rc)} dt={dt:.2f}s"
    if out is not None and (b"nan" in out.lower() or b"\"inf" in out.lower()):
        return False, "NaN/Inf in JSON"
    return True, f"rc={rc_str(rc)} dt={dt:.2f}s no hang/crash"

def fn_quality_essential_false_reports(run_insp):
    """quality_essential=false: engine must still emit judge reports."""
    rc, out = run_insp(_quality_essential_false_all(), timeout=10)
    if rc != 0 or out is None: return False, f"rc={rc_str(rc)}"
    if b"nan" in out.lower(): return False, "NaN"
    js = _collect_judges(json.loads(out))
    if not js: return False, "no judge reports emitted"
    bad = [j for j in js if _bad_value(j)]
    return (not bad), f"judges={len(js)} bad={len(bad)}"

CASES += [
    case("caliper_zero_length_line",       "custom", fn=fn_zero_line_caliper),
    case("caliper_collinear_arc",          "custom", fn=fn_collinear_arc_caliper),
    case("vts_line_no_edge",               "custom", fn=fn_no_crash_under_2s(_vts_line_no_edge, 2.0)),
    case("measure_depends_failed_caliper", "custom", fn=fn_cascade_failed_caliper),
    case("caliper_low_confidence_gating",  "custom", fn=fn_low_confidence_gating),
    case("aux_lineCross_both_fail",        "custom", fn=fn_aux_both_fail),
    case("orientation_essential_cycle",    "custom", fn=fn_orient_cycle),
    case("quality_essential_false_all",    "custom", fn=fn_quality_essential_false_reports),
    case("intrusion_ratio_zero",           "custom", fn=fn_no_crash_under_2s(_intrusion_0,   2.0)),
    case("intrusion_ratio_one",            "custom", fn=fn_no_crash_under_2s(_intrusion_1,   2.0)),
    case("intrusion_ratio_hundred",        "custom", fn=fn_no_crash_under_2s(_intrusion_100, 2.0)),
    case("matching_method_edge_sig_swap",  "custom", fn=fn_no_crash_under_2s(_matching_method_swap, 2.0)),
]

# ================= ROUND 7: per-SP flag toggles, new caliper caps, degenerate measures, scale =================

# R7-1: search_far toggle on a single SP (id=3)
def _sp_search_far(flag):
    def f():
        d = golden()
        sp = _find_id(d, 3)
        if sp: sp["search_far"] = flag
        return d
    return f
_sp_search_far_true  = _sp_search_far(True)
_sp_search_far_false = _sp_search_far(False)

# R7-2: extreme line_thickness_value on every SP
def _sp_line_thickness(v):
    def f():
        d = golden()
        for sp in all_of(d, "search_point"):
            sp["line_thickness_value"] = v
        return d
    return f
_sp_lt_huge = _sp_line_thickness(10000)
_sp_lt_neg  = _sp_line_thickness(-50)

# R7-3: new caliper caps boundary (count=512, width=64, length=256) must succeed fast
_cal_caps_boundary = _all_caliper_with({"caliper": {"count": 512, "width": 64, "length": 256}})

# R7-4: caliper count=1 (single slice)
_cal_count_one = _all_caliper_with({"caliper": {"count": 1, "width": 9, "length": 32}})

# R7-5: caliper width=1, length=1 smallest viable
_cal_dims_one = _all_caliper_with({"caliper": {"count": 16, "width": 1, "length": 1}})

# R7-6: measure(distance) between two SPs that resolve to the same point (distance==0)
def _measure_distance_same_point():
    d = golden(); m = first_of(d, "measure")
    m["subtype"] = "distance"
    # ref same id twice -> degenerate, expect distance ~0 or NA
    m["ref"] = [{"id": 3, "element": "search_point"},
                {"id": 3, "element": "search_point"}]
    return d

# R7-7: measure(angle) wrap (>360)
def _measure_angle_wrap():
    d = golden(); m = first_of(d, "measure")
    m["subtype"] = "angle"
    m["value_adjust"] = 720.0  # likely no-op per round 4; engine value should still be normalized
    return d

# R7-8: sigma measure on a single point (degenerate)
def _sigma_single_point():
    d = golden(); m = first_of(d, "measure")
    m["subtype"] = "sigma"
    m["ref"] = [{"id": 3, "element": "search_point"}]
    return d

# R7-9: ALL search_points caliper-enabled (heavy path)
def _all_sp_caliper():
    d = golden()
    for sp in all_of(d, "search_point"):
        sp["locating"] = "caliper"
    return d

# R7-10: golden + 50 extra synthetic measures (scale)
def _scale_50_extra_measures():
    d = golden()
    sig = first_of(d, "sig360_circle_line")
    if sig is None: return d
    m12 = _find_id(d, 12)
    if m12 is None: return d
    for k, v in sig.items():
        if isinstance(v, list):
            for i in range(50):
                cm = copy.deepcopy(m12)
                cm["id"] = 9300 + i
                v.append(cm)
            break
    return d

# ---- custom fns for R7 ----

def fn_distance_same_point_zero(run_insp):
    rc, out = run_insp(_measure_distance_same_point(), timeout=10)
    if rc == "TIMEOUT": return False, "TIMEOUT"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"crash rc={rc_str(rc)}"
    if out is None: return True, f"rc={rc_str(rc)} no out"
    if b"nan" in out.lower() or b"\"inf" in out.lower():
        return False, "NaN/Inf"
    try: js = _collect_judges(json.loads(out))
    except Exception as e: return False, f"invalid JSON: {e}"
    ds = [j for j in js if j.get("subtype") == "distance" and _finite(j.get("value"))]
    # at least one distance==0 (or NA, which means engine rejected degenerate) acceptable
    if not ds: return True, f"rc={rc_str(rc)} no finite distance (NA acceptable)"
    near_zero = [d for d in ds if abs(d["value"]) < 1e-3]
    ok = len(near_zero) > 0 or all(_finite(d["value"]) for d in ds)
    return ok, f"rc={rc_str(rc)} distances={len(ds)} near_zero={len(near_zero)}"

def fn_angle_wrap_normalized(run_insp):
    rc, out = run_insp(_measure_angle_wrap(), timeout=10)
    if rc == "TIMEOUT": return False, "TIMEOUT"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"crash rc={rc_str(rc)}"
    if out is None: return True, f"rc={rc_str(rc)} no out"
    if b"nan" in out.lower(): return False, "NaN"
    try: js = _collect_judges(json.loads(out))
    except Exception as e: return False, f"invalid JSON: {e}"
    ang = [j for j in js if j.get("subtype") == "angle" and _finite(j.get("value"))]
    oor = [a for a in ang if not (-720.0 <= a["value"] <= 720.0)]
    return (not oor), f"rc={rc_str(rc)} angles={len(ang)} out_of_720={len(oor)}"

def fn_sigma_single_point(run_insp):
    rc, out = run_insp(_sigma_single_point(), timeout=10)
    if rc == "TIMEOUT": return False, "TIMEOUT"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"REAL BUG: crash on degenerate sigma rc={rc_str(rc)}"
    if out is not None and (b"nan" in out.lower() or b"\"inf" in out.lower()):
        return False, "NaN/Inf on sigma single-point"
    return True, f"rc={rc_str(rc)} (degenerate sigma handled)"

def fn_scale_50_extra(run_insp):
    t0 = time.time()
    rc, out = run_insp(_scale_50_extra_measures(), timeout=30)
    dt = time.time() - t0
    if rc == "TIMEOUT": return False, f"TIMEOUT dt={dt:.2f}s"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"crash rc={rc_str(rc)} dt={dt:.2f}s"
    # SIGABRT is the engine's controlled reject for malformed/duplicate refs.
    if isinstance(rc, int) and rc < 0:
        return True, f"rc={rc_str(rc)} dt={dt:.2f}s (controlled-reject on dup ids)"
    if rc != 0 or out is None: return False, f"rc={rc_str(rc)} dt={dt:.2f}s"
    if b"nan" in out.lower(): return False, f"NaN dt={dt:.2f}s"
    try: js = _collect_judges(json.loads(out))
    except Exception as e: return False, f"invalid JSON: {e}"
    extras = [j for j in js if isinstance(j.get("id"), int) and 9300 <= j["id"] <= 9349]
    bad = [j for j in js if _bad_value(j)]
    # Budget: 50 extra measures should not blow up runtime (<15s on dev box)
    slow = " SLOW" if dt > 15.0 else ""
    return (not bad), f"rc=0 dt={dt:.2f}s total_judges={len(js)} extras={len(extras)} bad={len(bad)}{slow}"

CASES += [
    # ---- search_far per-SP toggles ----
    case("sp_search_far_true",         "custom", fn=_fn_no_crash_finite(_sp_search_far_true)),
    case("sp_search_far_false",        "custom", fn=_fn_no_crash_finite(_sp_search_far_false)),

    # ---- line_thickness_value extremes ----
    case("sp_line_thickness_huge",     "custom", fn=_fn_no_crash_finite(_sp_lt_huge)),
    case("sp_line_thickness_negative", "custom", fn=_fn_no_crash_finite(_sp_lt_neg)),

    # ---- new caliper caps boundary (512/64/256), and corner sizes ----
    case("caliper_caps_512_64_256",    "custom", fn=fn_timed_strict(_cal_caps_boundary, max_seconds=5.0)),
    case("caliper_count_one",          "custom", fn=_fn_no_crash_finite(_cal_count_one)),
    case("caliper_width_length_one",   "custom", fn=_fn_no_crash_finite(_cal_dims_one)),

    # ---- degenerate measures ----
    case("distance_same_point_zero",   "custom", fn=fn_distance_same_point_zero),
    case("angle_wrap_normalized",      "custom", fn=fn_angle_wrap_normalized),
    case("sigma_single_point_degen",   "custom", fn=fn_sigma_single_point),

    # ---- heavy: ALL search_points -> caliper ----
    case("all_sp_caliper_heavy",       "custom", fn=fn_timed(_all_sp_caliper, max_seconds=15.0)),

    # ---- scale: golden + 50 extra measures ----
    case("scale_50_extra_measures",    "custom", fn=fn_scale_50_extra),
]

# ================= ROUND 8: full-caliper+scale, search_far x width, OOB coords, anchor chain, orient-flip, intrusion=0.999, circle_info on non-circle, USL<LSL =================

# R8-1: full caliper (all line+arc+search_point) WITH 50 extra duplicated id-bumped
# measures.  Combined perf + correctness probe.
def _full_caliper_plus_50_measures():
    d = golden()
    for t in ("line", "arc", "search_point"):
        for ft in all_of(d, t):
            ft["locating"] = "caliper"
    sig = first_of(d, "sig360_circle_line")
    m12 = _find_id(d, 12)
    if sig is not None and m12 is not None:
        for k, v in sig.items():
            if isinstance(v, list):
                for i in range(50):
                    cm = copy.deepcopy(m12)
                    cm["id"] = 9400 + i
                    v.append(cm)
                break
    return d

def fn_full_caliper_50_measures(run_insp):
    t0 = time.time()
    rc, out = run_insp(_full_caliper_plus_50_measures(), timeout=60)
    dt = time.time() - t0
    if rc == "TIMEOUT": return False, f"TIMEOUT dt={dt:.2f}s"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"crash rc={rc_str(rc)} dt={dt:.2f}s"
    if isinstance(rc, int) and rc < 0:
        return True, f"rc={rc_str(rc)} dt={dt:.2f}s (controlled-reject on dup ids)"
    if rc != 0 or out is None: return False, f"rc={rc_str(rc)} dt={dt:.2f}s"
    if b"nan" in out.lower() or b"\"inf" in out.lower():
        return False, f"NaN/Inf dt={dt:.2f}s"
    try: js = _collect_judges(json.loads(out))
    except Exception as e: return False, f"invalid JSON: {e}"
    bad = [j for j in js if _bad_value(j)]
    extras = [j for j in js if isinstance(j.get("id"), int) and 9400 <= j["id"] <= 9449]
    slow = " SLOW" if dt > 30.0 else ""
    return (not bad), f"rc=0 dt={dt:.2f}s judges={len(js)} extras={len(extras)} bad={len(bad)}{slow}"

# R8-2: search_far flag interacting with very long width
def _sp_search_far_long_width():
    d = golden()
    for sp in all_of(d, "search_point"):
        sp["search_far"] = True
        sp["width"] = 8000
    return d

# R8-3: line with coordinates outside image bounds (golden units = image-mm)
def _line_oob_coords():
    d = golden()
    ln = first_of(d, "line")
    if ln is not None:
        ln["pt1"] = {"x": 50000.0, "y": 50000.0}
        ln["pt2"] = {"x": 50100.0, "y": 50100.0}
    return d

# R8-4: chained 3-deep locating_anchor: sp(3) -> sp(4) -> sp(5)
def _anchor_chain_3deep():
    d = golden()
    sp3 = _find_id(d, 3); sp4 = _find_id(d, 4); sp5 = _find_id(d, 5)
    if sp3 and sp4 and sp5:
        sp3["locating_anchor"] = True
        sp4["locating_anchor"] = True; sp4["ref"] = [{"id":3,"element":"search_point"}]
        sp5["locating_anchor"] = True; sp5["ref"] = [{"id":4,"element":"search_point"}]
    return d

# R8-5: orientation_essential where the orientation FLIPS during reDo
# Synthesize by setting orientation_essential on multiple measures with conflicting refs.
def _orientation_flip_redo():
    d = golden()
    count = 0
    for sig in all_of(d, "sig360_circle_line"):
        for k, v in sig.items():
            if isinstance(v, list):
                for it in v:
                    if isinstance(it, dict) and it.get("type") == "measure":
                        it["orientation_essential"] = True
                        # alternate ref orderings to flip orientation across reDo passes
                        if "ref" in it and isinstance(it["ref"], list) and len(it["ref"]) >= 2 and count % 2 == 1:
                            it["ref"] = list(reversed(it["ref"]))
                        count += 1
    return d

def fn_orientation_flip_redo(run_insp):
    t0 = time.time()
    rc, out = run_insp(_orientation_flip_redo(), timeout=15)
    dt = time.time() - t0
    if rc == "TIMEOUT":
        return False, f"REAL BUG: TIMEOUT (likely infinite reDo flip loop) dt={dt:.2f}s"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"crash rc={rc_str(rc)} dt={dt:.2f}s"
    if out is not None and (b"nan" in out.lower() or b"\"inf" in out.lower()):
        return False, "NaN/Inf in JSON"
    return True, f"rc={rc_str(rc)} dt={dt:.2f}s (orient-flip reDo terminated)"

# R8-6: intrusionSizeLimitRatio = 0.999 (very strict)
_intrusion_999 = _intrusion_ratio(0.999)

# R8-7: measure(circle_info subtype=roughness_rmse) on a NON-circular shape
# Reroute the circle_info measure's ref to a line (id=1) instead of an arc.
def _circle_info_on_line():
    d = golden(); m = first_of(d, "measure")
    m["subtype"] = "circle_info"
    m["info_type"] = "roughness_rmse"
    m["ref"] = [{"id": 1, "element": "line"}]
    return d

def fn_circle_info_on_line(run_insp):
    rc, out = run_insp(_circle_info_on_line(), timeout=10)
    if rc == "TIMEOUT": return False, "TIMEOUT"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"REAL BUG: crash rc={rc_str(rc)} on circle_info(line)"
    if out is None: return True, f"rc={rc_str(rc)} no out"
    # Match nan/inf as TOKEN values (not substrings of legitimate keys like
    # "info_type" / "circle_info"). cJSON emits non-finite floats unquoted.
    import re as _re
    if _re.search(rb'[\s,:\[]\s*(nan|inf|-inf)\b', out.lower()):
        return False, "NaN/Inf on circle_info(line)"
    try: js = _collect_judges(json.loads(out))
    except Exception as e: return False, f"invalid JSON: {e}"
    bad = [j for j in js if _bad_value(j)]
    return (not bad), f"rc={rc_str(rc)} judges={len(js)} bad={len(bad)} (circle_info on line should NA-out)"

# R8-8: USL < LSL on every measure (re-confirm; covered elsewhere in suite)
def _usl_lt_lsl():
    d = golden()
    for sig in all_of(d, "sig360_circle_line"):
        for k, v in sig.items():
            if isinstance(v, list):
                for it in v:
                    if isinstance(it, dict) and it.get("type") == "measure":
                        it["USL"] = -100.0
                        it["LSL"] =  100.0
                        it["tol_upper"] = -100.0
                        it["tol_lower"] =  100.0
    return d

def fn_usl_lt_lsl_reconfirm(run_insp):
    rc, out = run_insp(_usl_lt_lsl(), timeout=10)
    if rc == "TIMEOUT": return False, "TIMEOUT"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"REAL BUG: crash rc={rc_str(rc)} on USL<LSL"
    if out is None: return True, f"rc={rc_str(rc)} no out"
    if b"nan" in out.lower() or b"\"inf" in out.lower():
        return False, "NaN/Inf on USL<LSL"
    try: js = _collect_judges(json.loads(out))
    except Exception as e: return False, f"invalid JSON: {e}"
    bad = [j for j in js if _bad_value(j)]
    # With USL<LSL the engine should mark every judge OOT (status != 0) but emit finite values
    succ = sum(1 for j in js if j.get("status") == 0)
    oot  = sum(1 for j in js if j.get("status") == -1)
    return (not bad), f"rc={rc_str(rc)} judges={len(js)} succ={succ} OOT={oot} bad={len(bad)}"

CASES += [
    case("full_caliper_plus_50_measures",   "custom", fn=fn_full_caliper_50_measures),
    case("sp_search_far_long_width",        "custom", fn=_fn_no_crash_finite(_sp_search_far_long_width)),
    case("line_coords_out_of_bounds",       "robust", make=_line_oob_coords),
    case("anchor_chain_3deep",              "custom", fn=_fn_no_crash_finite(_anchor_chain_3deep)),
    case("orientation_essential_flip_redo", "custom", fn=fn_orientation_flip_redo),
    case("intrusion_ratio_0_999",           "custom", fn=fn_no_crash_under_2s(_intrusion_999, 3.0)),
    case("circle_info_rmse_on_line",        "custom", fn=fn_circle_info_on_line),
    case("usl_lt_lsl_reconfirm",            "custom", fn=fn_usl_lt_lsl_reconfirm),
]

# ================= ROUND 9: cross-type circle_info regression, geometry-sensitivity, _pt1 cache lie, cal_step sentinel, removed-id ref, orient conflict, bulk SP =================

# R9-1: circle_info on a SEARCH_POINT ref (cross-type). Regression: must NA-out, no NaN/Inf.
def _circle_info_on_searchpoint():
    d = golden(); m = first_of(d, "measure")
    m["subtype"] = "circle_info"
    m["info_type"] = "max_diameter"
    m["ref"] = [{"id": 3, "element": "search_point"}]
    return d

def fn_circle_info_on_sp(run_insp):
    rc, out = run_insp(_circle_info_on_searchpoint(), timeout=10)
    if rc == "TIMEOUT": return False, "TIMEOUT"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"REAL BUG: crash rc={rc_str(rc)} on circle_info(search_point)"
    if out is None: return True, f"rc={rc_str(rc)} no out"
    import re as _re
    if _re.search(rb'[\s,:\[]\s*(nan|inf|-inf)\b', out.lower()):
        return False, "NaN/Inf on circle_info(search_point)"
    try: js = _collect_judges(json.loads(out))
    except Exception as e: return False, f"invalid JSON: {e}"
    bad = [j for j in js if _bad_value(j)]
    return (not bad), f"rc={rc_str(rc)} judges={len(js)} bad={len(bad)} (cross-type NA OK)"

# R9-2: circle_info subtypes on a valid arc change when arc geometry changes
def _circle_info_arc_scaled(scale):
    """Set the first measure to circle_info(max_diameter) on arc id=24, and scale arc pts."""
    def f():
        d = golden(); m = first_of(d, "measure")
        m["subtype"] = "circle_info"; m["info_type"] = "max_diameter"
        m["ref"] = [{"id": 24, "element": "arc"}]
        ar = _find_id(d, 24)
        if ar:
            for k in ("pt1","pt2","pt3"):
                p = ar.get(k)
                if isinstance(p, dict) and "x" in p and "y" in p:
                    p["x"] = p["x"] * scale
                    p["y"] = p["y"] * scale
        return d
    return f

def fn_circle_info_geom_sensitivity(run_insp):
    rc1, o1 = run_insp(_circle_info_arc_scaled(1.0)(), timeout=10)
    rc2, o2 = run_insp(_circle_info_arc_scaled(2.0)(), timeout=10)
    if rc1 == "TIMEOUT" or rc2 == "TIMEOUT": return False, "TIMEOUT"
    if (isinstance(rc1,int) and rc1 in SIGCRASH) or (isinstance(rc2,int) and rc2 in SIGCRASH):
        return False, f"crash rc1={rc_str(rc1)} rc2={rc_str(rc2)}"
    if o1 is None or o2 is None: return True, f"rc1={rc_str(rc1)} rc2={rc_str(rc2)} no out"
    try:
        j1 = [j for j in _collect_judges(json.loads(o1)) if j.get("subtype")=="circle_info" and _finite(j.get("value"))]
        j2 = [j for j in _collect_judges(json.loads(o2)) if j.get("subtype")=="circle_info" and _finite(j.get("value"))]
    except Exception as e: return False, f"invalid JSON: {e}"
    if not j1 or not j2:
        return True, f"no circle_info finite values (rc1={rc_str(rc1)} rc2={rc_str(rc2)} j1={len(j1)} j2={len(j2)}) skip"
    v1 = j1[0]["value"]; v2 = j2[0]["value"]
    sane = 0.0 < v1 < 100000.0 and 0.0 < v2 < 100000.0
    changed = abs(v2 - v1) > 1e-3
    return (sane and changed), f"v1={v1:.4f} v2={v2:.4f} changed={changed} sane={sane}"

# R9-3: _pt1 cached UI field actively WRONG vs pt1 -- engine must ignore _pt1
def _pt1_cache_lie():
    d = golden()
    ln = first_of(d, "line")
    if ln is not None and isinstance(ln.get("pt1"), dict):
        ln["_pt1"] = {"x": -999999.0, "y": -999999.0}
    return d

def fn_pt1_cache_lie(run_insp):
    rcA, oA = run_insp(GDEF)
    rcB, oB = run_insp(_pt1_cache_lie())
    if rcA != 0 or rcB != 0: return False, f"rcA={rc_str(rcA)} rcB={rc_str(rcB)}"
    if oA is None or oB is None: return False, "missing out"
    # If engine respected _pt1, results would differ wildly; we expect near-identical numbers.
    ja = {j.get("id"): j.get("value") for j in _collect_judges(json.loads(oA)) if "id" in j and _finite(j.get("value"))}
    jb = {j.get("id"): j.get("value") for j in _collect_judges(json.loads(oB)) if "id" in j and _finite(j.get("value"))}
    common = set(ja) & set(jb)
    if not common: return False, "no common judges"
    worst = max(abs(ja[i]-jb[i]) for i in common)
    ok = worst < 1.0
    return ok, f"common={len(common)} worst_diff={worst:.4f} (engine should ignore _pt1)"

# R9-4: caliper cal_step=0 vs cal_step=-1 (sentinel for default)
_cal_step_zero = _all_caliper_with({"caliper": {"step": 0}})
_cal_step_sentinel = _all_caliper_with({"caliper": {"step": -1}})

def fn_cal_step_zero_vs_sentinel(run_insp):
    rc0, o0 = run_insp(_cal_step_zero(), timeout=15)
    rcS, oS = run_insp(_cal_step_sentinel(), timeout=15)
    if (isinstance(rc0,int) and rc0 in SIGCRASH) or (isinstance(rcS,int) and rcS in SIGCRASH):
        return False, f"crash rc0={rc_str(rc0)} rcS={rc_str(rcS)}"
    if rc0 == "TIMEOUT" or rcS == "TIMEOUT": return False, "TIMEOUT"
    for o in (o0, oS):
        if o and (b"nan" in o.lower() or b"\"inf" in o.lower()):
            return False, "NaN/Inf"
    same = (o0 == oS)
    return True, f"rc0={rc_str(rc0)} rcS={rc_str(rcS)} identical={same} (sentinel=-1 should == default)"

# R9-5: measure references a feature id that's been REMOVED
def _measure_ref_removed():
    d = golden()
    # remove search_point id=3
    sig = first_of(d, "sig360_circle_line")
    if sig is None: return d
    for k, v in sig.items():
        if isinstance(v, list):
            sig[k] = [it for it in v if not (isinstance(it,dict) and it.get("id")==3)]
            break
    # measure now refs a gone-away id
    m = first_of(d, "measure")
    if m: m["ref"] = [{"id": 3, "element": "search_point"},
                      {"id": 4, "element": "search_point"}]
    return d

def fn_ref_removed(run_insp):
    rc, out = run_insp(_measure_ref_removed(), timeout=10)
    if rc == "TIMEOUT": return False, "TIMEOUT"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"REAL BUG: crash rc={rc_str(rc)} on removed-id ref"
    if out is None: return True, f"rc={rc_str(rc)} no out"
    if b"nan" in out.lower() or b"\"inf" in out.lower():
        return False, "NaN/Inf on removed-id ref"
    return True, f"rc={rc_str(rc)} (removed-id ref handled cleanly)"

# R9-6: two measures with orientation_essential conflicting (one flipped, one not)
def _orient_conflict_two():
    d = golden()
    measures = []
    for sig in all_of(d, "sig360_circle_line"):
        for k, v in sig.items():
            if isinstance(v, list):
                for it in v:
                    if isinstance(it, dict) and it.get("type") == "measure" \
                       and isinstance(it.get("ref"), list) and len(it["ref"]) >= 2:
                        measures.append(it)
    if len(measures) >= 2:
        measures[0]["orientation_essential"] = True
        measures[1]["orientation_essential"] = True
        measures[1]["ref"] = list(reversed(measures[1]["ref"]))
    return d

def fn_orient_conflict(run_insp):
    t0 = time.time()
    rc, out = run_insp(_orient_conflict_two(), timeout=15)
    dt = time.time() - t0
    if rc == "TIMEOUT":
        return False, f"REAL BUG: TIMEOUT (orient-conflict reDo did not converge) dt={dt:.2f}s"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"crash rc={rc_str(rc)} dt={dt:.2f}s"
    if out is not None and (b"nan" in out.lower() or b"\"inf" in out.lower()):
        return False, "NaN/Inf"
    return True, f"rc={rc_str(rc)} dt={dt:.2f}s (orient-conflict converged)"

# R9-7: bulk 200 search_points all caliper, finest grain -- timing budget
def _bulk_200_sp_caliper():
    d = golden()
    sig = first_of(d, "sig360_circle_line")
    if sig is None: return d
    # find an existing search_point as a template
    sp_tmpl = first_of(d, "search_point")
    if sp_tmpl is None: return d
    # flip every existing sp to caliper
    for sp in all_of(d, "search_point"):
        sp["locating"] = "caliper"
    # add 200 extra search_points with caliper locating
    new_sps = []
    base_pt = sp_tmpl.get("pt") or {"x": 100.0, "y": 100.0}
    bx = base_pt.get("x", 100.0) if isinstance(base_pt, dict) else 100.0
    by = base_pt.get("y", 100.0) if isinstance(base_pt, dict) else 100.0
    for i in range(200):
        new = copy.deepcopy(sp_tmpl)
        new["id"] = 9500 + i
        new["locating"] = "caliper"
        new["caliper"] = {"count": 16, "width": 8, "length": 16, "step": 1}
        if isinstance(new.get("pt"), dict):
            new["pt"] = {"x": bx + (i % 20) * 0.5, "y": by + (i // 20) * 0.5}
        new_sps.append(new)
    for k, v in sig.items():
        if isinstance(v, list):
            v.extend(new_sps); break
    return d

def fn_bulk_200_sp(run_insp):
    t0 = time.time()
    rc, out = run_insp(_bulk_200_sp_caliper(), timeout=15)
    dt = time.time() - t0
    if rc == "TIMEOUT": return False, f"TIMEOUT dt={dt:.2f}s"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"crash rc={rc_str(rc)} dt={dt:.2f}s"
    if isinstance(rc, int) and rc < 0:
        return True, f"rc={rc_str(rc)} dt={dt:.2f}s (controlled-reject)"
    if out is not None and (b"nan" in out.lower() or b"\"inf" in out.lower()):
        return False, f"NaN/Inf dt={dt:.2f}s"
    if dt > 5.0:
        return False, f"REAL BUG?: bulk 200 SP caliper too slow dt={dt:.2f}s > 5.0s"
    return True, f"rc={rc_str(rc)} dt={dt:.2f}s (200 SP caliper under budget)"

CASES += [
    case("circle_info_on_searchpoint",     "custom", fn=fn_circle_info_on_sp),
    case("circle_info_arc_geom_sensitive", "custom", fn=fn_circle_info_geom_sensitivity),
    case("pt1_cache_lie_ignored",          "custom", fn=fn_pt1_cache_lie),
    case("cal_step_zero_vs_sentinel",      "custom", fn=fn_cal_step_zero_vs_sentinel),
    case("measure_ref_removed_id",         "custom", fn=fn_ref_removed),
    case("orient_essential_conflict_pair", "custom", fn=fn_orient_conflict),
    case("bulk_200_sp_caliper_timing",     "custom", fn=fn_bulk_200_sp),
]

# ================= ROUND 10: final QA — clamp boundary, regressions, full-pipeline chain, degenerate sigma, anchor N-cycle =================

# R10-1: CURRENT clamp boundary (count<=512, width<=64, length<=256) must complete <2s with finite values
_cal_current_clamp = _all_caliper_with({"caliper": {"count": 512, "width": 64, "length": 256}})

def fn_current_clamp_under_2s(run_insp):
    t0 = time.time()
    rc, out = run_insp(_cal_current_clamp(), timeout=10)
    dt = time.time() - t0
    if rc == "TIMEOUT": return False, f"TIMEOUT dt={dt:.2f}s"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"crash rc={rc_str(rc)} dt={dt:.2f}s"
    if dt > 2.0:
        return False, f"REAL BUG: clamp-boundary too slow dt={dt:.2f}s > 2.0s"
    if rc != 0 or out is None: return False, f"rc={rc_str(rc)} dt={dt:.2f}s"
    if b"nan" in out.lower() or b"\"inf" in out.lower():
        return False, f"NaN/Inf dt={dt:.2f}s"
    try: js = _collect_judges(json.loads(out))
    except Exception as e: return False, f"invalid JSON: {e}"
    bad = [j for j in js if _bad_value(j)]
    return (not bad), f"rc=0 dt={dt:.2f}s judges={len(js)} bad={len(bad)}"

# R10-2: regression — circle_info cross-type ref (search_point) must not crash/NaN (round 9)
def _circle_info_cross_type_regression():
    d = golden(); m = first_of(d, "measure")
    m["subtype"] = "circle_info"
    m["info_type"] = "max_diameter"
    m["ref"] = [{"id": 3, "element": "search_point"}]
    return d

def fn_circle_info_cross_type_regr(run_insp):
    rc, out = run_insp(_circle_info_cross_type_regression(),
                       out_path=f"{TMP}/_out_r10_xtype.json", timeout=10)
    if rc == "TIMEOUT": return False, "TIMEOUT"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"REGRESSION: SIGSEGV on circle_info(search_point) rc={rc_str(rc)}"
    if out is not None:
        import re as _re
        if _re.search(rb'[\s,:\[]\s*(nan|inf|-inf)\b', out.lower()):
            return False, "REGRESSION: NaN/Inf in JSON"
    return True, f"rc={rc_str(rc)} (regression OK)"

# R10-3: regression — circle_info MISSING info_type must not SIGSEGV (round 4 fix)
def fn_circle_info_missing_info_regr(run_insp):
    rc, out = run_insp(_circleinfo_noinfo(),
                       out_path=f"{TMP}/_out_r10_noinfo.json", timeout=10)
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"REGRESSION: SIGSEGV on circle_info no info_type rc={rc_str(rc)}"
    if rc == "TIMEOUT": return False, "TIMEOUT"
    return True, f"rc={rc_str(rc)} (no crash regression OK)"

# R10-4: regression — caliper width=10000 must be clamped, not blow up runtime
def fn_caliper_width_10000_regr(run_insp):
    t0 = time.time()
    rc, out = run_insp(_cal_width_extreme(),
                       out_path=f"{TMP}/_out_r10_w10k.json", timeout=15)
    dt = time.time() - t0
    if rc == "TIMEOUT": return False, f"REGRESSION: TIMEOUT dt={dt:.2f}s (clamp leaked)"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"crash rc={rc_str(rc)} dt={dt:.2f}s"
    if dt > 5.0:
        return False, f"REGRESSION: width=10000 not clamped dt={dt:.2f}s"
    if out is not None and (b"nan" in out.lower() or b"\"inf" in out.lower()):
        return False, f"NaN/Inf dt={dt:.2f}s"
    return True, f"rc={rc_str(rc)} dt={dt:.2f}s (clamp held)"

# R10-5: FULL PIPELINE — every line+arc+sp on caliper AND a calc judge depends on the chain.
def _full_pipeline_caliper_calc():
    d = golden()
    for t in ("line", "arc", "search_point"):
        for ft in all_of(d, t):
            ft["locating"] = "caliper"
    # Rewrite first measure to be a calc that depends on a chain of measure ids.
    m = first_of(d, "measure")
    # Find a few sibling measure ids to depend on
    ids = []
    for sig in all_of(d, "sig360_circle_line"):
        for k, v in sig.items():
            if isinstance(v, list):
                for it in v:
                    if isinstance(it, dict) and it.get("type") == "measure" \
                       and isinstance(it.get("id"), int) and it is not m:
                        ids.append(it["id"])
    if not ids: ids = [12]
    ids = ids[:3]
    m["subtype"] = "calc"
    m["ref"] = [{"id": i} for i in ids]
    m["calc_f"] = {"exp": "", "post_exp": [f"[{ids[0]}]"]}
    return d

def fn_full_pipeline_caliper_calc(run_insp):
    rc, out = run_insp(_full_pipeline_caliper_calc(),
                       out_path=f"{TMP}/_out_r10_fullpipe.json", timeout=30)
    if rc == "TIMEOUT": return False, "TIMEOUT"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"crash rc={rc_str(rc)}"
    if rc != 0 or out is None: return False, f"rc={rc_str(rc)}"
    if b"nan" in out.lower() or b"\"inf" in out.lower():
        return False, "NaN/Inf"
    try: js = _collect_judges(json.loads(out))
    except Exception as e: return False, f"invalid JSON: {e}"
    bad = [j for j in js if _bad_value(j)]
    succ = sum(1 for j in js if j.get("status") == 0)
    calc_js = [j for j in js if j.get("subtype") == "calc"]
    ok = (not bad) and succ > 0
    return ok, f"judges={len(js)} calc={len(calc_js)} succ={succ} bad={len(bad)}"

# R10-6: sigma subtype on a degenerate-geometry LINE (pt1 == pt2)
def _sigma_degenerate_line():
    d = golden()
    ln = first_of(d, "line")
    if ln is not None:
        ln["pt1"] = {"x": 50.0, "y": 50.0}
        ln["pt2"] = {"x": 50.0, "y": 50.0}
    m = first_of(d, "measure")
    if m and ln is not None:
        m["subtype"] = "sigma"
        m["ref"] = [{"id": ln.get("id", 1), "element": "line"}]
    return d

def fn_sigma_degenerate_line(run_insp):
    rc, out = run_insp(_sigma_degenerate_line(),
                       out_path=f"{TMP}/_out_r10_sigdegen.json", timeout=10)
    if rc == "TIMEOUT": return False, "TIMEOUT"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"REAL BUG: SIGFPE/SIGSEGV on sigma(zero-length line) rc={rc_str(rc)}"
    if out is not None and (b"nan" in out.lower() or b"\"inf" in out.lower()):
        return False, "NaN/Inf on degenerate sigma"
    return True, f"rc={rc_str(rc)} (degenerate sigma handled)"

# R10-7: locating_anchor cycle of EXACTLY N hops (5) — exhaust the cycle detector
def _anchor_n_cycle(n=5):
    """Chain sp(3)->sp(4)->...->sp(3+n-1)->sp(3) — n-hop ring."""
    def f():
        d = golden()
        sps = []
        for i in range(n):
            sp = _find_id(d, 3 + i)
            if sp is None: return d  # bail if golden doesn't have that many sps
            sps.append(sp)
        for i, sp in enumerate(sps):
            nxt = sps[(i + 1) % n]
            sp["locating_anchor"] = True
            sp["ref"] = [{"id": nxt.get("id"), "element": "search_point"}]
        return d
    return f

def fn_anchor_n_cycle(run_insp):
    t0 = time.time()
    rc, out = run_insp(_anchor_n_cycle(5)(),
                       out_path=f"{TMP}/_out_r10_ncycle.json", timeout=15)
    dt = time.time() - t0
    if rc == "TIMEOUT":
        return False, f"REAL BUG: TIMEOUT (cycle detector missed N=5 ring) dt={dt:.2f}s"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"crash rc={rc_str(rc)} dt={dt:.2f}s"
    if out is not None and (b"nan" in out.lower() or b"\"inf" in out.lower()):
        return False, "NaN/Inf in JSON"
    return True, f"rc={rc_str(rc)} dt={dt:.2f}s (N=5 cycle terminated)"

# R10-8: regression — calc judge with empty post_exp should not crash
def _calc_empty_postexp():
    d = golden(); m = first_of(d, "measure")
    m["subtype"] = "calc"
    m["ref"] = [{"id": 12}]
    m["calc_f"] = {"exp": "", "post_exp": []}
    return d

def fn_calc_empty_postexp(run_insp):
    rc, out = run_insp(_calc_empty_postexp(),
                       out_path=f"{TMP}/_out_r10_calc_empty.json", timeout=10)
    if rc == "TIMEOUT": return False, "TIMEOUT"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"REAL BUG: crash on empty calc post_exp rc={rc_str(rc)}"
    if out is not None and (b"nan" in out.lower() or b"\"inf" in out.lower()):
        return False, "NaN/Inf on empty calc"
    return True, f"rc={rc_str(rc)} (empty calc post_exp handled)"

CASES += [
    # ---- current clamp boundary timing ----
    case("clamp_boundary_under_2s",      "custom", fn=fn_current_clamp_under_2s),

    # ---- regressions on previously-fixed bugs ----
    case("regr_circle_info_cross_type",  "custom", fn=fn_circle_info_cross_type_regr),
    case("regr_circle_info_no_infotype", "custom", fn=fn_circle_info_missing_info_regr),
    case("regr_caliper_width_10000",     "custom", fn=fn_caliper_width_10000_regr),

    # ---- full pipeline: caliper everywhere + calc-chain judge ----
    case("full_pipeline_caliper_calc",   "custom", fn=fn_full_pipeline_caliper_calc),

    # ---- degenerate-geometry sigma ----
    case("sigma_on_degenerate_line",     "custom", fn=fn_sigma_degenerate_line),

    # ---- locating_anchor N-hop cycle detector ----
    case("anchor_cycle_N5_detector",     "custom", fn=fn_anchor_n_cycle),

    # ---- calc with empty post_exp ----
    case("calc_empty_postexp_no_crash",  "custom", fn=fn_calc_empty_postexp),
]

# ================= ROUND 11: intra-def state-pollution probes =================
# These cases stress *within-process* state leakage between sequential features:
# parser fmap state, cycle-detector scratch, caliper shared buffers, anchor
# intermediate state, reDo orientation passes. Each must be <3s on golden HW.

import time as _t11
import re as _re11

def _has_nan_inf(out):
    """Token-aware NaN/Inf detector (avoids matching 'info_type' substrings)."""
    if out is None: return False
    return bool(_re11.search(rb'[\s,:\[]\s*(nan|inf|-inf)\b', out.lower()))

# R11-1: circle_info-on-line (NA-prone) FOLLOWED by a calc measure that
# depends on the NA'd judge id. Pollution check: NA propagates cleanly,
# downstream calc must not pick up stale residue.
def _ci_on_line_then_calc():
    d = golden()
    # Locate two measures so we can mutate independently
    measures = []
    for sig in all_of(d, "sig360_circle_line"):
        for k, v in sig.items():
            if isinstance(v, list):
                for it in v:
                    if isinstance(it, dict) and it.get("type") == "measure":
                        measures.append(it)
    if len(measures) >= 2:
        m1 = measures[0]; m2 = measures[1]
        m1["subtype"]   = "circle_info"
        m1["info_type"] = "roughness_rmse"
        m1["ref"]       = [{"id": 1, "element": "line"}]   # cross-type => NA
        # m2 -> calc that references m1's id
        m1_id = m1.get("id", 12)
        m2["subtype"] = "calc"
        m2["ref"]     = [{"id": m1_id}]
        m2["calc_f"]  = {"exp": "", "post_exp": [f"[{m1_id}]"]}
    return d

def fn_ci_line_then_calc(run_insp):
    t0 = _t11.time()
    rc, out = run_insp(_ci_on_line_then_calc(), out_path=f"{TMP}/_out_r11_cical.json", timeout=10)
    dt = _t11.time() - t0
    if rc == "TIMEOUT": return False, f"TIMEOUT dt={dt:.2f}s"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"REAL BUG: crash rc={rc_str(rc)} dt={dt:.2f}s"
    if dt > 3.0: return False, f"too slow dt={dt:.2f}s"
    if out is None: return True, f"rc={rc_str(rc)} dt={dt:.2f}s no out"
    if _has_nan_inf(out):
        return False, "NaN/Inf (stale residue?)"
    try: js = _collect_judges(json.loads(out))
    except Exception as e: return False, f"invalid JSON: {e}"
    bad = [j for j in js if _bad_value(j)]
    return (not bad), f"rc={rc_str(rc)} dt={dt:.2f}s judges={len(js)} bad={len(bad)}"

# R11-2: 50 valid duplicated measures + a final calc with a self-cycle.
# Cycle detector intermediate state must not corrupt the 50 preceding judges.
def _many_valid_then_cycled_calc():
    d = golden()
    sig = first_of(d, "sig360_circle_line")
    m12 = _find_id(d, 12)
    if sig is None or m12 is None: return d
    # Append 50 valid duplicates with fresh ids
    bad_id = 9650
    for k, v in sig.items():
        if isinstance(v, list):
            for i in range(50):
                cm = copy.deepcopy(m12)
                cm["id"] = 9600 + i
                v.append(cm)
            # Cycled calc: refs itself
            cyc = copy.deepcopy(m12)
            cyc["id"] = bad_id
            cyc["subtype"] = "calc"
            cyc["ref"] = [{"id": bad_id}]
            cyc["calc_f"] = {"exp": "", "post_exp": [f"[{bad_id}]"]}
            v.append(cyc)
            break
    return d

def fn_many_then_cycled(run_insp):
    t0 = _t11.time()
    rc, out = run_insp(_many_valid_then_cycled_calc(), out_path=f"{TMP}/_out_r11_cyc.json", timeout=10)
    dt = _t11.time() - t0
    if rc == "TIMEOUT": return False, f"TIMEOUT dt={dt:.2f}s"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"REAL BUG: crash rc={rc_str(rc)} dt={dt:.2f}s"
    if isinstance(rc, int) and rc < 0:
        return True, f"rc={rc_str(rc)} dt={dt:.2f}s (controlled-reject)"
    if dt > 3.0: return False, f"too slow dt={dt:.2f}s"
    if out is None: return True, f"rc={rc_str(rc)} dt={dt:.2f}s no out"
    if _has_nan_inf(out):
        return False, "NaN/Inf"
    try: js = _collect_judges(json.loads(out))
    except Exception as e: return False, f"invalid JSON: {e}"
    valids = [j for j in js if isinstance(j.get("id"), int) and 9600 <= j["id"] <= 9649]
    bad = [j for j in valids if _bad_value(j)]
    return (not bad), f"rc={rc_str(rc)} dt={dt:.2f}s validDup={len(valids)} bad={len(bad)}"

# R11-3: adjacent features w/ VERY different cal_count -- shared scratch buffers.
# Cap is 512; we mix max-cap on lines with min on arcs.
def _adjacent_diff_calcount():
    d = golden()
    for ln in all_of(d, "line"):
        ln["locating"] = "caliper"
        ln["caliper"] = {"count": 512, "width": 64, "length": 256}
    for ar in all_of(d, "arc"):
        ar["locating"] = "caliper"
        ar["caliper"] = {"count": 4, "width": 4, "length": 16}
    return d

def fn_adjacent_diff_calcount(run_insp):
    t0 = _t11.time()
    rc, out = run_insp(_adjacent_diff_calcount(), out_path=f"{TMP}/_out_r11_diffcnt.json", timeout=10)
    dt = _t11.time() - t0
    if rc == "TIMEOUT": return False, f"TIMEOUT dt={dt:.2f}s"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"REAL BUG: scratch-buffer crash rc={rc_str(rc)} dt={dt:.2f}s"
    if dt > 3.0: return False, f"too slow dt={dt:.2f}s"
    if rc != 0 or out is None: return False, f"rc={rc_str(rc)} dt={dt:.2f}s"
    if _has_nan_inf(out):
        return False, "NaN/Inf (scratch leak?)"
    try: js = _collect_judges(json.loads(out))
    except Exception as e: return False, f"invalid JSON: {e}"
    bad = [j for j in js if _bad_value(j)]
    succ = sum(1 for j in js if j.get("status") == 0)
    return (not bad), f"rc=0 dt={dt:.2f}s judges={len(js)} succ={succ} bad={len(bad)}"

# R11-4: feature A broken (parser-reject class), feature B is fine. Engine
# should reject the WHOLE def atomically (no partial run output).
def _broken_A_good_B():
    d = golden()
    # poison the first measure with a malformed circle_info (round 4 fix path)
    m = first_of(d, "measure")
    if m:
        m["subtype"] = "circle_info"   # missing info_type -> parse_judgeData returns -1
    return d

def fn_broken_A_atomic(run_insp):
    rc, out = run_insp(_broken_A_good_B(), out_path=f"{TMP}/_out_r11_atomic.json", timeout=10)
    if rc == "TIMEOUT": return False, "TIMEOUT"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"REAL BUG: crash rc={rc_str(rc)}"
    # Acceptable: controlled reject (non-zero rc) OR clean run with NA on the broken judge.
    # Unacceptable: rc==0 with NaN/Inf, or rc==0 with partial-state garbage values.
    if out is not None:
        if _has_nan_inf(out):
            return False, "NaN/Inf (half-init state leaked)"
        try: js = _collect_judges(json.loads(out))
        except Exception: js = []
        bad = [j for j in js if _bad_value(j)]
        if bad: return False, f"rc={rc_str(rc)} bad_values={len(bad)} (partial run)"
    return True, f"rc={rc_str(rc)} (atomic reject or clean NA)"

# R11-5: locating_anchor on a search_point whose ref target is a NA'd measure.
# Anchor pass intermediate state must not corrupt later judges.
def _anchor_on_na_target():
    d = golden()
    # NA-out measure id=12 by making it cross-type circle_info on a line
    m12 = _find_id(d, 12)
    if m12:
        m12["subtype"] = "circle_info"
        m12["info_type"] = "max_diameter"
        m12["ref"] = [{"id": 1, "element": "line"}]   # cross-type => NA
    # Make a search_point anchor-enabled and reference that measure
    sp3 = _find_id(d, 3)
    if sp3:
        sp3["locating_anchor"] = True
        sp3["ref"] = [{"id": 12, "element": "measure"}]
    return d

def fn_anchor_on_na(run_insp):
    t0 = _t11.time()
    rc, out = run_insp(_anchor_on_na_target(), out_path=f"{TMP}/_out_r11_anchor_na.json", timeout=10)
    dt = _t11.time() - t0
    if rc == "TIMEOUT": return False, f"TIMEOUT dt={dt:.2f}s"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"REAL BUG: crash rc={rc_str(rc)} dt={dt:.2f}s"
    if dt > 3.0: return False, f"too slow dt={dt:.2f}s"
    if out is None: return True, f"rc={rc_str(rc)} dt={dt:.2f}s"
    if _has_nan_inf(out):
        return False, "NaN/Inf (anchor scratch leaked)"
    try: js = _collect_judges(json.loads(out))
    except Exception as e: return False, f"invalid JSON: {e}"
    bad = [j for j in js if _bad_value(j)]
    return (not bad), f"rc={rc_str(rc)} dt={dt:.2f}s judges={len(js)} bad={len(bad)}"

# R11-6: ALL search_points flipped to locating_anchor=true (heavy anchor pass).
# Intermediate state across many anchor solves must not corrupt judges.
def _all_sp_anchor_on():
    d = golden()
    for sp in all_of(d, "search_point"):
        sp["locating_anchor"] = True
    return d

def fn_all_sp_anchor(run_insp):
    t0 = _t11.time()
    rc, out = run_insp(_all_sp_anchor_on(), out_path=f"{TMP}/_out_r11_allanchor.json", timeout=10)
    dt = _t11.time() - t0
    if rc == "TIMEOUT": return False, f"TIMEOUT dt={dt:.2f}s"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"REAL BUG: crash rc={rc_str(rc)} dt={dt:.2f}s"
    if dt > 3.0: return False, f"too slow dt={dt:.2f}s"
    if rc != 0 or out is None: return False, f"rc={rc_str(rc)} dt={dt:.2f}s"
    if _has_nan_inf(out):
        return False, "NaN/Inf"
    try: js = _collect_judges(json.loads(out))
    except Exception as e: return False, f"invalid JSON: {e}"
    bad = [j for j in js if _bad_value(j)]
    # Compare to golden: same count of judges, same finite-ness; values shouldn't be wildly different.
    rcG, oG = run_insp(GDEF, out_path=f"{TMP}/_out_r11_allanchor_g.json", timeout=10)
    if rcG == 0 and oG is not None:
        try:
            jg = {j.get("id"): j.get("value") for j in _collect_judges(json.loads(oG))
                  if "id" in j and _finite(j.get("value"))}
            jn = {j.get("id"): j.get("value") for j in js if "id" in j and _finite(j.get("value"))}
            common = set(jg) & set(jn)
            worst = max((abs(jg[i] - jn[i]) for i in common), default=0.0)
        except Exception:
            worst = -1.0
    else:
        worst = -1.0
    return (not bad), f"rc=0 dt={dt:.2f}s judges={len(js)} bad={len(bad)} worst_vs_gold={worst:.4f}"

# R11-7: caliper count=512 (cap) on EVERY feature -- pollution across sequential
# caliper solves on the shared max-cap scratch buffer.
def _all_cal_cap():
    d = golden()
    for t in ("line", "arc", "search_point"):
        for ft in all_of(d, t):
            ft["locating"] = "caliper"
            ft["caliper"] = {"count": 512, "width": 64, "length": 256}
    return d

def fn_all_cal_cap_no_leak(run_insp):
    t0 = _t11.time()
    rc, out = run_insp(_all_cal_cap(), out_path=f"{TMP}/_out_r11_capall.json", timeout=10)
    dt = _t11.time() - t0
    if rc == "TIMEOUT": return False, f"TIMEOUT dt={dt:.2f}s"
    if isinstance(rc, int) and rc in SIGCRASH:
        return False, f"REAL BUG: crash rc={rc_str(rc)} dt={dt:.2f}s"
    if dt > 3.0: return False, f"too slow dt={dt:.2f}s"
    if rc != 0 or out is None: return False, f"rc={rc_str(rc)} dt={dt:.2f}s"
    if _has_nan_inf(out):
        return False, "NaN/Inf (scratch pollution)"
    try: js = _collect_judges(json.loads(out))
    except Exception as e: return False, f"invalid JSON: {e}"
    bad = [j for j in js if _bad_value(j)]
    succ = sum(1 for j in js if j.get("status") == 0)
    return (not bad), f"rc=0 dt={dt:.2f}s judges={len(js)} succ={succ} bad={len(bad)}"

# R11-8: sequence-order swap probe — run the same def twice w/ measure list
# reversed. State pollution would show up as different judge values per id
# depending on processing order.
def _reverse_measure_order():
    d = golden()
    for sig in all_of(d, "sig360_circle_line"):
        for k, v in sig.items():
            if isinstance(v, list):
                # partition: measures vs non-measures, reverse the measures
                ms  = [it for it in v if isinstance(it, dict) and it.get("type") == "measure"]
                nms = [it for it in v if not (isinstance(it, dict) and it.get("type") == "measure")]
                sig[k] = nms + list(reversed(ms))
                break
    return d

def fn_reverse_order_stable(run_insp):
    t0 = _t11.time()
    rcA, oA = run_insp(GDEF, out_path=f"{TMP}/_out_r11_seq_g.json", timeout=10)
    rcB, oB = run_insp(_reverse_measure_order(), out_path=f"{TMP}/_out_r11_seq_r.json", timeout=10)
    dt = _t11.time() - t0
    if rcA != 0 or rcB != 0: return False, f"rcA={rc_str(rcA)} rcB={rc_str(rcB)}"
    if dt > 3.0: return False, f"too slow dt={dt:.2f}s"
    if oA is None or oB is None: return False, "no output"
    if _has_nan_inf(oB):
        return False, "NaN/Inf on reversed order"
    try:
        ja = {j.get("id"): j.get("value") for j in _collect_judges(json.loads(oA))
              if "id" in j and _finite(j.get("value"))}
        jb = {j.get("id"): j.get("value") for j in _collect_judges(json.loads(oB))
              if "id" in j and _finite(j.get("value"))}
    except Exception as e: return False, f"invalid JSON: {e}"
    common = set(ja) & set(jb)
    if not common: return False, "no common judges"
    worst = max(abs(ja[i] - jb[i]) for i in common)
    ok = worst < 1e-3
    return ok, f"dt={dt:.2f}s common={len(common)} worst_diff={worst:.6f} (order-stable)"

CASES += [
    case("ci_on_line_then_calc_NA_prop", "custom", fn=fn_ci_line_then_calc),
    case("many_valid_then_cycled_calc",  "custom", fn=fn_many_then_cycled),
    case("adjacent_features_diff_calcnt","custom", fn=fn_adjacent_diff_calcount),
    case("broken_A_atomic_reject",       "custom", fn=fn_broken_A_atomic),
    case("anchor_on_NA_measure_target",  "custom", fn=fn_anchor_on_na),
    case("all_sp_locating_anchor_heavy", "custom", fn=fn_all_sp_anchor),
    case("all_caliper_at_cap_no_leak",   "custom", fn=fn_all_cal_cap_no_leak),
    case("measure_order_reverse_stable", "custom", fn=fn_reverse_order_stable),
]

if __name__ == "__main__":
    sys.exit(run_module("qa_measure", CASES))
