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
const TARGET_URL = process.env.WEBCTL_URL || 'http://localhost:8080';
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

const context = await chromium.launchPersistentContext(USERDATA, {
  headless: HEADLESS,
  viewport: FIT_WINDOW ? null : VIEWPORT,
  // Size the OS window to match, or a 1920x1080 viewport just gets scrollbars
  // inside a smaller window.
  args: ['--disable-features=Translate',
         ...(FIT_WINDOW ? ['--start-maximized']
                        : [`--window-size=${VIEWPORT.width},${VIEWPORT.height + 90}`])],
});
const page = context.pages()[0] || (await context.newPage());

page.on('console', (msg) => record('console.' + msg.type(), msg.text()));
page.on('pageerror', (err) => record('pageerror', err.message, { stack: err.stack }));
page.on('requestfailed', (req) =>
  record('requestfailed', `${req.method()} ${req.url()}`, { error: req.failure()?.errorText })
);
page.on('response', (res) => {
  if (res.status() >= 400) record('http' + res.status(), `${res.request().method()} ${res.url()}`);
});

record('daemon', `navigating to ${TARGET_URL}`);
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
    if (u.pathname === '/health') return json(res, 200, { ok: true, url: page.url(), seq });

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
