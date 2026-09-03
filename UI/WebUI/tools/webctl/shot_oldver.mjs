import { makeCtl, toMain, dismissCamModal, loadRecipe, freshPage, sleep } from './lib_enter.mjs';
const ctl = makeCtl('http://127.0.0.1:8765');
const { ev } = ctl;
const OUT = 'C:/Users/w2110/AppData/Local/Temp/claude/C--Users-w2110-Documents-workspace-visSele/8b8b78b7-1ef0-4c98-8245-47dba9add707/scratchpad';

await freshPage(ctl, 'http://127.0.0.1:8083/');
await toMain(ctl); await dismissCamModal(ctl);
console.log('recipe:', await loadRecipe(ctl, 'data/test2'));
await ev(`window.__GP_STORE__.dispatch({ type: 'Edit_Mode' })`);
for (let i = 0; i < 60; i++) {
  const s = await ev(`JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value)`);
  if (String(s).indexOf('DEFCONF') >= 0) break;
  await sleep(400);
}
await ev(`window.__GP_STORE__.dispatch({ type: 'DefConf_Lock_Level_Update', data: 0 })`);
for (let i = 0; i < 40; i++) {
  if (await ev(`!!document.querySelector('[data-testid="upgrade-def"]')`)) break;
  await sleep(300);
}
await sleep(800);
await fetch(`http://127.0.0.1:8765/shot?path=${encodeURIComponent(OUT + '/oldver_banner.png')}`);
console.log('shot 1: banner');

// the confirm
await ev(`document.querySelector('[data-testid="upgrade-def"]').click()`);
for (let i = 0; i < 30; i++) {
  if (await ev(`/升級並開始設定/.test(document.body.textContent||'')`)) break;
  await sleep(200);
}
await sleep(500);
await fetch(`http://127.0.0.1:8765/shot?path=${encodeURIComponent(OUT + '/oldver_confirm.png')}`);
console.log('shot 2: confirm');
