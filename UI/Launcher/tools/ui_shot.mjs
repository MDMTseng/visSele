// Open the app and photograph the SETTINGS page, so a UI change is verified by
// looking at it rather than by the fact that it compiled.
//
//   node tools/ui_shot.mjs <out.png>
//     SOAK_APP_ROOT / SOAK_WORKING_DIR -- same meaning as soak.mjs
//
// The launcher starts a real core, so this cannot run while a soak owns the
// control port; it refuses rather than producing a screenshot of the wrong
// process's UI.
import { _electron as electron } from 'playwright';
import { createRequire } from 'node:module';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const require = createRequire(import.meta.url);
const { Config } = require('../src/config');

const here = path.dirname(fileURLToPath(import.meta.url));
const APP_DIR = path.resolve(here, '..');
const REPO = path.resolve(here, '..', '..', '..');
const APP_ROOT = process.env.SOAK_APP_ROOT || path.join(REPO, 'export_v2', 'app');
const WORKING_DIR = process.env.SOAK_WORKING_DIR
  || path.join(REPO, 'InspectionCore', 'Core0_1');
const OUT = process.argv[2] || path.join(os.tmpdir(), 'ui-setting.png');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

{
  const control = require('../src/control');
  const r = await control.ping(4098, 800);
  if (r.ok) { console.log('FAILED: a core is already running -- stop it first'); process.exit(1); }
}

const userData = fs.mkdtempSync(path.join(os.tmpdir(), 'uishot-launcher-'));
const cfg = new Config(userData);
cfg.values.appRoot = APP_ROOT;
cfg.values.workingDir = WORKING_DIR;
cfg.save();

console.log('appRoot ' + APP_ROOT);
const app = await electron.launch({ args: [APP_DIR, `--user-data-dir=${userData}`] });
const page = await app.firstWindow();

let onApp = false;
for (let i = 0; i < 90; i++) {
  const u = decodeURIComponent(page.url());
  if (/WebUI/i.test(u)) { onApp = true; break; }
  await sleep(1000);
}
if (!onApp) { console.log('FAILED: never reached the WebUI -- ' + page.url()); await app.close(); process.exit(1); }
await page.waitForLoadState('domcontentloaded');
await sleep(8000);

// A bench with no camera puts a modal over everything; it ships its own way out.
const skipCamera = () => page.evaluate(() => {
  let n = 0;
  for (const b of document.querySelectorAll('button, .ant-btn, a'))
    if (b.offsetParent && (b.innerText || '').includes('跳過相機連線')) { b.click(); n++; }
  return n;
}).catch(() => 0);
for (let i = 0; i < 6; i++) { if (await skipCamera()) await sleep(1200); else break; }

const clickText = (label) => page.evaluate((label) => {
  for (const e of document.querySelectorAll('button, .ant-btn, a, li, div')) {
    const t = (e.innerText || '').trim();
    if (t === label && e.offsetParent) { e.click(); return true; }
  }
  return false;
}, label);

if (!await clickText('設定')) {
  await page.screenshot({ path: OUT, fullPage: true }).catch(() => {});
  console.log('FAILED: no 設定 entry found; shot of what was on screen -> ' + OUT);
  await app.close(); process.exit(1);
}
await sleep(3000);

// Assert the new sections are actually rendered before believing the picture.
const marks = ['報告 .xreps', '每 N 筆上傳 1 筆', '留在追蹤視窗的時間',
               '後端位址', '檢測監控頁'];
const seen = await page.evaluate((m) => {
  const t = document.body.innerText;
  return m.map((x) => [x, t.includes(x)]);
}, marks);
for (const [m, ok] of seen) console.log(`  ${ok ? 'OK  ' : 'MISS'} ${m}`);


// UI_SHOT_EDIT=1: type into a field and prove the value reaches
// machine_custom_setting. A control that renders is not a control that works --
// this page keeps its own copy of the setting in local state and only pushes it
// out through onMachCusSettingUpdate, so "it appeared on screen" says nothing
// about whether an edit is kept.
if (process.env.UI_SHOT_EDIT === '1') {
  await page.evaluate(() => {
    for (const x of document.querySelectorAll('.ant-drawer-close')) if (x.offsetParent) x.click();
  }).catch(() => {});
  await sleep(800);
  const TARGET = '檢測監控頁';
  const PROBE = 'https://probe.invalid:9/?days=1&at=top&sha=0123456789&';
  const box = await page.evaluateHandle((label) => {
    for (const d of document.querySelectorAll('div')) {
      if (d.children.length === 0 && (d.innerText || '').trim().startsWith(label)) {
        const wrap = d.parentElement;
        const inp = wrap && wrap.querySelector('input');
        if (inp) return inp;
      }
    }
    return null;
  }, TARGET);
  const el = box.asElement();
  if (!el) {
    console.log(`  EDIT FAIL: no input under ${TARGET}`);
  } else {
    await el.click({ clickCount: 3 });
    await el.fill(PROBE);
    await sleep(1200);
    const readBack = await el.inputValue();
    // Open RAW: it renders machine_custom_setting itself, so it is the page's
    // own answer to "what did you actually store", not a second copy of the box.
    await page.evaluate(() => {
      for (const h of document.querySelectorAll('.ant-collapse-header'))
        if ((h.innerText || '').includes('RAW')) h.click();
    });
    await sleep(800);
    const inRaw = await page.evaluate((p) => {
      const pre = document.querySelector('.ant-collapse-content pre');
      return pre ? pre.innerText.includes(p) : null;
    }, PROBE);
    console.log(`  EDIT field accepts typing : ${readBack === PROBE ? 'OK' : 'FAIL (' + readBack + ')'}`);
    console.log(`  EDIT reaches the setting  : ${inRaw === true ? 'OK' : inRaw === null ? 'FAIL (no RAW pre)' : 'FAIL (not in machine_custom_setting)'}`);
    if (inRaw !== true) {
      const dbg = await page.evaluate(() => {
        const pres = [...document.querySelectorAll('pre')];
        return { n: pres.length, lines: pres.map((p) => (p.innerText.split(String.fromCharCode(10))
          .filter((l) => l.includes('monitor_url')).join(' | ') || '(no monitor_url line)')) };
      });
      console.log('  DEBUG pre count ' + dbg.n);
      dbg.lines.forEach((l, i) => console.log('  DEBUG pre[' + i + '] ' + l.slice(0, 160)));
    }
  }
}
await page.screenshot({ path: OUT });
const BOT = OUT.replace(/\.png$/i, '') + '-bottom.png';
const scrolled = await page.evaluate(() => {
  for (const e of document.querySelectorAll('div')) {
    if (e.scrollHeight > e.clientHeight + 40 && /auto|scroll/.test(getComputedStyle(e).overflowY)) {
      e.scrollTop = e.scrollHeight; return { h: e.scrollHeight, c: e.clientHeight };
    }
  }
  return null;
});
console.log(scrolled ? `  scroll container ${scrolled.c}px viewport / ${scrolled.h}px content`
                     : '  NO scrollable container found');
await sleep(600);
await page.screenshot({ path: BOT });
console.log('shot -> ' + OUT);
console.log('shot -> ' + BOT);
await app.close();
