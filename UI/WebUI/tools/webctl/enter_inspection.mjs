// Drive the WebUI from cold to a running Inspection UI, by script.
//
// Written because the flow was worked out by hand three times and lost each
// time. It is four steps and every one of them has a trap:
//
//   1. The diagnostics drawer opens over the page on load. Its close button is
//      `.ant-drawer-close`; the generic `.ant-btn...` selector matches four
//      other buttons first and Playwright clicks the wrong one, then times out
//      because the <pre> in the drawer intercepts the pointer.
//   2. Inspection mode must be chosen (檢測方式 -> 測試) BEFORE the play button
//      does anything. There are three elements reading 測試 on the page -- a
//      title tag, one in 製程, one in 檢測方式 -- and only the last is the mode.
//   3. The play button is the 100x100 one in the bottom bar. It has the same
//      class as its 50x50 neighbours, so it is selected by DOM index, resolved
//      at runtime rather than hard-coded.
//   4. antd controls are SPANs; a JS .click() on them silently does nothing.
//      Every interaction here goes through a real Playwright click.
//
// KNOWN BROKEN (2026-08-09): pressing start in the Inspection UI raises
// "轉速沒有寫進裝置,未進入檢測模式" -- SETTABLE_KEYS in src/script.jsx:1555 is
// a list of FLAT keys (plate_freq, ...) while the device now accepts only
// grouped ones ({"plate":{"freq":...}}). The device silently ignores unknown
// keys and still acks true, so the UI only finds out by reading the value back.
// Until that is fixed, this script gets you to a loaded recipe with the station
// ROI applied, and the plate has to be started from the test harness instead.
//
//   node enter_inspection.mjs [--url http://localhost:8081/] [--shot out.png]
//
// Needs webctld running (node webctl.mjs start) and a core on 4090.

import { spawnSync } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const args = process.argv.slice(2);
const flag = (name, dflt) => {
  const i = args.indexOf('--' + name);
  return i >= 0 && args[i + 1] ? args[i + 1] : dflt;
};

const URL = flag('url', 'http://localhost:8081/');
const SHOT = flag('shot', '');

function ctl(...a) {
  const r = spawnSync('node', [path.join(HERE, 'webctl.mjs'), ...a],
                      { encoding: 'utf8' });
  const out = (r.stdout || '') + (r.stderr || '');
  if (out.includes('ERR:')) throw new Error(a[0] + ': ' + out.trim().slice(0, 300));
  return out;
}
const evalJs = (expr) => {
  const out = ctl('eval', expr);
  const m = out.match(/"result":\s*("(?:[^"\\]|\\.)*"|[^\n]*)/);
  return m ? m[1].replace(/^"|"$/g, '') : '';
};
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

const step = (n, msg) => console.log(`[${n}] ${msg}`);

try {
  step(1, 'goto ' + URL);
  ctl('goto', URL);
  await sleep(12000);

  // The drawer is not always present -- only close it if it is, so a warm page
  // does not fail here.
  const hasDrawer = evalJs("(()=>document.querySelector('.ant-drawer-close')?'yes':'no')()");
  if (hasDrawer === 'yes') {
    step(2, 'close diagnostics drawer');
    ctl('click', '.ant-drawer-close');
    await sleep(3000);
  } else {
    step(2, 'no drawer open, skipping');
  }

  // 檢測方式 -> 測試. Pick by position: the mode row is the LAST of the tags
  // reading 測試, so take the highest index rather than a fixed nth.
  step(3, 'select inspection mode 測試');
  const modeIdx = evalJs(
    "(()=>{const t=[...document.querySelectorAll('span.ant-tag-has-color')]" +
    ".filter(e=>e.textContent.trim()==='測試');" +
    "return String(t.length?t.length-1:-1)})()");
  if (modeIdx === '-1') throw new Error('no 測試 mode tag found');
  ctl('click', `span.ant-tag-has-color:text-is('測試') >> nth=${modeIdx}`);
  await sleep(4000);

  // The play button: the widest button in the bottom-right bar.
  step(4, 'press play (enter Inspection UI)');
  const playIdx = evalJs(
    "(()=>{const all=[...document.querySelectorAll('button.ant-btn')];" +
    "let best=-1,bw=0;all.forEach((e,i)=>{const r=e.getBoundingClientRect();" +
    "if(r.top>innerHeight*0.8&&r.left>innerWidth*0.8&&r.width>bw){bw=r.width;best=i}});" +
    "return String(best)})()");
  if (playIdx === '-1') throw new Error('play button not found');
  ctl('click', `button.ant-btn >> nth=${playIdx}`);
  await sleep(12000);

  // Confirm we actually landed in the Inspection UI rather than silently
  // staying put: the station-region readout only exists there.
  const station = evalJs(
    "(()=>{const e=[...document.querySelectorAll('*')]" +
    ".find(x=>x.children.length===0&&/^\\d+[x\u00d7]\\d+ @/.test(x.textContent.trim()));" +
    "return e?e.textContent.trim():'NOT-IN-INSPECTION-UI'})()");
  step(5, 'station region: ' + station);
  if (station === 'NOT-IN-INSPECTION-UI')
    throw new Error('did not reach the Inspection UI');

  if (SHOT) {
    ctl('shot', path.resolve(SHOT));
    step(6, 'screenshot -> ' + path.resolve(SHOT));
  }
  console.log('OK: Inspection UI is up, recipe and station ROI applied.');
  console.log('NOTE: starting the plate from the UI is broken (SETTABLE_KEYS ' +
              'sends flat keys). Start it from the harness.');
} catch (e) {
  console.error('FAILED: ' + e.message);
  process.exit(1);
}
