// A REAL upgrade, driven end to end, photographed at every step.
//
//     node tools/upgrade_walkthrough.mjs [outDir]
//
// Everything happens in a throwaway userData + appRoot, so the machine's own
// launcher state, installed versions and current.json are untouched. The update
// source is the real sync folder -- that part is not simulated, because the
// thing most worth testing is whether what is actually sitting in X2Updates
// gets offered, installed and selected.
//
// The core is NOT started: startCore would take the window away from the shell
// three seconds in, which is the very defect this walkthrough is meant to show
// rather than trip over. The run sets autoStartOff so the shell stays put; if
// that switch does not exist yet the run says so instead of pretending.
import { _electron as electron } from 'playwright';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';

const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname.slice(1)), '..');
const REPO = path.resolve(ROOT, '..', '..');
const OUT = path.resolve(process.argv[2] || path.join(REPO, '_upgrade_shots'));
const SRC_RC2 = path.join(REPO, 'export_v2', 'app', '2.0.0-rc2');
const UPDATE_SRC = path.join(REPO, 'InspectionCore', 'Core0_1', 'data', 'sync', 'DEV', 'X2Updates', 'INSP');
const WORKDIR = path.join(REPO, 'InspectionCore', 'Core0_1');

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let step = 0;
const shots = [];

async function shot(win, name) {
  const file = path.join(OUT, `${String(++step).padStart(2, '0')}_${name}.png`);
  await win.screenshot({ path: file });
  const note = await win.evaluate(() => {
    const t = (id) => { const e = document.getElementById(id); return e ? e.innerText.trim() : ''; };
    const modal = document.getElementById('modal');
    return {
      status: t('coreStatus').replace(/\s+/g, ' ').slice(0, 120),
      banner: t('banner').replace(/\s+/g, ' ').slice(0, 160),
      modal: modal ? modal.innerText.replace(/\s+/g, ' ').slice(0, 200) : '',
      versions: t('versions').replace(/\s+/g, ' ').slice(0, 200),
      updateSrc: t('updateSrc').replace(/\s+/g, ' ').slice(0, 120),
      // The log is the only place the install path reports itself, so the
      // walkthrough has to keep it -- a screenshot crops it, a summary drops it.
      log: t('log').split(String.fromCharCode(10)).slice(-14).join(' | ').slice(0, 900),
    };
  });
  shots.push({ file: path.basename(file), name, ...note });
  console.log(`\n== ${step}. ${name}`);
  for (const [k, v] of Object.entries(note)) if (v) console.log(`   ${k}: ${v}`);
  return note;
}

// Click a visible element by its exact trimmed text, the way a finger would.
async function tap(win, label) {
  const ok = await win.evaluate((label) => {
    for (const e of document.querySelectorAll('button, .btn, a')) {
      if (!e.offsetParent) continue;
      if ((e.innerText || '').trim() !== label) continue;
      e.click(); return true;
    }
    return false;
  }, label);
  if (!ok) throw new Error(`no visible control labelled "${label}"`);
  await sleep(700);
}

async function main() {
  fs.rmSync(OUT, { recursive: true, force: true });
  fs.mkdirSync(OUT, { recursive: true });

  const userData = fs.mkdtempSync(path.join(os.tmpdir(), 'lw-ud-'));
  const appRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'lw-apps-'));

  // A machine already on rc2, exactly as the line has it.
  console.log('staging rc2 into the throwaway appRoot (273 MB, ~20 s)...');
  fs.cpSync(SRC_RC2, path.join(appRoot, '2.0.0-rc2'), { recursive: true });
  fs.writeFileSync(path.join(appRoot, 'current.json'),
                   JSON.stringify({ version: '2.0.0-rc2' }, null, 2));

  fs.writeFileSync(path.join(userData, 'launcher.json'), JSON.stringify({
    appRoot,
    workingDir: WORKDIR,
    updateSource: UPDATE_SRC,
    checkUpdates: true,
    autoStart: false,          // honoured only once the switch exists
    splashHoldMs: 3000,
    keepVersions: 3,
  }, null, 2));
  console.log('userData:', userData);
  console.log('appRoot :', appRoot);

  const app = await electron.launch({
    args: [ROOT, `--user-data-dir=${userData}`],
    env: { ...process.env, LAUNCHER_NO_AUTOSTART: '1' },
  });
  const win = await app.firstWindow();
  // The shell swallows nothing on purpose, but an exception thrown inside a
  // button's async onClick never reaches its own try/catch -- so watch the
  // renderer directly rather than trusting the on-screen log to be complete.
  win.on('console', (m) => { if (m.type() === 'error') console.log('   [console] ' + m.text()); });
  win.on('pageerror', (e) => console.log('   [pageerror] ' + e.message));
  await win.waitForLoadState('domcontentloaded');
  await sleep(4000);

  await shot(win, 'launcher-opened');

  // Does the shell still have the window, or did the core take it?
  const stillShell = await win.evaluate(() => !!document.getElementById('coreStatus'));
  if (!stillShell) {
    console.log('\n!! the core took the window -- the shell is gone, so the rest of the');
    console.log('   walkthrough cannot be driven. This IS the autostart defect.');
    await shot(win, 'core-took-the-window');
    await app.close();
    return finish();
  }

  await shot(win, 'update-offer');

  const hasOffer = await win.evaluate(() => !!document.getElementById('modal'));
  if (!hasOffer) throw new Error('no update offer modal appeared');

  // --- install ---------------------------------------------------------------
  await tap(win, '安裝');
  await sleep(600);
  await shot(win, 'unlock-prompt');

  // The speed bump. Type the word the way an operator does.
  await win.evaluate(() => {
    const i = document.querySelector('#modal input[type=password]');
    if (i) { i.value = 'xception'; i.dispatchEvent(new Event('input', { bubbles: true })); }
    else throw new Error('unlock field not found');
  });
  await shot(win, 'unlock-typed');
  await tap(win, '確定');

  // Verifying 300 SHA256s over 275 MB is not instant.
  for (let i = 0; i < 40; i++) {
    const done = await win.evaluate(() =>
      (document.getElementById('log').innerText || '').includes('已設為現行')
      || (document.getElementById('log').innerText || '').includes('安裝失敗'));
    if (done) break;
    if (i === 3) await shot(win, 'installing');
    await sleep(3000);
  }
  await shot(win, 'installed-and-selected');

  // --- start the new version -------------------------------------------------
  await tap(win, '啟動');
  await sleep(9000);
  const gone = await win.evaluate(() => !document.getElementById('coreStatus'));
  await shot(win, gone ? 'app-ui-took-over' : 'after-start');

  await app.close();
  finish();
}

function finish() {
  fs.writeFileSync(path.join(OUT, 'steps.json'), JSON.stringify(shots, null, 2));
  console.log(`
${shots.length} screenshots -> ${OUT}`);
}

main().catch(async (e) => { console.error('FAILED:', e.message); finish(); process.exit(1); });
