// CoreLogPanel — live tail of inspd_log (Core-side logs delivered over WS).
//
// Speaks to inspd_log.v1 via CoreLogClient.  Self-contained: pass a `url`
// (default ws://<host>:4091/log) and drop it into any container.  Shows:
//   - Crash banner (pinned at top once a `crash` frame arrives)
//   - Status bar (connecting / connected / reconnecting / crashed)
//   - Filter bar (min-level dropdown, module multi-select pulled via
//     get_modules, free-text needle that filters client-side)
//   - Pause toggle (freezes the scroll/buffer for inspection)
//   - Buffer (plain scrollable list capped at 5000 entries, drop-oldest)
//   - Footer: dump_now button + entry count
//
// Why plain scroll instead of virtualization: at 5000 entries on a real
// floor unit the list is ~1 MB DOM; React 16's reconciler handles it fine
// and we'd need to add a new dep for a virtualizer.  Cap + drop-oldest is
// the simpler bound.  If the touchscreen grows slow, swap the body for
// react-window.
//
// The component owns the CoreLogClient lifecycle (mount = start, unmount =
// stop).  Reconnect is automatic; nothing to plumb from the parent.

import React, { useEffect, useMemo, useRef, useState, useCallback } from 'react';
import { CoreLogClient } from 'JSSRCROOT/comm/CoreLogClient';
import { mkLog } from 'UTIL/logger';

const log = mkLog('comm.corelog');

const BUFFER_CAP = 5000;
const LEVEL_LABELS = ['TRACE', 'DEBUG', 'INFO', 'WARN', 'ERROR', 'FATAL'];
const LEVEL_COLOR = {
  T: '#888', D: '#6aa', I: '#0a0', W: '#c80', E: '#c33', F: '#a04',
};

function defaultUrl() {
  if (typeof window === 'undefined') return 'ws://127.0.0.1:4091/log';
  const host = window.location && window.location.hostname || '127.0.0.1';
  return `ws://${host}:4091/log`;
}

function fmtTs(ms) {
  // ms is producer-relative.  Render as MMM:SS.mmm so it stays compact.
  if (typeof ms !== 'number') return '';
  const total = Math.floor(ms);
  const s = Math.floor(total / 1000);
  const m = Math.floor(s / 60);
  return `${String(m).padStart(2,'0')}:${String(s % 60).padStart(2,'0')}.${String(total % 1000).padStart(3,'0')}`;
}

function StatusPill({ status }) {
  const map = {
    idle:          ['gray',   'idle'],
    connecting:    ['blue',   'connecting…'],
    connected:     ['green',  'connected'],
    reconnecting:  ['orange', `reconnecting (attempt ${status.attempt || '?'})`],
    crashed:       ['red',    'CORE CRASHED'],
    closed:        ['gray',   'closed'],
  };
  const [c, label] = map[status.state] || ['gray', status.state || '?'];
  return (
    <span style={{
      display: 'inline-block', padding: '2px 8px', borderRadius: 4,
      background: c, color: '#fff', fontSize: 12, fontWeight: 600, marginRight: 8,
    }}>{label}</span>
  );
}

function CrashBanner({ crash, onDismiss }) {
  const [open, setOpen] = useState(false);
  if (!crash) return null;
  return (
    <div style={{
      background: '#3a0808', color: '#fff', padding: 10, borderRadius: 4,
      margin: '6px 0', fontFamily: 'monospace', fontSize: 12,
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
        <span style={{ fontSize: 14, fontWeight: 700 }}>💥 {crash.signal || 'CRASH'}</span>
        <span style={{ opacity: 0.85 }}>{crash.utc}</span>
        <span style={{ flex: 1 }} />
        <button onClick={() => setOpen((o) => !o)}>{open ? 'hide' : 'show stack'}</button>
        {onDismiss && <button onClick={onDismiss}>dismiss</button>}
      </div>
      <div style={{ marginTop: 4 }}>dump: {crash.dump_path}</div>
      {open && (
        <pre style={{ marginTop: 6, whiteSpace: 'pre-wrap', maxHeight: 200, overflow: 'auto' }}>
          {(crash.frames || []).map((f) => `#${f.idx}  ${f.addr}  ${f.symbol || '?'}` +
            (f.file ? `  (${f.file}:${f.line})` : '')).join('\n')}
          {crash.ring_tail && crash.ring_tail.length > 0 && '\n\n--- ring_tail (last lines before death) ---\n' +
            crash.ring_tail.map((e) => `[${fmtTs(e.ts_ms)}][${e.level_char}][${e.module}] ${e.text}`).join('\n')}
        </pre>
      )}
    </div>
  );
}

function LogRow({ entry, needle }) {
  // Plain row.  No memoization — React 16 keyed update is fast enough.
  const col = LEVEL_COLOR[entry.level_char] || '#444';
  let textNode = entry.text;
  if (needle && entry.text) {
    const i = entry.text.toLowerCase().indexOf(needle.toLowerCase());
    if (i >= 0) {
      textNode = (
        <>
          {entry.text.slice(0, i)}
          <mark style={{ background: 'yellow', color: 'black' }}>{entry.text.slice(i, i + needle.length)}</mark>
          {entry.text.slice(i + needle.length)}
        </>
      );
    }
  }
  return (
    <div style={{
      display: 'flex', gap: 8, fontFamily: 'monospace', fontSize: 12,
      borderBottom: '1px solid rgba(0,0,0,0.05)', padding: '2px 4px',
    }}>
      <span style={{ color: '#888', minWidth: 90 }}>{fmtTs(entry.ts_ms)}</span>
      <span style={{ color: col, fontWeight: 700, minWidth: 14 }}>{entry.level_char}</span>
      <span style={{ color: '#357', minWidth: 110 }}>{entry.module}</span>
      <span style={{ flex: 1, wordBreak: 'break-all' }}>{textNode}</span>
    </div>
  );
}

export default function CoreLogPanel({ url, height = '70vh' }) {
  const wsUrl = url || defaultUrl();

  const [entries, setEntries]   = useState([]);          // ring buffer
  const [status, setStatus]     = useState({ state: 'idle' });
  const [hello, setHello]       = useState(null);
  const [modules, setModules]   = useState([]);
  const [crash, setCrash]       = useState(null);
  const [minLevel, setMinLevel] = useState(2);           // INFO
  const [modFilter, setModFilter] = useState([]);        // server-side ns globs
  const [needle, setNeedle]     = useState('');          // client-side text
  const [paused, setPaused]     = useState(false);
  const [autoscroll, setAutoscroll] = useState(true);

  const clientRef = useRef(null);
  const bufRef    = useRef([]);                         // mutable mirror, no re-render
  const pausedRef = useRef(false);
  const listRef   = useRef(null);

  // Re-apply paused without bouncing the effect below
  useEffect(() => { pausedRef.current = paused; }, [paused]);

  useEffect(() => {
    const c = new CoreLogClient({ url: wsUrl });
    clientRef.current = c;

    const offHello = c.on('hello', (h) => { setHello(h); });
    const offLog   = c.on('log',   (e) => {
      if (pausedRef.current) return;
      bufRef.current.push(e);
      if (bufRef.current.length > BUFFER_CAP) bufRef.current.splice(0, bufRef.current.length - BUFFER_CAP);
    });
    const offBack  = c.on('backlog', ({ items }) => {
      if (!items || items.length === 0) return;
      bufRef.current.push(...items);
      if (bufRef.current.length > BUFFER_CAP) bufRef.current.splice(0, bufRef.current.length - BUFFER_CAP);
    });
    const offCrash = c.on('crash', (cr) => { setCrash(cr); });
    const offSt    = c.on('status', (s) => { setStatus(s); });

    c.start();
    c.subscribe({ min_level: minLevel, modules: modFilter.length ? modFilter : null,
                  include_ephemeral: minLevel <= 1, backlog: { tail_n: 500 } })
      .catch((e) => log.warn('[subscribe-fail]', { err: String(e) }));

    // Periodically flush bufRef → state so the UI shows new entries.
    const flush = setInterval(() => {
      const next = bufRef.current.slice();
      setEntries(next);
    }, 250);

    return () => {
      clearInterval(flush);
      offHello(); offLog(); offBack(); offCrash(); offSt();
      c.stop();
      clientRef.current = null;
    };
  }, [wsUrl]);

  // Re-subscribe when filters change.
  useEffect(() => {
    const c = clientRef.current;
    if (!c) return;
    c.subscribe({ min_level: minLevel, modules: modFilter.length ? modFilter : null,
                  include_ephemeral: minLevel <= 1, backlog: null }).catch(() => {});
  }, [minLevel, modFilter]);

  // Fetch module list on connect.
  useEffect(() => {
    if (status.state !== 'connected') return;
    const c = clientRef.current;
    if (!c) return;
    c.getModules().then((mods) => setModules(mods || [])).catch(() => {});
  }, [status.state]);

  // Autoscroll: scrollTop = scrollHeight after entries update.
  useEffect(() => {
    if (!autoscroll || paused) return;
    const el = listRef.current;
    if (el) el.scrollTop = el.scrollHeight;
  }, [entries, autoscroll, paused]);

  // Client-side text filter
  const visible = useMemo(() => {
    if (!needle) return entries;
    const n = needle.toLowerCase();
    return entries.filter((e) => (e.text || '').toLowerCase().includes(n) ||
                                  (e.module || '').toLowerCase().includes(n));
  }, [entries, needle]);

  const onDumpNow = useCallback(() => {
    const c = clientRef.current; if (!c) return;
    c.dumpNow().then((ack) => alert('dump written: ' + (ack && ack.dump_path)))
               .catch((e) => alert('dump failed: ' + String(e)));
  }, []);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height, fontFamily: 'monospace' }}>
      <div style={{ padding: 4 }}>
        <StatusPill status={status} />
        {hello && <span style={{ fontSize: 12, opacity: 0.7 }}>
          drainer v{hello.drainer_version} · pid {hello.producer_pid} · ring {hello.ring_mb}MB · started {hello.started_utc}
        </span>}
      </div>

      <CrashBanner crash={crash} onDismiss={() => setCrash(null)} />

      <div style={{ display: 'flex', gap: 8, padding: 4, alignItems: 'center', flexWrap: 'wrap' }}>
        <label style={{ fontSize: 12 }}>
          min level{' '}
          <select value={minLevel} onChange={(e) => setMinLevel(parseInt(e.target.value, 10))}>
            {LEVEL_LABELS.map((l, i) => <option key={l} value={i}>{l}</option>)}
          </select>
        </label>
        <label style={{ fontSize: 12, flex: 1 }}>
          filter{' '}
          <input value={needle} onChange={(e) => setNeedle(e.target.value)}
                 placeholder="text or module substring"
                 style={{ width: 'min(60%, 360px)', fontFamily: 'monospace' }} />
        </label>
        <label style={{ fontSize: 12 }}>
          <input type="checkbox" checked={paused} onChange={(e) => setPaused(e.target.checked)} /> pause
        </label>
        <label style={{ fontSize: 12 }}>
          <input type="checkbox" checked={autoscroll} onChange={(e) => setAutoscroll(e.target.checked)} /> follow
        </label>
        <button onClick={onDumpNow}>dump now</button>
        <button onClick={() => { bufRef.current = []; setEntries([]); }}>clear</button>
      </div>

      {modules.length > 0 && (
        <div style={{ padding: '2px 4px', fontSize: 11, opacity: 0.75 }}>
          modules:{' '}
          {modules.map((m) => (
            <label key={m.name} style={{ marginRight: 6 }}>
              <input
                type="checkbox"
                checked={modFilter.includes(m.name)}
                onChange={(e) => setModFilter((prev) => e.target.checked
                  ? [...prev, m.name]
                  : prev.filter((x) => x !== m.name))}
              />{' '}{m.name}<sub style={{ opacity: 0.5 }}>{LEVEL_LABELS[m.effective_level] || '?'}</sub>
            </label>
          ))}
          {modFilter.length > 0 && (
            <button style={{ marginLeft: 8 }} onClick={() => setModFilter([])}>clear modules</button>
          )}
        </div>
      )}

      <div ref={listRef} style={{
        flex: 1, overflow: 'auto', background: '#fafafa',
        border: '1px solid #ddd', borderRadius: 2,
      }}>
        {visible.length === 0 && (
          <div style={{ padding: 16, color: '#888', textAlign: 'center', fontSize: 12 }}>
            {status.state === 'connected' ? 'waiting for log frames…' : 'no entries'}
          </div>
        )}
        {visible.map((e, i) => (
          <LogRow key={i + ':' + e.ts_ms + ':' + e.module} entry={e} needle={needle} />
        ))}
      </div>

      <div style={{ padding: 4, fontSize: 11, opacity: 0.7 }}>
        {visible.length}/{entries.length} entries
        {entries.length >= BUFFER_CAP && ` (cap ${BUFFER_CAP}; dropping oldest)`}
      </div>
    </div>
  );
}
