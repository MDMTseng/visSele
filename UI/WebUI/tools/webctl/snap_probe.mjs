// Press the WebUI's "take a new image" button N times, from outside the browser.
//
// DefConfUI's 立即 sends BPG "EX" with trigger_type 0 (software trigger) and a
// timeout; the reply is an SS carrying ACK, followed by SG (report) and IM
// (image) when a frame was actually obtained. This sends exactly that, so a
// snap path that fails intermittently can be MEASURED instead of retried by
// hand -- which is how the two bugs behind "sometimes it does nothing" were
// found (see CORE0_1_CAVEATS §M).
//
//   node snap_probe.mjs [N=10] [timeout_ms=3000] [ws=ws://127.0.0.1:4090]
//   GAP=400        ms between presses -- the interval is the variable that
//                  exposed the stale-timer bug, so it is a knob
//   RAW=1          print the SS reply body
//   RECONNECT=1    send RC camera_ez_reconnect first (restarts acquisition)
//
// The UI sends timeout -1, which the core clamps to 30s. Probe with something
// smaller so a failure costs seconds rather than half a minute -- but do at
// least one -1 run before believing it is fixed, because the timeout length is
// what decides how long a stale timer stays armed.
import WebSocket from 'ws';

const N   = process.argv[2] ? parseInt(process.argv[2], 10) : 10;
const TMO = process.argv[3] ? parseInt(process.argv[3], 10) : 3000;
const URL = process.argv[4] || 'ws://127.0.0.1:4090';

const BPG_HDR = 9;
const enc = new TextEncoder();
function frame(type, prop, pgID, obj) {
  const str = obj == null ? '' : JSON.stringify(obj);
  const body = enc.encode(str);
  const buf = new Uint8Array(BPG_HDR + body.length + 1);
  buf[0] = type.charCodeAt(0); buf[1] = type.charCodeAt(1); buf[2] = prop;
  buf[3] = pgID >> 8; buf[4] = pgID & 255;
  const len = buf.length - BPG_HDR;
  buf[5] = len >>> 24; buf[6] = (len >> 16) & 255; buf[7] = (len >> 8) & 255; buf[8] = len & 255;
  buf.set(body, BPG_HDR);
  buf[BPG_HDR + body.length] = 0;
  return buf;
}
function header(dv) {
  const b = new Uint8Array(dv);
  if (b.length < BPG_HDR) return null;
  return { type: String.fromCharCode(b[0], b[1]), prop: b[2],
           pgID: (b[3] << 8) | b[4],
           length: b[5] * 0x01000000 + (b[6] << 16) + (b[7] << 8) + b[8] };
}

let pg = 1;
const ws = new WebSocket(URL);
ws.binaryType = 'arraybuffer';
const results = [];
let pending = null;

function shot(i) {
  return new Promise((resolve) => {
    const id = pg++;
    const t0 = Date.now();
    pending = { id, resolve, t0, got: { SS: null, SG: false, IM: false } };
    ws.send(frame('EX', 0, id, {
      trigger_type: 0, timeout: TMO,
      img_property: { down_samp_level: 2 },
    }));
    setTimeout(() => {
      if (pending && pending.id === id) {
        const p = pending; pending = null;
        p.resolve({ i, ms: Date.now() - p.t0, ack: null, note: 'no SS reply' });
      }
    }, TMO + 20000);
  });
}

ws.on('open', () => setTimeout(run, 300));
ws.on('error', (e) => { console.error('ws error:', e.message); process.exit(1); });

ws.on('message', (data) => {
  if (!(data instanceof ArrayBuffer)) data = data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
  const b = new Uint8Array(data);
  const h = header(data);
  if (!h) return;
  if (h.type === 'HR') { ws.send(frame('HR', 0, pg++, { a: ['d'] })); return; }
  if (!pending || h.pgID !== pending.id) return;
  if (h.type === 'IM') { pending.got.IM = true; return; }
  if (h.type === 'SG') { pending.got.SG = true; return; }
  if (h.type === 'SS') {
    // Read ACK with a regex, not JSON.parse: the SS body arrives truncated
    // (the reply is cut at the wire length, mid-string in errMsg), so a strict
    // parse fails on a perfectly good ACK. IM/SG presence is the real success
    // signal anyway -- ACK is the core's opinion, a frame is the fact.
    let ack = null, raw = '';
    try {
      raw = new TextDecoder().decode(b.subarray(BPG_HDR)).replace(/\0[\s\S]*$/, '');
      const mm = raw.match(/"ACK"\s*:\s*(true|false)/);
      if (mm) ack = mm[1] === 'true';
    } catch (e) {}
    if (process.env.RAW) console.log('   SS raw:', raw);
    const p = pending; pending = null;
    p.resolve({ ms: Date.now() - p.t0, ack, IM: p.got.IM, SG: p.got.SG });
  }
});

async function run() {
  if (process.env.RECONNECT) {
    console.log('[probe] sending RC camera_ez_reconnect first');
    ws.send(frame('RC', 0, pg++, { target: 'camera_ez_reconnect' }));
    await new Promise((s) => setTimeout(s, 6000));
  }
  for (let i = 0; i < N; i++) {
    const r = await shot(i);
    results.push(r);
    console.log(`#${String(i + 1).padStart(2)}  ack=${String(r.ack).padStart(5)}  ` +
                `${String(r.ms).padStart(6)} ms  IM=${r.IM ? 'y' : 'n'} SG=${r.SG ? 'y' : 'n'}` +
                (r.note ? '  ' + r.note : ''));
    await new Promise((s) => setTimeout(s, parseInt(process.env.GAP || '400', 10)));
  }
  const ok = results.filter((r) => r.ack === true).length;
  const ms = results.filter((r) => r.ack === true).map((r) => r.ms).sort((a, b) => a - b);
  console.log(`\n=> ${ok}/${N} ACK` +
    (ms.length ? `   ok latency min/med/max ${ms[0]}/${ms[Math.floor(ms.length / 2)]}/${ms[ms.length - 1]} ms` : ''));
  ws.close(); process.exit(ok === N ? 0 : 1);
}
