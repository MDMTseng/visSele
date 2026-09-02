// The line's envelope fit, and the switch it replaces.
//
// 凸點連線 is the CONTOUR path's envelope -- it walks the contour for the
// touching vertices. The caliper path has hits, not a contour, so offering that
// switch there is offering a setting the mode cannot honour, which is how a
// knob becomes folklore. In caliper mode it is replaced by 擬合方式, whose
// three options differ ONLY in the offset: the direction is always least
// squares and the line slides along its own normal onto the extreme inlier.
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
const pick = () => ev(`(function(){
  var ei=window.__GP_STORE__.getState().UIData.edit_info;
  var sl=(ei._obj&&ei._obj.shapeList)||[];
  var t=sl.find(function(s){return s.type==='line'&&s.locating==='caliper';});
  if(!t) return 'no caliper line';
  window.__GP_STORE__.dispatch({ type: 'Edit_Tar_Update', data: t });
  window.__GP_STORE__.dispatch({ type: 'Shape_Edit' });
  window.__GP_STORE__.dispatch({ type: 'Edit_Tar_Update', data: t });
  return t.id+' '+t.name+' locating='+t.locating;})()`);
let sel = await pick();
for (let i = 0; i < 20; i++) {
  const up = await ev(`[].slice.call(document.querySelectorAll('*')).some(function(x){
    return x.children.length===0 && (x.textContent||'').trim()==='\u540D\u7A31';})`);
  if (up) break;
  await sleep(300);
  sel = await pick();
}
console.log('selected  :', sel);
console.log('panel     :', await ev(`(function(){
  var all=[].slice.call(document.querySelectorAll('*'));
  var host=all.filter(function(x){return x.children.length===0 &&
    (x.textContent||'').trim()==='\u540D\u7A31';})[0];
  var p=host; for(var k=0;k<8&&p.parentElement;k++) p=p.parentElement;
  var t=p.innerText;
  return (t.indexOf('\u51F8\u9EDE\u9023\u7DDA')>=0 ? '凸點連線 STILL SHOWN' : '凸點連線 hidden')
       + ' | ' + (t.indexOf('\u64EC\u5408\u65B9\u5F0F')>=0 ? '擬合方式 shown' : '擬合方式 MISSING')
       + ' | ' + (t.indexOf('\u5E73\u5747')>=0 ? 'default 平均' : 'default ?');})()`));

// The def must carry what the panel chose, and the core must move the line --
// direction unchanged, offset only. Verified in the core with --insp; here the
// check is that the choice reaches the shape at all.
await ev(`window.__GP_STORE__.dispatch({ type: 'DefConf_Lock_Level_Update', data: 0 })`);
await sleep(300);
for (const want of ['前凸點', '後凸點', '平均']) {
  const done = await ev(`(function(){
    var links=[].slice.call(document.querySelectorAll('a'));
    var dd=links.filter(function(a){ return /平均|前凸點|後凸點/.test(a.textContent||''); })[0];
    if(!dd) return 'no dropdown';
    dd.click();
    return 'opened';})()`);
  if (done !== 'opened') { console.log('fit_mode  :', done); break; }
  await sleep(400);
  const set = await ev(`(function(){
    var it=[].slice.call(document.querySelectorAll('.ant-dropdown-menu-item'))
      .filter(function(e){ return (e.textContent||'').trim()==='${want}'; })[0];
    if(!it) return 'no item ${want}';
    it.click(); return 'clicked';})()`);
  await sleep(600);
  console.log('  ' + want + ' -> ' + set + ' | shape.fit_mode=' + await ev(`(function(){
    var sh=(window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList||[]).find(function(x){return x.id===1;});
    return sh && sh.fit_mode;})()`));
}
