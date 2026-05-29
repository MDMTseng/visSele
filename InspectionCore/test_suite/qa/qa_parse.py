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

# ---------- ROUND 2: deeper / combined adversarial builders ----------

def mk_aux_line_ref_self():
    # aux_line whose ref points to its own id (self-referential ref graph)
    d = golden(); a = first_of(d, "aux_line")
    if a is not None:
        a["ref"] = [{"id": a.get("id", 0), "element": "aux_line"}]
    return d

def mk_aux_line_ref_cycle():
    # two aux features referencing each other (ref-graph cycle)
    d = golden()
    al = first_of(d, "aux_line"); ap = first_of(d, "aux_point")
    if al is not None and ap is not None:
        al["ref"] = [{"id": ap.get("id"), "element": "aux_point"}]
        ap["ref"] = [{"id": al.get("id"), "element": "aux_line"}]
    return d

def mk_aux_line_ref_empty_dicts():
    # ref is a list of empty dicts (missing id/element keys)
    d = golden(); a = first_of(d, "aux_line")
    if a is not None: a["ref"] = [{}, {}, {}]
    return d

def mk_sign360_signature_huge_array():
    # extreme array size in sign360.signature (allocation / OOB on iteration)
    d = golden(); s = first_of(d, "sign360")
    if s is not None: s["signature"] = [0.0] * 200000
    return d

def mk_sign360_signature_typeconfusion():
    # signature elements are strings/objects, not numbers
    d = golden(); s = first_of(d, "sign360")
    if s is not None:
        s["signature"] = ["x", {"a": 1}, None, [1, 2], True]
        s["area"] = {"not": "an-area"}
    return d

def mk_sign360_pt_negative_area():
    d = golden(); s = first_of(d, "sign360")
    if s is not None:
        s["pt1"] = {"x": -0.0, "y": -0.0}
        s["pt2"] = {"x": -0.0, "y": -0.0}   # degenerate zero-extent circle
        s["area"] = -1
    return d

def mk_nan_string_coords():
    # NaN / Infinity smuggled in as strings on a line point
    return mut_many("line", {
        "pt1": {"x": "NaN", "y": "Infinity"},
        "pt2": {"x": "-Infinity", "y": "nan"},
    })()

def mk_searchpoint_margin_array():
    # type confusion on multiple nested numeric fields at once
    return mut_many("search_point", {
        "margin": [1, 2, 3],
        "width": {"deep": {"deeper": 5}},
        "angleDeg": "1e999",
        "line_thickness_value": [[]],
    })()

def mk_measure_calc_selfref_huge():
    # calc post_exp referencing itself + an enormous RPN program
    d = golden(); m = first_of(d, "measure")
    mid = m.get("id", 12)
    m["subtype"] = "calc"
    m["ref"] = [{"id": mid}]
    m["calc_f"] = {"exp": "", "post_exp": ([f"[{mid}]", "+"] * 5000)}
    return d

def mk_measure_calc_garbage_tokens():
    # calc RPN with junk operators / type-confused tokens
    return mut_calc(["[12]", None, {"x": 1}, "@@@", "+", "/", 1e308, "[99999]"])()

def mk_mixed_valid_invalid_features():
    # interleave several broken features among the valid ones
    d = golden()
    feats = first_of(d, "sig360_circle_line")["features"]
    feats.insert(0, {"type": "line", "id": "not-an-int", "pt1": None, "pt2": [1]})
    feats.insert(2, {"type": "search_point"})            # almost-empty feature
    feats.insert(4, {"type": 999, "id": 1})              # type confusion mid-list
    feats.append({"type": "measure", "subtype": "calc",
                  "ref": [{"id": 7}], "calc_f": {"post_exp": ["[7]", "*", "*"]}})
    return d

def mk_all_features_same_id():
    # collapse every feature id to a single value (mass id collision)
    d = golden()
    for f in first_of(d, "sig360_circle_line")["features"]:
        if isinstance(f, dict) and "id" in f: f["id"] = 1
    return d

def mk_negative_and_giant_ids():
    d = golden()
    feats = [f for f in first_of(d, "sig360_circle_line")["features"]
             if isinstance(f, dict) and "id" in f]
    for i, f in enumerate(feats):
        f["id"] = [-2147483648, 2147483647, 9999999999999999][i % 3]
    return d

# raw: duplicate keys in the same JSON object (parser last-wins / collision)
RAW_DUP_KEYS = ('{"type":"binary_processing_group","featureSet":[],'
                '"featureSet":[{"type":"sig360_circle_line","features":[],'
                '"features":[{"type":"line","id":1,"id":2,"pt1":{"x":0,"x":9,"y":0}}]}]}')

# raw: literal NaN / Infinity tokens (non-standard JSON; many parsers accept)
RAW_NAN_LITERALS = ('{"type":"binary_processing_group","featureSet":'
                    '[{"type":"sig360_circle_line","features":'
                    '[{"type":"line","id":1,"pt1":{"x":NaN,"y":Infinity},'
                    '"pt2":{"x":-Infinity,"y":0}}]}]}')

# ===== ROUND 3: parser-edge cases not previously hit =====

def mk_searchpoint_anchor_no_pair():
    # search_point flagged as locating_anchor but anchorPair refs are absent
    return mut_many("search_point", {
        "locating_anchor": True,
        # explicitly omit anchorPair / pair_id
    })()

def mk_searchpoint_anchor_dangling_pair():
    # locating_anchor + anchorPair pointing to non-existent feature id
    return mut_many("search_point", {
        "locating_anchor": True,
        "anchorPair": [{"id": 88888, "element": "search_point"}],
        "pair_id": 88888,
    })()

def mk_searchpoint_anchor_circular():
    # two search_points each declared anchor and referencing each other as pair
    d = golden()
    sps = all_of(d, "search_point")
    if len(sps) >= 2:
        a, b = sps[0], sps[1]
        a["locating_anchor"] = True
        b["locating_anchor"] = True
        a["anchorPair"] = [{"id": b.get("id"), "element": "search_point"}]
        b["anchorPair"] = [{"id": a.get("id"), "element": "search_point"}]
        a["pair_id"] = b.get("id"); b["pair_id"] = a.get("id")
    return d

def mk_sign360_sigdata_is_string():
    # signature.signature_data wrong shape: string instead of numeric array
    d = golden(); s = first_of(d, "sign360")
    if s is not None:
        s["signature"] = {"signature_data": "this should be a number array"}
    return d

def mk_sign360_sigdata_mixed_types():
    # signature_data array elements mixed: numbers, strings, nulls, nested objects
    d = golden(); s = first_of(d, "sign360")
    if s is not None:
        s["signature"] = {"signature_data": [1.0, "x", None, {"k": 2}, [3, 4], True, float('nan') if False else 0]}
    return d

def mk_sig360_missing_features_key():
    # top-level sig360_circle_line missing the required "features" key entirely
    d = golden(); s = first_of(d, "sig360_circle_line")
    if s is not None: s.pop("features", None)
    return d

def mk_sig360_features_wrong_type():
    # sig360_circle_line.features is a number (not array, not object)
    d = golden(); s = first_of(d, "sig360_circle_line")
    if s is not None: s["features"] = 3.14159
    return d

def mk_deep_ref_chain():
    # build a 6-deep ref chain a->b->c->d->e->f->a (cyclic, long)
    d = golden()
    feats = first_of(d, "sig360_circle_line")["features"]
    auxes = [f for f in feats if isinstance(f, dict) and f.get("type") in ("aux_point", "aux_line", "measure")]
    if len(auxes) >= 2:
        ids = [a.get("id", i+100) for i, a in enumerate(auxes)]
        # cyclic chain over whatever aux/measure features exist
        for i, a in enumerate(auxes):
            nxt = ids[(i + 1) % len(ids)]
            a["ref"] = [{"id": nxt, "element": a.get("type", "aux_point")}]
    return d

def mk_control_chars_in_name():
    # control bytes + embedded NUL in the feature "name" field
    return mut_set("line", "name", "evil\x00\x01\x02\x07\x1bname\nwith\tcontrols")()

def mk_huge_string_name():
    # 2 MB string literal as a name field
    return mut_set("line", "name", "A" * (2 * 1024 * 1024))()

def mk_mixed_type_array():
    # features array mixing dict feature + bare int + string + list
    d = golden()
    feats = first_of(d, "sig360_circle_line")["features"]
    feats.insert(1, 42)
    feats.insert(2, "just-a-string")
    feats.insert(3, [1, 2, 3])
    feats.insert(4, None)
    return d

# top-level a JSON array: parser uses reports[0]. style access; what happens
# if the def itself is an array at root?
RAW_ARRAY_OF_DEFS = json.dumps([golden(), golden()])

# duplicate "id" keys inside same feature (JSON allows; last-wins per RFC)
RAW_DUP_FEATURE_KEYS = ('{"type":"binary_processing_group","featureSet":'
                       '[{"type":"sig360_circle_line","features":'
                       '[{"type":"line","id":1,"id":2,"id":3,'
                        '"pt1":{"x":0,"y":0,"x":99,"x":7},'
                        '"pt2":{"x":1,"y":1}}]}]}')

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

    # ===== ROUND 2: deeper / combined adversarial cases =====
    # F. ref-graph corruption (self / cycle / empty)
    case("aux_line_ref_self",       "robust", make=mk_aux_line_ref_self),
    case("aux_line_ref_cycle",      "robust", make=mk_aux_line_ref_cycle),
    case("aux_line_ref_empty_dicts","robust", make=mk_aux_line_ref_empty_dicts),

    # G. sign360 signature fuzzing
    case("sign360_sig_huge_array",  "robust", make=mk_sign360_signature_huge_array),
    case("sign360_sig_typeconfuse", "robust", make=mk_sign360_signature_typeconfusion),
    case("sign360_degenerate_area", "robust", make=mk_sign360_pt_negative_area),

    # H. NaN/Inf-as-string + deep nested type confusion
    case("nan_inf_string_coords",   "robust", make=mk_nan_string_coords),
    case("searchpoint_nested_confuse","robust", make=mk_searchpoint_margin_array),

    # I. calc / RPN adversarial
    case("calc_selfref_huge_rpn",   "robust", make=mk_measure_calc_selfref_huge),
    case("calc_garbage_tokens",     "robust", make=mk_measure_calc_garbage_tokens),

    # J. mixed valid+invalid features / id chaos
    case("mixed_valid_invalid",     "robust", make=mk_mixed_valid_invalid_features),
    case("all_features_same_id",    "robust", make=mk_all_features_same_id),
    case("negative_giant_ids",      "robust", make=mk_negative_and_giant_ids),

    # K. raw byte: duplicate keys + literal NaN/Inf JSON extensions
    case("raw_duplicate_keys",      "robust", raw=RAW_DUP_KEYS),
    case("raw_nan_inf_literals",    "robust", raw=RAW_NAN_LITERALS),

    # L. determinism on an accepted-but-corrupt def (degenerate sign360 -> exit0);
    #    must stay stable across runs despite the degenerate geometry.
    case("det_degenerate_sign360",  "determinism", make=mk_sign360_pt_negative_area),

    # ===== ROUND 3: parser-edge cases not previously hit =====
    # M. locating_anchor (search_point anchor) misuse
    case("anchor_no_pair",          "robust", make=mk_searchpoint_anchor_no_pair),
    case("anchor_dangling_pair",    "robust", make=mk_searchpoint_anchor_dangling_pair),
    case("anchor_circular_pair",    "robust", make=mk_searchpoint_anchor_circular),

    # N. sign360 signature.signature_data wrong shape / mixed
    case("sign360_sigdata_string",  "robust", make=mk_sign360_sigdata_is_string),
    case("sign360_sigdata_mixed",   "robust", make=mk_sign360_sigdata_mixed_types),

    # O. sig360_circle_line top-level required keys missing / wrong-typed
    case("sig360_missing_features", "robust", make=mk_sig360_missing_features_key),
    case("sig360_features_scalar",  "robust", make=mk_sig360_features_wrong_type),

    # P. long cyclic ref chains
    case("deep_ref_chain_cyclic",   "robust", make=mk_deep_ref_chain),

    # Q. control chars / NUL / huge string literal in name
    case("name_control_chars_nul",  "robust", make=mk_control_chars_in_name),
    case("name_huge_string_2MB",    "robust", make=mk_huge_string_name),

    # R. heterogeneous element types in features[] (dict + int + str + list + null)
    case("features_mixed_types",    "robust", make=mk_mixed_type_array),

    # S. raw: top-level JSON array (def parser uses reports[0]. style accessors)
    case("raw_top_level_array_of_defs", "robust", raw=RAW_ARRAY_OF_DEFS),

    # T. raw: duplicate keys inside same feature object (JSON last-wins)
    case("raw_dup_feature_keys",    "robust", raw=RAW_DUP_FEATURE_KEYS),
]

if __name__ == "__main__":
    sys.exit(run_module("qa_parse", CASES))
