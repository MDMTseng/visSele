// Single logging facade for the WebUI. Aggressive-unify migration target.
//
// Why this exists
// ---------------
// The codebase had ~200 raw `console.log`/`console.warn`/`console.error` calls
// across 32 files plus 10 files using `loglevel.getLogger` with ad-hoc names,
// no per-namespace level control, no runtime override, and no convention for
// what `info` vs `debug` vs `warn` mean. This module replaces that mess with
// ONE entry point and a small contract.
//
// Contract
// --------
// Every file gets its logger via `mkLog("ns.sub.ns")` and uses ONLY:
//   log.debug(msg, structured?)  // devtools-only noise; off in production
//   log.info(msg,  structured?)  // lifecycle events (connect, load, mode flip)
//   log.warn(msg,  structured?)  // recoverable problems (reconnect, NAK, fallback)
//   log.error(msg, structured?)  // bugs and incidents (uncaught throw, sha1 mismatch)
// Convention: first arg is a `[event]` tag string; second arg is an object of
// structured fields. Mirrors the recent BPG_WS / websocket.js style.
//
// Namespace registry
// ------------------
// Namespaces are dotted (`comm.ws`, `comm.bpg`, `editor.shapes`, `canvas.draw`,
// `ui.defconf`, `db.idb`, `i18n`, ...). The registry below is the authoritative
// list — add new ones here so the namespaces don't fragment over time.
// Unknown namespaces still work (loglevel auto-creates loggers) but won't get
// the friendly default level.
//
// Runtime level control
// ---------------------
// Default level is read from localStorage at init time:
//   localStorage.setItem('logLevel', 'debug')          // global default
//   localStorage.setItem('logLevel:comm.ws', 'trace')  // per-namespace override
// Levels: 'trace' | 'debug' | 'info' | 'warn' | 'error' | 'silent'.
// Plain `?logLevel=debug` URL param works too (handy on the floor unit when
// localStorage isn't writable).
//
// diagLog integration
// -------------------
// loglevel calls go through console.* which `diagLog.initDiag()` wraps. So
// EVERY log line through `mkLog` lands in the ring buffer at the right severity
// without extra work — `initDiag()` must run BEFORE the first import that
// triggers a mkLog call.

import * as loglevel from 'loglevel';

const VALID_LEVELS = new Set(['trace', 'debug', 'info', 'warn', 'error', 'silent']);

// Authoritative namespace list. Add new ones here.
export const NAMESPACES = Object.freeze({
  'comm.ws':       'info',   // WebSocket lifecycle (open/close/error/reconnect)
  'comm.bpg':      'info',   // BPG framing/dispatch
  'comm.db':       'info',   // DB_WS inspection-record persistence
  'comm.api':      'info',   // generic API middleware (MW_API)
  'editor.model':  'info',   // InspectionEditorLogic (def model, refs, lookups)
  'editor.reducer':'info',   // UICtrlReducer state transitions
  'editor.action': 'info',   // ActionThrottle middleware
  'editor.shapes': 'info',   // per-shape modules (line/arc/measure/...)
  'editor.def':    'info',   // def-file serialize + sha1 + load/save flows
  'canvas.draw':   'warn',   // renderUTIL + per-frame draw (NOISY at info)
  'canvas.ctrl':   'info',   // EverCheckCanvasComponent ctrl + interaction
  'canvas.cam':    'info',   // CameraCtrl
  'ui.main':       'info',   // MAINUI top-level UI shell
  'ui.defconf':    'info',   // DefConfUI (def editor)
  'ui.insp':       'info',   // InspectionUI
  'ui.report':     'info',   // RepDisplayUI
  'ui.calib':      'info',   // BackLightCalibUI + camera calib flows
  'ui.instinsp':   'info',   // InstInspUI
  'ui.base':       'info',   // component/baseComponent.jsx (JsonEditBlock etc.)
  'ui.rdx':        'info',   // component/rdxComponent.jsx
  'ui.boundary':   'warn',   // ComponentBoundary / RootErrorBoundary
  'db.idb':        'info',   // local IndexedDB queue
  'i18n':          'warn',   // dictLookUp warnings only
  'qa':            'info',   // dev-only QA hooks
});

function readLevelFromEnv(ns) {
  // URL param: ?logLevel=debug or ?logLevel:comm.ws=trace
  if (typeof window !== 'undefined' && window.location && window.location.search) {
    try {
      const sp = new URLSearchParams(window.location.search);
      const perNs = sp.get('logLevel:' + ns);
      if (perNs && VALID_LEVELS.has(perNs)) return perNs;
      const all = sp.get('logLevel');
      if (all && VALID_LEVELS.has(all)) return all;
    } catch (_) { /* ignore malformed */ }
  }
  if (typeof localStorage !== 'undefined') {
    try {
      const perNs = localStorage.getItem('logLevel:' + ns);
      if (perNs && VALID_LEVELS.has(perNs)) return perNs;
      const all = localStorage.getItem('logLevel');
      if (all && VALID_LEVELS.has(all)) return all;
    } catch (_) { /* ignore SecurityError on some embedded webviews */ }
  }
  return null;
}

let _initialized = false;

// Apply default + override levels to every registered namespace. Safe to call
// multiple times; idempotent. Call once at boot from script.jsx after initDiag.
export function initLogger() {
  if (_initialized) return;
  _initialized = true;
  for (const ns of Object.keys(NAMESPACES)) {
    const env = readLevelFromEnv(ns);
    const lvl = env || NAMESPACES[ns];
    loglevel.getLogger(ns).setLevel(lvl, false);
  }
  // Root logger fallback for any unknown namespaces.
  const rootEnv = readLevelFromEnv('');
  loglevel.setLevel(rootEnv || 'info', false);
  // R-quick-wins #7 safety net: any logger constructed during the import chain
  // BEFORE initDiag wrapped console.* will have its bound method refs pointing
  // at the raw console — bypassing the diag ring buffer. Force every existing
  // logger to re-setLevel so they rebind to the (now-wrapped) console. This
  // matters only during the migration from `loglevel.getLogger` direct usage
  // to `mkLog` (which is created lazily, after initLogger runs). Once every
  // file uses mkLog, this loop becomes a no-op.
  try {
    const all = (typeof loglevel.getLoggers === 'function') ? loglevel.getLoggers() : {};
    Object.keys(all).forEach((name) => {
      const lg = all[name];
      if (lg && typeof lg.setLevel === 'function' && typeof lg.getLevel === 'function') {
        lg.setLevel(lg.getLevel(), false);
      }
    });
    loglevel.setLevel(loglevel.getLevel(), false);
  } catch (_) { /* loglevel API drift — diag ring still captures raw console */ }
}

// Create / fetch a namespaced logger. Will warn (in dev) if the namespace
// isn't in the registry, so drift gets flagged.
export function mkLog(ns) {
  if (!_initialized) initLogger();
  if (process.env.NODE_ENV !== 'production' && !(ns in NAMESPACES)) {
    // eslint-disable-next-line no-console
    console.warn('[logger] unregistered namespace:', ns, '— add it to UTIL/logger.js NAMESPACES');
  }
  return loglevel.getLogger(ns);
}

// Exposed for QA / devtools manual flips.
export function setLevel(ns, level) {
  if (!VALID_LEVELS.has(level)) return false;
  loglevel.getLogger(ns).setLevel(level, false);
  return true;
}

export function listLevels() {
  const out = {};
  for (const ns of Object.keys(NAMESPACES)) {
    out[ns] = loglevel.getLogger(ns).getLevel();
  }
  return out;
}
