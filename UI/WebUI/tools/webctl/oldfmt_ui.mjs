// A DEF THE MACHINE WILL REFUSE HAS TO SAY SO IN THE EDITOR.
//
// The core no longer loads a shape_based def stored in the old format: it kept
// the coarse feature levels but not the ROI windows the refiner reads, so it
// could only ever locate coarsely -- with a high score and a report that read
// as normal. Refusing is right; finding out when the machine will not run is
// not, because that is the worst moment and the least informative place.
//
// The editor can tell at load time. edit_info.__shape_cache is the def's own
// cache, carried in by the load out of @__SBM_INFO__, and a cache written
// before the change has no `roi`.
//
// Both fixtures are checked to have actually LOADED. That matters more than it
// sounds: an earlier version of this check used defs that quietly failed to
// load, read the empty defaults that left behind, and concluded the editor
// could not see the cache at all. It can; the probe could not see the def.
import fs from 'node:fs';
import path from 'node:path';
import JSum from 'jsum';
import { makeCtl, toMain, dismissCamModal, loadRecipe, freshPage, sleep } from './lib_enter.mjs';
const ctl = makeCtl('http://127.0.0.1:8765');
const { ev } = ctl;
const APP = process.argv[2] || 'http://127.0.0.1:8083/';
const DATA = process.argv[3]
  || path.resolve('../../../../InspectionCore/Core0_1/data');

// THE FIXTURE IS MADE HERE, not picked from the folder.
//
// The first version named a def that happened to still be in the old format.
// Then that def was converted and this suite started asserting the opposite of
// what it meant -- a check whose subject can be edited out from under it is not
// a check. So: take a def that IS self-contained, strip the roi back off, and
// re-stamp featureSet_sha1 so the editor will still open it (it hard-blocks a
// digest mismatch). That is an old-format def by construction, every run.
function makeFixtures(srcName) {
  const src = path.join(DATA, srcName + '.hydef');
  const def = JSON.parse(fs.readFileSync(src, 'utf8'));
  const sbmOf = (f) => (f.inherentfeatures || []).find(
    (e) => e && (e.name === '@__SBM_INFO__' || e.shape_cache));
  if (!sbmOf(def.featureSet[0]) || !sbmOf(def.featureSet[0]).shape_cache.roi)
    throw new Error(`${srcName} is not self-contained; convert it first`);

  const sha1 = (featureSet) => {
    const c = JSON.parse(JSON.stringify(featureSet));
    c.forEach((f) => Object.keys(f).filter((k) => k.startsWith('__'))
                       .forEach((k) => { delete f[k]; }));
    return JSum.digest(c, 'sha1', 'hex');
  };
  const write = (name, mutate) => {
    const d = JSON.parse(JSON.stringify(def));
    mutate(d);
    d.featureSet_sha1 = sha1(d.featureSet);
    fs.writeFileSync(path.join(DATA, name + '.hydef'), JSON.stringify(d, null, 1));
    const png = path.join(DATA, srcName + '.png');
    if (fs.existsSync(png)) fs.copyFileSync(png, path.join(DATA, name + '.png'));
  };
  write('_fmt_old', (d) => { delete sbmOf(d.featureSet[0]).shape_cache.roi; });
  write('_fmt_new', () => {});
}
function dropFixtures() {
  for (const n of ['_fmt_old', '_fmt_new']) {
    for (const ext of ['.hydef', '.png']) {
      const p = path.join(DATA, n + ext);
      if (fs.existsSync(p)) fs.unlinkSync(p);
    }
  }
}

// The WARNING and the BUTTON are separate on purpose: the warning is a fact
// about the file and shows under a lock too, the button needs an unlocked def.
const banner = () => ev(`(function(){
  var b=document.querySelector('[data-testid="oldfmt-banner"]');
  if(!b) return 'absent';
  return b.getBoundingClientRect().width===0 ? 'zero-size' : 'present';})()`);

const buttonAndLock = () => ev(`(function(){
  var lock=window.__GP_STORE__.getState().UIData.defConf_lock_level;
  var b=document.querySelector('[data-testid="oldfmt-def"]');
  return 'lock=' + lock + '/button=' + (b ? 'yes' : 'no');})()`);

const state = () => ev(`(function(){
  var ei=window.__GP_STORE__.getState().UIData.edit_info, sc=ei.__shape_cache;
  return (ei.locating_engine||'sig360') + '/' + (sc ? (sc.roi?'roi':'no-roi') : 'no-cache');})()`);

async function enterDef(name) {
  const loaded = await loadRecipe(ctl, 'data/' + name);
  if (loaded === null) throw new Error(
    `'${name}' did not load (loadRecipe returned no def name). The fixtures for `
    + 'this check must be defs this UI actually opens -- see the note at the top.');
  await ev(`window.__GP_STORE__.dispatch({ type: 'Edit_Mode' })`);
  for (let i = 0; i < 60; i++) {
    const s = await ev(`JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value)`);
    if (String(s).indexOf('DEFCONF') >= 0) break;
    await sleep(400);
  }
  await sleep(800);
}

async function leave() {
  await ev(`window.__GP_STORE__.dispatch({ type: 'EXIT' })`);
  await sleep(1200);
  await toMain(ctl);
}

let fail = 0;
const check = (what, got, want) => {
  const ok = got === want;
  console.log(`  ${ok ? 'ok  ' : 'FAIL'} ${what}: ${got}${ok ? '' : '  (expected ' + want + ')'}`);
  if (!ok) fail++;
};

process.env.WEBCTL_COLD = '1';
await freshPage(ctl, APP);
await toMain(ctl);
await dismissCamModal(ctl);

makeFixtures(process.env.OLDFMT_SRC || 'test2_sbm');

console.log('old format (_fmt_old)');
await enterDef('_fmt_old');
check('def state', await state(), 'shape_based/no-roi');
check('banner', await banner(), 'present');
// Locked is how this suite enters, and the warning has to survive it -- that
// was the bug this check found: the condition was copied from the migration
// banner, which hides under a lock for a reason that does not apply here.
check('under lock', await buttonAndLock(), 'lock=1/button=no');
await leave();

console.log('self-contained (_fmt_new)');
await enterDef('_fmt_new');
check('def state', await state(), 'shape_based/roi');
check('banner', await banner(), 'absent');

// Always, pass or fail: the fixtures live in the machine's data/ folder and a
// leftover _fmt_old is a def someone can open by accident.
dropFixtures();

console.log(fail ? `\n${fail} FAILED` : '\nPASS');
process.exit(fail ? 1 : 0);
