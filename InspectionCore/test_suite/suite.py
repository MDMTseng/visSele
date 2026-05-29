#!/usr/bin/env python3
"""
Core0_1 inspection-engine test suite (black-box, via the `visSele --insp` harness).

Grows over rounds. Covers:
  - determinism      : same def -> byte-identical output across runs
  - golden_regression: golden def output == committed baseline
  - robustness       : malformed/corner-case defs must NOT memory-crash (no SIGSEGV/SIGBUS);
                       a controlled SIGABRT (parse reject) is acceptable for invalid defs.

Run:  python3 suite.py            # build NOT required; uses current binary
Exit: 0 if all pass, else number of failures.
"""
import json, os, subprocess, sys, copy, signal

ROOT   = "/Users/mdm/workspace/visSele/InspectionCore"
BUILD  = ROOT + "/build/mac-arm64"
CORE   = ROOT + "/Core0_1"
VIS    = BUILD + "/visSele"
TEST   = "/Users/mdm/workspace/HY_sync/DEV/test"
IMG    = TEST + "/10221 BOS-LT12BH4211 SORTING_bk.png"
GDEF   = TEST + "/10221 BOS-LT12BH4211 SORTING_bk.hydef"
EXPECT = os.path.dirname(os.path.abspath(__file__)) + "/expected"
TMP    = "/tmp/suite"

os.makedirs(EXPECT, exist_ok=True)
os.makedirs(TMP, exist_ok=True)

SIGCRASH = {-signal.SIGSEGV, -signal.SIGBUS, -signal.SIGILL, -signal.SIGFPE}

def run_insp(def_path, out_path, img=IMG):
    """Run visSele --insp from Core0_1 cwd. Returns (returncode, out_bytes_or_None)."""
    env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD)
    try:
        p = subprocess.run([VIS, "--insp", img, def_path, out_path],
                           cwd=CORE, env=env, capture_output=True, timeout=120)
        rc = p.returncode
    except subprocess.TimeoutExpired:
        return ("TIMEOUT", None)
    out = None
    if os.path.exists(out_path):
        with open(out_path, "rb") as f:
            out = f.read()
    return (rc, out)

def write_def(obj, name):
    path = f"{TMP}/{name}.hydef"
    with open(path, "w") as f:
        json.dump(obj, f)
    return path

def golden():
    return json.load(open(GDEF))

def first_of(o, ftype, _hit=None):
    """Return the first dict with type==ftype (DFS)."""
    if isinstance(o, dict):
        if o.get("type") == ftype:
            return o
        for v in o.values():
            r = first_of(v, ftype)
            if r is not None:
                return r
    elif isinstance(o, list):
        for v in o:
            r = first_of(v, ftype)
            if r is not None:
                return r
    return None

# ---- mutation helpers (operate on a deep copy of the golden def) ----
def mut_del(ftype, key):
    def f():
        d = golden(); t = first_of(d, ftype); t.pop(key, None); return d
    return f

def mut_set(ftype, key, val):
    def f():
        d = golden(); t = first_of(d, ftype); t[key] = val; return d
    return f

def mut(fn):
    return fn

def mut_many(ftype, kv):
    def f():
        d = golden(); t = first_of(d, ftype)
        for k, v in kv.items(): t[k] = v
        return d
    return f

def mut_calc(post_exp):
    """Convert the first measure into a subtype=calc judge with the given post_exp."""
    def f():
        d = golden(); m = first_of(d, "measure")
        m["subtype"] = "calc"
        m["calc_f"] = {"exp": "", "post_exp": list(post_exp)}
        m.pop("ref", None)
        return d
    return f

def mut_calc_self_ref():
    def f():
        d = golden(); m = first_of(d, "measure")
        mid = m.get("id")
        m["subtype"] = "calc"; m.pop("ref", None)
        m["calc_f"] = {"exp": "", "post_exp": [f"[{mid}]", "1", "$+$"]}
        return d
    return f

def mut_calc_mutual_cycle():
    def f():
        d = golden(); ms = []
        def c(o):
            if isinstance(o, dict):
                if o.get("type") == "measure": ms.append(o)
                for v in o.values(): c(v)
            elif isinstance(o, list):
                for v in o: c(v)
        c(d)
        if len(ms) >= 2:
            a, b = ms[0], ms[1]
            for m, other in ((a, b), (b, a)):
                m["subtype"] = "calc"; m.pop("ref", None)
                m["calc_f"] = {"exp": "", "post_exp": [f"[{other['id']}]", "1", "$+$"]}
        return d
    return f

def mut_dup_measure_ids():
    def f():
        d = golden(); ms = []
        def c(o):
            if isinstance(o, dict):
                if o.get("type") == "measure": ms.append(o)
                for v in o.values(): c(v)
            elif isinstance(o, list):
                for v in o: c(v)
        c(d)
        if len(ms) >= 2: ms[1]["id"] = ms[0]["id"]
        return d
    return f

def mut_sp_caliper():
    def f():
        d = golden()
        def w(o):
            if isinstance(o, dict):
                if o.get("type") == "search_point": o["locating"] = "caliper"
                for v in o.values(): w(v)
            elif isinstance(o, list):
                for v in o: w(v)
        w(d); return d
    return f

def mut_calc_chain():
    """measure A = [B]+1 ; measure B = [12]*2  (A -> B -> distance#12)."""
    def f():
        d = golden(); ms = []
        def c(o):
            if isinstance(o, dict):
                if o.get("type") == "measure": ms.append(o)
                for v in o.values(): c(v)
            elif isinstance(o, list):
                for v in o: c(v)
        c(d)
        if len(ms) >= 2:
            a, b = ms[0], ms[1]
            b["subtype"] = "calc"; b.pop("ref", None)
            b["calc_f"] = {"exp": "", "post_exp": ["[12]", "2", "$*$"]}
            a["subtype"] = "calc"; a.pop("ref", None)
            a["calc_f"] = {"exp": "", "post_exp": [f"[{b['id']}]", "1", "$+$"]}
        return d
    return f

def mut_caliper_plus_calc():
    def f():
        d = golden()
        def w(o):
            if isinstance(o, dict):
                if o.get("type") in ("line", "arc", "circle"): o["locating"] = "caliper"
                for v in o.values(): w(v)
            elif isinstance(o, list):
                for v in o: w(v)
        w(d)
        m = first_of(d, "measure")
        m["subtype"] = "calc"; m.pop("ref", None)
        m["calc_f"] = {"exp": "", "post_exp": ["[12]", "[13]", "$+$"]}
        return d
    return f

def mut_angle_del_pt1():
    """Make the first measure an angle subtype missing pt1 (probe unchecked deref)."""
    def f():
        d = golden(); m = first_of(d, "measure")
        m["subtype"] = "angle"
        m.pop("pt1", None)
        return d
    return f

def mut_angle_same_line():
    """measure(angle) referencing the same line id twice."""
    def f():
        d = golden(); m = None
        def c(o):
            nonlocal m
            if isinstance(o, dict):
                if o.get("type") == "measure" and o.get("subtype") == "angle" and m is None: m = o
                for v in o.values(): c(v)
            elif isinstance(o, list):
                for v in o: c(v)
        c(d)
        if m is not None and m.get("ref"):
            rid = m["ref"][0]["id"]
            m["ref"] = [{"id": rid, "type": "line"}, {"id": rid, "type": "line"}]
        return d
    return f

def write_raw(text, name):
    path = f"{TMP}/{name}.hydef"
    with open(path, "w") as f:
        f.write(text)
    return path

def mut_caliper():
    def f():
        d = golden()
        def w(o):
            if isinstance(o, dict):
                if o.get("type") in ("line", "arc", "circle"): o["locating"] = "caliper"
                for v in o.values(): w(v)
            elif isinstance(o, list):
                for v in o: w(v)
        w(d); return d
    return f

def mut_empty_features():
    def f():
        d = golden()
        feats = {"line","arc","circle","search_point","measure","aux_point","aux_line","sign360"}
        def strip(o):
            if isinstance(o, dict):
                for k, v in list(o.items()):
                    if isinstance(v, list):
                        o[k] = [x for x in v if not (isinstance(x, dict) and x.get("type") in feats)]
                        for x in o[k]: strip(x)
                    else: strip(v)
            elif isinstance(o, list):
                for x in o: strip(x)
        strip(d); return d
    return f

def mut_dup_line_ids():
    def f():
        d = golden(); lines = []
        def c(o):
            if isinstance(o, dict):
                if o.get("type") == "line": lines.append(o)
                for v in o.values(): c(v)
            elif isinstance(o, list):
                for v in o: c(v)
        c(d)
        if len(lines) >= 2: lines[1]["id"] = lines[0]["id"]
        return d
    return f

def mut_cyclic_aux():
    def f():
        d = golden(); t = first_of(d, "aux_point")
        if t is not None: t["ref"] = [{"id": t.get("id"), "type": "aux_point"},
                                       {"id": t.get("id"), "type": "aux_point"}]
        return d
    return f

# =====================================================================
# CASE REGISTRY  (append new cases each round)
# kind: "determinism" | "golden" | "robust"
# robust cases: build a def via .make() and assert no memory crash.
# =====================================================================
CASES = []
def case(name, kind, **kw):
    CASES.append(dict(name=name, kind=kind, **kw));

# --- Round 1 (note 10) ---
case("determinism_golden", "determinism")
case("golden_regression",  "golden")
case("line_missing_id",        "robust", make=mut_del("line", "id"))
case("measure_missing_name",   "robust", make=mut_del("measure", "name"))
case("line_overlong_name",     "robust", make=mut_set("line", "name", "A"*200))
case("measure_missing_subtype","robust", make=mut_del("measure", "subtype"))
case("line_missing_pt1",       "robust", make=mut_del("line", "pt1"))

# --- Round 2 (note 9) ---
case("arc_missing_pt1",        "robust", make=mut_del("arc", "pt1"))
case("arc_missing_pt3",        "robust", make=mut_del("arc", "pt3"))
case("arc_missing_direction",  "robust", make=mut_del("arc", "direction"))
case("arc_missing_margin",     "robust", make=mut_del("arc", "margin"))
case("line_missing_pt2",       "robust", make=mut_del("line", "pt2"))
case("line_missing_margin",    "robust", make=mut_del("line", "margin"))
case("sp_missing_angleDeg",    "robust", make=mut_del("search_point", "angleDeg"))
case("sp_missing_ref",         "robust", make=mut_del("search_point", "ref"))
case("sp_missing_pt1",         "robust", make=mut_del("search_point", "pt1"))
case("sp_missing_width",       "robust", make=mut_del("search_point", "width"))
case("measure_missing_ref",    "robust", make=mut_del("measure", "ref"))
case("measure_missing_value",  "robust", make=mut_del("measure", "value"))
case("measure_ref_nonexistent","robust", make=mut_set("measure", "ref",
        [{"id": 99999, "type": "search_point"}, {"id": 99998, "type": "search_point"}]))
case("line_id_as_string",      "robust", make=mut_set("line", "id", "not_a_number"))
case("line_margin_as_string",  "robust", make=mut_set("line", "margin", "wide"))
case("line_pt1_as_number",     "robust", make=mut_set("line", "pt1", 5))

# --- Round 3 (note 8) ---
case("invalid_json",           "robust", raw='{ this is not valid json ]]')
case("empty_string_def",       "robust", raw='')
case("empty_featureSet",       "robust", make=mut_empty_features())
case("duplicate_line_ids",     "robust", make=mut_dup_line_ids())
case("cyclic_aux_ref",         "robust", make=mut_cyclic_aux())
case("inverted_limits",        "robust", make=mut_set("measure", "USL", -999))  # USL<LSL
case("zero_margin_line",       "robust", make=mut_set("line", "margin", 0))
case("negative_coords_line",   "robust", make=mut_set("line", "pt1", {"x": -50000, "y": -50000}))
case("determinism_caliper",    "determinism", make=mut_caliper())

# --- Round 4 (note 7): numerical degeneracy ---
case("arc_coincident_pts",     "robust", make=mut_many("arc",
        {"pt1": {"x": 100, "y": 100}, "pt2": {"x": 100, "y": 100}, "pt3": {"x": 100, "y": 100}}))
case("arc_collinear_pts",      "robust", make=mut_many("arc",
        {"pt1": {"x": 100, "y": 100}, "pt2": {"x": 200, "y": 200}, "pt3": {"x": 300, "y": 300}}))
case("line_zero_length",       "robust", make=mut_many("line",
        {"pt1": {"x": 150, "y": 150}, "pt2": {"x": 150, "y": 150}}))
case("measure_angle_same_line","robust", make=mut_angle_same_line())
case("line_huge_id",           "robust", make=mut_set("line", "id", 2147483648))
case("line_unknown_field",     "robust", make=mut_set("line", "zzz_unknown_field", {"a": [1, 2, 3]}))
case("unknown_toplevel_key",   "robust", make=mut_set("sig360_circle_line", "zzz_future_key", 12345))

# --- Round 5 (note 6): CALC evaluator + angle deref probe ---
case("calc_sum",               "robust", make=mut_calc(["[12]", "[13]", "$+$"]))
case("calc_scalar",            "robust", make=mut_calc(["[12]", "2", "$*$"]))
case("calc_max",               "robust", make=mut_calc(["[12]", "[13]", "$,$", "max$"]))
case("calc_div_zero",          "robust", make=mut_calc(["[12]", "0", "$/$"]))
case("calc_ref_missing",       "robust", make=mut_calc(["[99999]", "1", "$+$"]))
case("calc_malformed_postfix", "robust", make=mut_calc(["$+$"]))            # stack underflow probe
case("calc_huge_ref_id",       "robust", make=mut_calc(["[2147483647]", "1", "$+$"]))
case("calc_empty_postexp",     "robust", make=mut_calc([]))
case("measure_angle_missing_pt1","robust", make=mut_angle_del_pt1())       # unchecked-deref probe
case("determinism_calc",       "determinism", make=mut_calc(["[12]", "[13]", "$+$"]))

# --- Round 6 (note 5): CALC cycles + determinism stress + sp caliper ---
case("calc_self_ref",          "robust", make=mut_calc_self_ref())
case("calc_mutual_cycle",      "robust", make=mut_calc_mutual_cycle())
case("calc_unsupported_fn",    "robust", make=mut_calc(["[12]", "sin$"]))
case("duplicate_measure_ids",  "robust", make=mut_dup_measure_ids())
case("determinism_5x",         "determinism", runs=5)
case("determinism_sp_caliper", "determinism", make=mut_sp_caliper())

# --- Round 7 (note 4): image-side + ref-type robustness ---
case("img_missing",            "robust", defpath=GDEF, img="/tmp/does_not_exist.png")
case("def_missing",            "robust", defpath="/tmp/does_not_exist_def.hydef")
case("img_alt_nonbk",          "robust", defpath=GDEF,
        img="/Users/mdm/workspace/HY_sync/DEV/test/10221 BOS-LT12BH4211 SORTING.png")
case("calc_ref_to_line_id",    "robust", make=mut_calc(["[1]", "1", "$+$"]))   # [1] is a line, not a measure
case("calc_deep_chain",        "robust", make=mut_calc_chain())
case("determinism_alt_img",    "determinism",
        img="/Users/mdm/workspace/HY_sync/DEV/test/10221 BOS-LT12BH4211 SORTING.png")

# --- Round 8 (note 3): value / literal / limit edges ---
case("calc_float_literal",     "robust", make=mut_calc(["[12]", "3.14", "$*$"]))
case("calc_negative_literal",  "robust", make=mut_calc(["[12]", "-5", "$+$"]))
case("name_empty",             "robust", make=mut_set("line", "name", ""))
case("name_with_quotes",       "robust", make=mut_set("line", "name", 'a"b\\c\td'))
case("value_as_string",        "robust", make=mut_set("measure", "value", "abc"))
case("usl_equals_lsl",         "robust", make=mut_many("measure", {"USL": 8.5, "LSL": 8.5}))
case("sp_width_zero",          "robust", make=mut_set("search_point", "width", 0))
case("sp_negative_margin",     "robust", make=mut_set("search_point", "margin", -10))

# --- Round 9 (note 2): structural/CALC stress ---
case("determinism_degenerate", "determinism", make=mut_many("arc",
        {"pt1": {"x": 100, "y": 100}, "pt2": {"x": 100, "y": 100}, "pt3": {"x": 100, "y": 100}}))
case("calc_long_postexp",      "robust", make=mut_calc(["[12]"] + ["[13]", "$+$"] * 100))
case("calc_f_not_object",      "robust", make=mut_many("measure", {"subtype": "calc", "calc_f": "notanobject"}))
case("wrong_schema_def",       "robust", raw='{"hello":"world","x":[1,2,3]}')
case("sp_ref_nonexistent",     "robust", make=mut_set("search_point", "ref",
        [{"id": 99999, "type": "line"}]))
case("aux_missing_ref",        "robust", make=mut_del("aux_point", "ref"))

# --- Round 10 (note 1): output sanity + combined + final gate ---
case("golden_output_sanity",   "schema")
case("combined_caliper_calc",  "robust", make=mut_caliper_plus_calc())
case("determinism_combined",   "determinism", make=mut_caliper_plus_calc())
case("determinism_caliper_alt_img", "determinism", make=mut_caliper(),
        img="/Users/mdm/workspace/HY_sync/DEV/test/10221 BOS-LT12BH4211 SORTING.png")

# =====================================================================
def rc_str(rc):
    if rc == "TIMEOUT": return "TIMEOUT"
    if isinstance(rc, int) and rc < 0:
        try: return signal.Signals(-rc).name
        except Exception: return f"signal{-rc}"
    return f"exit{rc}"

def main():
    results = []
    for c in CASES:
        name, kind = c["name"], c["kind"]
        if kind == "determinism":
            dp = write_def(c["make"](), name) if "make" in c else GDEF
            runs = c.get("runs", 2)
            outs, rcs = [], []
            for k in range(runs):
                rc, o = run_insp(dp, f"{TMP}/det_{name}_{k}.json", img=c.get("img", IMG))
                rcs.append(rc); outs.append(o)
            ident = all(o is not None and o == outs[0] for o in outs)
            ok = (all(r == 0 for r in rcs) and outs[0] is not None and ident)
            detail = f"{runs}x runs, all-exit0={all(r==0 for r in rcs)} identical={ident}"
        elif kind == "schema":
            rc, out = run_insp(GDEF, f"{TMP}/schema.json")
            ok, detail = False, rc_str(rc)
            if rc == 0 and out is not None:
                try:
                    j = json.loads(out)
                    low = out.lower()
                    has_reports = "reports" in out.decode("utf-8", "ignore")
                    no_nan = (b"nan" not in low and b"inf" not in low)
                    ok = has_reports and no_nan
                    detail = f"validJSON reports={has_reports} no_nan/inf={no_nan}"
                except Exception as e:
                    detail = f"JSON parse error: {e}"
        elif kind == "golden":
            rc, out = run_insp(GDEF, f"{TMP}/golden.json")
            base = f"{EXPECT}/10221.json"
            if not os.path.exists(base):
                if out is not None:
                    open(base, "wb").write(out)
                ok = (rc == 0 and out is not None); detail = f"{rc_str(rc)} baseline-created"
            else:
                exp = open(base, "rb").read()
                ok = (rc == 0 and out == exp); detail = f"{rc_str(rc)} match={out==exp}"
        elif kind == "robust":
            if "defpath" in c:      dp = c["defpath"]
            elif "raw" in c:        dp = write_raw(c["raw"], name)
            else:                   dp = write_def(c["make"](), name)
            rc, out = run_insp(dp, f"{TMP}/{name}.json", img=c.get("img", IMG))
            ok = (rc not in SIGCRASH and rc != "TIMEOUT")  # no memory-unsafe crash / no hang
            detail = rc_str(rc) + (" (controlled-reject)" if rc == -signal.SIGABRT else "")
        else:
            ok, detail = False, "unknown kind"
        results.append((name, kind, ok, detail))

    print(f"{'CASE':<28} {'KIND':<13} {'RESULT':<6} DETAIL")
    fails = 0
    for name, kind, ok, detail in results:
        print(f"{name:<28} {kind:<13} {'PASS' if ok else 'FAIL':<6} {detail}")
        if not ok: fails += 1
    print(f"\n{len(results)-fails}/{len(results)} passed, {fails} failed")
    return fails

if __name__ == "__main__":
    sys.exit(main())
