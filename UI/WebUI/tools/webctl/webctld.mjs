#!/usr/bin/env node
// webctl daemon: owns a Playwright Chromium page on the WebUI and exposes a
// tiny localhost HTTP API so the (stateless) CLI can drive it across calls.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { chromium } from 'playwright';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const PORT = Number(process.env.WEBCTL_PORT || 8765);
// 8081 is the vite dev server and 8082 the production preview; nothing has
// served 8080 on this bench for a long time, so the old default silently
// produced a daemon pointed at a dead port -- every suite then failed on an
// empty page, which reads as an application fault rather than a wrong URL.
// The startup probe below says so out loud instead.
const TARGET_URL = process.env.WEBCTL_URL || 'http://localhost:8081';
const HEADLESS = process.env.WEBCTL_HEADLESS === '1';
const USERDATA = path.join(__dirname, '.userdata');
const LOGFILE = path.join(__dirname, 'console.log');

const RING_MAX = 5000;
const ring = [];
let seq = 0;
const logStream = fs.createWriteStream(LOGFILE, { flags: 'a' });

function record(kind, text, extra = {}) {
  const entry = { id: ++seq, t: Date.now(), kind, text, ...extra };
  ring.push(entry);
  if (ring.length > RING_MAX) ring.shift();
  logStream.write(JSON.stringify(entry) + '\n');
}

// Default stays 1600x1000 on purpose: golden snapshots are coordinate-sensitive
// (see the /viewport endpoint), so changing it under regress.mjs would
// invalidate the baselines. Override per-run with WEBCTL_VIEWPORT=1920x1080
// when you want a headed window big enough to watch the real UI.
const VIEWPORT = (() => {
  const m = /^(\d+)x(\d+)$/.exec(process.env.WEBCTL_VIEWPORT || '');
  return m ? { width: +m[1], height: +m[2] } : { width: 1600, height: 1000 };
})();

// A HEADED window is for a human to look at, so it must fit the screen it is on.
// The fixed 1600x1000 forced a window taller and wider than a laptop display,
// and the bottom and right of the UI -- where the play button and the toolbar
// live -- ended up off-screen with no scrollbar to reach them.
//
// viewport:null tells Playwright to stop overriding the viewport and let it
// follow the real window, and --start-maximized then sizes that window to the
// display. HEADLESS keeps the fixed viewport: that is what regress.mjs runs
// under, and its golden snapshots are coordinate-sensitive (see /viewport), so
// they must not move. An explicit WEBCTL_VIEWPORT still wins in either mode.
const EXPLICIT_VIEWPORT = /^(\d+)x(\d+)$/.test(process.env.WEBCTL_VIEWPORT || '');
const FIT_WINDOW = !HEADLESS && !EXPLICIT_VIEWPORT;

// let, not const, and behind ensurePage() below: the page and the context used
// to be created once at module load and never again. Close the tab, crash the
// renderer, or kill Chromium, and every request from then on failed -- so a
// whole run of suites went red while re-running any one of them alone passed,
// because that run got a fresh daemon. That is exactly the shape of the three
// intermittents in worklist 3.3 (doorbell, r6_inspection T1, r7_inspbug T1),
// and it was watched happening: killing Chromium by hand took the entire
// runner from green to 21 FAILs in 164ms.
let context = await chromium.launchPersistentContext(USERDATA, {
  headless: HEADLESS,
  viewport: FIT_WINDOW ? null : VIEWPORT,
  // Size the OS window to match, or a 1920x1080 viewport just gets scrollbars
  // inside a smaller window.
  args: ['--disable-features=Translate',
         ...(FIT_WINDOW ? ['--start-maximized']
                        : [`--window-size=${VIEWPORT.width},${VIEWPORT.height + 90}`])],
});
let page = context.pages()[0] || (await context.newPage());

// The listeners belong to a page object, so a rebuilt page needs them again --
// forgetting that is how a recovered daemon goes quiet in the event log while
// still answering requests, which is worse than being down.
function attachHandlers(p) {
  p.on('console', (msg) => record('console.' + msg.type(), msg.text()));
  p.on('pageerror', (err) => record('pageerror', err.message, { stack: err.stack }));
  p.on('requestfailed', (req) =>
    record('requestfailed', `${req.method()} ${req.url()}`, { error: req.failure()?.errorText })
  );
  p.on('response', (res) => {
    if (res.status() >= 400) record('http' + res.status(), `${res.request().method()} ${res.url()}`);
  });
}
attachHandlers(page);

// Rebuild whatever died, in the order it can die: page first (cheap, common --
// somebody closed the tab), then the whole browser (Chromium gone).
//
// Recovery is RECORDED, not silent. A daemon that quietly re-navigates has
// thrown away the page state a suite was relying on, and a test that then fails
// for a different reason is worse to debug than one that fails loudly here.
let rebuilds = 0;
async function ensurePage() {
  if (page && !page.isClosed()) return;
  rebuilds++;
  try {
    // No pre-flight check on the context. An earlier version guarded with
    // `!context.browser()`, which is version-dependent -- older Playwright
    // returns null there for a PERSISTENT context, and this daemon uses
    // launchPersistentContext, so on those versions every closed tab would
    // have forced a full browser relaunch. Asking newPage() is the check:
    // it succeeds on a live context and throws on a dead one, which is
    // exactly the distinction, and it is what actually did the work the one
    // time this fired for real (Chromium killed by hand, 2026-08-21).
    page = await context.newPage();
  } catch (e) {
    record('daemon.rebuild', `context unusable (${e.message}) -- relaunching Chromium`);
    try { await context.close(); } catch { /* already gone */ }
    context = await chromium.launchPersistentContext(USERDATA, {
      headless: HEADLESS,
      viewport: FIT_WINDOW ? null : VIEWPORT,
      args: ['--disable-features=Translate',
             ...(FIT_WINDOW ? ['--start-maximized']
                            : [`--window-size=${VIEWPORT.width},${VIEWPORT.height + 90}`])],
    });
    page = context.pages()[0] || (await context.newPage());
  }
  attachHandlers(page);
  record('daemon.rebuild', `page rebuilt (#${rebuilds}) -- navigating to ${TARGET_URL}`);
  await page.goto(TARGET_URL, { waitUntil: 'domcontentloaded' })
            .catch((e) => record('daemon.error', e.message));
}

record('daemon', `navigating to ${TARGET_URL}`);
// Complain loudly rather than serving an empty page. A daemon pointed at a
// port nobody is listening on answers every request perfectly -- with a blank
// document -- and the suites that follow fail on missing selectors, which is a
// long way from "the web server is not running".
try {
  const probe = await fetch(TARGET_URL, { method: 'GET' });
  if (!probe.ok) throw new Error(`HTTP ${probe.status}`);
} catch (e) {
  const msg = `NOTHING IS SERVING ${TARGET_URL} (${e.message}). `
            + 'Start the WebUI (npm run dev -> :8081, npm run preview -> :8082) '
            + 'or set WEBCTL_URL. Every suite will fail on an empty page until then.';
  record('daemon.error', msg);
  console.error(`
  !! ${msg}
`);
}
await page.goto(TARGET_URL, { waitUntil: 'domcontentloaded' }).catch((e) => record('daemon.error', e.message));

function json(res, code, obj) {
  const body = JSON.stringify(obj);
  res.writeHead(code, { 'Content-Type': 'application/json' });
  res.end(body);
}
function readBody(req) {
  return new Promise((resolve) => {
    const chunks = [];
    req.on('data', (c) => chunks.push(c));
    req.on('end', () => {
      // Decode the whole body once as UTF-8; concatenating Buffers as strings
      // (d += c) corrupts multibyte chars (e.g. CJK) split across chunks.
      const d = Buffer.concat(chunks).toString('utf8');
      try {
        resolve(d ? JSON.parse(d) : {});
      } catch {
        resolve({});
      }
    });
  });
}

const server = http.createServer(async (req, res) => {
  const u = new URL(req.url, 'http://x');
  const q = u.searchParams;
  try {
    // Before anything touches `page`. Cheap when the page is alive (one
    // isClosed() call) and the only thing standing between a closed tab and a
    // whole run of red suites.
    await ensurePage();
    if (u.pathname === '/health')
      return json(res, 200, { ok: true, url: page.url(), seq, rebuilds });

    if (u.pathname === '/url') return json(res, 200, { url: page.url() });

    if (u.pathname === '/logs') {
      const since = Number(q.get('since') || 0);
      const kind = q.get('kind');
      let out = ring.filter((e) => e.id > since);
      if (kind) out = out.filter((e) => e.kind.startsWith(kind));
      if (q.get('clear') === '1') ring.length = 0;
      return json(res, 200, { seq, logs: out });
    }

    if (u.pathname === '/shot') {
      const out = q.get('path') || path.join(__dirname, `shot-${Date.now()}.png`);
      const sel = q.get('selector');
      const fullPage = q.get('full') === '1';
      if (sel) await page.locator(sel).screenshot({ path: out });
      else await page.screenshot({ path: out, fullPage });
      return json(res, 200, { path: out });
    }

    if (req.method === 'POST') {
      const b = await readBody(req);
      const timeout = b.timeout || 10000;
      switch (u.pathname) {
        case '/goto':
          await page.goto(b.url || TARGET_URL, { waitUntil: 'domcontentloaded', timeout });
          return json(res, 200, { url: page.url() });
        case '/reload':
          await page.reload({ waitUntil: 'domcontentloaded', timeout });
          return json(res, 200, { url: page.url() });
        case '/click':
          await page.click(b.selector, { timeout });
          return json(res, 200, { ok: true });
        case '/fill':
          await page.fill(b.selector, String(b.value ?? ''), { timeout });
          return json(res, 200, { ok: true });
        case '/press':
          await page.press(b.selector || 'body', b.key, { timeout });
          return json(res, 200, { ok: true });
        case '/mouse':              // selector-free coordinate click (replay fallback)
          await page.mouse.click(b.x, b.y);
          return json(res, 200, { ok: true });
        case '/key':                // keyboard key press at page level
          await page.keyboard.press(b.key);
          return json(res, 200, { ok: true });
        case '/viewport':           // match the recording's viewport so coords align
          await page.setViewportSize({ width: b.width, height: b.height });
          return json(res, 200, { ok: true });
        case '/wait':
          await page.waitForSelector(b.selector, { state: b.state || 'visible', timeout });
          return json(res, 200, { ok: true });
        case '/eval': {
          const result = await page.evaluate((expr) => {
            // eslint-disable-next-line no-eval
            const r = eval(expr);
            return r;
          }, b.expr);
          return json(res, 200, { result });
        }
        case '/text': {
          const t = await page.locator(b.selector).allInnerTexts();
          return json(res, 200, { text: t });
        }
        case '/shutdown':
          json(res, 200, { ok: true });
          setTimeout(async () => {
            await context.close();
            process.exit(0);
          }, 100);
          return;
        default:
          return json(res, 404, { error: 'unknown endpoint ' + u.pathname });
      }
    }
    return json(res, 404, { error: 'unknown endpoint ' + u.pathname });
  } catch (e) {
    return json(res, 500, { error: e.message, stack: e.stack });
  }
});

server.listen(PORT, '127.0.0.1', () => {
  record('daemon', `webctld listening on 127.0.0.1:${PORT}`);
  console.log(`webctld ready on http://127.0.0.1:${PORT} (page ${page.url()})`);
});
