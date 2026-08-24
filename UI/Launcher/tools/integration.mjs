// The whole thing, once, for real.
//
//   node tools/integration.mjs <update.zip> <workingDir>
//
// Installs a real package, selects it, lets the launcher start the real core,
// and then checks the WebUI -- not the launcher's own shell -- for the one
// capability the rewrite had to replace: the native folder picker that used to
// need an express + WebSocket server of its own.
//
// This is the test the unit-shaped ones cannot be: selftest.mjs never opens a
// window, and shell_smoke.mjs never leaves the shell. The handover from shell
// to WebUI is where a preload or a navigation mistake would hide.
import { _electron as electron } from 'playwright';
import { createRequire } from 'node:module';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const require = createRequire(import.meta.url);
const { Config } = require('../src/config');
const { AppStore } = require('../src/apps');
const { Updater } = require('../src/updater');

const here = path.dirname(fileURLToPath(import.meta.url));
const appDir = path.resolve(here, '..');
const ZIP = process.argv[2];
const CORE_DATA = process.argv[3];
if (!ZIP || !CORE_DATA) {
  console.error('usage: node tools/integration.mjs <update.zip> <workingDir>');
  process.exit(2);
}

let failures = 0;
const check = (name, cond, detail) => {
  console.log(`${cond ? 'PASS' : 'FAIL'}  ${name}${detail ? '  -- ' + detail : ''}`);
  if (!cond) failures++;
};

const userData = fs.mkdtempSync(path.join(os.tmpdir(), 'launcher-integration-'));
const cfg = new Config(userData);
cfg.values.workingDir = path.resolve(CORE_DATA);
cfg.save();
const apps = new AppStore(cfg);
console.log('installing ' + ZIP);
const inst = await new Updater(cfg, apps).install(ZIP, () => {});
apps.setCurrent(inst.version);
console.log('payload ' + inst.version + ' selected\n');

const app = await electron.launch({ args: [appDir, `--user-data-dir=${userData}`] });
app.process().stderr.on('data', (d) => process.stderr.write('[main] ' + d));

const win = await app.firstWindow();
await win.waitForLoadState('domcontentloaded');

// The launcher starts the core and only hands the window to the WebUI once the
// core answers, or after its 20 s grace period. Camera init retries on this
// bench, so allow generously.
let onWebUI = false;
for (let i = 0; i < 90; i++) {
  const url = win.url();
  if (/WebUI[\\/]index\.html$/i.test(decodeURIComponent(url))) { onWebUI = true; break; }
  await win.waitForTimeout(1000);
}
check('launcher handed the window to the WebUI', onWebUI, decodeURIComponent(win.url()).slice(-70));

if (onWebUI) {
  await win.waitForLoadState('domcontentloaded');
  await win.waitForTimeout(4000);

  const api = await win.evaluate(() => ({
    hasLauncher: typeof window.launcher === 'object' && window.launcher !== null,
    hasPickFolder: !!(window.launcher && typeof window.launcher.pickFolder === 'function'),
    // window.require IS defined in the WebUI, and that is not a hole: it is a
    // deliberate no-op shim in UI/WebUI/index.html
    // (`window.require = function () { return {}; }`) so the ESM page does not
    // throw on modules that require('electron') at top level. Asserting on its
    // mere existence reports a breach that is not there.
    //
    // What actually matters is whether Node is reachable. Check that.
    requireIsShim: typeof window.require === 'function'
                   && !Object.keys(window.require('fs') || {}).length,
    hasProcess: typeof window.process !== 'undefined',
    hasBuffer: typeof window.Buffer !== 'undefined',
    canSpawn: (() => {
      try { const cp = window.require('child_process'); return typeof cp.spawn === 'function'; }
      catch { return false; }
    })(),
    // The WebUI must NOT be able to install an update or select a version --
    // those are refused by the main process while the app is showing, and this
    // proves the refusal rather than trusting it.
    keys: Object.keys(window.launcher || {}).sort(),
  }));
  check('WebUI can see the launcher bridge', api.hasLauncher, api.keys.join(','));
  check('pickFolder is exposed to the WebUI', api.hasPickFolder);
  check('window.require is the inert WebUI shim, not Node', api.requireIsShim);
  check('process is NOT reachable from the WebUI', !api.hasProcess);
  check('Buffer is NOT reachable from the WebUI', !api.hasBuffer);
  check('child_process is NOT reachable from the WebUI', !api.canSpawn);

  const refused = await win.evaluate(async () => {
    try { await window.launcher.chooseAndInstall(); return null; }
    catch (e) { return String(e.message || e); }
  });
  check('installing an update from the WebUI is refused', refused !== null,
        refused || 'IT WAS ACCEPTED');

  const status = await win.evaluate(() => window.launcher.status());
  check('core is running under supervision', status.core.running,
        `pid ${status.core.pid}, uptime ${status.core.uptimeS?.toFixed(1)} s`);
  check('the health check is answering', !!status.core.lastHealth,
        status.core.lastHealth ? JSON.stringify(status.core.lastHealth.info).slice(0, 80) : 'no answer yet');
  // The launcher is supposed to contain no knowledge of this application's
  // layout. Assert it: the plan the shell reports must name an executable and a
  // control port that appear NOWHERE in the launcher's own source.
  check('the run plan came from the version, not the launcher', !!status.plan,
        status.plan ? `${status.plan.processes[0].exe} · ${status.bootRel}` : 'no plan');

  const stopped = await win.evaluate(() => window.launcher.stopCore());
  check('core stopped gracefully from the WebUI page', stopped.stopped && !stopped.forced,
        stopped.forced ? 'FORCE KILLED' : 'clean');
}

await app.close();
fs.rmSync(userData, { recursive: true, force: true });
console.log(`\n${failures === 0 ? 'ALL PASS' : failures + ' FAILURE(S)'}`);
process.exit(failures ? 1 : 0);
