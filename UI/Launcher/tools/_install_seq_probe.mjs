// Replay the shell's install sequence with every return value printed.
//
// The walkthrough shows the install finishing and then nothing at all -- no
// success line, no error line, no exception. That is consistent with two very
// different faults, so this runs the same three IPC calls directly, on a fresh
// appRoot, and prints what each one answers.
import { _electron as electron } from 'playwright';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';

const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname.slice(1)), '..');
const REPO = path.resolve(ROOT, '..', '..');
const SRC_RC2 = path.join(REPO, 'export_v2', 'app', '2.0.0-rc2');
const UPDATE_SRC = path.join(REPO, 'InspectionCore', 'Core0_1', 'data', 'sync', 'DEV', 'X2Updates', 'INSP');

const userData = fs.mkdtempSync(path.join(os.tmpdir(), 'lw-seq-ud-'));
const appRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'lw-seq-apps-'));
console.log('staging rc2...');
fs.cpSync(SRC_RC2, path.join(appRoot, '2.0.0-rc2'), { recursive: true });
fs.writeFileSync(path.join(appRoot, 'current.json'), JSON.stringify({ version: '2.0.0-rc2' }));
fs.writeFileSync(path.join(userData, 'launcher.json'), JSON.stringify({
  appRoot, workingDir: path.join(REPO, 'InspectionCore', 'Core0_1'),
  updateSource: UPDATE_SRC, autoStart: false, checkUpdates: true,
}, null, 2));

const app = await electron.launch({ args: [ROOT, `--user-data-dir=${userData}`] });
const win = await app.firstWindow();
await win.waitForLoadState('domcontentloaded');
win.on('pageerror', (e) => console.log('[pageerror]', e.message));
await new Promise((r) => setTimeout(r, 3000));

const out = await win.evaluate(async () => {
  const L = window.launcher, say = [];
  const step = async (name, fn) => {
    const t0 = Date.now();
    try {
      const v = await fn();
      say.push(`${name} (${Date.now() - t0} ms) -> ${JSON.stringify(v)}`);
      return v;
    } catch (e) { say.push(`${name} (${Date.now() - t0} ms) THREW: ${e.message}`); return null; }
  };
  const offer = await step('updateOffer', () => L.updateOffer());
  await step('unlock', () => L.unlock('xception'));
  const r = await step('installFromSource', () => L.installFromSource(offer && offer.file));
  say.push(`>>> shell would ${r === undefined ? 'RETURN SILENTLY (r undefined)'
            : !r.ok ? 'log 安裝失敗' : 'go on to retrySetup'}`);
  await step('retrySetup', () => L.retrySetup(offer && offer.version));
  const st = await step('status.current', async () => (await L.status()).current);
  return say;
});
out.forEach((l) => console.log(l));
console.log('current.json ->', fs.readFileSync(path.join(appRoot, 'current.json'), 'utf8').trim());
await app.close();
