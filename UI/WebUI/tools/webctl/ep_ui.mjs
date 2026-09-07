// Drive the edge-profile picker end to end: open the def, select a caliper
// line, press 檢查邊緣強度, and read what the panel actually rendered.
//
// Asserts data-* attributes, never geometry: on this UI a wrong selector
// clicks instead of failing, which produces a green run for a broken panel.
import { makeCtl, toMain, dismissCamModal, loadRecipe, freshPage, sleep } from './lib_enter.mjs';
const ctl = makeCtl('http://127.0.0.1:8765');
const { ev } = ctl;

// WEBCTL_COLD=1 forces the reload. NOT a ?v= cache-buster: script.jsx builds the
// core WS URL by regex over the page URL and a query string breaks it (its own
// comment says so), which strands the app at SPLASH with a closed socket.
await freshPage(ctl, 'http://127.0.0.1:8081/');
await toMain(ctl); await dismissCamModal(ctl);
console.log('recipe:', await loadRecipe(ctl, 'data/test1'));
await ev(`window.__GP_STORE__.dispatch({ type: 'Edit_Mode' })`);
for (let i = 0; i < 60; i++) {
  const s = await ev(`JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value)`);
  if (String(s).indexOf('DEFCONF') >= 0) break;
  await sleep(400);
}
// Selecting a shape from outside the app is two pieces of state -- the mode and
// the target -- and setting them races. Rather than guess an order, set both and
// wait for the CONDITION that matters: the property sheet is on screen. Retried
// because the first dispatch can land while the editor is still mounting.
const pick = async () => ev(`(function(){
  var ei=window.__GP_STORE__.getState().UIData.edit_info;
  var sl=(ei._obj&&ei._obj.shapeList)||[];
  var t=sl.find(function(s){return s.type==='line'&&s.locating==='caliper';});
  if(!t) return 'no caliper line';
  window.__GP_STORE__.dispatch({ type: 'Edit_Tar_Update', data: t });
  window.__GP_STORE__.dispatch({ type: 'Shape_Edit' });
  window.__GP_STORE__.dispatch({ type: 'Edit_Tar_Update', data: t });
  return t.id+' '+t.name+' min_strength='+(t.edge&&t.edge.min_strength);
})()`);
let sel = await pick();
for (let i = 0; i < 12; i++) {
  if (await ev(`!!document.querySelector('[data-testid="edge-profile-check"]')`)) break;
  await sleep(500);
  sel = await pick();
}
console.log('selected:', sel);
await sleep(1200);

const found = await ev(`!!document.querySelector('[data-testid="edge-profile-check"]')`);
console.log('check button present:', found);
if (!found) { console.log('FAIL: picker not rendered'); process.exit(1); }

await ev(`document.querySelector('[data-testid="edge-profile-check"]').click()`);
for (let i = 0; i < 60; i++) {
  const plot = await ev(`(function(){var e=document.querySelector('[data-testid="edge-profile-plot"]');
    return e ? e.getAttribute('data-calipers')+'/'+e.getAttribute('data-pass') : null;})()`);
  const note = await ev(`(function(){var b=document.querySelector('[data-testid="edge-profile-check"]');
    return b ? b.textContent : '';})()`);
  if (plot) { console.log('PLOT rendered  calipers/passing =', plot); break; }
  if (i === 59) console.log('timed out; button text:', note);
  await sleep(500);
}
const readout = await ev(`(function(){
  var e=document.querySelector('[data-testid="edge-profile-plot"]');
  if(!e) return 'none';
  var s=document.querySelector('[data-testid="edge-profile-slider"]');
  return 'calipers='+e.getAttribute('data-calipers')+' pass='+e.getAttribute('data-pass')
       +' slider='+(s&&s.value)+' max='+(s&&s.max);
})()`);
console.log(readout);

// Two things are asserted: the LOCAL re-pick (the payload is ungated, so moving
// the threshold recounts which calipers would find their edge with no round
// trip), and that the value lands on the shape.
//
// The second needs the def UNLOCKED. defConf_lock_level != 0 filters Shape_Set
// out in the reducer, so with the lock on the slider redraws and writes
// nothing -- and so does the property sheet's own min_strength field, which is
// how that was first mistaken for a bug in this control.
// Shape_Set is filtered while the def is locked; unlock before asserting that
// anything lands.
await ev(`window.__GP_STORE__.dispatch({ type: 'DefConf_Lock_Level_Update', data: 0 })`);
await sleep(400);
console.log('lock level    :', await ev(`window.__GP_STORE__.getState().UIData.defConf_lock_level`));

async function setSlider(v) {
  await ev(`(function(){
    var s=document.querySelector('[data-testid="edge-profile-slider"]');
    var setter=Object.getOwnPropertyDescriptor(window.HTMLInputElement.prototype,'value').set;
    setter.call(s, '${v}');
    s.dispatchEvent(new Event('input',{bubbles:true}));
    s.dispatchEvent(new MouseEvent('mouseup',{bubbles:true}));
    return true;})()`);
  await sleep(1200);
  return ev(`(function(){
    var e=document.querySelector('[data-testid="edge-profile-plot"]');
    var st=window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList.find(function(s){return s.id===1;});
    var sh=(window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList||[]).find(function(x){return x.id===1;});
    return 'pass='+(e?e.getAttribute('data-pass'):'?')
         +' shown='+(document.querySelector('[data-testid="edge-profile-slider"]')||{}).value
         +' shape='+(sh&&sh.edge&&sh.edge.min_strength);
  })()`);
}
// Each step drags and releases, because the commit is on release now: during a
// drag nothing is written, which is what keeps the previous hits on the canvas
// to compare against.
console.log('slider -> 105 :', await setSlider(105));
console.log('slider -> 200 :', await setSlider(200));
console.log('slider -> 60  :', await setSlider(60));

// THE PROBE MUST NOT LEAVE THE CAMERA RUNNING.
//
// The first version started a CI subscription it never stopped, and the fake
// camera streamed for as long as the panel was open. A picker that quietly
// pins the pipeline is worse than no picker, so this is asserted, not assumed.
const hz = () => ev(`(function(){try{return window.__DIAG__().msgHz;}catch(e){return -1;}})()`);
await sleep(3000);
const idle = await hz();
await ev(`document.querySelector('[data-testid="edge-profile-recheck"]').click()`);
await sleep(1500);
const during = await hz();
await sleep(6000);
const after = await hz();
console.log(`msgHz  idle ${Number(idle).toFixed(2)}  during ${Number(during).toFixed(2)}  after ${Number(after).toFixed(2)}`);
console.log(Number(after) <= Number(idle) + 1 ? 'OK: the stream stopped' : 'FAIL: still streaming');

// 自動設定: a suggestion derived from the two numbers that bound the floor --
// the weakest edge found and the strongest competing peak.
const auto = await ev(`(function(){
  var b=document.querySelector('[data-testid="edge-profile-auto"]');
  if(!b) return 'no auto button';
  var s=b.getAttribute('data-suggest');
  // data-clean is on the PLOT, not the button: it describes the evidence, not
  // the control. Reading it off the button returned null and printed it as a
  // result -- a test reporting "null" as if it were an answer.
  var pl=document.querySelector('[data-testid="edge-profile-plot"]');
  var c=pl?pl.getAttribute('data-clean'):'?';
  b.click();
  return 'suggest='+s+' cleanGap='+c;})()`);
await sleep(500);
// Null-safe, like the settled read below: a failed probe removes the plot and
// leaves the panel explaining why, and a throw here would bury that message.
console.log('auto          :', auto, '->', await ev(`(function(){
  var e=document.querySelector('[data-testid="edge-profile-plot"]');
  var sh=(window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList||[]).find(function(x){return x.id===1;});
  var mn=' shape='+(sh&&sh.edge&&sh.edge.min_strength);
  if (e) return 'pass='+e.getAttribute('data-pass')+mn;
  var t=document.body.innerText, i=t.indexOf('檢查邊緣強度');
  return 'NO PLOT -- ' + (i<0?'picker gone':t.slice(i,i+60).split(String.fromCharCode(10)).join(' | ')) + mn;})()`));

// Letting go of the slider runs ONE inspection, so the canvas shows what the
// new threshold actually did. Asserted by the busy state appearing on release
// and not during the drag -- an inspection per pixel of travel would make both
// the control and the machine useless.
const busyNow = () => ev(`(function(){var b=document.querySelector('[data-testid="edge-profile-recheck"]');
  return b ? b.textContent.indexOf('檢查中') >= 0 : false;})()`);
await ev(`(function(){
  var s=document.querySelector('[data-testid="edge-profile-slider"]');
  var setter=Object.getOwnPropertyDescriptor(window.HTMLInputElement.prototype,'value').set;
  [70,72,74,76].forEach(function(v){ setter.call(s,String(v)); s.dispatchEvent(new Event('input',{bubbles:true})); });
  return true;})()`);
await sleep(300);
console.log('busy during drag :', await busyNow(), '(expected false)');
// The inspection is fast enough on a cached image that polling from node can
// miss the whole busy window. Watch from inside the page instead: hook the
// button's text for a second and report whether it ever said 檢查中.
await ev(`(function(){
  window.__SAW_BUSY__ = false;
  var t0 = Date.now();
  (function poll(){
    var b = document.querySelector('[data-testid="edge-profile-recheck"]');
    if (b && b.textContent.indexOf('檢查中') >= 0) window.__SAW_BUSY__ = true;
    if (Date.now() - t0 < 2500) requestAnimationFrame(poll);
  })();
  document.querySelector('[data-testid="edge-profile-slider"]')
    .dispatchEvent(new MouseEvent('mouseup', { bubbles: true }));
  return true;})()`);
await sleep(3000);
console.log('probe on release :', await ev(`window.__SAW_BUSY__`), '(expected true)');
// Null-safe on purpose: if the probe failed the plot is gone and the panel is
// showing why, and a test that throws here hides the message that explains it.
console.log('settled pass     :', await ev(`(function(){
  var e=document.querySelector('[data-testid="edge-profile-plot"]');
  if (e) return e.getAttribute('data-pass');
  var t=document.body.innerText, i=t.indexOf('檢查邊緣強度');
  return 'NO PLOT -- ' + (i<0 ? 'picker gone' : t.slice(i, i+60).split(String.fromCharCode(10)).join(' | '));})()`));
console.log('shape min        :', await ev(`(function(){
  var sh=(window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList||[]).find(function(x){return x.id===1;});
  return sh&&sh.edge&&sh.edge.min_strength;})()`));

// THE REPORTED SYMPTOM: the hits vanished the moment the thumb moved.
//
// Committing the threshold changes the shape, and a shape change drops the
// inspection report -- so the overlay the operator was comparing against
// disappeared at the start of every drag. Nothing is committed during a drag
// now, so the last real answer stays on screen until a new one replaces it.
const hits = () => ev(`(function(){
  var r=window.__GP_STORE__.getState().UIData.edit_info.inspReport;
  try { return r.reports[0].detectedLines[0].extra.cal_hits.length; }
  catch(e) { return 0; }})()`);
const before = await hits();
await ev(`(function(){
  var s=document.querySelector('[data-testid="edge-profile-slider"]');
  var setter=Object.getOwnPropertyDescriptor(window.HTMLInputElement.prototype,'value').set;
  [90,95,100].forEach(function(v){ setter.call(s,String(v)); s.dispatchEvent(new Event('input',{bubbles:true})); });
  return true;})()`);
await sleep(500);
const midDrag = await hits();
console.log(`hits on canvas   : before ${before}, mid-drag ${midDrag}`,
  before > 0 && midDrag === before ? '-> kept' : '-> LOST');

// THE WHOLE SHEET, not just this block, must paint its own ground.
//
// .overlay is `background: none` by design -- every floating panel in the app
// uses it, and most sit over something plain. This one sits over the CAMERA
// IMAGE, so the frame read straight through the labels and on a dark part of
// the object "min_strength" was simply not there. Walk up from a real label and
// require an opaque ancestor before the body.
console.log('sheet ground :', await ev(`(function(){
  var lbl=[].slice.call(document.querySelectorAll('*')).filter(function(e){
    return e.children.length===0 && (e.textContent||'').trim()==='min_strength';})[0];
  if(!lbl) return 'no sheet';
  var p=lbl, hops=0;
  while(p && p!==document.body && hops<12){
    var m=/rgba?\(([^)]+)\)/.exec(getComputedStyle(p).backgroundColor);
    var a=m ? (m[1].split(',').length<4 ? 1 : parseFloat(m[1].split(',')[3])) : 0;
    if(a>=0.99) return 'opaque at '+hops+' hops: '+(p.className||p.tagName);
    p=p.parentElement; hops++;
  }
  return 'TRANSPARENT all the way to body';})()`));

// NO WHITE TEXT ON A PALE GROUND.
//
// Read-only values had no colour of their own and inherited the app's dark-theme
// white, so the type row read as a label with nothing after it. The inputs never
// showed the problem because INPUT_STYLE sets its own ink; only the plain text
// did.
//
// The test compares each leaf against THE GROUND BEHIND IT, not against a fixed
// idea of light. White on the purple header and white in a blue dropdown are
// correct and must not be reported -- a check that cries wolf on those is a
// check nobody reads. SVG text is painted by `fill`, not `color`, so it is read
// from the right property or it reports the inherited colour it never uses.
console.log('sheet ink    :', await ev(`(function(){
  var host=[].slice.call(document.querySelectorAll('*')).filter(function(x){
    return x.children.length===0 && (x.textContent||'').trim()==='min_strength';})[0];
  if(!host) return 'no sheet';
  var panel=host; for(var k=0;k<8 && panel.parentElement;k++) panel=panel.parentElement;
  var lum=function(c){ var m=(c||'').match(/[0-9]+/g);
    return (m&&m.length>=3) ? (+m[0]+ +m[1]+ +m[2])/3 : null; };
  var ground=function(e){
    for(var p=e;p;p=p.parentElement){
      var cs=getComputedStyle(p), m=(cs.backgroundColor||'').match(/[0-9.]+/g);
      if(!m) continue;
      var a=(m.length<4)?1:parseFloat(m[3]);
      if(a>=0.99) return (+m[0]+ +m[1]+ +m[2])/3;
    }
    return 255; }; 
  var bad=[];
  [].slice.call(panel.querySelectorAll('*')).forEach(function(e){
    if(e.children.length) return;
    var t=(e.textContent||'').trim(); if(!t) return;
    var cs=getComputedStyle(e);
    var ink=lum(e.ownerSVGElement||e.tagName==='text' ? cs.fill : cs.color);
    if(ink===null) return;
    if(ink>200 && ground(e)>160) bad.push(t.slice(0,12));
  });
  return bad.length ? 'WHITE ON PALE: '+bad.slice(0,8).join(' ') : 'every leaf legible against its own ground';})()`));
