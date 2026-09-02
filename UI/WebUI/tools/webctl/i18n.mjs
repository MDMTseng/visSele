// EVERY SETTING ON THE PRIMITIVE SHEET READS IN CHINESE.
//
// The machine is operated in Chinese and these fields were on screen in
// English -- locating, min_strength, include_range, manual_offset. A setting
// nobody can read is a setting nobody adjusts, which is how min_strength ended
// up at whatever the default was on every def in the field.
//
// Asserts on the RENDERED text, not on the dictionary: a key added to zh_TW and
// not wired into the sheet passes a dictionary test and still shows English.
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

const pick = (type) => ev(`(function(){
  var ei=window.__GP_STORE__.getState().UIData.edit_info;
  var sl=(ei._obj&&ei._obj.shapeList)||[];
  var t=sl.find(function(s){return s.type==='${type}'&&s.locating==='caliper';});
  if(!t) return 'none';
  window.__GP_STORE__.dispatch({ type: 'Edit_Tar_Update', data: t });
  window.__GP_STORE__.dispatch({ type: 'Shape_Edit' });
  window.__GP_STORE__.dispatch({ type: 'Edit_Tar_Update', data: t });
  return t.type+' '+t.name;})()`);

// The English that must not survive anywhere in the panel. Kept as a list of
// what the sheets used to print rather than "any latin word": the def's own
// NAME is latin ([4][1]), and so are numbers and units.
const BANNED = ['locating', 'min_strength', 'include_range', 'manual_offset',
                'min_inliers', 'max_error', 'polarity', 'strongest', 'falling',
                'rising', 'contour', 'caliper'];
for (const type of ['line', 'arc', 'search_point']) {
  console.log('selected  :', await pick(type));
  await sleep(900);
  const left = await ev(`(function(){
    var host=[].slice.call(document.querySelectorAll('*')).filter(function(x){
      return x.children.length===0 && /^(\u540D\u7A31|\u985E\u578B)$/.test((x.textContent||'').trim());})[0];
    if(!host) return 'NO SHEET';
    var panel=host; for(var k=0;k<8 && panel.parentElement;k++) panel=panel.parentElement;
    var txt=panel.innerText;
    var bad=${JSON.stringify(BANNED)}.filter(function(w){ return txt.indexOf(w)>=0; });
    return bad.length ? 'ENGLISH LEFT: '+bad.join(' ') : 'all translated';})()`);
  console.log('  ' + type + ': ' + left);
}
