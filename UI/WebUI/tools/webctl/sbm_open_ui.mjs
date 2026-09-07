// OPENING THE STUDIO MUST NOT LOOK LIKE A FAILED EXTRACTION.
//
// SBMStudio2 sends an SF on mount to ask the core what the def already locates
// with -- deliberately WITHOUT `regenerate`, so nothing is re-extracted. The
// core answers from getShapeFeaturePointsJson, which reads shape_feat_mm /
// shape_roi_mm and only emits shape_cache when shape_cache_fp is set.
//
// A def loaded through the self-contained path filled none of those, so the
// answer was empty and the studio -- which cannot tell "nothing to show" from
// "extraction produced nothing" -- greeted a working def with 生成特徵失敗.
import { makeCtl, toMain, dismissCamModal, loadRecipe, freshPage, sleep } from './lib_enter.mjs';
const ctl = makeCtl('http://127.0.0.1:8765'); const { ev, api } = ctl;
const APP = process.argv[2] || 'http://127.0.0.1:8083/';
const DEF = process.argv[3] || 'data/test1';
const has = (s) => ev(`!!document.querySelector(${JSON.stringify(s)})`);

process.env.WEBCTL_COLD = '1';
await freshPage(ctl, APP);
await toMain(ctl); await dismissCamModal(ctl);
if ((await loadRecipe(ctl, DEF)) === null) { console.log('FAIL: ' + DEF + ' did not load'); process.exit(1); }
await ev(`window.__GP_STORE__.dispatch({ type: 'Edit_Mode' })`);
for (let i = 0; i < 60; i++) {
  const s = await ev(`JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value)`);
  if (String(s).indexOf('DEFCONF') >= 0) break; await sleep(400);
}
// VISIBLE, not merely present: antd keeps the closed drawer in the DOM, so a
// querySelector hit clicks a hidden button and times out.
const drawerOpen = () => ev(`(function(){var m=document.querySelector('.ant-drawer-mask');
  if(!m) return false; var r=m.getBoundingClientRect();
  return r.height>10 && getComputedStyle(m).display!=='none';})()`);
if (await drawerOpen()) { await api('/click', { selector: '.ant-drawer-close' }); await sleep(1500); }
await sleep(2500);

// Unlock. The studio button is not rendered under a def lock -- "an editor that
// cannot edit should not be reachable at all" -- and the harness enters locked.
await ev(`window.__GP_STORE__.dispatch({ type: 'DefConf_Lock_Level_Update', data: 0 })`);
await sleep(1200);
if (!(await has('[data-testid="sbm-studio-v2"]'))) {
  console.log('FAIL: the SBM studio button is still not rendered (lock='
    + (await ev(`window.__GP_STORE__.getState().UIData.defConf_lock_level`)) + ')');
  process.exit(1);
}
await api('/click', { selector: '[data-testid="sbm-studio-v2"]' });
await sleep(7000);

// The dialog is an antd Modal.error; find it by its title text.
const err = await ev(`(function(){
  var t = Array.from(document.querySelectorAll('.ant-modal-confirm-title,.ant-modal-title'))
            .map(function(e){ return (e.textContent||'').trim(); });
  return JSON.stringify(t);})()`);
const feats = await ev(`(function(){
  var ei=window.__GP_STORE__.getState().UIData.edit_info;
  var sc=ei.__shape_cache;
  return JSON.stringify({ cache: sc? (sc.roi?'roi':'no-roi') : 'none',
                          featPts: (window.__SBM_FEAT_N__===undefined)?null:window.__SBM_FEAT_N__ });})()`);

let fail = 0;
const bad = /生成特徵失敗|還沒有樣板影像/.test(err);
console.log('  ' + (bad ? 'FAIL' : 'ok  ') + ' dialogs on open: ' + err);
if (bad) fail++;
console.log('  ---- ' + feats);
console.log(fail ? '\nFAILED' : '\nPASS');
process.exit(fail ? 1 : 0);
