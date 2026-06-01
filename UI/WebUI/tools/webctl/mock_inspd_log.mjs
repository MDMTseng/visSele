// Mock inspd_log v1 WebSocket server (OTel-aligned schema).
//
// Speaks the protocol documented in InspectionCore/docs/LOGGING_WEBUI.md so
// the WebUI's CoreLogPanel can be built end-to-end before Phase F.2 ships.
//
// Replays either fixture lines OR a provided insp.log file as a live stream.
// Field names follow the OTel Logs Data Model (severityNumber/severityText/
// body/attributes.{module,code.filepath,code.lineno,code.function}, etc.) so
// a standard otel-collector receiver could ingest this with a thin WS shim.
//
// Usage:
//   node mock_inspd_log.mjs                        # synthetic stream
//   node mock_inspd_log.mjs --file insp.log        # replay disk file
//   node mock_inspd_log.mjs --port 4091 --rate 8   # ~8 lines/sec
//   node mock_inspd_log.mjs --crash-after 30       # simulate crash @ 30s
//   node mock_inspd_log.mjs --drop-every 50        # emit a `dropped` every 50 frames

import { WebSocketServer } from 'ws';
import fs from 'fs';
import path from 'path';
import os from 'os';
import zlib from 'zlib';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

// ─── CLI ──────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2);
const arg = (k, d) => { const i = argv.indexOf(k); return i >= 0 ? argv[i + 1] : d; };
const flag = (k) => argv.includes(k);

const PORT       = parseInt(arg('--port', '4091'), 10);
const RATE_LPS   = parseFloat(arg('--rate', '5'));
const REPLAY     = arg('--file', null);
const CRASH_S    = parseFloat(arg('--crash-after', '0'));
const DROP_EVERY = parseInt(arg('--drop-every', '0'), 10); // 0 = never
const VERBOSE    = flag('--verbose');

// ─── OTel severity scale ──────────────────────────────────────────────────
// TRACE=1, DEBUG=5, INFO=9, WARN=13, ERROR=17, FATAL=21. Gaps reserved for
// OTel variants (INFO2=10, ERROR3=19 etc).
const SEV_NUM   = { T:1, D:5, I:9, W:13, E:17, F:21 };
const SEV_TEXT  = { 1:'TRACE', 5:'DEBUG', 9:'INFO', 13:'WARN', 17:'ERROR', 21:'FATAL' };

const MODULES   = [
  'core', 'cam.bmp', 'cam.acu', 'cam.gige',
  'match.linemod', 'match.sig360', 'match.caliper',
  'comm.bpg', 'comm.smem', 'comm.ws',
  'fs.config', 'fs.def', 'inspd_log',
];

const T0_MS      = Date.now();
const T0_NS      = BigInt(T0_MS) * 1_000_000n;
const STARTED_NS = T0_NS.toString();
const PRODUCER_PID = 12345;
const RING_MB    = 16;
const RING_SLOTS = 65534;
const RESOURCE = {
  'service.name': 'visSele',
  'service.instance.id': String(PRODUCER_PID),
  'process.pid': PRODUCER_PID,
  'host.name': os.hostname(),
};

// Per-module effective severity (server-side state; tweakable via setLevel).
const effective = Object.fromEntries(MODULES.map((m) => [m, SEV_NUM.I]));
let globalSeverity = SEV_NUM.I;

// ─── Time helpers ─────────────────────────────────────────────────────────
function nowUnixNano() { return (BigInt(Date.now()) * 1_000_000n).toString(); }
function msSinceStart() { return Date.now() - T0_MS; }

// ─── Synthetic line generator ─────────────────────────────────────────────
let synthSeq = 0;
function synthLine() {
  const mod = MODULES[Math.floor(Math.random()*MODULES.length)];
  const r = Math.random();
  const sn = r < 0.02 ? SEV_NUM.E
           : r < 0.08 ? SEV_NUM.W
           : r < 0.4  ? SEV_NUM.D
           :            SEV_NUM.I;
  return {
    type: 'log',
    timeUnixNano: nowUnixNano(),
    tsMsSinceStart: msSinceStart(),
    severityNumber: sn,
    severityText:   SEV_TEXT[sn],
    body: `seq=${++synthSeq} ${['ok','warn','retry','ack','frame','heartbeat','dispatch'][Math.floor(Math.random()*7)]} value=${Math.floor(Math.random()*1000)}`,
    attributes: {
      module: mod,
      'code.filepath': mod.replace(/\./g,'_') + '.cpp',
      'code.lineno':   100 + Math.floor(Math.random()*900),
      'code.function': 'fn_' + Math.floor(Math.random()*50),
    },
    traceId: null,
    spanId:  null,
  };
}

// ─── Replay parser (disk format) ──────────────────────────────────────────
const DISK_RE = /^\[\s*(\d+\.\d+)\]\[([TDIWEF])\]\[([^\]]+?)\s*\]\[([^:]+):(\d+)\s+([^\]]+)\]\s*(.*)$/;
function parseDiskLine(line) {
  const m = DISK_RE.exec(line);
  if (!m) return null;
  const tsMs = Math.round(parseFloat(m[1]) * 1000);
  const sn = SEV_NUM[m[2]] || SEV_NUM.I;
  return {
    type: 'log',
    timeUnixNano: (T0_NS + BigInt(tsMs) * 1_000_000n).toString(),
    tsMsSinceStart: tsMs,
    severityNumber: sn,
    severityText:   SEV_TEXT[sn],
    body: m[7],
    attributes: {
      module: m[3].trim(),
      'code.filepath': m[4].trim(),
      'code.lineno':   parseInt(m[5], 10),
      'code.function': m[6].trim(),
    },
    traceId: null,
    spanId:  null,
  };
}

let replayLines = null;
let replayIdx   = 0;
if (REPLAY) {
  try {
    // Transparent gunzip if the path ends in .gz (matches the baseline/
    // inspd_log fixture convention).
    const buf = fs.readFileSync(REPLAY);
    const text = REPLAY.endsWith('.gz') ? zlib.gunzipSync(buf).toString('utf8') : buf.toString('utf8');
    replayLines = text.split(/\r?\n/).filter(Boolean);
    console.log(`[mock] replay file: ${REPLAY} (${replayLines.length} lines${REPLAY.endsWith('.gz') ? ', gunzipped' : ''})`);
  } catch (e) {
    console.error(`[mock] cannot read --file: ${e.message}`); process.exit(1);
  }
}

function nextLine() {
  if (replayLines && replayIdx < replayLines.length) {
    const parsed = parseDiskLine(replayLines[replayIdx++]);
    if (parsed) return parsed;
    return {
      type: 'log',
      timeUnixNano: nowUnixNano(),
      tsMsSinceStart: msSinceStart(),
      severityNumber: SEV_NUM.I, severityText: 'INFO',
      body: replayLines[replayIdx - 1],
      attributes: { module: 'replay', 'code.filepath': REPLAY, 'code.lineno': replayIdx, 'code.function': 'raw' },
      traceId: null, spanId: null,
    };
  }
  return synthLine();
}

// ─── Backlog cache ────────────────────────────────────────────────────────
const BACKLOG_CAP = 2000;
const backlog = [];
function recordBacklog(entry) {
  backlog.push(entry);
  if (backlog.length > BACKLOG_CAP) backlog.shift();
}

// ─── Subscription filtering ───────────────────────────────────────────────
function globMatch(glob, value) {
  const re = new RegExp('^' + glob.replace(/[.+?^${}()|[\]\\]/g,'\\$&').replace(/\*/g,'.*') + '$');
  return re.test(value);
}
function passesFilter(entry, sub) {
  if (!sub) return false;
  if (entry.severityNumber < sub.minSeverityNumber) return false;
  const mod = entry.attributes && entry.attributes.module;
  if (sub.modules && sub.modules.length > 0) {
    if (!mod || !sub.modules.some((g) => globMatch(g, mod))) return false;
  }
  // DEBUG/TRACE (≤5) are ephemeral; require opt-in.
  if (entry.severityNumber <= SEV_NUM.D && !sub.includeEphemeral) return false;
  return true;
}

// ─── WS server ────────────────────────────────────────────────────────────
const wss = new WebSocketServer({
  port: PORT,
  handleProtocols: (protocols) => (protocols.has('inspd_log.v1') ? 'inspd_log.v1' : false),
  path: '/log',
});

console.log(`[mock] inspd_log v1 (OTel-aligned) listening on ws://127.0.0.1:${PORT}/log`);
console.log(`[mock] replay=${REPLAY ? 'file' : 'synthetic'} rate=${RATE_LPS}/s crash_after=${CRASH_S}s drop_every=${DROP_EVERY}`);

wss.on('connection', (ws, req) => {
  if (VERBOSE) console.log(`[mock] client connected from ${req.socket.remoteAddress}`);

  // hello
  ws.send(JSON.stringify({
    type: 'hello',
    drainerVersion: 1,
    ringVersion:    2,
    ringSlots:      RING_SLOTS,
    ringMb:         RING_MB,
    startedUnixNano: STARTED_NS,
    logDir: '/tmp/insp-mock/',
    resource: RESOURCE,
  }));

  let sub = null;
  let alive = true;
  let frameCount = 0;
  let lastPing = Date.now();
  let pingTimer = setInterval(() => {
    if (Date.now() - lastPing > 60_000) {
      try { ws.close(1001, 'silent client'); } catch (_) {}
    }
  }, 5000);

  function sendAck(id, ok, error, extra) {
    const msg = error ? { type:'ack', id, ok:false, error } : { type:'ack', id, ok:true, ...extra };
    ws.send(JSON.stringify(msg));
  }

  ws.on('message', (raw) => {
    let msg;
    try { msg = JSON.parse(raw.toString()); } catch (_) { return; }
    lastPing = Date.now();

    switch (msg.type) {
      case 'subscribe': {
        sub = {
          minSeverityNumber: typeof msg.minSeverityNumber === 'number' ? msg.minSeverityNumber : 1,
          modules: Array.isArray(msg.modules) ? msg.modules.slice() : null,
          includeEphemeral: !!msg.includeEphemeral,
        };
        sendAck(msg.id, true);
        if (msg.backlog) {
          let items;
          if (typeof msg.backlog.tailN === 'number') {
            items = backlog.slice(-msg.backlog.tailN).filter((e) => passesFilter(e, sub));
          } else if (typeof msg.backlog.sinceUnixNano === 'string') {
            const cutoff = BigInt(msg.backlog.sinceUnixNano);
            items = backlog.filter((e) => BigInt(e.timeUnixNano) >= cutoff && passesFilter(e, sub));
          } else items = [];
          const CHUNK = 100;
          for (let i = 0; i < items.length; i += CHUNK) {
            const slice = items.slice(i, i + CHUNK);
            ws.send(JSON.stringify({ type:'backlogChunk', items: slice, more: i + CHUNK < items.length }));
          }
          if (items.length === 0) ws.send(JSON.stringify({ type:'backlogChunk', items:[], more:false }));
        }
        break;
      }
      case 'setLevel': {
        const sn = msg.severityNumber;
        if (typeof sn !== 'number' || sn < 0 || sn > 24) { sendAck(msg.id, false, 'bad severityNumber'); break; }
        if (msg.scope === 'global') { globalSeverity = sn; sendAck(msg.id, true); }
        else if (msg.scope === 'module' && msg.module) {
          if (!(msg.module in effective)) { sendAck(msg.id, false, 'unknown module'); break; }
          effective[msg.module] = sn;
          sendAck(msg.id, true);
        } else sendAck(msg.id, false, 'bad scope');
        break;
      }
      case 'getModules': {
        ws.send(JSON.stringify({
          type: 'modules', id: msg.id,
          modules: Object.entries(effective).map(([name, sn]) => ({ name, effectiveSeverityNumber: sn })),
        }));
        break;
      }
      case 'dumpNow': {
        const stamp = new Date().toISOString().replace(/[-:.]/g,'').slice(0,15) + 'Z';
        const dumpPath = `/tmp/insp-mock/dump_${stamp}.dump`;
        sendAck(msg.id, true, null, { dumpPath });
        break;
      }
      case 'ping': {
        ws.send(JSON.stringify({ type:'pong', id: msg.id }));
        break;
      }
      default:
        sendAck(msg.id, false, 'unknown type');
    }
  });

  ws.on('close', () => {
    alive = false;
    clearInterval(pingTimer);
    if (VERBOSE) console.log('[mock] client disconnected');
  });
  ws.on('error', () => { /* ignore */ });

  // Per-client send loop.
  const interval = setInterval(() => {
    if (!alive) { clearInterval(interval); return; }
    const e = nextLine();
    recordBacklog(e);
    if (passesFilter(e, sub)) {
      try { ws.send(JSON.stringify(e)); } catch (_) {}
      frameCount++;
      // Optional backpressure simulation.
      if (DROP_EVERY > 0 && frameCount % DROP_EVERY === 0) {
        try {
          ws.send(JSON.stringify({
            type: 'dropped',
            count: Math.floor(2 + Math.random() * 40),
            sinceUnixNano: nowUnixNano(),
          }));
        } catch (_) {}
      }
    }
  }, Math.max(10, Math.floor(1000 / RATE_LPS)));

  // Optional simulated crash.
  if (CRASH_S > 0) {
    setTimeout(() => {
      if (!alive) return;
      const ringTail = backlog.slice(-50);
      ws.send(JSON.stringify({
        type: 'crash',
        signal: 'SIGABRT',
        signalRaw: 6,
        timeUnixNano: nowUnixNano(),
        dumpPath: '/tmp/insp-mock/crash_' + new Date().toISOString().replace(/[-:.]/g,'').slice(0,15) + 'Z.dump',
        frames: [
          { idx: 0, addr: '0x1047bcc50', symbol: '?',           file: null, line: null },
          { idx: 1, addr: '0x19c9ea744', symbol: '_sigtramp',   file: null, line: null },
          { idx: 2, addr: '0x10010ad8c', symbol: 'doInspect',   file: 'wiringPanel.cpp', line: 2076 },
        ],
        ringTail,
        ephemeralTail: [],
      }));
      setTimeout(() => { try { ws.close(1011, 'producer crash'); } catch (_) {} }, 200);
    }, CRASH_S * 1000);
  }
});

process.on('SIGINT', () => { console.log('[mock] shutting down'); wss.close(() => process.exit(0)); });
