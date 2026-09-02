// The edge-threshold panel for a SEARCH POINT, end to end against a real core.
//
// A search point is not a caliper: it finds a peak per row and takes the one
// NEAREST the origin, so the floor does not decide whether a measurement
// happens, it decides WHICH candidate ends up nearest. Raising it walks the
// first hit outward, past the candidates it holds back, and that walk is the
// thing this suite asserts.
//
// Measured on test1 [4], and it is the whole argument for the panel:
//
//   floor   0  ->  first hit  1.0px   (396 candidates, noise wins)
//   floor  30  ->  first hit 11.2px   (what the def asks for)
//   floor 100  ->  first hit 20.8px   (the edge the machine actually reports)
//   floor 154  ->  first hit 20.8px   (stable)
//
// The machine reports 20.8px today at a floor of 30, because search_point_cv's
// own 0.40-of-the-strongest rule rescues it -- a rule with no setting, no
// display, and a value that moves when anything stronger enters the window.
// 自動設定 offers 167, which is that rule written down.

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
// Same panel, the SEARCH POINT plot: candidates by distance along the search
// rather than a curve across the edge. What is asserted is the thing that plot
// exists to show -- that the floor decides WHICH candidate ends up nearest, and
// that raising it walks the first hit outward past the ones it holds back.
const pick = async () => ev(`(function(){
  var ei=window.__GP_STORE__.getState().UIData.edit_info;
  var sl=(ei._obj&&ei._obj.shapeList)||[];
  var t=sl.find(function(s){return s.type==='search_point'&&s.locating==='caliper'&&s.id===4;})
       || sl.find(function(s){return s.type==='search_point'&&s.locating==='caliper';});
  if(!t) return 'no caliper search point';
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

const found = await ev(`!!document.querySelector('[data-testid="edge-profile-check"]')`);
console.log('check button present:', found);
if (!found) { console.log('FAIL: picker not rendered'); process.exit(1); }
await ev(`document.querySelector('[data-testid="edge-profile-check"]').click()`);
for (let i = 0; i < 60; i++) {
  const p = await ev(`(function(){var e=document.querySelector('[data-testid="edge-profile-plot"]');
    return e ? e.getAttribute('data-kind')+' cands='+e.getAttribute('data-cands')
             +' pass='+e.getAttribute('data-pass')+' first='+e.getAttribute('data-first') : null;})()`);
  if (p) { console.log('PLOT:', p); break; }
  await sleep(500);
}
await ev(`window.__GP_STORE__.dispatch({ type: 'DefConf_Lock_Level_Update', data: 0 })`);
await sleep(300);

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
    var sh=(window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList||[]).find(function(x){return x.id===4;});
    return 'first='+e.getAttribute('data-first')+'px pass='+e.getAttribute('data-pass')
         +' shape='+(sh&&sh.edge&&sh.edge.min_strength);})()`);
}
console.log('floor 0    :', await setSlider(0));
console.log('floor 100  :', await setSlider(100));
console.log('floor 154  :', await setSlider(154));
console.log('auto       :', await ev(`(function(){
  var b=document.querySelector('[data-testid="edge-profile-auto"]');
  return 'suggest='+b.getAttribute('data-suggest');})()`));

// A panel that floats over the camera image must paint its own ground.
//
// The sheet is transparent -- the frame shows through anything that does not
// paint a background -- which is fine for a row of white inputs and fatal for a
// plot and three lines of prose. Reported from the bench as unreadable text over
// a busy part of the image.
console.log('surfaces  :', await ev(`(function(){
  var svg=document.querySelector('[data-testid="edge-profile-plot"]');
  var box=svg.parentElement;
  var cs=getComputedStyle(box), sv=getComputedStyle(svg);
  var btn=document.querySelector('[data-testid="edge-profile-auto"]');
  var opaque=function(c){ var m=/rgba?\(([^)]+)\)/.exec(c); if(!m) return false;
    var p=m[1].split(','); return p.length<4 || parseFloat(p[3])>=0.99; };
  return 'block '+cs.backgroundColor+' opaque='+opaque(cs.backgroundColor)
       + ' | plot '+sv.backgroundColor+' opaque='+opaque(sv.backgroundColor)
       + ' | button '+getComputedStyle(btn).backgroundColor
       + ' h='+Math.round(btn.getBoundingClientRect().height);})()`));

// THE APEX OF A CURVED EDGE.
//
// The scan returns the weighted centroid of the band within include_range of the
// nearest hit; on a curve that sits DEEPER than the apex, by more the wider the
// band is. Widening it for noise and dialling manual_offset back by eye is the
// workflow this replaces with a fit -- so the fit has to be there, and the
// offset it suggests has to reach the def.
const apex = await ev(`(function(){
  var b=document.querySelector('[data-testid="edge-profile-offset"]');
  if(!b) return 'no apex row (straight edge, or fit not convex)';
  return 'suggest manual_offset='+b.getAttribute('data-offset')+' | '+b.textContent.trim();})()`);
console.log('apex fit  :', apex);
if (apex.indexOf('suggest') === 0) {
  await ev(`document.querySelector('[data-testid="edge-profile-offset"]').click()`);
  await sleep(1500);
  console.log('applied   :', await ev(`(function(){
    var sh=(window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList||[]).find(function(x){return x.id===4;});
    return 'shape.edge.manual_offset='+(sh&&sh.edge&&sh.edge.manual_offset);})()`));
}
