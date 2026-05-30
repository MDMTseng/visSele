// Per-shape property-sheet schema dispatcher (keystone phase B).
//
// Each shape module (shapes/<type>.js) owns its own `buildWhiteListKey(ctx)`
// returning JUST its distinctive keys. This file merges them with the BASE
// schema (the common keys every shape has) and exposes one entry point,
// `buildSchema(edit_tar, ctx)`, that DefConfUI.GenTarEditUI calls inside
// useMemo for render-stability.
//
// History: phase 2 (commit 15e0153b) extracted the giant inline `whiteListKey`
// literal from DefConfUI into a `buildSharedSchema` + `buildDefaultSchema`
// here. Phase B (this commit) dissolves those into per-shape modules.

import { getShapeModule } from '.';

// Base keys every shape's property sheet shows. JsonEditBlock filters by
// which keys actually exist on the shape, so present-on-all is the rule.
function buildBaseSchema() {
  return {
    type: 'div',
    subtype: 'div',
    name: 'input',
    margin: 'SimpleSetup',
  };
}

// Build the whiteListKey for the given shape. Merges base + per-shape slice
// (line/arc/search_point/measure each contribute their distinctive keys; for
// measure, the slice further dispatches by subtype for sub-specific keys
// like info_type and calc_f). Unregistered types get just the base.
export function buildSchema(edit_tar, ctx) {
  const mod = getShapeModule(edit_tar.type);
  const fullCtx = { ...ctx, edit_tar };
  const slice = (mod && mod.buildWhiteListKey) ? mod.buildWhiteListKey(fullCtx) : {};
  return { ...buildBaseSchema(), ...slice };
}
