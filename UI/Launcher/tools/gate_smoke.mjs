// The three-tap setup gate on the splash screen.
//
//   node tools/gate_smoke.mjs
//
// Installs a throwaway version whose "core" is a node process that just sits
// there, and whose UI is a page saying APPUI. Then runs the launcher twice:
// once left alone, once tapped three times. The first must end up on the
// application's UI; the second must stay on the launcher with the core stopped.
import { _electron as electron } from 'playwright';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const appDir = path.resolve(here, '..');
let failures = 0;
const check = (n, c, d) => { console.log(`${c ? 'PASS' : 'FAIL'}  ${n}${d ? '  -- ' + d : ''}`); if (!c) failures++; };

// --- a throwaway installed version -------------------------------------------
const root = fs.mkdtempSync(path.join(os.tmpdir(), 'gate-apps-'));
const work = fs.mkdtempSync(path.join(os.tmpdir(), 'gate-work-'));
const V = '9.9.9';
const vdir = path.join(root, V);
fs.mkdirSync(path.join(vdir, 'scripts'), { recursive: true });
fs.writeFileSync(path.join(vdir, 'info.json'), JSON.stringify({ version: V }));
// The launcher refuses an exe outside the version directory -- rightly -- so the
// stand-in core is a copy of node inside it.
fs.copyFileSync(process.execPath, path.join(vdir, 'core.exe'));
fs.writeFileSync(path.join(vdir, 'app.html'), '<!doctype html><title>APPUI</title><body>APPUI');
fs.writeFileSync(path.join(vdir, 'scripts', 'boot.js'), `
module.exports = { apiVersion: 1, describe(ctx) { return {
  name: 'gate test ${V}',
  ui: { indexPath: 'app.html' },
  processes: [{ id: 'core', primary: true,
    exe: 'core.exe', args: ['-e', 'setInterval(()=>{},1000)'],
    cwd: '@working' }],
}; } };
`);
fs.writeFileSync(path.join(root, 'current.json'), JSON.stringify({ version: V }));

async function run(tap, shotPath) {
  const userData = fs.mkdtempSync(path.join(os.tmpdir(), 'gate-ud-'));
  fs.writeFileSync(path.join(userData, 'launcher.json'),
    JSON.stringify({ appRoot: root, workingDir: work, splashHoldMs: 3000 }));
  const app = await electron.launch({ args: [appDir, `--user-data-dir=${userData}`] });
  const win = await app.firstWindow();
  await win.waitForLoadState('domcontentloaded');
  let sawGate = false;
  try {
    await win.waitForSelector('#gate:not(.hidden)', { timeout: 8000 });
    sawGate = true;
    if (shotPath) await win.screenshot({ path: shotPath });
    if (tap) for (let i = 0; i < 3; i++) { await win.locator('#gate').click({ position: { x: 10, y: 10 } }); }
  } catch { /* reported by the caller */ }
  return { app, win, sawGate };
}

// --- 1. left alone: the application's UI takes the window --------------------
{
  const { app, win, sawGate } = await run(false, process.env.GATE_SHOT || null);
  check('the gate is shown while the core boots', sawGate);
  let title = '';
  for (let i = 0; i < 40 && title !== 'APPUI'; i++) { await new Promise(r => setTimeout(r, 250)); title = await win.title().catch(() => ''); }
  check('left alone, the application UI takes over', title === 'APPUI', title);
  await app.close();
}

// --- 2. three taps: the launcher keeps the screen and the core is stopped ----
{
  const { app, win, sawGate } = await run(true);
  check('the gate is shown while the core boots (tap run)', sawGate);
  // The stand-in core declares no control channel, so stopping it waits out the
  // grace period and then force kills. Poll rather than guess.
  let running = true;
  for (let i = 0; i < 60 && running; i++) {
    await new Promise(r => setTimeout(r, 500));
    running = await win.evaluate(() => window.launcher.status().then(s => s.core.running)).catch(() => true);
  }
  const title = await win.title().catch(() => '');
  check('after three taps the app UI does NOT take over', title !== 'APPUI', title);
  if (process.env.GATE_SHOT2) await win.screenshot({ path: process.env.GATE_SHOT2 });
  const banner = await win.textContent('#banner').catch(() => '');
  check('the launcher says it is in setup mode', /設定模式/.test(banner), banner.slice(0, 60));
  check('the core was stopped', running === false, String(running));
  const gateHidden = await win.evaluate(() => document.getElementById('gate').className.includes('hidden'));
  check('the countdown is gone', gateHidden === true);
  await app.close();
}

console.log(failures ? `${failures} FAILURES` : '--- all pass ---');
process.exit(failures ? 1 : 0);
