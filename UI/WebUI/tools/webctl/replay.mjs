#!/usr/bin/env node
// replay.mjs -- replay a recorded UI action sequence (from the in-app
// actionRecorder's ⤓ export / __rec.dump()) through the webctl Playwright
// daemon, to reconstruct a reported failure state ("parachute" to the scene).
//
// Usage:
//   node replay.mjs <recording.json> [--from N] [--to M] [--slowmo MS] [--dry]
//
// Each step clicks/fills by its recorded locator (data-testid > text > css),
// pacing roughly to the recorded timing. Stops at the first failed step and
// screenshots it -- that step + its expected context is the scene to debug.
import fs from 'node:fs';

const PORT = Number(process.env.WEBCTL_PORT || 8765);
const BASE = `http://127.0.0.1:${PORT}`;

const argv = process.argv.slice(2);
const file = argv.find((a) => !a.startsWith('--'));
const flag = (k, d) => { const i = argv.indexOf('--' + k); return i >= 0 ? argv[i + 1] : d; };
const has = (k) => argv.includes('--' + k);
const FROM = Number(flag('from', 0));
const TO = Number(flag('to', Infinity));
const SLOW = Number(flag('slowmo', 250));
const DRY = has('dry');

if (!file) {
  console.error('usage: node replay.mjs <recording.json> [--from N --to M --slowmo MS --dry]');
  process.exit(2);
}

const rec = JSON.parse(fs.readFileSync(file, 'utf8'));
const steps = rec.steps || rec;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function api(path, body) {
  const res = await fetch(BASE + path, {
    method: body ? 'POST' : 'GET',
    headers: body ? { 'Content-Type': 'application/json' } : undefined,
    body: body ? JSON.stringify(body) : undefined,
  });
  return res.json();
}

const ctxNote = (ctx) => {
  if (!ctx) return '';
  const m = ctx.modals ? ` modals[${ctx.modals.join(',')}]` : '';
  return `  -> ${ctx.url || ''}${m}`;
};

// Wait for the UI to actually reach the state this step was recorded in, instead
// of relying on the recorded delay: loading spinners cleared AND the expected
// modal(s) present. This adapts to real async latency (WS round-trips etc.), so
// the operator doesn't have to over-pause while recording.
async function waitSettle(ctx, timeout = 10000) {
  const probe =
    "({spin: document.querySelectorAll('.ant-spin-spinning').length," +
    " modals: Array.from(document.querySelectorAll('.ant-modal-wrap'))" +
    ".filter(m=>m.offsetParent!==null)" +
    ".map(m=>{var t=m.querySelector('.ant-modal-title');return (t&&t.innerText||'').trim();})" +
    ".filter(Boolean)})";
  const t0 = Date.now();
  for (;;) {
    const r = await api('/eval', { expr: probe }).catch(() => ({}));
    const st = (r && r.result) || {};
    const spinnerClear = (st.spin || 0) === 0;
    const modalsOk = !ctx || !ctx.modals || ctx.modals.every((m) => (st.modals || []).includes(m));
    if (spinnerClear && modalsOk) return true;
    if (Date.now() - t0 > timeout) return false; // proceed anyway; the action's own wait may still succeed
    await sleep(150);
  }
}

if (DRY) {
  steps.forEach((s, i) => console.log(`#${i} ${s.kind} ${s.sel}${s.weak ? ' (weak)' : ''}${ctxNote(s.ctx)}`));
  process.exit(0);
}

const health = await api('/health').catch(() => null);
if (!health) {
  console.error(`webctld not reachable on :${PORT} -- start it: cd tools/webctl && node webctld.mjs`);
  process.exit(2);
}

let failures = 0;
// Match the recording's viewport so coordinate replay aligns with the captured layout.
const vp = steps.find((s) => s.vw && s.vh);
if (vp) { await api('/viewport', { width: vp.vw, height: vp.vh }); console.log(`viewport set to ${vp.vw}x${vp.vh}`); }

for (let i = 0; i < steps.length; i++) {
  if (i < FROM || i > TO) continue;
  const s = steps[i];
  await sleep(SLOW);                 // small floor so the UI can begin reacting
  await waitSettle(s.ctx);           // then wait for this step's recorded state (spinner clear + modals)

  const label = `#${i} ${s.kind} ${s.sel || (s.key ? 'key:' + s.key : '')}${s.weak ? '  (weak)' : ''}`;
  try {
    // Standalone key press (Enter/Tab/Escape/arrows).
    if (s.kind === 'key') {
      const r = await api('/key', { key: s.key });
      if (r && r.error) throw new Error(String(r.error).split('\n')[0]);
      console.log('ok   ' + label + ctxNote(s.ctx));
      continue;
    }
    const ep = s.kind === 'fill' ? '/fill' : '/click';
    const mk = (selector, t) => (s.kind === 'fill'
      ? { selector, value: s.value ?? '', timeout: t }
      : { selector, timeout: t });
    // Locator chain: primary (testid/text) -> css alt -> raw coordinate click.
    // The coordinate fallback is selector-free, so it survives missing hooks,
    // CJK text, and DOM churn as long as the layout/viewport matches.
    const locs = [s.sel, ...(s.alt && s.alt !== s.sel ? [s.alt] : [])].filter(Boolean);
    let done = false, how = '', lastErr = null;
    for (let li = 0; li < locs.length && !done; li++) {
      const r = await api(ep, mk(locs[li], 4000));
      if (!(r && r.error)) { done = true; how = li > 0 ? '  [via css alt]' : ''; }
      else lastErr = String(r.error).split('\n')[0];
    }
    if (!done && s.x != null && s.y != null) {
      // fill can't be done by coords; for clicks, click the recorded point
      if (s.kind === 'click') {
        const r = await api('/mouse', { x: s.x, y: s.y });
        if (!(r && r.error)) { done = true; how = `  [via coords ${s.x},${s.y}]`; }
        else lastErr = String(r.error).split('\n')[0];
      }
    }
    if (!done) throw new Error(lastErr || 'no locator matched');
    console.log('ok   ' + label + how + ctxNote(s.ctx));
  } catch (e) {
    failures++;
    await api(`/shot?path=${encodeURIComponent(`replay-fail-${i}.png`)}`).catch(() => {});
    console.error('FAIL ' + label);
    console.error('     ' + e.message);
    if (s.ctx) console.error('     expected context here: ' + JSON.stringify(s.ctx));
    console.error(`     screenshot: replay-fail-${i}.png  <-- this is the scene to investigate`);
    break;
  }
}
console.log(failures ? '\nReplay stopped at the failing step.' : `\nReplay completed: ${steps.length} steps, no failures.`);
process.exit(failures ? 1 : 0);
