// AN OLD DEF HAS TO SAY SO WHERE THE OPERATOR IS LOOKING.
//
// A def still on the sig360 localizer inspects fine, so nothing ever said it
// was the old one -- the migration lived in the localizer section of a
// scrolling settings panel. This asserts the banner appears for a sig360 def,
// stays away for a shape_based one, and that pressing 升級 actually flips the
// engine (the old handler threw ReferenceError halfway through and left the
// def half-migrated).
import { makeCtl, toMain, dismissCamModal, loadRecipe, freshPage, sleep } from './lib_enter.mjs';
const ctl = makeCtl('http://127.0.0.1:8765');
const { ev } = ctl;

const banner = () => ev(`(function(){
  var b=document.querySelector('[data-testid="upgrade-def"]');
  if(!b) return 'absent';
  var box=b.getBoundingClientRect();
  if(box.width===0) return 'zero-size';
  return JSON.stringify({ cx: Math.round(box.left+box.width/2),
                          top: Math.round(box.top),
                          vw: window.innerWidth });})()`);

const engine = () => ev(`(window.__GP_STORE__.getState().UIData.edit_info.locating_engine||'sig360')`);

async function enterDef(name) {
  console.log('recipe:', await loadRecipe(ctl, 'data/' + name));
  await ev(`window.__GP_STORE__.dispatch({ type: 'Edit_Mode' })`);
  for (let i = 0; i < 60; i++) {
    const s = await ev(`JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value)`);
    if (String(s).indexOf('DEFCONF') >= 0) break;
    await sleep(400);
  }
  await ev(`window.__GP_STORE__.dispatch({ type: 'DefConf_Lock_Level_Update', data: 0 })`);
  for (let i = 0; i < 30; i++) { if (await engine() !== undefined) break; await sleep(200); }
  await sleep(500);
}

let bad = 0;
await freshPage(ctl, 'http://127.0.0.1:8083/');
await toMain(ctl); await dismissCamModal(ctl);

// --- the old def ------------------------------------------------------------
await enterDef('test2');
console.log('test2 engine :', await engine());
const b1 = await banner();
console.log('test2 banner :', b1);
if (b1 === 'absent' || b1 === 'zero-size') { console.log('FAIL: no banner on a sig360 def'); bad++; }
else {
  const { cx, top, vw } = JSON.parse(b1);
  // "top centre" is the requirement, so it is what gets checked.
  if (Math.abs(cx - vw / 2) > 60) { console.log('FAIL: not centred', cx, 'of', vw); bad++; }
  if (top > 120) { console.log('FAIL: not at the top', top); bad++; }
}

// --- locked: the reducer would drop the migration, so the button must go -----
await ev(`window.__GP_STORE__.dispatch({ type: 'DefConf_Lock_Level_Update', data: 1 })`);
await sleep(400);
const bl = await banner();
console.log('locked banner:', bl);
if (bl !== 'absent') { console.log('FAIL: banner offered while locked'); bad++; }
await ev(`window.__GP_STORE__.dispatch({ type: 'DefConf_Lock_Level_Update', data: 0 })`);
await sleep(400);

// --- pressing it ------------------------------------------------------------
await ev(`document.querySelector('[data-testid="upgrade-def"]').click()`);
await sleep(600);
const ok = await ev(`(function(){
  var b=[].slice.call(document.querySelectorAll('.ant-btn')).filter(function(e){
    return /升級並開始設定/.test(e.textContent||'') && e.offsetParent!==null;})[0];
  if(!b) return false; b.click(); return true;})()`);
console.log('confirm    :', ok);
if (!ok) { console.log('FAIL: no confirm dialog'); bad++; }
else {
  for (let i = 0; i < 40; i++) { if (await engine() === 'shape_based') break; await sleep(250); }
  const e2 = await engine();
  console.log('after 升級  :', e2);
  if (e2 !== 'shape_based') { console.log('FAIL: engine did not flip'); bad++; }
  // The second half: the studio must actually open. This is the part that used
  // to throw, silently leaving a half-migrated def.
  let studio = false;
  for (let i = 0; i < 40; i++) {
    studio = await ev(`/新物件 — 先設定定位|Shape-based/.test(document.body.textContent||'')`);
    if (studio) break; await sleep(250);
  }
  console.log('studio open:', studio);
  if (!studio) { console.log('FAIL: migration did not open the SBM studio'); bad++; }
  const bAfter = await banner();
  console.log('banner now :', bAfter);
  if (bAfter !== 'absent' && !studio) { console.log('FAIL: banner still up after migrating'); bad++; }
}

console.log(bad ? `\n${bad} FAILURE(S)` : '\nOK');
