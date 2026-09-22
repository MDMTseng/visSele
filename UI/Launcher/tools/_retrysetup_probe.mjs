// Call retrySetup the way the shell calls it, and report what comes back.
// The install path logs nothing at all after "installed ... select it to make it
// current", which is consistent with a success, a rejection AND a hang -- so ask
// directly instead of inferring.
import { _electron as electron } from 'playwright';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';

const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname.slice(1)), '..');
const REPO = path.resolve(ROOT, '..', '..');
const APPROOT = process.argv[2];                 // an appRoot with rc2 current + rc3 installed
if (!APPROOT) throw new Error('usage: node _retrysetup_probe.mjs <appRoot>');

const userData = fs.mkdtempSync(path.join(os.tmpdir(), 'lw-probe-'));
fs.writeFileSync(path.join(userData, 'launcher.json'), JSON.stringify({
  appRoot: APPROOT,
  workingDir: path.join(REPO, 'InspectionCore', 'Core0_1'),
  updateSource: path.join(REPO, 'InspectionCore', 'Core0_1', 'data', 'sync', 'DEV', 'X2Updates', 'INSP'),
  autoStart: false, checkUpdates: false,
}, null, 2));

const app = await electron.launch({ args: [ROOT, `--user-data-dir=${userData}`] });
const win = await app.firstWindow();
await win.waitForLoadState('domcontentloaded');
await new Promise((r) => setTimeout(r, 2500));

const out = await win.evaluate(async () => {
  const say = [];
  try {
    const u = await window.launcher.unlock('xception');
    say.push('unlock -> ' + JSON.stringify(u));
  } catch (e) { say.push('unlock THREW: ' + e.message); }

  // Race it: a handler that never settles is indistinguishable from a slow one
  // unless you put a clock on it, and "never settles" is the hypothesis.
  const timeout = new Promise((r) => setTimeout(() => r('__timeout__'), 15000));
  try {
    const r = await Promise.race([window.launcher.retrySetup('2.0.0-rc3'), timeout]);
    say.push('retrySetup -> ' + (r === '__timeout__' ? 'NEVER SETTLED in 15 s' : JSON.stringify(r)));
  } catch (e) { say.push('retrySetup THREW: ' + e.message); }

  try {
    const st = await window.launcher.status();
    say.push('current after -> ' + st.current);
  } catch (e) { say.push('status THREW: ' + e.message); }
  return say;
});
out.forEach((l) => console.log(l));
console.log('current.json on disk ->', fs.readFileSync(path.join(APPROOT, 'current.json'), 'utf8').trim());
await app.close();
