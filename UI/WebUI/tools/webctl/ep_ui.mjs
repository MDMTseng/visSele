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
// Select the first caliper line by id, the way the canvas does.
const sel = await ev(`(function(){
  var ei=window.__GP_STORE__.getState().UIData.edit_info;
  var sl=(ei._obj&&ei._obj.shapeList)||[];
  var t=sl.find(function(s){return s.type==='line'&&s.locating==='caliper';});
  if(!t) return 'no caliper line';
  window.__GP_STORE__.dispatch({ type: 'Edit_Tar_Update', data: t });
  // The panel that hosts the property sheet only exists in SHAPE_EDIT; the
  // selection alone leaves the mode at NEUTRAL.
  window.__GP_STORE__.dispatch({ type: 'Shape_Edit' });
  return t.id+' '+t.name+' min_strength='+(t.edge&&t.edge.min_strength);
})()`);
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

// The slider must do two things: re-pick locally (no round trip) and land on
// the shape. Asserting only the first would pass on a control that shows a
// number and changes nothing.
async function setSlider(v) {
  await ev(`(function(){
    var s=document.querySelector('[data-testid="edge-profile-slider"]');
    var setter=Object.getOwnPropertyDescriptor(window.HTMLInputElement.prototype,'value').set;
    setter.call(s, '${v}');
    s.dispatchEvent(new Event('input',{bubbles:true}));
    s.dispatchEvent(new Event('change',{bubbles:true}));
    return true;})()`);
  await sleep(400);
  return ev(`(function(){
    var e=document.querySelector('[data-testid="edge-profile-plot"]');
    var st=window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList.find(function(s){return s.id===1;});
    return (e?e.getAttribute('data-pass'):'?')+' / min_strength='+(st&&st.edge&&st.edge.min_strength);
  })()`);
}
console.log('slider -> 105 :', await setSlider(105));
console.log('slider -> 200 :', await setSlider(200));
console.log('slider -> 60  :', await setSlider(60));
