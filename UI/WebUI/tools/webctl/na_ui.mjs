// AN NA MUST NOT MOVE THE DEF, AND MUST NOT VANISH.
//
// Reported from the bench: force a primitive to NA (min_inliers above the
// caliper count is one way) and pressing CHECK left it un-findable on screen --
// and the def kept whatever the failed measurement produced, so pressing CHECK
// twice drifted it twice.
//
// Two separate defects behind that, both asserted here:
//   * the NA branch forward-transformed the shape into IMAGE frame while the
//     def editor renders in OBJECT frame, moving it by the object's position.
//   * CHECK wrote every adjusted shape back into the shape list, NA included.
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
await sleep(300);

const geom = () => ev(`(function(){
  var sh=(window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList||[])
    .find(function(x){return x.id===1;});
  if(!sh) return 'gone';
  return JSON.stringify({p1:sh.pt1, p2:sh.pt2, st:sh.inspection_status});})()`);

// Force NA the way the bench did: ask for more inliers than there are calipers.
await ev(`(function(){
  var st=window.__GP_STORE__.getState().UIData.edit_info;
  var sh=(st._obj.shapeList||[]).find(function(x){return x.id===1;});
  var next=JSON.parse(JSON.stringify(sh));
  next.caliper.min_inliers = 99;
  window.__GP_STORE__.dispatch({ type:'Shape_Set', data:{ shape: next, id: 1 } });
  return 1;})()`);
await sleep(600);
const before = await geom();
console.log('before CHECK :', before);

for (let round = 1; round <= 2; round++) {
  // INST_CHECK is the path that rewrites the WHOLE shape list, which is the one
  // that could corrupt the def. The sheet's own CHECK only exists once a shape
  // is selected and updates that shape alone.
  const clicked = await ev(`(function(){
    var b=[].slice.call(document.querySelectorAll('*')).filter(function(e){
      return e.children.length===0 && /INST_CHECK/.test(e.textContent||'');})[0];
    if(!b) return false;
    b.click(); return true;})()`);
  if (!clicked) { console.log('FAIL: no INST_CHECK button'); break; }
  // Wait for evidence the CHECK actually ran -- caliper hits on the shape --
  // rather than for a guess at how long a round trip takes.
  for (let i = 0; i < 30; i++) {
    const landed = await ev(`(function(){
      var sh=(window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList||[])
        .find(function(x){return x.id===1;});
      return !!(sh && sh.cal_hits && sh.cal_hits.length);})()`);
    if (landed) break;
    await sleep(300);
  }
  // dismiss the warning modal if one came up
  await ev(`(function(){var ok=[].slice.call(document.querySelectorAll('.ant-btn'))
    .filter(function(b){return /OK|\u78BA\u5B9A/.test(b.textContent||'');})[0];
    if(ok) ok.click(); return !!ok;})()`);
  await sleep(800);
  console.log('after CHECK ' + round + ':', await geom());
  console.log('    status  :', await ev(`(function(){
    var sh=(window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList||[])
      .find(function(x){return x.id===1;});
    return 'inspection_status='+(sh&&sh.inspection_status)
         +' na_reason='+((sh&&sh.na_reason)||'-')
         +' hits='+((sh&&sh.cal_hits||[]).length);})()`));
}
console.log(await geom() === before ? 'OK: the def did not move' : 'FAIL: NA moved the def');

// THE OTHER HALF: a SUCCESS must still snap, or this "fix" is just a way to
// stop CHECK working. Put min_inliers back, nudge the line off the edge, and
// the measurement should pull it back.
await ev(`(function(){
  var st=window.__GP_STORE__.getState().UIData.edit_info;
  var sh=(st._obj.shapeList||[]).find(function(x){return x.id===1;});
  var next=JSON.parse(JSON.stringify(sh));
  next.caliper.min_inliers = 5;
  next.pt1.y += 0.02; next.pt2.y += 0.02;      // 20um off the edge
  window.__GP_STORE__.dispatch({ type:'Shape_Set', data:{ shape: next, id: 1 } });
  return 1;})()`);
await sleep(600);
const nudged = await geom();
console.log('nudged       :', nudged);
await ev(`(function(){
  var b=[].slice.call(document.querySelectorAll('*')).filter(function(e){
    return e.children.length===0 && /INST_CHECK/.test(e.textContent||'');})[0];
  if(b) b.click(); return !!b;})()`);
await sleep(2500);
await ev(`(function(){var ok=[].slice.call(document.querySelectorAll('.ant-btn'))
  .filter(function(b){return /OK|確定/.test(b.textContent||'');})[0];
  if(ok) ok.click(); return !!ok;})()`);
// Wait for the RESULT to land, not for a duration. The report arrives before
// the shape list is updated from it, and reading in between showed the nudged
// geometry and called the snap broken -- a test failing on its own timing.
let snapped = nudged;
for (let i = 0; i < 30; i++) {
  snapped = await geom();
  if (snapped !== nudged) break;
  await sleep(300);
}
console.log('after CHECK  :', snapped);
console.log(snapped !== nudged ? 'OK: a SUCCESS still snaps' : 'FAIL: the snap is gone');
