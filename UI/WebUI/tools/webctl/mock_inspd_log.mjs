// Mock inspd_log v1 WebSocket server.
//
// Speaks the protocol documented in InspectionCore/docs/LOGGING_WEBUI.md so
// the WebUI's CoreLogPanel can be built end-to-end before Phase F.2 ships.
// Replays either fixture lines OR a provided insp.log file as a live stream.
//
// Usage:
//   node tools/mock_inspd_log.mjs                           # synthetic stream
//   node tools/mock_inspd_log.mjs --file /path/to/insp.log  # replay disk file
//   node tools/mock_inspd_log.mjs --port 4091 --rate 8      # ~8 lines/sec
//   node tools/mock_inspd_log.mjs --crash-after 30          # send `crash` after 30s
//
// Protocol notes (v1):
//   - On open: server sends `hello` immediately.
//   - Client sends `subscribe` with min_level/modules/backlog.
//   - Server replies backlog as backlog_chunk frames, then live `log` / `log_batch`.
//   - set_level / get_modules / dump_now / ping all handled per the doc.

import { WebSocketServer } from 'ws';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

// ─── CLI ──────────────────────────────────────────────────────────────────
const argv = process.argv.slice(2);
const arg = (k, d) => {
  const i = argv.indexOf(k);
  return i >= 0 ? argv[i + 1] : d;
};
const flag = (k) => argv.includes(k);

const PORT       = parseInt(arg('--port', '4091'), 10);
const RATE_LPS   = parseFloat(arg('--rate', '5'));               // lines/sec
const REPLAY     = arg('--file', null);
const CRASH_S    = parseFloat(arg('--crash-after', '0'));        // 0 = no crash
const VERBOSE    = flag('--verbose');

// ─── Module + level catalogue ─────────────────────────────────────────────
const LEVELS    = ['T', 'D', 'I', 'W', 'E', 'F']; // 0..5
const LEVEL_OF  = { T:0, D:1, I:2, W:3, E:4, F:5 };
const MODULES   = [
  'core', 'cam.bmp', 'cam.acu', 'cam.gige',
  'match.linemod', 'match.sig360', 'match.caliper',
  'comm.bpg', 'comm.smem', 'comm.ws',
  'fs.config', 'fs.def', 'inspd_log',
];

const STARTED_UTC = new Date().toISOString().replace(/[-:.]/g,'').slice(0,15) + 'Z';
const PRODUCER_PID = 12345;
const RING_MB = 16;
const RING_SLOTS = 65534;

const T0_MS = Date.now();

// Per-module effective level (server-side state; can be tweaked by set_level).
const effective = Object.fromEntries(MODULES.map((m) => [m, LEVEL_OF.I]));
let globalLevel = LEVEL_OF.I;

// ─── Synthetic line generator ─────────────────────────────────────────────
let synthSeq = 0;
function synthLine() {
  const mod = MODULES[Math.floor(Math.random()*MODULES.length)];
  const lvIdx = Math.random() < 0.02 ? 4 : Math.random() < 0.08 ? 3 : Math.random() < 0.4 ? 1 : 2;
  return {
    type: 'log',
    ts_ms: Date.now() - T0_MS,
    level: lvIdx,
    level_char: LEVELS[lvIdx],
    module: mod,
    file: mod.replace(/\./g,'_') + '.cpp',
    line: 100 + Math.floor(Math.random()*900),
    func: 'fn_' + Math.floor(Math.random()*50),
    text: `seq=${++synthSeq} ${['ok','warn','retry','ack','frame','heartbeat','dispatch'][Math.floor(Math.random()*7)]} value=${Math.floor(Math.random()*1000)}`,
  };
}

// ─── Replay parser ────────────────────────────────────────────────────────
// Parses one disk-format line like:
//   [  1019.144][I][cam.bmp       ][CameraLayer_BMP.cpp:245 fn_name] body
const DISK_RE = /^\[\s*(\d+\.\d+)\]\[([TDIWEF])\]\[([^\]]+?)\s*\]\[([^:]+):(\d+)\s+([^\]]+)\]\s*(.*)$/;
function parseDiskLine(line) {
  const m = DISK_RE.exec(line);
  if (!m) return null;
  return {
    type: 'log',
    ts_ms: Math.round(parseFloat(m[1]) * 1000),
    level: LEVEL_OF[m[2]],
    level_char: m[2],
    module: m[3].trim(),
    file: m[4].trim(),
    line: parseInt(m[5], 10),
    func: m[6].trim(),
    text: m[7],
  };
}

let replayLines = null;
let replayIdx   = 0;
if (REPLAY) {
  try {
    replayLines = fs.readFileSync(REPLAY, 'utf8').split(/\r?\n/).filter(Boolean);
    console.log(`[mock] replay file: ${REPLAY} (${replayLines.length} lines)`);
  } catch (e) {
    console.error(`[mock] cannot read --file: ${e.message}`);
    process.exit(1);
  }
}

function nextLine() {
  if (replayLines && replayIdx < replayLines.length) {
    const parsed = parseDiskLine(replayLines[replayIdx++]);
    if (parsed) return parsed;
    // unparseable → wrap as a free-form text line
    return {
      type: 'log',
      ts_ms: Date.now() - T0_MS,
      level: 2, level_char: 'I', module: 'replay',
      file: REPLAY, line: replayIdx, func: 'raw',
      text: replayLines[replayIdx - 1],
    };
  }
  return synthLine();
}

// ─── Backlog cache for new subscribers ────────────────────────────────────
const BACKLOG_CAP = 2000;
const backlog = [];
function recordBacklog(entry) {
  backlog.push(entry);
  if (backlog.length > BACKLOG_CAP) backlog.shift();
}

// ─── Subscription filtering ───────────────────────────────────────────────
function globMatch(glob, value) {
  // Simple `*` glob. `cam.*` → /^cam\..*$/
  const re = new RegExp('^' + glob.replace(/[.+?^${}()|[\]\\]/g,'\\$&').replace(/\*/g,'.*') + '$');
  return re.test(value);
}
function passesFilter(entry, sub) {
  if (!sub) return false;
  if (entry.level < sub.min_level) return false;
  if (sub.modules && sub.modules.length > 0) {
    if (!sub.modules.some((g) => globMatch(g, entry.module))) return false;
  }
  if (entry.level <= LEVEL_OF.D && !sub.include_ephemeral) return false;
  return true;
}

// ─── WS server ────────────────────────────────────────────────────────────
const wss = new WebSocketServer({
  port: PORT,
  handleProtocols: (protocols) => (protocols.has('inspd_log.v1') ? 'inspd_log.v1' : false),
  path: '/log',
});

console.log(`[mock] inspd_log v1 mock listening on ws://127.0.0.1:${PORT}/log`);
console.log(`[mock] replay=${REPLAY ? 'file' : 'synthetic'} rate=${RATE_LPS}/s crash_after=${CRASH_S}s`);

wss.on('connection', (ws, req) => {
  if (VERBOSE) console.log(`[mock] client connected from ${req.socket.remoteAddress}`);

  // hello first.
  ws.send(JSON.stringify({
    type: 'hello',
    drainer_version: 1,
    ring_version: 2,
    ring_slots: RING_SLOTS,
    ring_mb: RING_MB,
    started_utc: STARTED_UTC,
    producer_pid: PRODUCER_PID,
    log_dir: '/tmp/insp-mock/',
  }));

  let sub = null; // { min_level, modules, include_ephemeral }
  let alive = true;
  let lastPing = Date.now();
  let pingTimer = setInterval(() => {
    if (Date.now() - lastPing > 60_000) {
      try { ws.close(1001, 'silent client'); } catch (_) {}
      return;
    }
  }, 5000);

  function sendAck(id, ok, error) {
    ws.send(JSON.stringify(error ? { type:'ack', id, ok:false, error } : { type:'ack', id, ok:true }));
  }

  ws.on('message', (raw) => {
    let msg;
    try { msg = JSON.parse(raw.toString()); }
    catch (_) { return; }
    lastPing = Date.now();

    switch (msg.type) {
      case 'subscribe': {
        sub = {
          min_level: typeof msg.min_level === 'number' ? msg.min_level : 0,
          modules: Array.isArray(msg.modules) ? msg.modules.slice() : null,
          include_ephemeral: !!msg.include_ephemeral,
        };
        sendAck(msg.id, true);
        // Deliver backlog matching the new filters.
        if (msg.backlog) {
          let items;
          if (typeof msg.backlog.tail_n === 'number') {
            items = backlog.slice(-msg.backlog.tail_n).filter((e) => passesFilter(e, sub));
          } else if (typeof msg.backlog.since_ms === 'number') {
            items = backlog.filter((e) => e.ts_ms >= msg.backlog.since_ms && passesFilter(e, sub));
          } else items = [];
          // Chunk in batches of 100.
          const CHUNK = 100;
          for (let i = 0; i < items.length; i += CHUNK) {
            const slice = items.slice(i, i + CHUNK);
            ws.send(JSON.stringify({
              type: 'backlog_chunk', items: slice, more: i + CHUNK < items.length,
            }));
          }
          if (items.length === 0) ws.send(JSON.stringify({ type:'backlog_chunk', items:[], more:false }));
        }
        break;
      }
      case 'set_level': {
        const lv = msg.level;
        if (typeof lv !== 'number' || lv < 0 || lv > 6) { sendAck(msg.id, false, 'bad level'); break; }
        if (msg.scope === 'global') { globalLevel = lv; sendAck(msg.id, true); }
        else if (msg.scope === 'module' && msg.module) {
          if (!(msg.module in effective)) { sendAck(msg.id, false, 'unknown module'); break; }
          effective[msg.module] = lv;
          sendAck(msg.id, true);
        } else sendAck(msg.id, false, 'bad scope');
        break;
      }
      case 'get_modules': {
        ws.send(JSON.stringify({
          type: 'modules', id: msg.id,
          modules: Object.entries(effective).map(([name, lv]) => ({ name, effective_level: lv })),
        }));
        break;
      }
      case 'dump_now': {
        const stamp = new Date().toISOString().replace(/[-:.]/g,'').slice(0,15) + 'Z';
        const dump_path = `/tmp/insp-mock/dump_${stamp}.dump`;
        ws.send(JSON.stringify({ type:'ack', id: msg.id, ok:true, dump_path }));
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

  // Per-client send loop (paced by --rate).
  const interval = setInterval(() => {
    if (!alive) { clearInterval(interval); return; }
    const e = nextLine();
    recordBacklog(e);
    if (passesFilter(e, sub)) {
      try { ws.send(JSON.stringify(e)); } catch (_) {}
    }
  }, Math.max(10, Math.floor(1000 / RATE_LPS)));

  // Optional simulated crash.
  if (CRASH_S > 0) {
    setTimeout(() => {
      if (!alive) return;
      const utc = new Date().toISOString().replace(/[-:.]/g,'').slice(0,15) + 'Z';
      const ring_tail = backlog.slice(-50);
      ws.send(JSON.stringify({
        type: 'crash',
        signal: 'SIGABRT',
        signal_raw: 6,
        utc,
        dump_path: `/tmp/insp-mock/crash_${utc}.dump`,
        frames: [
          { idx: 0, addr: '0x1047bcc50', symbol: '?',           file: null, line: null },
          { idx: 1, addr: '0x19c9ea744', symbol: '_sigtramp',   file: null, line: null },
          { idx: 2, addr: '0x10010ad8c', symbol: 'doInspect',   file: 'wiringPanel.cpp', line: 2076 },
        ],
        ring_tail,
        ephemeral_tail: [],
      }));
      // Drainer is expected to drop after crash.
      setTimeout(() => { try { ws.close(1011, 'producer crash'); } catch (_) {} }, 200);
    }, CRASH_S * 1000);
  }
});

process.on('SIGINT', () => { console.log('[mock] shutting down'); wss.close(() => process.exit(0)); });
