// WHAT A NUMBER FIELD WILL ACCEPT.
//
// The core clamps -- 512 calipers, a 2-caliper floor for a line, 3 for an arc --
// but it clamps AFTER the def is written. A count of 0 or -5 was taken by the
// field, saved, and then quietly run as 2: the number on screen was not the
// number the machine used.
//
// So the bound is enforced on commit, and this drives the real control the way
// a person does -- type, blur -- and reads back what the SHAPE ended up with.
// Asserting the input's min attribute would only prove the spinner is
// decorated; typing is what got past it.
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

const pick = (type) => ev(`(function(){
  var ei=window.__GP_STORE__.getState().UIData.edit_info;
  var sl=(ei._obj&&ei._obj.shapeList)||[];
  var t=sl.find(function(s){return s.type==='${type}'&&s.locating==='caliper';});
  if(!t) return null;
  window.__GP_STORE__.dispatch({ type: 'Edit_Tar_Update', data: t });
  window.__GP_STORE__.dispatch({ type: 'Shape_Edit' });
  window.__GP_STORE__.dispatch({ type: 'Edit_Tar_Update', data: t });
  return t.id;})()`);

async function select(type) {
  let id = await pick(type);
  for (let i = 0; i < 20; i++) {
    const up = await ev(`[].slice.call(document.querySelectorAll('*')).some(function(x){
      return x.children.length===0 && (x.textContent||'').trim()==='\u540D\u7A31';})`);
    if (up) break;
    await sleep(300);
    id = await pick(type);
  }
  return id;
}

// Type into the field whose label is `label`, blur, and report what the shape got.
// The label text comes from the dictionary, so the test asks the PAGE what a
// field is called instead of carrying a second copy of the translation that can
// silently stop matching.
const labelFor = (key) => ev(`(function(){
  var d=window.__GP_STORE__.getState().UIData.DICT;
  return (d && d._ && d._['${key}']) || '${key}';})()`);

async function typeInto(label, text, id, path) {
  const ok = await ev(`(function(){
    var all=[].slice.call(document.querySelectorAll('*'));
    var lbl=all.filter(function(x){ return x.children.length===0 &&
      (x.textContent||'').trim()==='${label}'; })[0];
    if(!lbl) return 'no label';
    var row=lbl.parentElement, inp=row.querySelector('input[type=number]');
    if(!inp) return 'no input';
    // focus BEFORE blur: React's onBlur does not fire on an element that was
    // never focused, so the first version of this typed into the box, called
    // blur(), and asserted against a value that had never been committed.
    inp.focus();
    var set=Object.getOwnPropertyDescriptor(window.HTMLInputElement.prototype,'value').set;
    set.call(inp, '${text}');
    inp.dispatchEvent(new Event('input',{bubbles:true}));
    inp.blur();
    return 'typed';})()`);
  if (ok !== 'typed') return ok;
  await sleep(500);
  return ev(`(function(){
    var sh=(window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList||[])
      .find(function(x){return x.id===${id};});
    return ${path};})()`);
}

const idL = await select('line');
console.log('line id   :', idL);
const Lcount = await labelFor('count'), Lmin = await labelFor('min_strength');
console.log('  count -5    ->', await typeInto(Lcount, '-5', idL, 'sh.caliper.count'), '(floor 2)');
console.log('  count 9999  ->', await typeInto(Lcount, '9999', idL, 'sh.caliper.count'), '(cap 512)');
console.log('  count 10.7  ->', await typeInto(Lcount, '10.7', idL, 'sh.caliper.count'), '(whole)');
console.log('  strength -20->', await typeInto(Lmin, '-20', idL, 'sh.edge.min_strength'), '(floor 0)');

const idS = await select('search_point');
console.log('spoint id :', idS);
const Loff = await labelFor('manual_offset');
console.log('  offset -0.004 ->', await typeInto(Loff, '-0.004', idS, 'sh.edge.manual_offset'), '(SIGNED: kept)');
