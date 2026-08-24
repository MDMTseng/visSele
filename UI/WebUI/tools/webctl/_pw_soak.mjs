// Drive the PRODUCTION WebUI into a running inspection, then measure its memory.
//
//   node _pw_soak.mjs [minutes] [url]
//
// Why Playwright directly rather than the webctld harness: the harness reaches
// for window.__GP_STORE__, which only the dev bundle exposes -- and the memory
// number worth having is the PRODUCTION one (the reported climb was 600MB ->
// 2.5GB on :8082). Driving the real buttons needs no debug handle, and being
// able to screenshot means the state is observed rather than assumed.
//
// The click order is not arbitrary and cost an hour to find:
//   recipe FIRST  -- it declares which 製程 are legal
//   then 製程 from that set -- picking one outside it greys the play button
//   then 檢測方式, then play, then the machine's own start button
import { chromium } from 'playwright';
import { execSync } from 'node:child_process';

const MIN = Number(process.argv[2] || 20);
const URL = process.argv[3] || 'http://localhost:8082/';
const OUT = 'C:/Users/w2110/Downloads/pw';
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

const browser = await chromium.launch({ headless: true });
const ctx = await browser.newContext({ viewport: { width: 1600, height: 950 } });
const page = await ctx.newPage();
page.on('pageerror', e => console.log('  [pageerror]', e.message.slice(0, 110)));
await page.goto(URL, { waitUntil: 'domcontentloaded' });
await sleep(6000);

const clickText = async (label, tag = '*') => page.evaluate(({ label, tag }) => {
  for (const e of document.querySelectorAll(tag)) {
    const t = (e.innerText || '').trim();
    if (t !== label || e.children.length || !e.offsetParent) continue;
    e.click(); return true;
  } return false; }, { label, tag });
const clickIcon = async (icon, nth = 0) => page.evaluate(({ icon, nth }) => {
  let k = 0;
  for (const b of document.querySelectorAll('button')) {
    if (!b.offsetParent || b.disabled) continue;
    if (b.querySelector('[class*="' + icon + '"]')) { if (k++ === nth) { b.click(); return true; } }
  } return false; }, { icon, nth });

console.log('[1] recipe');
await clickText('跳過相機連線', 'button'); await sleep(1500);
await clickIcon('anticon-folder-open'); await sleep(3000);
await clickText('test1.hydef'); await sleep(900);
await page.evaluate(() => { for (const e of document.querySelectorAll('*')) {
  if ((e.innerText || '').trim() === 'test1.hydef' && !e.children.length && e.offsetParent) {
    e.dispatchEvent(new MouseEvent('dblclick', { bubbles: true })); return; } } });
await sleep(5000);
console.log('[2] 製程 + 檢測方式');
await clickText('11沖壓成形'); await sleep(1200);
await clickText('全檢'); await sleep(1200);
console.log('[3] into the Inspection UI');
await clickIcon('anticon-caret-right'); await sleep(9000);
// NOT pressing the machine's start button when it is already running: that
// control toggles, so "start" on a running machine stops it. Entering the
// Inspection view is enough to receive the stream, which is all the memory
// question needs.
if (process.env.PW_START_MACHINE === '1') {
  console.log('[4] start the machine');
  await clickIcon('anticon-caret-right'); await sleep(9000);
} else {
  console.log('[4] machine already running -- observing only');
  await sleep(6000);
}
// The diagnostics drawer covers the right half and the panel we need to read.
await page.evaluate(() => { const x = document.querySelector('.ant-drawer-close');
  if (x && x.offsetParent) x.click(); });
await sleep(2000);
await page.screenshot({ path: OUT + '/08_running.png' });

// chrome-headless-shell, not chrome: that is the binary Playwright launches
// headless, and filtering on the wrong name silently reports 0 MB forever.
const rss = () => { try {
  const o = execSync('tasklist /FI "IMAGENAME eq chrome-headless-shell.exe" /FO CSV /NH', { encoding: 'utf8', timeout: 15000 });
  let kb = 0; for (const l of o.split(/\r?\n/)) { const m = l.match(/"([\d,]+) K"\s*$/); if (m) kb += Number(m[1].replace(/,/g, '')); }
  return Math.round(kb / 1024);
} catch { return -1; } };

console.log('');
// The machine panel's own text, sampled every tick. A flat heap means nothing
// unless load actually reached the UI -- and the board running hard is not
// proof that the browser received any of it.
const panelText = () => page.evaluate(() => {
  for (const e of document.querySelectorAll('div')) {
    const t = (e.innerText || '');
    if (t.includes('rpm') && t.includes('/s') && t.length < 260) return t.replace(/\s+/g, ' ').trim();
  }
  return '';
}).catch(() => '');
console.log('t_min,heapMB,totalMB,rssMB,domNodes,panel');
const t0 = Date.now();
for (let i = 0; i <= MIN; i++) {
  const v = await page.evaluate(() => { const m = performance.memory || {};
    return { h: +(m.usedJSHeapSize / 1048576).toFixed(1), t: +(m.totalJSHeapSize / 1048576).toFixed(1),
             d: document.getElementsByTagName('*').length }; }).catch(() => null);
  const pt = await panelText();
  if (v) console.log([((Date.now() - t0) / 60000).toFixed(1), v.h, v.t, rss(), v.d, '"' + pt.slice(0, 110) + '"'].join(','));
  else console.log(((Date.now() - t0) / 60000).toFixed(1) + ',err');
  if (i === 2) await page.screenshot({ path: OUT + '/09_t2min.png' });
  await sleep(60000);
}
await page.screenshot({ path: OUT + '/10_end.png' });
await browser.close();
