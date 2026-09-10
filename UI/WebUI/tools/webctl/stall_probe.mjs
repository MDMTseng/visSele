// What holds the select thread? -- read the stall meter, drive a realistic mix,
// read it again, print the difference per packet type.
//
// The core handles every WS command inline on the thread that owns select(),
// so a slow handler is dead air for every client. wiringPanel's meter files
// each handler's wall time under its two-letter tag and ships the table in
// GS perif_pairing.lat_hist.cmd_by_tl. This drives the commands a person
// actually causes -- browse a folder, load a def, check it, save it, poke at
// a port that is not there -- and reports who blocked, for how long.
//
//   node stall_probe.mjs [--def <abs .hydef>] [--url ws://127.0.0.1:4090] [--rounds 5]
import WebSocket from 'ws';

const arg = (k, d) => { const i = process.argv.indexOf('--' + k); return i >= 0 ? process.argv[i + 1] : d; };
const URL    = arg('url', 'ws://127.0.0.1:4090');
const DEF    = arg('def', '');
const IMG    = arg('img', '');
const ROUNDS = parseInt(arg('rounds', '5'), 10);

const HDR = 9, enc = new TextEncoder(), dec = new TextDecoder();
function frame(type, pgID, obj) {
  const body = enc.encode(obj == null ? '' : JSON.stringify(obj));
  const buf = new Uint8Array(HDR + body.length + 1);
  buf[0] = type.charCodeAt(0); buf[1] = type.charCodeAt(1); buf[2] = 0;
  buf[3] = pgID >> 8; buf[4] = pgID & 255;
  const len = buf.length - HDR;
  buf[5] = len >>> 24; buf[6] = (len >> 16) & 255; buf[7] = (len >> 8) & 255; buf[8] = len & 255;
  buf.set(body, HDR);
  return buf;
}

let pg = 1;
const ws = new WebSocket(URL);
ws.binaryType = 'arraybuffer';
const waiters = [];
ws.on('message', (m) => {
  const b = new Uint8Array(m instanceof ArrayBuffer ? m : m.buffer.slice(m.byteOffset, m.byteOffset + m.byteLength));
  if (b.length < HDR) return;
  const type = String.fromCharCode(b[0], b[1]);
  const len  = b[5] * 0x01000000 + (b[6] << 16) + (b[7] << 8) + b[8];
  const txt  = dec.decode(b.subarray(HDR, HDR + len)).replace(/\0+$/, '');
  for (let i = waiters.length - 1; i >= 0; i--)
    if (waiters[i].type === type) { waiters.splice(i, 1)[0].resolve(txt); }
});
const send = (t, o) => ws.send(frame(t, pg++, o));
// Several commands answer under a DIFFERENT tag than they were sent with
// (LD replies FL, II replies IR), so the send tag and the tag to wait for are
// separate arguments rather than one assumed to be both.
const ask = (t, o, ms = 8000, reply = t) => new Promise((res) => {
  const w = { type: reply, resolve: res };
  waiters.push(w);
  setTimeout(() => { const i = waiters.indexOf(w); if (i >= 0) { waiters.splice(i, 1); res(null); } }, ms);
  send(t, o);
});
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function readMeter() {
  const txt = await ask('GS', { items: ['perif_pairing'] });
  if (!txt) return null;
  try { return JSON.parse(txt); } catch { return null; }
}
const table = (j) => (j && j.perif_pairing && j.perif_pairing.lat_hist
                       && j.perif_pairing.lat_hist.cmd_by_tl) || {};
const hist  = (j, k) => (j && j.perif_pairing && j.perif_pairing.lat_hist
                       && j.perif_pairing.lat_hist[k]) || null;

ws.on('open', async () => {
  await sleep(400);                       // let HR land
  const before = await readMeter();
  if (!before) { console.error('[stall] the core did not answer GS'); process.exit(3); }
  const t0 = table(before);

  for (let r = 0; r < ROUNDS; r++) {
    await ask('FB', { path: 'data' });                 // directory walk
    await ask('GS', { CameraSetting: 1 });             // cheap control
    if (DEF) {
      await ask('LD', { filename: DEF }, 8000, 'FL');   // parse a def off disk
      if (IMG) await ask('II', { imgsrc: IMG, deffile: DEF }, 25000, 'IR');
    }
    // A port that is not there: the path that constructs, throws, and cleans up.
    send('PD', { type: 'CONNECT', uart_name: 'COM99', baudrate: 230400, mode: '8N1' });
    await sleep(50);
    send('PD', { type: 'DISCONNECT', CONN_ID: 1 });
    await sleep(50);
    process.stderr.write(`\r[stall] round ${r + 1}/${ROUNDS}`);
  }
  process.stderr.write('\n');

  const after = await readMeter();
  const t1 = table(after);

  // The DELTA is the measurement -- since-boot totals carry whatever the last
  // session did, and this run is the experiment.
  const rows = [];
  for (const tl of Object.keys(t1)) {
    const a = t1[tl], b = t0[tl] || { n: 0, total_ms: 0, max_ms: 0 };
    const n = a.n - b.n;
    if (n <= 0) continue;
    rows.push({ tl, n, total: a.total_ms - b.total_ms,
                max: Math.max(a.max_ms, b.max_ms) === b.max_ms ? 0 : a.max_ms });
  }
  rows.sort((x, y) => y.total - x.total);

  console.log('\n  tag     n     total_ms    avg_ms    max_ms   share');
  const grand = rows.reduce((s, r) => s + r.total, 0) || 1;
  for (const r of rows)
    console.log(`  ${r.tl}  ${String(r.n).padStart(5)}  ${r.total.toFixed(1).padStart(10)}` +
                `  ${(r.total / r.n).toFixed(1).padStart(8)}  ${(r.max || 0).toFixed(1).padStart(8)}` +
                `  ${((100 * r.total) / grand).toFixed(1).padStart(5)}%`);

  for (const k of ['cmd', 'serve']) {
    const h = hist(after, k);
    if (h) console.log(`  ${k}: n=${h.n} avg=${(h.avg_ms || 0).toFixed(2)}ms max=${(h.max_ms || 0).toFixed(1)}ms`);
  }
  ws.close(); process.exit(0);
});
ws.on('error', (e) => { console.error('[stall]', e.message); process.exit(2); });
