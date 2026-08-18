// Drive the WebUI from cold to a running Inspection UI, by script.
//
//   node enter_inspection.mjs [--url http://localhost:8081/] [--mode 測試] [--shot out.png]
//
// The sequence itself lives in lib_enter.mjs and is shared with flows.mjs.
// This file used to carry its own copy of it, and that is exactly how it
// broke: every fix the flow needed -- SPLASH bounces, the camera-reconnect
// modal, a collapsed side menu, waiting on the state machine instead of a
// fixed sleep -- landed in flows.mjs, while this copy kept the original four
// steps and quietly stopped reaching the UI ("station region:
// NOT-IN-INSPECTION-UI"). One implementation now, so the next fix cannot miss
// one of them.
//
// Requires webctld already running against the same URL:
//   WEBCTL_URL=http://localhost:8081 node webctld.mjs
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { makeCtl, toMain, dismissCamModal, enterInspection, loadRecipe, sleep } from './lib_enter.mjs';

const flag = (name, dflt) => {
  const hit = process.argv.find((a) => a === `--${name}` || a.startsWith(`--${name}=`));
  if (!hit) return dflt;
  if (hit.includes('=')) return hit.split('=').slice(1).join('=');
  const i = process.argv.indexOf(hit);
  return process.argv[i + 1] ?? dflt;
};
const URL_ = flag('url', 'http://localhost:8081/');
const MODE = flag('mode', '測試');
const SHOT = flag('shot', '');
// Play does nothing without a loaded recipe. Defaults to the checked-in
// fixture so this works on a machine with an empty recipe DB -- pass --def
// (base path, no extension) to drive a real one.
const DEF  = flag('def', path.join(path.dirname(fileURLToPath(import.meta.url)),
                                   'fixtures', 'caliper_verify_tagged'));

const ctl = makeCtl();
const { api, ev } = ctl;
const step = (n, msg) => console.log(`[${n}] ${msg}`);

try {
  step(1, 'goto ' + URL_);
  await api('/goto', { url: URL_ });
  await sleep(12000);

  step(2, 'settling the state machine at MAIN');
  await toMain(ctl);

  step(3, 'clearing the camera-reconnect modal if present');
  const clear = await dismissCamModal(ctl);
  if (!clear) console.log('    (modal still up -- continuing, clicks may be intercepted)');

  step(4, 'loading recipe ' + DEF);
  const loaded = await loadRecipe(ctl, DEF);
  console.log('    loaded def: ' + loaded);
  await toMain(ctl);

  step(5, `entering the Inspection UI (mode ${MODE})`);
  const st = await enterInspection(ctl, { mode: MODE, log: (m) => console.log('    ' + m) });

  // The station-region readout only exists in the Inspection UI, so it is a
  // second, independent confirmation of what the state machine already said.
  const station = await ev(
    `(function(){var e=[...document.querySelectorAll('*')].find(function(x){return x.children.length===0&&/^\\d+[x×]\\d+ @/.test(x.textContent.trim())});return e?e.textContent.trim():'(no station readout)';})()`
  );
  step(6, `state ${st}, station region: ${station}`);

  if (SHOT) {
    await api('/shot', { path: path.resolve(SHOT) });
    step(7, 'screenshot -> ' + path.resolve(SHOT));
  }
  console.log('OK: Inspection UI is up, recipe and station ROI applied.');
} catch (e) {
  console.error('FAILED: ' + e.message);
  process.exit(1);
}
