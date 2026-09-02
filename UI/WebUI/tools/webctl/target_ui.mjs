// SETTING A TARGET AND ITS LIMITS WITHOUT THE KEYBOARD.
//
// These are the numbers an operator changes most, on a machine where typing is
// the slowest and least reliable thing they can do. 目標 takes the value just
// measured (rounded to 0.01 -- a reading is not a specification); each limit
// takes the target as a starting point, and +0.1 / +0.01 walks it out from
// there.
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
await ev(`window.__GP_STORE__.dispatch({ type: 'DefConf_Lock_Level_Update', data: 0 })`);

// A measured value only exists after a CHECK, and the button only offers itself
// when there is one -- so run one first.
await ev(`(function(){
  var b=[].slice.call(document.querySelectorAll('*')).filter(function(e){
    return e.children.length===0 && /INST_CHECK/.test(e.textContent||'');})[0];
  if(b) b.click(); return !!b;})()`);
for (let i = 0; i < 40; i++) {
  const got = await ev(`(function(){
    var j=((((window.__GP_STORE__.getState().UIData.edit_info.inspReport||{}).reports||[])[0]||{}).judgeReports)||[];
    return j.length>0;})()`);
  if (got) break;
  await sleep(300);
}
const meas = await ev(`(function(){
  var j=((((window.__GP_STORE__.getState().UIData.edit_info.inspReport||{}).reports||[])[0]||{}).judgeReports)||[];
  var m=j.find(function(e){return Number.isFinite(e.value);});
  return m ? m.id+':'+m.value : 'none';})()`);
console.log('measured  :', meas);
const mid = parseInt(String(meas).split(':')[0], 10);

const pick = () => ev(`(function(){
  var sl=(window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList||[]);
  var t=sl.find(function(s){return s.id===${mid};});
  if(!t) return 'none';
  window.__GP_STORE__.dispatch({ type: 'Edit_Tar_Update', data: t });
  window.__GP_STORE__.dispatch({ type: 'Shape_Edit' });
  window.__GP_STORE__.dispatch({ type: 'Edit_Tar_Update', data: t });
  return t.name;})()`);
let sel = await pick();
for (let i = 0; i < 20; i++) {
  const up = await ev(`[].slice.call(document.querySelectorAll('*')).some(function(x){
    return x.children.length===0 && (x.textContent||'').trim()==='\u540D\u7A31';})`);
  if (up) break;
  await sleep(300); sel = await pick();
}
console.log('selected  :', sel);

const shapeVal = (k) => ev(`(function(){
  var sh=(window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList||[])
    .find(function(x){return x.id===${mid};});
  return sh ? sh['${k}'] : 'gone';})()`);

// Open the row's popover and click the button whose text starts with the arrow.
async function clickAction(label, prefix) {
  const r = await ev(`(function(){
    var all=[].slice.call(document.querySelectorAll('*'));
    var lbl=all.filter(function(x){ return x.children.length===0 &&
      (x.textContent||'').trim()==='${label}'; })[0];
    if(!lbl) return 'no label ${label}';
    // The LABEL is the trigger -- Row wraps it in a Popover and marks it with a
    // dotted underline. Looking for a button inside the row found nothing and
    // reported "no popover trigger" for a control that works.
    lbl.click(); return 'opened';})()`);
  if (r !== 'opened') return r;
  await sleep(500);
  // Scope the search to the VISIBLE popover. Ant keeps every popover it has
  // ever opened mounted, so a document-wide query for "←目標" found the first
  // one ever rendered -- a different row's button, with a different onCommit,
  // and the assertion then failed against a control that works.
  return ev(`(function(){
    var open=[].slice.call(document.querySelectorAll('.ant-popover'))
      .filter(function(p){ return !p.classList.contains('ant-popover-hidden'); });
    if(!open.length) return 'no open popover';
    for (var i=0;i<open.length;i++){
      var b=[].slice.call(open[i].querySelectorAll('button'))
        .filter(function(x){ return (x.textContent||'').trim().indexOf('${prefix}')===0; })[0];
      if(b){ b.click(); return 'clicked'; }
    }
    return 'no button ${prefix} in the open popover';})()`);
}

console.log('目標 before:', await shapeVal('value'));
console.log('  ←measured:', await clickAction('\u76EE\u6A19', '\u2190'), '-> value =', await shapeVal('value'));
console.log('規格上限 before:', await shapeVal('USL'));
console.log('  ←目標    :', await clickAction('\u898F\u683C\u4E0A\u9650', '\u2190\u76EE\u6A19'), '-> USL =', await shapeVal('USL'));
