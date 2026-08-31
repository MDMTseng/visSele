// Drag the InspectionUI canvas and make sure it does not throw us out.
//
// A shape_based def has an empty inherentShapeList (no signature), and the
// canvas's ctrlLogic used to read that as a corrupt def and raise ERROR, which
// the state machine answers by leaving for MAIN. Zoom is draw() only, so it
// never fired -- it was the first DRAG that ended the inspection.
//   node regress_insp_drag.mjs [recipe]
//
// Needs a recipe with NO signature -- i.e. one built through TAKE -> SBM. The
// defs on this bench were all migrated from sig360 and still carry one, so the
// default is a local copy with its @__SIGNATURE__ entries stripped (see the
// note at the bottom); point it at any new-flow recipe instead.
import { makeCtl, toMain, dismissCamModal, loadRecipe, enterInspection, freshPage } from './lib_enter.mjs';
import { makeProbe, makeTally, sleep } from './_rf_lib.mjs';
const ctl = makeCtl('http://127.0.0.1:8765'); const { api, ev } = ctl;
const P = makeProbe(ev); const T = makeTally(); const { ok } = T;
const SM = 'window.__GP_STORE__.getState().UIData.c_state.value';
const model = process.argv[2] || 'data/_dragcheck';

await freshPage(ctl, 'http://127.0.0.1:8081/');
await P.waitFor('app', async () => (await ev(`typeof window.__GP_STORE__`)) === 'object', { timeout: 40000 });
await toMain(ctl); await dismissCamModal(ctl); await loadRecipe(ctl, model);
await P.waitFor('def in store', async () =>
  !!(await P.store(`((window.__GP_STORE__.getState().UIData.edit_info||{}).loadedDefFile||{}).featureSet`)),
  { timeout: 20000 });
ok('the def has no inherent shapes (the case that used to fail)',
   ((await P.store(`window.__GP_STORE__.getState().UIData.edit_info.inherentShapeList||[]`)) || []).length === 0);
await enterInspection(ctl, { mode: '測試' });
ok('the inspection screen opens',
   await P.waitFor('INSP_MODE', async () =>
     JSON.stringify(await P.store(SM)).indexOf('INSP_MODE') >= 0, { timeout: 30000 }));
// The canvas has to be laid out before it can be dragged; its own size says so.
await P.waitFor('canvas laid out', () => ev(
  `(function(){var c=document.querySelector('canvas'); if(!c) return false;
     var r=c.getBoundingClientRect(); return r.width>100 && r.height>100;})()`), { timeout: 15000 });
const box = await ev(`(function(){var c=document.querySelector('canvas'); if(!c) return '0';
  var r=c.getBoundingClientRect(); return JSON.stringify({x:r.x,y:r.y,w:r.width,h:r.height});})()`);
const b = JSON.parse(box);
const cx = Math.round(b.x + b.w/2), cy = Math.round(b.y + b.h/2);
await api('/drag', { x1: cx, y1: cy, x2: cx + 120, y2: cy + 60, steps: 12 });
// The kick-out, when it happens, is immediate -- one ERROR through the state
// machine. Half a second is a generous margin for "it did not happen".
await sleep(500);
const st = JSON.stringify(await P.store(SM));
ok('a drag does not end the inspection', st.indexOf('INSP_MODE') >= 0, `state=${st}`);
// Making the fixture, if data/_dragcheck is missing:
//   copy a shape_based .hydef, drop every inherentfeatures entry whose name
//   starts with @__SIGNATURE__, copy its .png alongside, then recompute
//   featureSet_sha1 (JSum.digest over featureSet with __-prefixed keys removed,
//   see InspectionEditorLogic) or the loader refuses the file.
await toMain(ctl).catch(() => {});   // leave MAIN warm for the next suite
process.exit(T.done() ? 1 : 0);
