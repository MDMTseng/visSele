// Per-shape property-sheet schema (keystone step 2).
//
// Extracted verbatim from DefConfUI.GenTarEditUI so the inline literal no
// longer re-creates the `whiteListKey` object on every render — JsonEditBlock
// can now compare-by-reference (via useMemo at the call site) and stop
// remounting subtrees, which had been losing editor sub-state (Round-3 finding).
//
// Two builders today; deeper per-shape decomposition (each type owning ONLY its
// distinctive keys, no shared union) is a follow-up — keeping behavior
// byte-identical to the legacy inline literal first.

import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';

// Used for: line, aux_point, search_point, measure (the "shared big" schema in
// the legacy code). Includes every possible key across those types;
// JsonEditBlock filters by which keys actually exist on the shape (which is
// per-type, per Shape_Attr_Fill).
//
// ctx = { edit_tar, shape_list, renderMethods, refChainHasLoop, ACT_EDIT_TAR_ELE_TRACE_UPDATE }
export function buildSharedSchema(ctx) {
  const { edit_tar, shape_list, renderMethods, refChainHasLoop, ACT_EDIT_TAR_ELE_TRACE_UPDATE } = ctx;
  return {
    // id: "div",
    type: "div",
    subtype: "div",
    name: "input",
    //pt1:null,
    angleDeg: "AngleRangeSetup",
    margin: "SimpleSetup",

    search_far: "switch",
    locating_anchor: "switch",

    vertex_touch_searching: "switch",
    // caliper locating mode for line/arc: "contour" (legacy) | "caliper".
    // Only line/arc shapes carry `locating` (set in Shape_Attr_Fill), so this
    // control only appears for them. Caliper uses sane defaults
    // (strongest/falling edge, count/width/length/step) unless a def overrides.
    locating: {
      __OBJ__: renderMethods.Dropdown_List,
      list: ["contour", "caliper"],
    },
    // line_thickness_value:"input-number",

    info_type: {
      __OBJ__: renderMethods.Dropdown_List,
      list: Object.keys(UIAct.SHAPE_TYPE._circle_info_type),
    },

    calc_f: {
      __OBJ__: renderMethods.Measure_Calc_Editor,
      measure_list: shape_list.filter(s =>
        (s.type == UIAct.SHAPE_TYPE.measure)
        && !refChainHasLoop(edit_tar, s, shape_list)
      ),
      ref_keyTrace_callback: (keyTrace) => {
        ACT_EDIT_TAR_ELE_TRACE_UPDATE(keyTrace);
      },
      ref: edit_tar.ref
    },
    value: "input-number",
    USL: "ULRangeSetup",
    LSL: "ULRangeSetup",
    UCL: "ULRangeSetup",
    LCL: "ULRangeSetup",

    value_b: "input-number",
    USL_b: "ULRangeSetup",
    LSL_b: "ULRangeSetup",
    UCL_b: "ULRangeSetup",
    LCL_b: "ULRangeSetup",

    back_value_setup: "switch",
    importance: "input-number",
    width: "SimpleSetup",

    quality_essential: "switch",
    orientation_essential: "switch",
    NGasNA: "switch",
    NAasNG: "switch",
    value_A: "input-number",
    value_B: "input-number",
    value_X: "input-number",
    value_Y: "input-number",
    ref: (edit_tar.subtype === UIAct.SHAPE_TYPE.measure_subtype.calc) ?
      undefined :
      {
        __OBJ__: "div",
        ...[0, 1, 2].reduce((acc, key) => {
          acc[key + ""] =
          {
            __OBJ__: "btn",
            id: "div",
            element: "div"
          };
          return acc;
        }, {})

      },
    ref_baseLine: {
      __OBJ__: "btn",
      id: "div",
      element: "div"
    }
  };
}

// Used for: arc + any unregistered type (the legacy default branch).
export function buildDefaultSchema(edit_tar) {
  const whiteListKey = {
    //id:"div",
    type: "div",
    subtype: "div",
    name: "input",
    margin: "SimpleSetup"
  };
  if (edit_tar.type == UIAct.SHAPE_TYPE.arc) {
    // arc-only: direction switch (kept here verbatim; future per-shape work
    // would move this into shapes/arc.js's own schema contribution).
    whiteListKey.direction = "switch";
  }
  return whiteListKey;
}
