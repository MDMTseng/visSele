"""QA module: def-schema & parser robustness / fuzzing  (viewpoint: PARSER)

The Core0_1 def is JSON (.hydef): a top-level `binary_processing_group`
wrapping a `sig360_circle_line` whose `features[]` carry feature objects of
type line / arc / search_point / measure / aux_point / aux_line / sign360.
This module attacks the *parser / schema deserializer*, not the geometry math.
A sibling suite already covers basic missing id/name/pt and overlong-name, so
every case here goes BEYOND that: structural corruption, type confusion,
malformed JSON, unicode, and numeric extremes.

GOAL of a "robust" PASS: the engine may legitimately *reject* bad input with a
controlled SIGABRT / exit3 / exit4, but it must NEVER memory-fault
(SIGSEGV/SIGBUS/SIGILL/SIGFPE) or hang on attacker-controlled JSON. A crash on
any case below is a real engine/parser bug.

TEST PLAN (grouped):

A. MALFORMED / NON-CONFORMING JSON BYTES (parser front-door)
   1. raw_truncated_json        - valid prefix then EOF mid-object
   2. raw_trailing_garbage      - valid def followed by junk bytes
   3. raw_top_level_array       - root is [] not {} (wrong root container)
   4. raw_top_level_scalar      - root is a bare number
   5. raw_empty_file            - zero-byte file
   6. raw_deep_nesting          - 5000x nested arrays (stack-blow / recursion DoS)
   7. raw_bom_and_unicode_keys  - UTF-8 BOM + non-ASCII / unicode key names

B. TYPE CONFUSION ON STRUCTURAL CONTAINERS
   8. featureSet_is_string      - featureSet replaced by a string
   9. features_is_object        - features[] replaced by an object/map
  10. feature_is_scalar         - a feature entry is an int, not an object
  11. type_field_wrong_type     - feature "type" is a number not a string
  12. unknown_feature_type      - type = "no_such_feature_xyz"

C. FIELD-LEVEL TYPE / VALUE FUZZING ON GEOMETRY
  13. line_pt_is_array          - line.pt1 is [x,y] array instead of {x,y}
  14. pt_x_is_string            - line.pt1.x is a numeric string
  15. line_pt_missing_y         - point object missing the y coordinate
  16. numeric_extremes          - huge / tiny / negative-zero coords on a line
  17. arc_zero_and_neg_values   - arc with degenerate radius/angle fields

D. REFERENCE / RELATIONAL CORRUPTION
  18. searchpoint_ref_dangling  - search_point.ref points to non-existent id
  19. ref_is_wrong_shape        - ref is a scalar, not the expected list-of-dicts
  20. duplicate_feature_ids     - two features share id 1 (id collision)

E. DETERMINISM under a malformed-but-accepted def
  21. det_dangling_ref         - the dangling-ref def must be deterministic/stable
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qalib import *
import json, copy

# ---------- helpers that build whole-def dicts ----------

def _mut_first_feature(fn):
    def f():
        d = golden()
        feats = first_of(d, "sig360_circle_line")["features"]
        fn(d, feats)
        return d
    return f

def mk_featureSet_string():
    d = golden(); d["featureSet"] = "i am not a list"; return d

def mk_features_object():
    d = golden()
    s = first_of(d, "sig360_circle_line")
    s["features"] = {"0": s["features"][0]}   # map instead of array
    return d

def mk_feature_scalar():
    def fn(d, feats): feats.insert(0, 12345)
    return _mut_first_feature(fn)()

def mk_unknown_type():
    return mut_set("line", "type", "no_such_feature_xyz")()

def mk_type_number():
    return mut_set("line", "type", 7)()

def mk_pt_array():
    return mut_set("line", "pt1", [1.0, 2.0])()

def mk_pt_x_string():
    d = golden(); t = first_of(d, "line"); t["pt1"]["x"] = "3.14"; return d

def mk_pt_missing_y():
    d = golden(); t = first_of(d, "line"); t["pt1"].pop("y", None); return d

def mk_numeric_extremes():
    return mut_many("line", {
        "pt1": {"x": 1e308, "y": -1e308},
        "pt2": {"x": -0.0, "y": 1e-300},
        "margin": 1e9,
    })()

def mk_arc_degenerate():
    d = golden(); a = first_of(d, "arc")
    for k in ("radius", "r", "angleDeg", "margin", "width"):
        if k in a: a[k] = 0 if k != "radius" else -5
    return d

def mk_dangling_ref():
    d = golden(); sp = first_of(d, "search_point")
    sp["ref"] = [{"id": 99999, "element": "line"}]
    return d

def mk_ref_wrong_shape():
    return mut_set("search_point", "ref", 42)()

def mk_duplicate_ids():
    d = golden()
    feats = first_of(d, "sig360_circle_line")["features"]
    ids = [f for f in feats if isinstance(f, dict) and "id" in f]
    if len(ids) >= 2: ids[1]["id"] = ids[0]["id"]
    return d

# ---------- raw byte payloads ----------

def _golden_text():
    return json.dumps(golden())

RAW_TRUNCATED   = _golden_text()[: len(_golden_text()) // 2]
RAW_TRAILING    = _golden_text() + "\n}}}}garbage trailing bytes!!!"
RAW_TOP_ARRAY   = "[1,2,3]"
RAW_TOP_SCALAR  = "42"
RAW_EMPTY       = ""
RAW_DEEP        = "[" * 5000 + "]" * 5000
RAW_BOM_UNICODE = "﻿" + json.dumps(
    {"type": "binary_processing_group", "é中文": 1,
     "featureSet": [{"type": "sig360_circle_line",
                     "\U0001F4A9emoji": "nul", "features": []}]},
    ensure_ascii=False)

CASES = [
    # A. malformed JSON bytes
    case("raw_truncated_json",      "robust", raw=RAW_TRUNCATED),
    case("raw_trailing_garbage",    "robust", raw=RAW_TRAILING),
    case("raw_top_level_array",     "robust", raw=RAW_TOP_ARRAY),
    case("raw_top_level_scalar",    "robust", raw=RAW_TOP_SCALAR),
    case("raw_empty_file",          "robust", raw=RAW_EMPTY),
    case("raw_deep_nesting",        "robust", raw=RAW_DEEP),
    case("raw_bom_unicode_keys",    "robust", raw=RAW_BOM_UNICODE),

    # B. structural type confusion
    case("featureSet_is_string",    "robust", make=mk_featureSet_string),
    case("features_is_object",      "robust", make=mk_features_object),
    case("feature_is_scalar",       "robust", make=mk_feature_scalar),
    case("type_field_wrong_type",   "robust", make=mk_type_number),
    case("unknown_feature_type",    "robust", make=mk_unknown_type),

    # C. field-level geometry fuzzing
    case("line_pt_is_array",        "robust", make=mk_pt_array),
    case("pt_x_is_string",          "robust", make=mk_pt_x_string),
    case("line_pt_missing_y",       "robust", make=mk_pt_missing_y),
    case("numeric_extremes",        "robust", make=mk_numeric_extremes),
    case("arc_zero_and_neg_values", "robust", make=mk_arc_degenerate),

    # D. reference / relational corruption
    case("searchpoint_ref_dangling","robust", make=mk_dangling_ref),
    case("ref_is_wrong_shape",      "robust", make=mk_ref_wrong_shape),
    case("duplicate_feature_ids",   "robust", make=mk_duplicate_ids),

    # E. determinism on a malformed-but-accepted def
    case("det_dangling_ref",        "determinism", make=mk_dangling_ref),
]

if __name__ == "__main__":
    sys.exit(run_module("qa_parse", CASES))
