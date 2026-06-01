// CoreLogPanel — live tail of inspd_log (Core-side logs delivered over WS).
//
// Speaks to inspd_log.v1 via CoreLogClient. Schema is OTel-aligned: every
// entry carries severityNumber/severityText/body/attributes.{module,code.*}
// + timeUnixNano. See InspectionCore/docs/LOGGING_WEBUI.md §3.3.
//
// Self-contained: pass a `url` (default ws://<host>:4091/log) and drop it in.
// The component owns the CoreLogClient lifecycle (mount=start, unmount=stop).
// Reconnect is automatic; nothing to plumb from the parent.
//
// Renders:
//   - Crash banner (pinned at top once a `crash` frame arrives)
//   - Status bar (idle/connecting/connected/reconnecting/crashed)
//   - Filter bar (min severity dropdown, free-text needle, pause + follow,
//                 dump-now, clear)
//   - Module checkbox row (server-side filter, populated from getModules)
//   - Buffer body: log rows interleaved with gap markers ("N lines lost")
//                  from `dropped` frames; capped at 5000 entries
//
// Buffer entries are tagged with kind='log' or kind='gap' so the renderer
// can distinguish.  Both share a monotonic id for React keying.

import React, { useEffect, useMemo, useRef, useState, useCallback } from 'react';
import { CoreLogClient } from 'JSSRCROOT/comm/CoreLogClient';
import { mkLog } from 'UTIL/logger';

const log = mkLog('comm.corelog');

const BUFFER_CAP = 5000;

// OTel severity scale ranks: { label, severityNumber }.
const SEVERITIES = [
  { label: 'TRACE', sn: 1  },
  { label: 'DEBUG', sn: 5  },
  { label: 'INFO',  sn: 9  },
  { label: 'WARN',  sn: 13 },
  { label: 'ERROR', sn: 17 },
  { label: 'FATAL', sn: 21 },
];
function sevLabel(sn) {
  // Pick the highest defined OTel rank ≤ sn (handles INFO2=10, ERROR3=19, etc).
  for (let i = SEVERITIES.length - 1; i >= 0; i--) if (sn >= SEVERITIES[i].sn) return SEVERITIES[i].label;
  return '?';
}
const SEV_COLOR = { TRACE: '#888', DEBUG: '#6aa', INFO: '#0a0', WARN: '#c80', ERROR: '#c33', FATAL: '#a04' };

function defaultUrl() {
  if (typeof window === 'undefined') return 'ws://127.0.0.1:4091/log';
  const host = window.location && window.location.hostname || '127.0.0.1';
  return `ws://${host}:4091/log`;
}

function fmtTs(ms) {
  // Producer-relative MM:SS.mmm.  tsMsSinceStart is provided as a convenience.
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
  // Render ringTail entries as compressed lines using the OTel field shape.
  const tailLine = (e) => {
    const mod = (e.attributes && e.attributes.module) || '?';
    const sev = e.severityText || sevLabel(e.severityNumber);
    return `[${fmtTs(e.tsMsSinceStart)}][${sev[0]}][${mod}] ${e.body || ''}`;
  };
  return (
    <div style={{
      background: '#3a0808', color: '#fff', padding: 10, borderRadius: 4,
      margin: '6px 0', fontFamily: 'monospace', fontSize: 12,
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
        <span style={{ fontSize: 14, fontWeight: 700 }}>💥 {crash.signal || 'CRASH'}</span>
        <span style={{ opacity: 0.85 }}>{crash.timeUnixNano}</span>
        <span style={{ flex: 1 }} />
        <button onClick={() => setOpen((o) => !o)}>{open ? 'hide' : 'show stack'}</button>
        {onDismiss && <button onClick={onDismiss}>dismiss</button>}
      </div>
      <div style={{ marginTop: 4 }}>dump: {crash.dumpPath}</div>
      {open && (
        <pre style={{ marginTop: 6, whiteSpace: 'pre-wrap', maxHeight: 240, overflow: 'auto' }}>
          {(crash.frames || []).map((f) => `#${f.idx}  ${f.addr}  ${f.symbol || '?'}` +
            (f.file ? `  (${f.file}:${f.line})` : '')).join('\n')}
          {crash.ringTail && crash.ringTail.length > 0 &&
            '\n\n--- ringTail (last lines before death) ---\n' +
            crash.ringTail.map(tailLine).join('\n')}
        </pre>
      )}
    </div>
  );
}

function LogRow({ entry, needle }) {
  const sev = entry.severityText || sevLabel(entry.severityNumber);
  const col = SEV_COLOR[sev] || '#444';
  const mod = (entry.attributes && entry.attributes.module) || '';
  const body = entry.body || '';
  let textNode = body;
  if (needle && body) {
    const i = body.toLowerCase().indexOf(needle.toLowerCase());
    if (i >= 0) {
      textNode = (
        <>
          {body.slice(0, i)}
          <mark style={{ background: 'yellow', color: 'black' }}>{body.slice(i, i + needle.length)}</mark>
          {body.slice(i + needle.length)}
        </>
      );
    }
  }
  return (
    <div style={{
      display: 'flex', gap: 8, fontFamily: 'monospace', fontSize: 12,
      borderBottom: '1px solid rgba(0,0,0,0.05)', padding: '2px 4px',
    }}>
      <span style={{ color: '#888', minWidth: 90 }}>{fmtTs(entry.tsMsSinceStart)}</span>
      <span style={{ color: col, fontWeight: 700, minWidth: 14 }} title={sev}>{sev[0]}</span>
      <span style={{ color: '#357', minWidth: 110 }}>{mod}</span>
      <span style={{ flex: 1, wordBreak: 'break-all' }}>{textNode}</span>
    </div>
  );
}

function GapMarker({ entry }) {
  // Synthetic row inserted in place of dropped frames so the user can SEE the
  // gap rather than wondering why a chunk of context is missing.
  return (
    <div style={{
      fontFamily: 'monospace', fontSize: 11, padding: '4px 8px',
      background: '#fff4e0', color: '#9a5d00',
      borderTop: '1px dashed #c89000', borderBottom: '1px dashed #c89000',
      textAlign: 'center', fontStyle: 'italic',
    }}>
      ⚠ {entry.count} lines lost (backpressure)
    </div>
  );
}

export default function CoreLogPanel({ url, height = '70vh' }) {
  const wsUrl = url || defaultUrl();

  const [entries, setEntries]   = useState([]);          // ring buffer (logs + gaps)
  const [status, setStatus]     = useState({ state: 'idle' });
  const [hello, setHello]       = useState(null);
  const [modules, setModules]   = useState([]);
  const [crash, setCrash]       = useState(null);
  const [minSevIdx, setMinSevIdx] = useState(2);         // index into SEVERITIES; 2 = INFO
  const [modFilter, setModFilter] = useState([]);
  const [needle, setNeedle]     = useState('');
  const [paused, setPaused]     = useState(false);
  const [autoscroll, setAutoscroll] = useState(true);

  const clientRef = useRef(null);
  const bufRef    = useRef([]);                          // [{kind, id, ...payload}]
  const seqRef    = useRef(0);
  const pausedRef = useRef(false);
  const listRef   = useRef(null);

  const pushBuf = useCallback((kind, payload) => {
    const id = ++seqRef.current;
    bufRef.current.push({ kind, id, ...payload });
    if (bufRef.current.length > BUFFER_CAP) {
      bufRef.current.splice(0, bufRef.current.length - BUFFER_CAP);
    }
  }, []);

  useEffect(() => { pausedRef.current = paused; }, [paused]);

  useEffect(() => {
    const c = new CoreLogClient({ url: wsUrl });
    clientRef.current = c;

    const offHello   = c.on('hello',   (h) => { setHello(h); });
    const offLog     = c.on('log',     (e) => { if (!pausedRef.current) pushBuf('log', e); });
    const offBack    = c.on('backlog', ({ items }) => {
      if (!items || items.length === 0) return;
      for (const it of items) pushBuf('log', it);
    });
    const offDrop    = c.on('dropped', (d) => { pushBuf('gap', d); });
    const offCrash   = c.on('crash',   (cr) => { setCrash(cr); });
    const offStatus  = c.on('status',  (s) => { setStatus(s); });

    c.start();
    const minSn = SEVERITIES[minSevIdx].sn;
    c.subscribe({ minSeverityNumber: minSn,
                  modules: modFilter.length ? modFilter : null,
                  includeEphemeral: minSn <= 5,         // DEBUG/TRACE need opt-in
                  backlog: { tailN: 500 } })
      .catch((e) => log.warn('[subscribe-fail]', { err: String(e) }));

    const flush = setInterval(() => { setEntries(bufRef.current.slice()); }, 250);

    return () => {
      clearInterval(flush);
      offHello(); offLog(); offBack(); offDrop(); offCrash(); offStatus();
      c.stop();
      clientRef.current = null;
    };
  }, [wsUrl, pushBuf]); // eslint-disable-line react-hooks/exhaustive-deps

  // Re-subscribe when filters change.
  useEffect(() => {
    const c = clientRef.current;
    if (!c) return;
    const minSn = SEVERITIES[minSevIdx].sn;
    c.subscribe({ minSeverityNumber: minSn,
                  modules: modFilter.length ? modFilter : null,
                  includeEphemeral: minSn <= 5,
                  backlog: null }).catch(() => {});
  }, [minSevIdx, modFilter]);

  // Fetch module list once we connect.
  useEffect(() => {
    if (status.state !== 'connected') return;
    const c = clientRef.current;
    if (!c) return;
    c.getModules().then((mods) => setModules(mods || [])).catch(() => {});
  }, [status.state]);

  // Autoscroll on entry update.
  useEffect(() => {
    if (!autoscroll || paused) return;
    const el = listRef.current;
    if (el) el.scrollTop = el.scrollHeight;
  }, [entries, autoscroll, paused]);

  // Client-side text filter (against body + module). Gap markers always visible.
  const visible = useMemo(() => {
    if (!needle) return entries;
    const n = needle.toLowerCase();
    return entries.filter((e) => {
      if (e.kind === 'gap') return true;
      const body = (e.body || '').toLowerCase();
      const mod  = (e.attributes && e.attributes.module || '').toLowerCase();
      return body.includes(n) || mod.includes(n);
    });
  }, [entries, needle]);

  const onDumpNow = useCallback(() => {
    const c = clientRef.current; if (!c) return;
    c.dumpNow().then((ack) => alert('dump written: ' + (ack && ack.dumpPath)))
               .catch((e) => alert('dump failed: ' + String(e)));
  }, []);

  // Quick action: set every known module to the same severity. Wires into
  // setLevel(scope:module). Cheap; backend coalesces.
  const onSetAllModules = useCallback((sn) => {
    const c = clientRef.current; if (!c) return;
    Promise.all(modules.map((m) =>
      c.setLevel({ scope: 'module', module: m.name, severityNumber: sn }).catch(() => null)
    )).then(() => c.getModules()).then((mods) => setModules(mods || []));
  }, [modules]);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height, fontFamily: 'monospace' }}>
      <div style={{ padding: 4 }}>
        <StatusPill status={status} />
        {hello && <span style={{ fontSize: 12, opacity: 0.7 }}>
          drainer v{hello.drainerVersion} · {hello.resource && hello.resource['service.name']}@{hello.resource && hello.resource['host.name']} ·
          pid {hello.resource && hello.resource['process.pid']} · ring {hello.ringMb}MB
        </span>}
      </div>

      <CrashBanner crash={crash} onDismiss={() => setCrash(null)} />

      <div style={{ display: 'flex', gap: 8, padding: 4, alignItems: 'center', flexWrap: 'wrap' }}>
        <label style={{ fontSize: 12 }}>
          min severity{' '}
          <select value={minSevIdx} onChange={(e) => setMinSevIdx(parseInt(e.target.value, 10))}>
            {SEVERITIES.map((s, i) => <option key={s.label} value={i}>{s.label}</option>)}
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
        <div style={{ padding: '2px 4px', fontSize: 11, opacity: 0.85, display: 'flex', alignItems: 'center', flexWrap: 'wrap', gap: 8 }}>
          <span style={{ opacity: 0.7 }}>modules:</span>
          {modules.map((m) => (
            <label key={m.name}>
              <input
                type="checkbox"
                checked={modFilter.includes(m.name)}
                onChange={(e) => setModFilter((prev) => e.target.checked
                  ? [...prev, m.name]
                  : prev.filter((x) => x !== m.name))}
              />{' '}{m.name}<sub style={{ opacity: 0.5 }}>{sevLabel(m.effectiveSeverityNumber)}</sub>
            </label>
          ))}
          {modFilter.length > 0 && (
            <button onClick={() => setModFilter([])}>clear</button>
          )}
          <span style={{ flex: 1 }} />
          <span style={{ opacity: 0.7 }}>set all →</span>
          {SEVERITIES.map((s) => (
            <button key={s.label} title={`setLevel module=* severityNumber=${s.sn}`}
                    onClick={() => onSetAllModules(s.sn)}>{s.label}</button>
          ))}
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
        {visible.map((e) => e.kind === 'gap'
          ? <GapMarker key={'g' + e.id} entry={e} />
          : <LogRow    key={'l' + e.id} entry={e} needle={needle} />
        )}
      </div>

      <div style={{ padding: 4, fontSize: 11, opacity: 0.7 }}>
        {visible.length}/{entries.length} entries
        {entries.length >= BUFFER_CAP && ` (cap ${BUFFER_CAP}; dropping oldest)`}
      </div>
    </div>
  );
}
