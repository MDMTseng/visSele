// cam_param must survive a load -> regenerate round trip.
//
//   node regress_camparam_roundtrip.mjs [recipe ...]
//
// The def file's cam_param carries fields the editor does not own --
// exposure_time is the one that bit. A signature-less def (TAKE -> SBM)
// regenerates cam_param from the EDITOR's copy, and on a real machine that copy
// is the live camera_calibration report, which has no exposure_time. Two
// consequences: the field is dropped the next time the WebUI saves, and every
// exit from the editor warns "變更的欄位 cam_param" on a def nobody touched --
// saving does not settle it, because the core stamps the field back.
//
// A fake-camera bench never emits camera_calibration, so the editor keeps the
// def's own cam_param and the bug cannot happen here: the probe below installs
// a live-looking camera param first, which is what makes this test able to fail.
import { makeCtl, toMain, dismissCamModal, loadRecipe, freshPage } from './lib_enter.mjs';
import { makeProbe, sleep } from './_rf_lib.mjs';
const ctl = makeCtl('http://127.0.0.1:8765'); const { api, ev } = ctl; const P = makeProbe(ev);
await freshPage(ctl, 'http://127.0.0.1:8081/');
await P.waitFor('app', async () => (await ev(`typeof window.__GP_STORE__`)) === 'object', { timeout: 40000 });
await toMain(ctl); await dismissCamModal(ctl);
let fails = 0;
for (const m of (process.argv.slice(2).length ? process.argv.slice(2)
                 : ['data/test1', 'data/_dragcheck'])) {
  await loadRecipe(ctl, m);
  // Wait for the def to be IN the editor rather than for a clock: loadRecipe
  // returns when the load promise settles, and the editor state follows.
  await P.waitFor('def in store', async () =>
    !!(await P.store(`((window.__GP_STORE__.getState().UIData.edit_info||{}).loadedDefFile||{}).featureSet`)),
    { timeout: 20000 });
  await ev(`window.__GP_STORE__.dispatch({ type: 'Edit_Mode' })`);
  await P.waitFor('DEFCONF', async () =>
    JSON.stringify(await P.store(`window.__GP_STORE__.getState().UIData.c_state.value`))
      .indexOf('DEFCONF_MODE') >= 0, { timeout: 20000 });
  const r = await ev(`JSON.stringify((function(){
    // What the exit check compares: the loaded featureSet vs the regenerated one.
    var s = window.__GP_STORE__.getState().UIData.edit_info;
    var loaded = (((s.loadedDefFile||{}).featureSet||[])[0]||{});
    // A REAL machine emits a camera_calibration report, and the editor's
    // cameraParam is then the LIVE one -- which carries no exposure_time. That
    // is the state the bug needs; a fake-camera bench never reaches it, which
    // is why this had to be simulated to be seen at all.
    if (s._obj && s._obj.SetCameraParamInfo)
      s._obj.SetCameraParamInfo({ ppb2b: 1, mmpb2b: 0.0138859432190657 });
    var gen = s._obj && s._obj.GenerateFeature_sig360_circle_line
              ? s._obj.GenerateFeature_sig360_circle_line() : {};
    return { fileCam: loaded.cam_param, genCam: gen.cam_param,
             camSame: JSON.stringify(loaded.cam_param) === JSON.stringify(gen.cam_param),
             hash: s.DefFileHash };
  })())`);
  const j = JSON.parse(r);
  console.log(`${j.camSame ? '  ok  ' : '  FAIL'} ${m} cam_param survives the round trip`);
  if (!j.camSame) { console.log('    file:', JSON.stringify(j.fileCam));
                    console.log('    gen :', JSON.stringify(j.genCam)); fails++; }
  // Press the editor's own back button and see whether the unsaved-changes
  // dialog appears -- the thing the operator actually experiences.
  await ev(`(function(){var b=Array.from(document.querySelectorAll('.layout.black.vbox'))
      .find(function(e){return (e.textContent||'').trim().indexOf('<')===0;});
    if(b){ b.click(); return 'clicked'; } return 'no back button'; })()`);
  await P.waitFor('exit settled', async () =>
    (await ev(`!!document.querySelector('.ant-modal-body')`))
    || JSON.stringify(await P.store(`window.__GP_STORE__.getState().UIData.c_state.value`)) === '"MAIN"',
    { timeout: 8000 });
  console.log('   exit dialog:', await ev(`(function(){
    var t=document.querySelector('.ant-modal-body'); return t? t.textContent.slice(0,160) : 'none (left cleanly)'; })()`));
  await toMain(ctl);
}
console.log(fails ? (fails + ' FAIL')
                  : 'PASS: cam_param round-trips, so a clean def exits clean');
process.exit(fails ? 1 : 0);
