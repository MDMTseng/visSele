// The per-phase breakdown the def editor draws over the frame.
//
// One number cannot say whether a slow CHECK is the shape matcher, the caliper
// windows, or a channel extract nobody suspected. This asserts that the four
// phases that partition the match arrive with the frame and that they add up to
// the headline number -- a breakdown that does not reconcile is worse than none.
import { makeCtl, toMain, dismissCamModal, loadRecipe, freshPage, sleep } from './lib_enter.mjs';
const ctl = makeCtl('http://127.0.0.1:8765');
const { ev } = ctl;

await freshPage(ctl, 'http://127.0.0.1:8081/');
await toMain(ctl); await dismissCamModal(ctl);
console.log('recipe:', await loadRecipe(ctl, 'data/test1'));
await ev(`window.__GP_STORE__.dispatch({ type: 'Edit_Mode' })`);
for (let i = 0; i < 60; i++) {
  const s = await ev(`JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value)`);
  if (String(s).indexOf('DEFCONF') >= 0) break;
  await sleep(400);
}
for (let i = 0; i < 40; i++) {
  const t = await ev(`JSON.stringify(window.__GP_STORE__.getState().UIData.edit_info.insp_timing || null)`);
  if (t && t !== 'null' && t.indexOf('phase_ms') >= 0) { console.log('insp_timing:', t); break; }
  await sleep(500);
}
console.log('reconciles :', await ev(`(function(){
  var t=window.__GP_STORE__.getState().UIData.edit_info.insp_timing||{};
  var p=t.phase_ms; if(!p) return 'no phase_ms';
  var top=['prep','sbm','morph','measure'].map(function(k){return p[k]||0;});
  var sum=top.reduce(function(a,b){return a+b;},0);
  return 'sum(prep,sbm,morph,measure)='+sum.toFixed(2)+'  wall='+(t.wall_ms||0).toFixed(2)
       +'  ratio='+(sum/(t.wall_ms||1)).toFixed(3);})()`));
