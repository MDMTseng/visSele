// actionRecorder.js -- lightweight, dependency-free in-app UI action recorder.
//
// Passively captures user clicks/inputs into a ring buffer as a *replayable*
// step list: a stable Playwright locator (data-testid > text > css path) plus
// route/modal context and a relative timestamp. When a UI operation misbehaves,
// export the buffer (floating ⤓ button or `__rec.dump()`) and hand the JSON off
// -- it can be replayed through the webctl Playwright daemon (tools/webctl/
// replay.mjs) to "parachute" straight to the failure state instead of having to
// re-navigate the app blind.
//
// OFF by default. Enable per-session via localStorage.__rec_on = "1" (then
// reload); once enabled it records always-on into a ring buffer, so you don't
// have to remember to start before reproducing a bug. Toggle/clear via the
// floating widget or __rec.

const CAP = 400;

const state = {
  buf: [],
  recording: true,
  t0: (typeof performance !== 'undefined' ? performance.now() : Date.now()),
};

const now = () => (typeof performance !== 'undefined' ? performance.now() : Date.now());

// Walk up to the nearest meaningful interactive element (the click often lands
// on an inner <span>/icon of an antd Button).
function interactiveTarget(el) {
  const SEL = '[data-testid],button,a,[role=button],.ant-btn,.ant-menu-item,' +
              '.ant-tabs-tab,[role=tab],[role=menuitem],input,select,textarea';
  let n = el;
  for (let i = 0; n && n.nodeType === 1 && i < 8; i++, n = n.parentElement) {
    if (n.matches && n.matches(SEL)) return n;
  }
  return el;
}

// Mirror Playwright's selector-generator priority: test-id > role+name > text >
// css path. Always include a css-path `alt` (and coords, added by the caller) so
// replay has selector-free fallbacks when the primary fails (e.g. Playwright's
// text/role engines don't match CJK names reliably).
const IMPLICIT_ROLE = { BUTTON: 'button', A: 'link', INPUT: 'textbox', SELECT: 'combobox', TEXTAREA: 'textbox' };
function roleOf(el) {
  return (el.getAttribute && el.getAttribute('role'))
    || IMPLICIT_ROLE[el.tagName]
    || (el.classList && el.classList.contains('ant-btn') ? 'button' : null)
    || (el.classList && el.classList.contains('ant-menu-item') ? 'menuitem' : null)
    || (el.classList && el.classList.contains('ant-tabs-tab') ? 'tab' : null);
}
function accName(el) {
  return ((el.getAttribute && el.getAttribute('aria-label')) || el.innerText || el.textContent || '')
    .trim().replace(/\s+/g, ' ').slice(0, 40);
}

function locatorFor(el) {
  const alt = cssPath(el);
  // 1) test-id (on el or ancestor) -- most robust, layout-independent.
  let n = el;
  for (let i = 0; n && n.nodeType === 1 && i < 8; i++, n = n.parentElement) {
    const tid = n.getAttribute && n.getAttribute('data-testid');
    if (tid) return { sel: `[data-testid="${tid}"]`, testid: tid, alt };
  }
  const ascii = (s) => /^[\x20-\x7e]+$/.test(s);
  // 2) role + accessible name (Playwright's getByRole) -- next best.
  const role = roleOf(el);
  const name = accName(el);
  if (role && name) {
    return { sel: `role=${role}[name=${JSON.stringify(name)}]`, role, name, weak: !ascii(name), alt };
  }
  // 3) short visible text.
  if (name && name.length <= 40) {
    return { sel: `text=${JSON.stringify(name)}`, text: name, weak: !ascii(name), alt };
  }
  // 4) structural css path.
  return { sel: alt, weak: true, alt };
}

function cssPath(el) {
  const parts = [];
  let n = el;
  while (n && n.nodeType === 1 && parts.length < 6) {
    if (n.id) { parts.unshift('#' + CSS.escape(n.id)); break; }
    let s = n.tagName.toLowerCase();
    const par = n.parentElement;
    if (par) {
      const sib = Array.from(par.children).filter((c) => c.tagName === n.tagName);
      if (sib.length > 1) s += `:nth-of-type(${sib.indexOf(n) + 1})`;
    }
    parts.unshift(s);
    n = n.parentElement;
  }
  return parts.join(' > ');
}

// Route/modal context at event time -- captures local-component navigation state
// (modals via useState/setState) that Redux alone can't see.
function context() {
  const ctx = { url: location.hash || location.pathname };
  try {
    const st = window.__GP_STORE__ && window.__GP_STORE__.getState();
    if (st && st.UIData) ctx.uiState = st.UIData.UI_state || st.UIData.ui_state;
  } catch (e) { /* store shape may vary */ }
  const modals = Array.from(document.querySelectorAll('.ant-modal-wrap'))
    .filter((m) => m.offsetParent !== null)
    .map((m) => {
      const t = m.querySelector('.ant-modal-title');
      return (t && t.innerText || '').trim();
    })
    .filter(Boolean);
  if (modals.length) ctx.modals = modals;
  return ctx;
}

function push(kind, el, extra) {
  if (!state.recording) return;
  const tgt = interactiveTarget(el);
  const loc = locatorFor(tgt);
  state.buf.push({ t: Math.round(now() - state.t0), kind, ...loc, ctx: context(), ...(extra || {}) });
  if (state.buf.length > CAP) state.buf.shift();
  updateBadge();
}

// Element-center viewport coords for selector-free coordinate replay. We use the
// element's center (not the raw click point) so replay lands solidly on it, and
// it works even for synthetic clicks where clientX/Y are 0.
function coordsOf(el, e) {
  const r = el.getBoundingClientRect ? el.getBoundingClientRect() : { left: 0, top: 0, width: 0, height: 0 };
  const cx = (e && e.clientX) || (r.left + r.width / 2);
  const cy = (e && e.clientY) || (r.top + r.height / 2);
  return { x: Math.round(cx), y: Math.round(cy), vw: window.innerWidth, vh: window.innerHeight };
}

function onClick(e) {
  if (!state.recording) return;
  const tgt = interactiveTarget(e.target);
  push('click', tgt, coordsOf(tgt, e));
}
function onChange(e) {
  const el = e.target;
  if (el && el.matches && el.matches('input,textarea,select'))
    push('fill', el, { value: el.value, ...coordsOf(el, e) });
}
function onKey(e) {
  // record meaningful keys (Enter/Tab/Escape/arrows) -- skip plain typing (covered by 'fill')
  const k = e.key;
  if (['Enter', 'Tab', 'Escape', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Backspace', 'Delete'].includes(k))
    push('key', e.target, { key: k, ...coordsOf(e.target, e) });
}

// ---- public API ----------------------------------------------------------
export const recorder = {
  start() { state.recording = true; updateBadge(); },
  stop() { state.recording = false; updateBadge(); },
  clear() { state.buf = []; state.t0 = now(); updateBadge(); },
  steps() { return state.buf.slice(); },
  dump() {
    const out = {
      meta: { exportedAt: new Date().toISOString(), url: location.href, ua: navigator.userAgent, count: state.buf.length },
      steps: state.buf.slice(),
    };
    // copy to clipboard + console + download
    const json = JSON.stringify(out, null, 2);
    try { navigator.clipboard && navigator.clipboard.writeText(json); } catch (e) {}
    try { console.log('[actionRecorder] dump:', out); } catch (e) {}
    try {
      const blob = new Blob([json], { type: 'application/json' });
      const a = document.createElement('a');
      a.href = URL.createObjectURL(blob);
      a.download = `ui-actions-${Date.now()}.json`;
      a.click();
      setTimeout(() => URL.revokeObjectURL(a.href), 1000);
    } catch (e) {}
    return out;
  },
};

// ---- floating widget (plain DOM, no React) -------------------------------
let badge = null;
function updateBadge() {
  if (!badge) return;
  const dot = state.recording ? '🔴' : '⏸';
  badge.querySelector('.rec-count').textContent = `${dot} ${state.buf.length}`;
}

function mountWidget() {
  if (badge || typeof document === 'undefined') return;
  badge = document.createElement('div');
  badge.style.cssText =
    'position:fixed;right:8px;bottom:8px;z-index:2147483647;display:flex;gap:4px;' +
    'align-items:center;font:11px/1.4 monospace;background:#1f1f1fcc;color:#eee;' +
    'padding:3px 6px;border-radius:6px;user-select:none;opacity:.85';
  const count = document.createElement('span');
  count.className = 'rec-count'; count.style.cssText = 'cursor:pointer;min-width:42px';
  count.title = 'click to pause/resume recording';
  count.onclick = () => (state.recording ? recorder.stop() : recorder.start());
  const exp = mkBtn('⤓', 'export actions JSON', () => recorder.dump());
  const clr = mkBtn('✕', 'clear buffer', () => recorder.clear());
  badge.append(count, exp, clr);
  document.body.appendChild(badge);
  updateBadge();
}
function mkBtn(label, title, onclick) {
  const b = document.createElement('span');
  b.textContent = label; b.title = title;
  b.style.cssText = 'cursor:pointer;padding:0 4px;border-left:1px solid #555';
  b.onclick = onclick;
  return b;
}

let installed = false;
export function initActionRecorder() {
  if (installed || typeof document === 'undefined') return recorder;
  installed = true;
  document.addEventListener('click', onClick, true);   // capture phase
  document.addEventListener('change', onChange, true);
  document.addEventListener('keydown', onKey, true);
  const boot = () => mountWidget();
  if (document.body) boot(); else window.addEventListener('DOMContentLoaded', boot);
  window.__rec = recorder;   // console access + daemon /eval
  return recorder;
}

export default initActionRecorder;
