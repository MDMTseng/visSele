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

# ===== ROUND 4: less-explored parser corners =====

def mk_huge_int_string_id():
    # JSON number with 200 digits as feature id (cJSON parses to double, may overflow)
    d = golden(); t = first_of(d, "line")
    if t is not None: t["id"] = int("9" * 200)
    return d

def mk_hex_octal_string_coords():
    # hex/octal-style strings as numeric field values (non-JSON-number forms)
    return mut_many("line", {
        "pt1": {"x": "0x10", "y": "0o17"},
        "pt2": {"x": "0X1F", "y": "017"},
        "margin": "0xDEADBEEF",
    })()

def mk_inf_nan_quoted_everywhere():
    # +Inf/-Inf/NaN as JSON *strings* spread across many numeric fields
    return mut_many("search_point", {
        "margin":   "+Inf",
        "width":    "-Inf",
        "angleDeg": "NaN",
        "line_thickness_value": "+inf",
    })()

def mk_featureSet_single_object():
    # featureSet is a single object (not wrapped in an array)
    d = golden()
    fs = d.get("featureSet")
    if isinstance(fs, list) and fs:
        d["featureSet"] = fs[0]
    return d

def mk_multiple_bpg_siblings():
    # raw: multiple binary_processing_group siblings inside a wrapping array root
    d = golden()
    # wrap two copies inside featureSet of an outer bpg
    d2 = copy.deepcopy(d)
    d["featureSet"] = (d.get("featureSet") or []) + (d2.get("featureSet") or [])
    return d

RAW_BOM_TRAILING_WS = "﻿   \n\t" + _golden_text() + "   \n\t\r\n  "

# Unicode surrogate-pair (valid: U+1F4A9 PILE OF POO) in a name field, escaped
RAW_VALID_SURROGATE = ('{"type":"binary_processing_group","featureSet":'
                       '[{"type":"sig360_circle_line","features":'
                       '[{"type":"line","id":1,"name":"\\uD83D\\uDCA9poo",'
                        '"pt1":{"x":0,"y":0},"pt2":{"x":1,"y":1}}]}]}')

# Invalid lone-high-surrogate sequence (no trailing low surrogate)
RAW_INVALID_SURROGATE = ('{"type":"binary_processing_group","featureSet":'
                        '[{"type":"sig360_circle_line","features":'
                        '[{"type":"line","id":1,"name":"bad\\uD83Dxx",'
                         '"pt1":{"x":0,"y":0},"pt2":{"x":1,"y":1}}]}]}')

def mk_type_is_object():
    # feature "type" field is itself a JSON object (not string/number)
    return mut_set("line", "type", {"name": "line", "kind": "?"})()

def mk_calc_postexp_heterogeneous():
    # deeply nested calc_f.post_exp with non-string items: ints, arrays, null, dicts
    return mut_calc([
        "[12]", 3.14, None, [["nested"], 1, 2],
        {"obj": True}, "+", 2147483648, -1e308, "[7]", "*",
    ])()

def mk_ref_id_missing():
    # ref entries that lack 'id' key entirely
    d = golden(); sp = first_of(d, "search_point")
    if sp is not None: sp["ref"] = [{"element": "line"}, {"element": "arc"}]
    return d

def mk_ref_id_string_and_array():
    # ref id is a string in one entry and an array in another
    d = golden(); sp = first_of(d, "search_point")
    if sp is not None:
        sp["ref"] = [{"id": "twelve", "element": "line"},
                     {"id": [1, 2, 3], "element": "arc"},
                     {"id": {"nested": 5}}]
    return d

    # ===== ROUND 5: anchor cycles via ConstrainMap, calc ref chains, orientation_essential =====

def mk_anchor_self_cycle():
    # search_point with locating_anchor:true whose own ref points to itself
    d = golden(); sp = first_of(d, "search_point")
    if sp is not None:
        sp["locating_anchor"] = True
        sp["ref"] = [{"id": sp.get("id", 0), "element": "search_point"}]
        sp["anchorPair"] = [{"id": sp.get("id", 0), "element": "search_point"}]
    return d

def mk_anchor_cycle_via_constrainmap():
    # two locating_anchor search_points whose ref lists each other (ConstrainMap cycle)
    d = golden()
    sps = all_of(d, "search_point")
    if len(sps) >= 2:
        a, b = sps[0], sps[1]
        a["locating_anchor"] = True; b["locating_anchor"] = True
        a["ref"] = [{"id": b.get("id"), "element": "search_point"}]
        b["ref"] = [{"id": a.get("id"), "element": "search_point"}]
    return d

def mk_searchpoint_ref_to_calc():
    # search_point.ref points at a measure that is now a calc subtype
    d = golden()
    sp = first_of(d, "search_point"); m = first_of(d, "measure")
    if sp is not None and m is not None:
        mid = m.get("id", 12)
        m["subtype"] = "calc"; m["ref"] = [{"id": sp.get("id", 1)}]
        m["calc_f"] = {"exp": "", "post_exp": [f"[{sp.get('id',1)}]", "1", "+"]}
        sp["ref"] = [{"id": mid, "element": "measure"}]
    return d

def mk_orientation_essential_on_failing_sp():
    # mark a measure orientation_essential:true and corrupt its search_point dep
    d = golden()
    m = first_of(d, "measure"); sp = first_of(d, "search_point")
    if m is not None:
        m["orientation_essential"] = True
    if sp is not None:
        # break the search point so its judge fails -> reDo_orien path
        sp["margin"] = -1; sp["width"] = 0; sp["angleDeg"] = float('nan') if False else 1e308
        sp["ref"] = [{"id": 99999, "element": "line"}]
    return d

def mk_features_100k_mixed():
    # 100k feature array, mix of valid lines and broken entries
    d = golden()
    feats = first_of(d, "sig360_circle_line")["features"]
    base = {"type": "line", "id": 50000, "pt1": {"x": 0, "y": 0}, "pt2": {"x": 1, "y": 1}}
    add = []
    for i in range(100000):
        if i % 5 == 0:
            add.append({"type": "line", "id": 200000 + i,
                        "pt1": {"x": i, "y": i}, "pt2": {"x": i + 1, "y": i + 1}})
        elif i % 5 == 1:
            add.append(i)                           # bare scalar
        elif i % 5 == 2:
            add.append({"type": "no_such", "id": i})
        elif i % 5 == 3:
            add.append({"type": "line", "id": "bad"})
        else:
            add.append(None)
    feats.extend(add)
    return d

def mk_sign360_deep_nested_sigdata():
    # signature_data wrapped in 100 levels of nested arrays
    d = golden(); s = first_of(d, "sign360")
    if s is not None:
        deep = [1.0, 2.0, 3.0]
        for _ in range(100):
            deep = [deep]
        s["signature"] = {"signature_data": deep}
    return d

# raw: duplicate "type" key within one feature object (cJSON last-wins behavior)
RAW_DUP_TYPE_KEY = ('{"type":"binary_processing_group","featureSet":'
                    '[{"type":"sig360_circle_line","features":'
                    '[{"type":"line","type":"arc","type":"search_point","id":1,'
                    '"pt1":{"x":0,"y":0},"pt2":{"x":1,"y":1}}]}]}')

def mk_id_int_min():
    # id set to INT32_MIN (-2^31) -- engine often uses int32 for IDs
    d = golden(); t = first_of(d, "line")
    if t is not None: t["id"] = -2147483648
    return d

def mk_angle_edge_values():
    # angledOffset / angleDeg NaN, infinity, and wrap-around values across features
    d = golden()
    for ft in ("search_point", "arc", "sign360"):
        f = first_of(d, ft)
        if f is None: continue
        # NaN / Inf can't be JSON-encoded by python json by default;
        # use huge finite + wraparound combos that still trigger angle wrapping
        f["angledOffset"] = 1e308
        f["angleDeg"] = -1e308
        f["angle"] = 3.6e6                          # 10000 full turns
    return d

def mk_top_level_reports_conflict():
    # top-level "reports" key (collides with engine's output report list name)
    d = golden()
    d["reports"] = [{"id": 1, "judge": "fake", "fake": True}] * 50
    return d

def mk_regression_round2_combo():
    # regression: stack 3 round-2 mutations on same def to ensure no re-introduced bug
    d = golden()
    # mass id collision
    for f in first_of(d, "sig360_circle_line")["features"]:
        if isinstance(f, dict) and "id" in f: f["id"] = 1
    # NaN/Inf string coords on a line
    t = first_of(d, "line")
    if t:
        t["pt1"] = {"x": "NaN", "y": "Infinity"}
        t["pt2"] = {"x": "-Infinity", "y": "nan"}
    # garbage calc tokens
    m = first_of(d, "measure")
    if m:
        m["subtype"] = "calc"; m["ref"] = [{"id": 12}]
        m["calc_f"] = {"exp": "", "post_exp": ["[12]", None, "@@@", "+", "/"]}
    return d

# ===== ROUND 6: wider cycles, invalid-id detector boundary, json_seg_parser, nested defs, mem pressure =====

def mk_aux_3node_cycle():
    # 3-node cycle: aux_point -> aux_line -> measure -> aux_point
    d = golden()
    ap = first_of(d, "aux_point"); al = first_of(d, "aux_line"); m = first_of(d, "measure")
    if ap and al and m:
        ap["ref"] = [{"id": al.get("id"), "element": "aux_line"}]
        al["ref"] = [{"id": m.get("id"), "element": "measure"}]
        m["ref"]  = [{"id": ap.get("id"), "element": "aux_point"}]
    return d

def mk_aux_4node_cycle():
    # 4-node cycle weaving aux_point/aux_line pairs
    d = golden()
    aps = all_of(d, "aux_point"); als = all_of(d, "aux_line")
    if len(aps) >= 2 and len(als) >= 2:
        chain = [aps[0], als[0], aps[1], als[1]]
        for i, n in enumerate(chain):
            nxt = chain[(i+1) % len(chain)]
            n["ref"] = [{"id": nxt.get("id"), "element": nxt.get("type")}]
    return d

def mk_searchpoint_long_ref_chain():
    # Chain length ~= number of search_points: sp0->sp1->...->spN-1->sp0
    d = golden()
    sps = all_of(d, "search_point")
    if len(sps) >= 2:
        for i, s in enumerate(sps):
            nxt = sps[(i+1) % len(sps)]
            s["ref"] = [{"id": nxt.get("id"), "element": "search_point"}]
    return d

def mk_cycle_with_invalid_id():
    # Cycle detector facing an invalid id node mid-chain (should not loop forever)
    d = golden()
    ap = first_of(d, "aux_point"); al = first_of(d, "aux_line")
    if ap and al:
        ap["ref"] = [{"id": al.get("id"), "element": "aux_line"}]
        al["ref"] = [{"id": 99999, "element": "aux_point"}]   # dangling -> back-edge attempt
    return d

def mk_all_refs_invalid_ids():
    # Every aux/measure/search_point ref points to a non-existent id
    d = golden()
    for ft in ("aux_point", "aux_line", "measure", "search_point"):
        for f in all_of(d, ft):
            f["ref"] = [{"id": 99999 + (f.get("id") or 0), "element": ft}]
    return d

def mk_deep_lineCross_chain():
    # aux_point lineCross deep chain where intermediate aux_lines are degenerate
    d = golden()
    als = all_of(d, "aux_line"); aps = all_of(d, "aux_point")
    # break intermediate lines so the chain has failing nodes
    for al in als:
        al["subtype"] = "twoPoint"
        al["pt1"] = {"x": 0.0, "y": 0.0}
        al["pt2"] = {"x": 0.0, "y": 0.0}     # zero-length line -> lineCross fails
    # link aux_points sequentially as lineCross of pairs of (broken) aux_lines
    for i, ap in enumerate(aps):
        ap["subtype"] = "lineCross"
        if len(als) >= 2:
            ap["ref"] = [{"id": als[i % len(als)].get("id"), "element": "aux_line"},
                         {"id": als[(i+1) % len(als)].get("id"), "element": "aux_line"}]
    return d

# raw bytes targeting json_seg_parser stack limits / quoting attacks
RAW_JSP_HUGE_KEY = ('{"type":"binary_processing_group","'
                    + 'A' * 200000 + '":1,"featureSet":[]}')

RAW_JSP_UNTERMINATED_STRING = '{"type":"binary_processing_group","featureSet":"unterm'

RAW_JSP_ESCAPED_QUOTES = ('{"type":"binary_processing_group","featureSet":'
                          '[{"type":"sig360_circle_line","features":'
                          '[{"type":"line","id":1,"name":"' + ('\\"' * 5000) +
                          '","pt1":{"x":0,"y":0},"pt2":{"x":1,"y":1}}]}]}')

def mk_nested_measure_in_measure():
    # measure feature containing another full measure under unexpected key
    d = golden(); m = first_of(d, "measure")
    if m:
        inner = copy.deepcopy(m)
        inner["id"] = 77777
        m["nested_feature"] = inner
        m["features"] = [inner, inner]           # wrong-shape nesting
    return d

def mk_nested_featureSet_recursive():
    # A feature carrying a full featureSet inside it (recursive def-in-def)
    d = golden()
    feats = first_of(d, "sig360_circle_line")["features"]
    inner = copy.deepcopy(d)
    feats.append({"type": "line", "id": 4242,
                  "pt1": {"x": 0, "y": 0}, "pt2": {"x": 1, "y": 1},
                  "featureSet": inner.get("featureSet")})
    return d

def mk_calc_megastring_exp():
    # 1M-character single string in calc_f.exp -> cJSON_Print memory pressure
    d = golden(); m = first_of(d, "measure")
    if m:
        mid = m.get("id", 12)
        m["subtype"] = "calc"; m["ref"] = [{"id": mid}]
        m["calc_f"] = {"exp": "A" * (1024 * 1024),
                       "post_exp": [f"[{mid}]", "1", "+"]}
        m["name"] = "B" * (1024 * 1024)          # extra: huge name -> report serialize
    return d


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

    # ===== ROUND 4 =====
    # U. extreme JSON number edges
    case("huge_int_string_id",      "robust", make=mk_huge_int_string_id),
    case("hex_octal_string_coords", "robust", make=mk_hex_octal_string_coords),
    case("inf_nan_quoted_fields",   "robust", make=mk_inf_nan_quoted_everywhere),

    # V. featureSet shape variants
    case("featureSet_single_object","robust", make=mk_featureSet_single_object),
    case("multiple_bpg_siblings",   "robust", make=mk_multiple_bpg_siblings),

    # W. whitespace / BOM / surrogate-pair escapes (raw)
    case("raw_bom_trailing_ws",     "robust", raw=RAW_BOM_TRAILING_WS),
    case("raw_valid_surrogate_pair","robust", raw=RAW_VALID_SURROGATE),
    case("raw_invalid_surrogate",   "robust", raw=RAW_INVALID_SURROGATE),

    # X. type-field & post_exp deep type confusion
    case("type_field_is_object",    "robust", make=mk_type_is_object),
    case("calc_postexp_heterogeneous","robust", make=mk_calc_postexp_heterogeneous),

    # Y. ref id missing / wrong shapes
    case("ref_id_missing",          "robust", make=mk_ref_id_missing),
    case("ref_id_string_and_array", "robust", make=mk_ref_id_string_and_array),

    # Z. regression: stacked round-2 mutations
    case("regression_round2_combo", "robust", make=mk_regression_round2_combo),

    # ===== ROUND 5 =====
    case("anchor_self_cycle",          "robust", make=mk_anchor_self_cycle),
    case("anchor_cycle_constrainmap",  "robust", make=mk_anchor_cycle_via_constrainmap),
    case("searchpoint_ref_to_calc",    "robust", make=mk_searchpoint_ref_to_calc),
    case("orien_essential_failing_sp", "robust", make=mk_orientation_essential_on_failing_sp),
    case("features_100k_mixed",        "robust", make=mk_features_100k_mixed),
    case("sign360_sigdata_deep_nest",  "robust", make=mk_sign360_deep_nested_sigdata),
    case("raw_dup_type_key",           "robust", raw=RAW_DUP_TYPE_KEY),
    case("id_int32_min",               "robust", make=mk_id_int_min),
    case("angle_edge_values",          "robust", make=mk_angle_edge_values),
    case("top_level_reports_conflict", "robust", make=mk_top_level_reports_conflict),

    # ===== ROUND 6 =====
    case("aux_3node_cycle",            "robust", make=mk_aux_3node_cycle),
    case("aux_4node_cycle",            "robust", make=mk_aux_4node_cycle),
    case("searchpoint_long_ref_chain", "robust", make=mk_searchpoint_long_ref_chain),
    case("cycle_with_invalid_id",      "robust", make=mk_cycle_with_invalid_id),
    case("all_refs_invalid_ids",       "robust", make=mk_all_refs_invalid_ids),
    case("deep_lineCross_chain",       "robust", make=mk_deep_lineCross_chain),
    case("raw_jsp_huge_key",           "robust", raw=RAW_JSP_HUGE_KEY),
    case("raw_jsp_unterm_string",      "robust", raw=RAW_JSP_UNTERMINATED_STRING),
    case("raw_jsp_escaped_quotes",     "robust", raw=RAW_JSP_ESCAPED_QUOTES),
    case("nested_measure_in_measure",  "robust", make=mk_nested_measure_in_measure),
    case("nested_featureSet_recursive","robust", make=mk_nested_featureSet_recursive),
    case("calc_megastring_exp",        "robust", make=mk_calc_megastring_exp),

    # ===== ROUND 7: parse_judgeData subtype coverage + sig360 top-level fuzzing =====
    case("judge_angle_missing_pt1",        "robust", make=lambda: _judge_angle_missing_pt1()),
    case("judge_angle_missing_quadrant",   "robust", make=lambda: _judge_angle_missing_quadrant()),
    case("judge_angle_pt1_wrong_shape",    "robust", make=lambda: _judge_angle_pt1_string()),
    case("judge_circle_info_unknown_info", "robust", make=lambda: _judge_circle_info_unknown()),
    case("judge_circle_info_info_object",  "robust", make=lambda: _judge_circle_info_info_obj()),
    case("judge_sigma_subtype",            "robust", make=lambda: _judge_subtype_minimal("sigma")),
    case("judge_radius_subtype",           "robust", make=lambda: _judge_subtype_minimal("radius")),
    case("judge_distance_no_ref",          "robust", make=lambda: _judge_distance_no_ref()),
    case("judge_area_no_ref",              "robust", make=lambda: _judge_subtype_minimal("area")),
    case("judge_unknown_subtype",          "robust", make=lambda: _judge_subtype_minimal("totally_bogus")),
    case("cam_param_ppb2b_string",         "robust", make=lambda: _cam_param_set({"ppb2b": "abc", "mmpb2b": "xyz"})),
    case("cam_param_ppb2b_object",         "robust", make=lambda: _cam_param_set({"ppb2b": {"nested": 1}, "mmpb2b": [1, 2, 3]})),
    case("cam_param_missing",              "robust", make=lambda: _cam_param_remove()),
    case("intrusion_ratio_huge",           "robust", make=lambda: _toplevel_set({"intrusionSizeLimitRatio": 1e308})),
    case("intrusion_ratio_negative",       "robust", make=lambda: _toplevel_set({"intrusionSizeLimitRatio": -1.0})),
    case("intrusion_ratio_string",         "robust", make=lambda: _toplevel_set({"intrusionSizeLimitRatio": "huge"})),
    case("sig_st1_sim_thres_extreme",      "robust", make=lambda: _toplevel_set({"sig_st1_matching_sim_thres": 1e308,
                                                                                    "sig_match_sim_thres": -1e308,
                                                                                    "sig_relative_match_sim_thres": float('-0.0')})),
    case("back_value_setup_true_no_b",     "robust", make=lambda: _judge_back_value_setup_no_b()),
    case("tag_field_wrong_type",           "robust", make=lambda: _toplevel_set({"tag": "not-an-array"})),
    case("underscore_pt1_corrupt",         "robust", make=lambda: _line_underscore_pt1_corrupt()),
]


# ---------- ROUND 7 helpers ----------

def _first_judge(d):
    """Find a measure/judge feature in the def."""
    return first_of(d, "measure")

def _judge_angle_missing_pt1():
    d = golden(); j = _first_judge(d)
    if j is not None:
        j["subtype"] = "angle"
        j.pop("pt1", None)
        j["quadrant"] = 1
    return d

def _judge_angle_missing_quadrant():
    d = golden(); j = _first_judge(d)
    if j is not None:
        j["subtype"] = "angle"
        j["pt1"] = {"x": 1.0, "y": 2.0}
        j.pop("quadrant", None)
    return d

def _judge_angle_pt1_string():
    d = golden(); j = _first_judge(d)
    if j is not None:
        j["subtype"] = "angle"
        j["pt1"] = "not-a-point"
        j["quadrant"] = 2
    return d

def _judge_circle_info_unknown():
    d = golden(); j = _first_judge(d)
    if j is not None:
        j["subtype"] = "circle_info"
        j["info_type"] = "completely_unknown_info_type"
    return d

def _judge_circle_info_info_obj():
    d = golden(); j = _first_judge(d)
    if j is not None:
        j["subtype"] = "circle_info"
        j["info_type"] = {"nested": "type"}
    return d

def _judge_subtype_minimal(sub):
    d = golden(); j = _first_judge(d)
    if j is not None:
        j["subtype"] = sub
        for k in ("info_type", "quadrant", "calc_f"):
            j.pop(k, None)
    return d

def _judge_distance_no_ref():
    d = golden(); j = _first_judge(d)
    if j is not None:
        j["subtype"] = "distance"
        j["ref"] = []
    return d

def _judge_back_value_setup_no_b():
    d = golden(); j = _first_judge(d)
    if j is not None:
        j["back_value_setup"] = True
        for k in ("value_b", "USL_b", "LSL_b"):
            j.pop(k, None)
    return d

def _cam_param_set(kv):
    d = golden()
    fs = d.get("featureSet")
    if isinstance(fs, list) and fs:
        cp = fs[0].setdefault("cam_param", {})
        for k, v in kv.items(): cp[k] = v
    return d

def _cam_param_remove():
    d = golden()
    fs = d.get("featureSet")
    if isinstance(fs, list) and fs:
        fs[0].pop("cam_param", None)
    return d

def _toplevel_set(kv):
    d = golden()
    s = first_of(d, "sig360_circle_line")
    if s is not None:
        for k, v in kv.items(): s[k] = v
    return d

def _line_underscore_pt1_corrupt():
    # the cached "_pt1" field on lines: corrupt it to confirm parser ignores it
    d = golden(); t = first_of(d, "line")
    if t is not None:
        t["_pt1"] = {"x": "garbage", "y": [1, 2], "vx": None, "dist": {"k": 1}}
        t["_pt2"] = "not-an-object"
    return d

# ===== ROUND 8: stacked multi-bug combos + boundary chains =====

def mk_megacombo_multi_bug():
    # Combine 4 previously-buggy mutations on a single def at once
    d = golden()
    # 1) cyclic search_point chain
    sps = all_of(d, "search_point")
    if len(sps) >= 2:
        for i, s in enumerate(sps):
            nxt = sps[(i+1) % len(sps)]
            s["ref"] = [{"id": nxt.get("id"), "element": "search_point"}]
    # 2) circle_info judge missing info_type
    j = first_of(d, "measure")
    if j is not None:
        j["subtype"] = "circle_info"
        j.pop("info_type", None)
    # 3) overlong calc_f.exp on a second measure (reuse same one + huge exp)
    if j is not None:
        j["calc_f"] = {"exp": "Z" * (512 * 1024), "post_exp": ["[12]", "1", "+"]}
    # 4) line missing pt1
    t = first_of(d, "line")
    if t is not None:
        t.pop("pt1", None)
    return d

def mk_50_anchor_chain():
    # featureSet with 50 search_points all locating_anchor:true forming
    # target_id chain 0->1->2->...->49
    d = golden()
    s = first_of(d, "sig360_circle_line")
    if s is None: return d
    chain = []
    for i in range(50):
        chain.append({
            "type": "search_point",
            "id": 10000 + i,
            "name": f"a{i}",
            "locating_anchor": True,
            "target_id": 10000 + ((i + 1) % 50),
            "anchorPair": [{"id": 10000 + ((i + 1) % 50), "element": "search_point"}],
            "pair_id": 10000 + ((i + 1) % 50),
            "ref": [{"id": 10000 + ((i + 1) % 50), "element": "search_point"}],
            "pt1": {"x": float(i), "y": float(i)},
            "pt2": {"x": float(i+1), "y": float(i+1)},
            "margin": 5, "width": 3, "angleDeg": 0,
        })
    s["features"] = (s.get("features") or []) + chain
    return d

def mk_arc_direction_extreme():
    # arc.direction with extreme float values
    d = golden()
    arcs = all_of(d, "arc")
    for i, a in enumerate(arcs):
        a["direction"] = [1e308, -1e308, 1e-300, -1e-300][i % 4]
    if not arcs:
        a = first_of(d, "arc")
        if a is not None: a["direction"] = 1e308
    return d

def mk_sign360_sigdata_all_null():
    # signature.signature_data is an array of nulls
    d = golden(); s = first_of(d, "sign360")
    if s is not None:
        s["signature"] = {"signature_data": [None] * 200}
    return d

# Raw bytes: tag and name collisions (duplicate keys) across same id
RAW_TAG_NAME_COLLISIONS = ('{"type":"binary_processing_group","featureSet":'
                           '[{"type":"sig360_circle_line","features":'
                           '[{"type":"line","id":1,"tag":"A","tag":"B","tag":"C",'
                            '"name":"n1","name":"n2","name":"n3",'
                            '"pt1":{"x":0,"y":0},"pt2":{"x":1,"y":1}},'
                            '{"type":"arc","id":1,"tag":"X","name":"n1",'
                            '"pt1":{"x":0,"y":0},"pt2":{"x":1,"y":1},"pt3":{"x":2,"y":0}}]}]}')

def mk_id_is_float():
    # feature id is a float (3.5) not an int
    d = golden()
    feats = first_of(d, "sig360_circle_line")["features"]
    for f in feats:
        if isinstance(f, dict) and "id" in f:
            f["id"] = 3.5
            break
    # also set a line's id float
    t = first_of(d, "line")
    if t is not None: t["id"] = 3.5
    return d

def mk_anchor_null_coords():
    # locating_anchor:true on a search_point with NULL anchor coords
    d = golden(); sp = first_of(d, "search_point")
    if sp is not None:
        sp["locating_anchor"] = True
        sp["pt1"] = None
        sp["pt2"] = None
        sp["anchor_pt"] = None
        sp["anchorPair"] = [{"id": None, "element": None}]
    return d

def mk_featureset0_missing_5_present():
    # featureSet[0] not present but featureSet[5] is: a featureSet array
    # where indices 0..4 are null/scalars and index 5 is the real sig360 block
    d = golden()
    real = first_of(d, "sig360_circle_line")
    if real is None: return d
    real_copy = copy.deepcopy(real)
    d["featureSet"] = [None, 0, "skip", [], {}, real_copy]
    return d


CASES += [
    case("megacombo_multi_bug",        "robust", make=mk_megacombo_multi_bug),
    case("anchor_chain_50",            "robust", make=mk_50_anchor_chain),
    case("arc_direction_extreme",      "robust", make=mk_arc_direction_extreme),
    case("sign360_sigdata_all_null",   "robust", make=mk_sign360_sigdata_all_null),
    case("raw_tag_name_collisions",    "robust", raw=RAW_TAG_NAME_COLLISIONS),
    case("id_is_float",                "robust", make=mk_id_is_float),
    case("anchor_null_coords",         "robust", make=mk_anchor_null_coords),
    case("featureSet0_missing_5_pres", "robust", make=mk_featureset0_missing_5_present),
]


# ===== ROUND 9: well-hardened engine — creative valid/boundary cases =====

def mk_100_calc_chain():
    # 100 measures, all subtype=calc, each referencing the previous one via [id].
    # No cycles, valid DAG, exercising parser+calc dispatch under load.
    d = golden()
    s = first_of(d, "sig360_circle_line")
    if s is None: return d
    feats = s["features"]
    base_id = 30000
    chain = []
    for i in range(10):
        mid = base_id + i
        if i == 0:
            post = ["1.0", "2.0", "+"]
            ref = [{"id": mid}]
        else:
            prev = base_id + i - 1
            post = [f"[{prev}]", "1.0", "+"]
            ref = [{"id": prev}]
        chain.append({
            "type": "measure", "id": mid, "name": f"calc_{i}",
            "subtype": "calc", "ref": ref,
            "calc_f": {"exp": "", "post_exp": post},
        })
    s["features"] = feats + chain
    return d

def mk_long_chain_dangling_tail():
    # search_point cycle detector boundary: long chain spN -> spN-1 -> ... -> sp0 -> 99999 (non-existent)
    # Detector must walk N hops without false-positive cycle detection.
    d = golden()
    s = first_of(d, "sig360_circle_line")
    if s is None: return d
    feats = s["features"]
    N = 80
    base = 40000
    chain = []
    for i in range(N):
        sid = base + i
        nxt = (base + i - 1) if i > 0 else 99999  # tail points to non-existent id
        chain.append({
            "type": "search_point", "id": sid, "name": f"sp_chain_{i}",
            "ref": [{"id": nxt, "element": "search_point"}],
            "pt1": {"x": float(i), "y": 0.0},
            "pt2": {"x": float(i+1), "y": 0.0},
            "margin": 5, "width": 3, "angleDeg": 0,
        })
    s["features"] = feats + chain
    return d

def mk_anchor_tree_all_valid():
    # Every search_point locating_anchor:true with valid non-cyclic refs forming a tree
    # Root -> 2 children -> 4 grandchildren (7 total). No cycles.
    d = golden()
    s = first_of(d, "sig360_circle_line")
    if s is None: return d
    feats = s["features"]
    base = 50000
    # parents[i] for i=1..6 -> (i-1)//2
    tree = []
    for i in range(7):
        sid = base + i
        node = {
            "type": "search_point", "id": sid, "name": f"anc_{i}",
            "locating_anchor": True,
            "pt1": {"x": float(i), "y": float(i)},
            "pt2": {"x": float(i+1), "y": float(i+1)},
            "margin": 5, "width": 3, "angleDeg": 0,
        }
        if i > 0:
            parent = base + (i - 1) // 2
            node["ref"] = [{"id": parent, "element": "search_point"}]
            node["anchorPair"] = [{"id": parent, "element": "search_point"}]
            node["pair_id"] = parent
        tree.append(node)
    s["features"] = feats + tree
    return d

def mk_arc_degenerate_circumcenter():
    # arc whose pt2 is exactly collinear with pt1 and pt3 (degenerate circumcenter — infinite radius)
    d = golden(); a = first_of(d, "arc")
    if a is not None:
        a["pt1"] = {"x": 0.0, "y": 0.0}
        a["pt2"] = {"x": 5.0, "y": 0.0}     # exactly on segment pt1->pt3
        a["pt3"] = {"x": 10.0, "y": 0.0}
    return d

def mk_sign360_sigdata_empty():
    # sign360 with signature_data of length 0 (matching_without_signature path)
    d = golden(); s = first_of(d, "sign360")
    if s is not None:
        s["signature"] = {"signature_data": []}
    return d

def mk_12_measures_same_name():
    # 12 measures sharing the same "name" — cJSON ids unique, name uniqueness violated
    d = golden()
    s = first_of(d, "sig360_circle_line")
    if s is None: return d
    feats = s["features"]
    base = 60000
    new = []
    for i in range(12):
        new.append({
            "type": "measure", "id": base + i,
            "name": "duplicated_name",          # all same name
            "subtype": "calc",
            "ref": [{"id": base + i}],
            "calc_f": {"exp": "", "post_exp": ["1.0", "2.0", "+"]},
        })
    s["features"] = feats + new
    return d

def mk_valid_dag_id_name_subtype_combo():
    # Stress hardened parser fields together: unique ids, mixed unicode names,
    # explicit subtypes — all should be accepted cleanly.
    d = golden()
    s = first_of(d, "sig360_circle_line")
    if s is None: return d
    feats = s["features"]
    base = 70000
    new = []
    for i in range(20):
        new.append({
            "type": "measure", "id": base + i,
            "name": f"m_é中_{i}",
            "subtype": "calc",
            "ref": [{"id": base + i}],
            "calc_f": {"exp": "", "post_exp": [f"{i}.0", "1.0", "+"]},
        })
    s["features"] = feats + new
    return d

def mk_calc_wide_fanin():
    # one calc measure referencing 50 distinct previously-defined measures
    d = golden()
    s = first_of(d, "sig360_circle_line")
    if s is None: return d
    feats = s["features"]
    base = 80000
    leaves = []
    for i in range(50):
        leaves.append({
            "type": "measure", "id": base + i, "name": f"leaf_{i}",
            "subtype": "calc",
            "ref": [{"id": base + i}],
            "calc_f": {"exp": "", "post_exp": [f"{i}.0", "1.0", "+"]},
        })
    # aggregator references all leaves
    post = []
    for i in range(50):
        post.append(f"[{base + i}]")
        if i > 0: post.append("+")
    agg = {
        "type": "measure", "id": base + 999, "name": "agg",
        "subtype": "calc",
        "ref": [{"id": base + i} for i in range(50)],
        "calc_f": {"exp": "", "post_exp": post},
    }
    s["features"] = feats + leaves + [agg]
    return d


CASES += [
    case("calc_chain_100_valid",       "robust", make=mk_100_calc_chain),
    case("sp_long_chain_dangling_tail","robust", make=mk_long_chain_dangling_tail),
    case("anchor_tree_all_valid",      "robust", make=mk_anchor_tree_all_valid),
    case("arc_degenerate_circumcenter","robust", make=mk_arc_degenerate_circumcenter),
    case("sign360_sigdata_empty",      "robust", make=mk_sign360_sigdata_empty),
    case("twelve_measures_same_name",  "robust", make=mk_12_measures_same_name),
    case("valid_dag_id_name_subtype",  "robust", make=mk_valid_dag_id_name_subtype_combo),
    case("calc_wide_fanin_50",         "robust", make=mk_calc_wide_fanin),
]


# ===== ROUND 10: final torture — combined cycles, embedded BOM, NaN-string sig data =====

def mk_empty_pt1_object():
    # pt1 present but {} (empty object) rather than missing or wrong-typed
    d = golden(); t = first_of(d, "line")
    if t is not None:
        t["pt1"] = {}
        t["pt2"] = {}
    return d

def mk_embedded_bom_midstring():
    # UTF-8 BOM bytes embedded mid-string across multiple text fields
    d = golden()
    BOM = "﻿"
    t = first_of(d, "line")
    if t is not None:
        t["name"] = f"abc{BOM}def{BOM}{BOM}ghi"
        t["tag"]  = f"{BOM}{BOM}{BOM}"
    s = first_of(d, "sign360")
    if s is not None:
        s["name"] = f"sig{BOM}name{BOM}with{BOM}boms"
    sp = first_of(d, "search_point")
    if sp is not None:
        sp["name"] = f"{BOM}sp{BOM}"
    return d

def mk_triple_cycle_all_at_once():
    # Combine: search_point cycle + measure-calc cycle + aux cycle on one def
    d = golden()
    # 1) search_point cycle
    sps = all_of(d, "search_point")
    if len(sps) >= 2:
        for i, s in enumerate(sps):
            nxt = sps[(i+1) % len(sps)]
            s["ref"] = [{"id": nxt.get("id"), "element": "search_point"}]
    # 2) measure calc self-cycle
    m = first_of(d, "measure")
    if m is not None:
        mid = m.get("id", 12)
        m["subtype"] = "calc"
        m["ref"] = [{"id": mid}]
        m["calc_f"] = {"exp": "", "post_exp": [f"[{mid}]", f"[{mid}]", "+"]}
    # 3) aux cycle
    ap = first_of(d, "aux_point"); al = first_of(d, "aux_line")
    if ap and al:
        ap["ref"] = [{"id": al.get("id"), "element": "aux_line"}]
        al["ref"] = [{"id": ap.get("id"), "element": "aux_point"}]
    return d

def mk_sign360_sigdata_nan_string_entries():
    # signature_data array containing NaN-as-string and Inf-as-string entries
    d = golden(); s = first_of(d, "sign360")
    if s is not None:
        s["signature"] = {"signature_data": [
            1.0, "NaN", 2.0, "Infinity", 3.0, "-Infinity", "nan", "+inf", 4.0
        ] * 50}
    return d

def mk_huge_valid_def_with_malformed_subdefs():
    # Large but mostly valid def replicated, intermixed with malformed sub-feats
    d = golden()
    s = first_of(d, "sig360_circle_line")
    if s is None: return d
    feats = s["features"]
    base = 90000
    add = []
    for i in range(2000):
        if i % 7 == 0:
            # valid line
            add.append({"type": "line", "id": base + i,
                        "pt1": {"x": float(i), "y": 0.0},
                        "pt2": {"x": float(i+1), "y": 1.0}})
        elif i % 7 == 1:
            # valid measure calc
            add.append({"type": "measure", "id": base + i,
                        "subtype": "calc", "ref": [{"id": base + i}],
                        "calc_f": {"exp": "", "post_exp": ["1.0", "2.0", "+"]}})
        elif i % 7 == 2:
            # malformed: pt1 is empty object
            add.append({"type": "line", "id": base + i,
                        "pt1": {}, "pt2": {"x": 1, "y": 1}})
        elif i % 7 == 3:
            # malformed: unknown type
            add.append({"type": f"junk_{i}", "id": base + i})
        elif i % 7 == 4:
            # malformed: dangling ref
            add.append({"type": "search_point", "id": base + i,
                        "ref": [{"id": 9999999, "element": "line"}],
                        "pt1": {"x": 0, "y": 0}, "pt2": {"x": 1, "y": 1},
                        "margin": 5, "width": 3})
        elif i % 7 == 5:
            # malformed: scalar feature
            add.append(i)
        else:
            # valid arc-ish
            add.append({"type": "arc", "id": base + i,
                        "pt1": {"x": 0, "y": 0}, "pt2": {"x": 5, "y": 5},
                        "pt3": {"x": 10, "y": 0}})
    s["features"] = feats + add
    return d

def mk_pt_object_only_extra_keys():
    # pt1 object lacking x and y, but populated with bogus sibling keys
    d = golden(); t = first_of(d, "line")
    if t is not None:
        t["pt1"] = {"foo": 1, "bar": "baz", "X": 9.0, "Y": 8.0}  # capitalization wrong
        t["pt2"] = {"xx": 0, "yy": 0}
    return d

def mk_bom_truncated_combo():
    # mk_embedded_bom_midstring + truncation of features list mid-feature
    d = mk_embedded_bom_midstring()
    s = first_of(d, "sig360_circle_line")
    if s is not None and isinstance(s.get("features"), list) and len(s["features"]) > 3:
        # leave only the BOM-corrupted ones + half-malformed entries
        s["features"] = s["features"][:3] + [{"type": "line", "id": 5,
                                              "pt1": {}, "name": "﻿x﻿"}]
    return d

# raw: a huge mix — BOM + NaN literals + dup keys + huge string + cycle ref
RAW_TORTURE_COMBO = (
    "﻿" +
    '{"type":"binary_processing_group","featureSet":'
    '[{"type":"sig360_circle_line","features":'
    '[{"type":"line","id":1,"id":1,"name":"' + ("﻿" + "x") * 1000 + '",'
    '"pt1":{"x":NaN,"y":Infinity},"pt2":{"x":-Infinity,"y":0}},'
    '{"type":"search_point","id":2,"ref":[{"id":2,"element":"search_point"}],'
    '"pt1":{},"pt2":{"x":1,"y":1}},'
    '{"type":"measure","id":3,"subtype":"calc","ref":[{"id":3}],'
    '"calc_f":{"exp":"","post_exp":["[3]","[3]","+"]}}'
    ']}]}'
)


CASES += [
    case("empty_pt1_object",            "robust", make=mk_empty_pt1_object),
    case("embedded_bom_midstring",      "robust", make=mk_embedded_bom_midstring),
    case("triple_cycle_all_at_once",    "robust", make=mk_triple_cycle_all_at_once),
    case("sign360_sigdata_nan_strings", "robust", make=mk_sign360_sigdata_nan_string_entries),
    case("huge_def_malformed_subdefs",  "robust", make=mk_huge_valid_def_with_malformed_subdefs),
    case("pt_object_only_extra_keys",   "robust", make=mk_pt_object_only_extra_keys),
    case("bom_truncated_combo",         "robust", make=mk_bom_truncated_combo),
    case("raw_torture_combo",           "robust", raw=RAW_TORTURE_COMBO),
]


# ===== BONUS: combinatorial multi-bug-class interaction fuzz =====

def mk_combo_caliper_circleinfo_spcycle():
    # caliper huge count + circle_info on a line ref + search_point cycle, all in one def
    d = golden()
    # caliper huge count on a line
    t = first_of(d, "line")
    if t is not None:
        t["caliper"] = {"count": 99999, "step": 1, "length": 10, "width": 5}
    # circle_info judge whose ref points to a line (cross-type)
    j = first_of(d, "measure")
    line_id = t.get("id", 1) if t else 1
    if j is not None:
        j["subtype"] = "circle_info"
        j["info_type"] = "radius"
        j["ref"] = [{"id": line_id, "element": "line"}]
    # search_point cycle
    sps = all_of(d, "search_point")
    if len(sps) >= 2:
        for i, s in enumerate(sps):
            nxt = sps[(i+1) % len(sps)]
            s["ref"] = [{"id": nxt.get("id"), "element": "search_point"}]
    return d

def mk_combo_calc_chain_to_circleinfo_on_line():
    # CALC chain terminating at a circle_info measure whose ref is a line (NA result)
    d = golden()
    s = first_of(d, "sig360_circle_line")
    if s is None: return d
    t = first_of(d, "line")
    line_id = t.get("id", 1) if t else 1
    base = 41000
    # circle_info on a line -> NA
    ci = {"type": "measure", "id": base, "name": "ci_na",
          "subtype": "circle_info", "info_type": "radius",
          "ref": [{"id": line_id, "element": "line"}]}
    # chain of calcs pointing at ci, propagating NA forward
    chain = [ci]
    for i in range(1, 20):
        mid = base + i
        prev = base + i - 1
        chain.append({
            "type": "measure", "id": mid, "name": f"prop_{i}",
            "subtype": "calc", "ref": [{"id": prev}],
            "calc_f": {"exp": "", "post_exp": [f"[{prev}]", "1.0", "+"]},
        })
    s["features"] = s["features"] + chain
    return d

def mk_combo_multi_controlled_rejects():
    # Multiple controlled-reject conditions stacked; parser fails fast on first.
    # No SIGSEGV regardless of which one wins.
    d = golden()
    # caliper huge count
    t = first_of(d, "line")
    if t is not None:
        t["caliper"] = {"count": 99999}
    # FIFO def collision (duplicate ids)
    feats = first_of(d, "sig360_circle_line")["features"]
    for f in feats:
        if isinstance(f, dict) and "id" in f: f["id"] = 7
    # 1x1 effectively-empty post_exp on calc
    m = first_of(d, "measure")
    if m is not None:
        m["subtype"] = "calc"
        m["ref"] = [{"id": 7}]
        m["calc_f"] = {"exp": "", "post_exp": []}    # malformed: empty RPN
    return d

def mk_combo_deep_valid_dag_fanin_via_auxchain():
    # Accepted def: 50 calc leaves fan-in into one distance measure whose ref
    # walks aux_point -> aux_line -> caliper-bearing line. Deep valid DAG.
    d = golden()
    s = first_of(d, "sig360_circle_line")
    if s is None: return d
    # Anchor: a caliper-bearing line
    base = 55000
    line = {"type": "line", "id": base, "name": "anchor_line",
            "pt1": {"x": 0.0, "y": 0.0}, "pt2": {"x": 100.0, "y": 0.0},
            "caliper": {"count": 5, "step": 10, "length": 20, "width": 3}}
    # aux_line referencing the line
    aux_line = {"type": "aux_line", "id": base + 1, "name": "al_chain",
                "subtype": "twoPoint",
                "pt1": {"x": 0.0, "y": 5.0}, "pt2": {"x": 100.0, "y": 5.0},
                "ref": [{"id": base, "element": "line"}]}
    # aux_point referencing the aux_line
    aux_pt = {"type": "aux_point", "id": base + 2, "name": "ap_chain",
              "subtype": "lineCross",
              "ref": [{"id": base + 1, "element": "aux_line"},
                      {"id": base, "element": "line"}]}
    # distance measure referencing aux_pt
    dist = {"type": "measure", "id": base + 3, "name": "dist_target",
            "subtype": "distance",
            "ref": [{"id": base + 2, "element": "aux_point"},
                    {"id": base, "element": "line"}]}
    # 50 calc leaves all referencing dist
    leaves = []
    for i in range(50):
        mid = base + 100 + i
        leaves.append({
            "type": "measure", "id": mid, "name": f"fanleaf_{i}",
            "subtype": "calc",
            "ref": [{"id": base + 3}],
            "calc_f": {"exp": "", "post_exp": [f"[{base + 3}]", f"{i}.0", "+"]},
        })
    s["features"] = s["features"] + [line, aux_line, aux_pt, dist] + leaves
    return d

def mk_combo_all_crash_classes_simultaneous():
    # Everything bad at once: cyclic sp + missing circle_info info_type + line w/o id
    # + malformed RPN + caliper count=99999. Engine should controlled-reject.
    d = golden()
    # cyclic search_point
    sps = all_of(d, "search_point")
    if len(sps) >= 2:
        for i, sp in enumerate(sps):
            nxt = sps[(i+1) % len(sps)]
            sp["ref"] = [{"id": nxt.get("id"), "element": "search_point"}]
    # circle_info missing info_type
    j = first_of(d, "measure")
    if j is not None:
        j["subtype"] = "circle_info"
        j.pop("info_type", None)
        # malformed RPN postfix on same measure
        j["calc_f"] = {"exp": "", "post_exp": ["@@@", None, "/", "[]"]}
    # line missing id
    t = first_of(d, "line")
    if t is not None:
        t.pop("id", None)
        t["caliper"] = {"count": 99999, "step": 1}
    return d

def mk_combo_calc_self_ref_caliper_circleinfo():
    # calc self-ref + caliper huge + circle_info wrong-type combined
    d = golden()
    m = first_of(d, "measure")
    if m is not None:
        mid = m.get("id", 12)
        m["subtype"] = "calc"
        m["ref"] = [{"id": mid}]
        m["calc_f"] = {"exp": "", "post_exp": [f"[{mid}]", f"[{mid}]", "*"]}
    # second measure as circle_info on line
    s = first_of(d, "sig360_circle_line")
    t = first_of(d, "line")
    if s and t:
        s["features"].append({
            "type": "measure", "id": 31337, "name": "ci_x",
            "subtype": "circle_info", "info_type": "diameter",
            "ref": [{"id": t.get("id", 1), "element": "line"}],
        })
    # caliper huge on the line
    if t is not None:
        t["caliper"] = {"count": 99999, "step": 1, "length": 5, "width": 2}
    return d

def mk_combo_sp_cycle_calc_cycle_caliper():
    # search_point cycle + calc cycle + caliper huge + duplicate ids
    d = golden()
    # sp cycle
    sps = all_of(d, "search_point")
    if len(sps) >= 2:
        for i, sp in enumerate(sps):
            nxt = sps[(i+1) % len(sps)]
            sp["ref"] = [{"id": nxt.get("id"), "element": "search_point"}]
    # calc cycle: two measures referencing each other
    s = first_of(d, "sig360_circle_line")
    if s is not None:
        a_id, b_id = 61001, 61002
        s["features"].append({
            "type": "measure", "id": a_id, "subtype": "calc",
            "ref": [{"id": b_id}],
            "calc_f": {"exp": "", "post_exp": [f"[{b_id}]", "1.0", "+"]},
        })
        s["features"].append({
            "type": "measure", "id": b_id, "subtype": "calc",
            "ref": [{"id": a_id}],
            "calc_f": {"exp": "", "post_exp": [f"[{a_id}]", "1.0", "+"]},
        })
    # caliper huge
    t = first_of(d, "line")
    if t is not None:
        t["caliper"] = {"count": 99999}
    return d

def mk_combo_circleinfo_chain_aux_to_line():
    # circle_info judge with ref-chain that walks measure->aux_point->aux_line->line
    # (cross-type ref at multiple hops; engine should NA gracefully)
    d = golden()
    s = first_of(d, "sig360_circle_line")
    if s is None: return d
    t = first_of(d, "line")
    if t is None: return d
    line_id = t.get("id", 1)
    al_id, ap_id, ci_id = 70001, 70002, 70003
    s["features"].append({
        "type": "aux_line", "id": al_id, "subtype": "twoPoint",
        "pt1": {"x": 0, "y": 0}, "pt2": {"x": 1, "y": 1},
        "ref": [{"id": line_id, "element": "line"}],
    })
    s["features"].append({
        "type": "aux_point", "id": ap_id, "subtype": "lineCross",
        "ref": [{"id": al_id, "element": "aux_line"},
                {"id": line_id, "element": "line"}],
    })
    s["features"].append({
        "type": "measure", "id": ci_id, "name": "ci_chain",
        "subtype": "circle_info", "info_type": "center_x",
        "ref": [{"id": ap_id, "element": "aux_point"}],
    })
    return d


CASES += [
    case("combo_caliper_circleinfo_spcycle",     "robust", make=mk_combo_caliper_circleinfo_spcycle),
    case("combo_calc_chain_to_circleinfo_line",  "robust", make=mk_combo_calc_chain_to_circleinfo_on_line),
    case("combo_multi_controlled_rejects",       "robust", make=mk_combo_multi_controlled_rejects),
    case("combo_deep_valid_dag_fanin50",         "robust", make=mk_combo_deep_valid_dag_fanin_via_auxchain),
    case("combo_all_crash_classes_simul",        "robust", make=mk_combo_all_crash_classes_simultaneous),
    case("combo_calcself_caliper_circleinfo",    "robust", make=mk_combo_calc_self_ref_caliper_circleinfo),
    case("combo_spcycle_calccycle_caliper",      "robust", make=mk_combo_sp_cycle_calc_cycle_caliper),
    case("combo_circleinfo_chain_aux_to_line",   "robust", make=mk_combo_circleinfo_chain_aux_to_line),
]


if __name__ == "__main__":
    sys.exit(run_module("qa_parse", CASES))
