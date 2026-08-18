// Play must be ready only when every tag group SHOWN to the operator is
// satisfied.
//
//   node play_readiness.mjs [--url http://localhost:8081/]
//
// EXPECTED TO FAIL TODAY. This pins a defect rather than hiding it: the picker
// renders `new_tagGroupsPreset` -- the base groups PLUS the recipe's
// `已設定範圍` margin group, which carries maxCount:1 -- while readiness is
// computed against the base `tagGroupsPreset` alone (MAINUI.js). Select two
// margin tags and the group draws its warning triangle while play stays
// enabled; the operator inspects with limits they did not choose, and which of
// the two applies is decided by selection order (InspectionUI.js's .find()).
//
// Written as a test rather than fixed in passing because the fix is a
// behavioural decision -- refuse to start, or drop the extra tag, or merge the
// margins -- and that is the machine owner's call, not a side effect of adding
// a hook. `data-reason="tags-shown-only"` is the state this asserts against:
// readiness says yes, the rendered groups say no.
//
// Requires: vite + core + webctld. Non-destructive -- it selects tags on MAIN
// and never presses play.
import { makeCtl, toMain, dismissCamModal, loadRecipe, sleep } from './lib_enter.mjs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const URL_ = (process.argv.find((a) => a.startsWith('--url=')) || '').split('=')[1]
          || 'http://localhost:8081/';
const DEF = path.join(here, 'fixtures', 'caliper_verify_tagged');
const { api, ev } = makeCtl();

let fails = 0;
const check = (cond, what) => { if (!cond) { console.log('  FAIL ' + what); fails++; } return cond; };

const groups = () => ev(`[...document.querySelectorAll('[data-testid="tag-group"]')]
  .map(function(e){return {group:e.dataset.group, count:+e.dataset.count,
    min:e.dataset.min===''?null:+e.dataset.min, max:e.dataset.max===''?null:+e.dataset.max,
    fulfilled:e.dataset.fulfilled==='1'};})`);
const play = () => ev(`(function(){var e=document.querySelector('[data-testid="main-play"]');
  return e?{ready:e.dataset.ready==='1', reason:e.dataset.reason}:null;})()`);

console.log('[1] load the tagged fixture and settle at MAIN');
await api('/goto', { url: URL_ });
await sleep(12000);
await toMain({ api, ev });
await dismissCamModal({ api, ev });
await loadRecipe({ ev }, DEF);
await toMain({ api, ev });
await sleep(1500);

console.log('[2] groups as rendered:');
let g = await groups();
g.forEach((x) => console.log(`    ${x.group}  count=${x.count} min=${x.min} max=${x.max} fulfilled=${x.fulfilled}`));
const margin = g.find((x) => x.max === 1 && x.group !== '製程');
if (!margin) {
  console.log('\nSKIP: the recipe declares no margin group, so there is nothing to over-select.');
  process.exit(0);
}
console.log(`    margin group: ${margin.group} (${margin.count}/${margin.max})`);

// Select every tag in the margin group -- needs at least two to breach maxCount.
const marginTags = await ev(`[...document.querySelectorAll('[data-testid="tag-option"][data-group=${JSON.stringify(margin.group)}]')]
  .map(function(e){return e.dataset.tag;})`);
console.log('[3] margin tags available:', JSON.stringify(marginTags));
if (marginTags.length < 2) {
  console.log('\nSKIP: the margin group has fewer than two tags, so maxCount:1 cannot be breached.');
  process.exit(0);
}
for (const t of marginTags.slice(0, 2)) {
  await api('/click', { selector: `[data-testid="tag-option"][data-group=${JSON.stringify(margin.group)}][data-tag=${JSON.stringify(t)}]` });
  await sleep(1200);
}

g = await groups();
const m2 = g.find((x) => x.group === margin.group);
const p = await play();
console.log(`[4] after selecting 2: ${margin.group} count=${m2.count} fulfilled=${m2.fulfilled}`);
console.log(`    play ready=${p.ready} reason=${p.reason}`);

// The assertion: readiness is the AND of everything shown.
const allShownOk = g.every((x) => x.fulfilled);
check(p.ready === allShownOk,
      `play ready=${p.ready} but the rendered groups say ${allShownOk} `
    + `(${g.filter((x) => !x.fulfilled).map((x) => x.group).join(', ') || 'all satisfied'}) `
    + `-- reason="${p.reason}"`);

await api('/shot', { path: fails ? 'play_readiness_fail.png' : 'play_readiness.png' });
console.log(fails ? '\nFAIL: play does not reflect the groups it shows'
                  : '\nPASS: play readiness matches every rendered group');
process.exit(fails ? 1 : 0);
