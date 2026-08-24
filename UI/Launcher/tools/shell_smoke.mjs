// Launch the real Electron shell and check it comes up, renders, and refuses
// what it is supposed to refuse.
//
//   node tools/shell_smoke.mjs
//
// Points the launcher at a throwaway data root with NO installed version, the
// first-run state and the one most likely to be broken: every other screen is
// reachable only after something has been installed.
import { _electron as electron } from 'playwright';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const appDir = path.resolve(here, '..');
const userData = fs.mkdtempSync(path.join(os.tmpdir(), 'launcher-smoke-'));

let failures = 0;
const check = (name, cond, detail) => {
  console.log(`${cond ? 'PASS' : 'FAIL'}  ${name}${detail ? '  -- ' + detail : ''}`);
  if (!cond) failures++;
};

const app = await electron.launch({
  args: [appDir, `--user-data-dir=${userData}`],
  env: { ...process.env, ELECTRON_ENABLE_LOGGING: '1' },
});

const win = await app.firstWindow();
await win.waitForLoadState('domcontentloaded');
// The shell polls status every 5 s and renders on the first reply.
await win.waitForFunction(() => document.getElementById('ident').textContent.length > 0, null,
                          { timeout: 15000 });

check('window opened', true, await win.title());

const ident = await win.textContent('#ident');
check('identifies itself', /launcher .* electron /.test(ident), ident.replace(/\n/g, ' | '));
check('reports that no version is selected', /no version selected/.test(ident));

const banner = await win.textContent('#banner');
check('first-run banner explains what to do', /安裝更新包|工作目錄/.test(banner || ''), (banner || '').slice(0, 70));

// With no version installed there is nothing to start, so the button must be
// disabled -- and the banner, not a dead button, is what tells the operator
// what to do. An enabled Start here would be an invitation to a failure.
check('start is disabled when there is no version to start', !(await win.isEnabled('#btnStart')));
check('stop button is disabled with nothing running', !(await win.isEnabled('#btnStop')));

const versionsText = await win.textContent('#versions');
check('version table says it is empty', /尚未安裝/.test(versionsText));

// contextIsolation must actually be on: the renderer gets `launcher` and
// nothing else. If `require` is reachable here the whole security posture of
// the rewrite is undone, and it would still look fine on screen.
const exposure = await win.evaluate(() => ({
  hasLauncher: typeof window.launcher === 'object',
  hasRequire: typeof window.require !== 'undefined',
  hasProcess: typeof window.process !== 'undefined',
  keys: Object.keys(window.launcher || {}).sort(),
}));
check('preload API is present', exposure.hasLauncher, exposure.keys.join(','));
check('node require is NOT reachable from the renderer', !exposure.hasRequire);
check('process is NOT reachable from the renderer', !exposure.hasProcess);

// The main process must reject renderer calls that are wrong for the state,
// rather than trusting the UI to have disabled the button.
const rejected = await win.evaluate(async () => {
  try { await window.launcher.selectVersion('does-not-exist'); return null; }
  catch (e) { return String(e.message || e); }
});
check('selecting a missing version is refused', rejected !== null, rejected || 'IT WAS ACCEPTED');

await app.close();
fs.rmSync(userData, { recursive: true, force: true });
console.log(`\n${failures === 0 ? 'ALL PASS' : failures + ' FAILURE(S)'}`);
process.exit(failures ? 1 : 0);
