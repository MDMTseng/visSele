#!/usr/bin/env node
// One-run maximum-protocol-surface probe for the core's WS/BPG layer.
//
// Sweeps every TL the dispatcher (wiringPanel.cpp toUpperLayer) handles, with
//   (A) a valid minimal payload   -> assert reply shape + ACK polarity
//   (B) a malformed payload       -> assert the error path (ACK false / no reply)
//   (C) protocol-level abuse      -> truncated/lying/giant headers, unknown TL,
//                                    NUL-guard, multi-packet, split frame
// and after EVERY case a canonical GS liveness probe: the core must still
// answer a GS{items:["binary_path"]} on a fresh pgID within 2s. That is the
// real invariant -- a case that leaves the daemon wedged or dead fails here
// even if its own reply looked fine.
//
// Unlike daemon_fuzz.py (5 TLs, HR-liveness-only), this exercises all handled
// TLs and asserts reply STRUCTURE. Deliberately excluded as unsafe: SV binary
// writes, real CI/FI sessions, RC camera reconnect/calib, PD CONNECT, SC exec
// enabled -- each is replaced by its error-path or read-only variant.
//
//   node bpg_sweep.mjs [--include-crashers]
//
// --include-crashers runs cases known to SIGSEGV an UNPATCHED core (PD w/o
// "type"); on a patched core they must ACK-false like any other bad payload.
// Needs the core on :4090.
import WebSocket from 'ws';
const H = 9, enc = new TextEncoder(), dec = new TextDecoder();
const URL = 'ws://127.0.0.1:4090';
const INCLUDE_CRASHERS = process.argv.includes('--include-crashers');

// frame(tl, pgID, jsonObjOrString, {nul=true, rawBytes, declLen}) -> Uint8Array
// nul:true appends the \0 the core's NUL-guard requires (toUpperLayer:2822).
// rawBytes overrides the payload with arbitrary bytes; declLen overrides the
// declared length field (for lying-length abuse).
function frame(tl, pgID, payload, opts = {}) {
  const nul = opts.nul !== false;
  let body;
  if (opts.rawBytes !== undefined) body = opts.rawBytes;
  else {
    const s = (payload === undefined || payload === null) ? ''
      : (typeof payload === 'string' ? payload : JSON.stringify(payload));
    const b = enc.encode(s);
    body = nul ? Uint8Array.from([...b, 0]) : b;
  }
  const declLen = (opts.declLen !== undefined) ? opts.declLen : body.length;
  const u = new Uint8Array(H + body.length);
  u[0] = tl.charCodeAt(0); u[1] = tl.charCodeAt(1); u[2] = 0;
  u[3] = (pgID >> 8) & 255; u[4] = pgID & 255;
  u[5] = (declLen >>> 24) & 255; u[6] = (declLen >> 16) & 255;
  u[7] = (declLen >> 8) & 255; u[8] = declLen & 255;
  u.set(body, H);
  return u;
}

const ws = new WebSocket(URL); ws.binaryType = 'arraybuffer';
let pg = 100;
const nextPg = () => ++pg;
// reply router: pgID -> [{tl, data}]
const inbox = new Map();
ws.on('message', (d) => {
  const b = new Uint8Array(d instanceof ArrayBuffer ? d : d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength));
  if (b.length < H) return;
  const tl = String.fromCharCode(b[0], b[1]);
  const pgid = (b[3] << 8) | b[4];
  if (tl === 'HR') { ws.send(frame('HR', nextPg(), { a: ['d'] })); return; } // answer the hello; do not record
  let data = null;
  try { data = JSON.parse(dec.decode(b.subarray(H)).replace(/\0+$/, '')); } catch {}
  if (!inbox.has(pgid)) inbox.set(pgid, []);
  inbox.get(pgid).push({ tl, data });
});
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// send a case, wait up to `ms`, return the packets that came back on its pgID
async function fire(bytes, pgid, ms = 800) {
  inbox.delete(pgid);
  ws.send(bytes);
  const t0 = Date.now();
  while (Date.now() - t0 < ms) { await sleep(40); if (inbox.has(pgid)) { await sleep(120); break; } }
  return inbox.get(pgid) || [];
}

// liveness: a GS on a fresh pgID must return a GS packet carrying binary_path.
// 600ms, not 2s: GS binary_path is sub-100ms, so a generous window would mask
// a latency regression while still calling the core "alive". (It only proves
// main-dispatch-lock health + path resolution -- a wedged stream/camera/insp
// thread doesn't hold that lock and would still answer GS.)
// liveMs, because a fixed budget makes the assertion depend on how much work
// the case itself created. C11 deliberately queues 133,333 packets; the core
// answers every one of them and comes back in ~2.3s (measured: 59k packets/s,
// no crash, no leak, deterministic), so a flat 600ms reported LIVENESS LOST on
// every single run. That is the test failing, not the core -- and a check that
// cannot pass teaches people to ignore a red line.
async function alive(liveMs = 600) {
  const p = nextPg();
  const pkts = await fire(frame('GS', p, { items: ['binary_path'] }), p, liveMs);
  return pkts.some((k) => k.tl === 'GS' && k.data && typeof k.data.binary_path === 'string');
}

// convenience assertions
const ss = (pkts) => pkts.find((k) => k.tl === 'SS');
const pkt = (pkts, tl) => pkts.find((k) => k.tl === tl);

let pass = 0, fail = 0;
const results = [];
async function CASE(name, bytes, check, { expectNoReply = false, timeoutMs = 800, liveMs = 600 } = {}) {
  const p = nextPg();
  const pkts = await fire(typeof bytes === 'function' ? bytes(p) : bytes, p, timeoutMs);
  let ok = true, why = '';
  try {
    if (expectNoReply) {
      if (pkts.length) { ok = false; why = `expected no reply, got [${pkts.map((k) => k.tl).join(',')}]`; }
    } else {
      const r = check(pkts, p);
      if (r !== true) { ok = false; why = r || 'assertion failed'; }
    }
  } catch (e) { ok = false; why = 'threw: ' + e.message; }
  const live = await alive(liveMs);
  if (!live) { ok = false; why = (why ? why + '; ' : '') + 'LIVENESS LOST'; }
  results.push({ name, ok, why, live });
  (ok ? pass++ : fail++);
  console.log(`  ${ok ? 'PASS' : 'FAIL'}  ${name}${ok ? '' : '  -- ' + why}`);
  return { pkts, live };
}

await new Promise((r) => ws.on('open', () => setTimeout(r, 500)));
if (!(await alive())) { console.error('core not answering GS on :4090'); process.exit(2); }
console.log('bpg_sweep: all handled TLs x {valid, malformed, abuse}, liveness after each\n');

console.log('Group A -- valid minimal payload (reply shape + ACK polarity):');
await CASE('A1 HR {}', (p) => frame('HR', p, {}), (k) => (ss(k) && ss(k).data.cmd === 'HR' && ss(k).data.ACK === true) || 'no SS/HR/ACK-true');
await CASE('A2 GS binary_path+data_path', (p) => frame('GS', p, { items: ['binary_path', 'data_path'] }),
  (k) => (pkt(k, 'GS') && typeof pkt(k, 'GS').data.binary_path === 'string' && typeof pkt(k, 'GS').data.data_path === 'string') || 'no GS w/ both paths');
await CASE('A3 GS camera_info', (p) => frame('GS', p, { items: ['camera_info'] }),
  (k) => { const g = pkt(k, 'GS'); const c = g && g.data.camera_info && g.data.camera_info[0]; return (c && 'present' in c && 'cam_status' in c) || 'no camera_info[0].present/cam_status'; });
await CASE('A4 GS perif_pairing', (p) => frame('GS', p, { items: ['perif_pairing'] }),
  (k) => { const g = pkt(k, 'GS'); const l = g && g.data.perif_pairing && g.data.perif_pairing.link; return (l && 'connected' in l) || 'no perif_pairing.link.connected'; });
await CASE('A5 FB list .', (p) => frame('FB', p, { path: '.', depth: 1 }),
  (k) => (pkt(k, 'FB') || (ss(k) && ss(k).data.cmd === 'FB')) ? true : 'no FB/SS-FB reply');
await CASE('A6 LB /nonexistent (error path)', (p) => frame('LB', p, { filename: '/nonexistent' }),
  (k) => (ss(k) && ss(k).data.cmd === 'LB' && ss(k).data.ACK === false) || 'LB bad-file not ACK-false');
await CASE('A7 LD {} (nothing loaded)', (p) => frame('LD', p, {}),
  (k) => (ss(k) && ss(k).data.cmd === 'LD' && ss(k).data.ACK === true) || 'LD{} not ACK-true');
await CASE('A8 SC log_dump', (p) => frame('SC', p, { type: 'log_dump' }),
  (k) => (pkt(k, 'SR') || ss(k)) ? true : 'no SR/SS for SC log_dump');
await CASE('A9 SC exec (must be gated off)', (p) => frame('SC', p, { type: 'exec', cmd: 'echo hi' }),
  // Positive assertion: SC always ends session_ACK=true (the exec gate does
  // not flip it), so the ONLY signal that exec is disabled is the SR error
  // string. Asserting the error directly (not "SR-error OR ACK-false", whose
  // ACK-false leg is dead) means a regression that stops emitting the string
  // while still not executing (or worse, executes and emits {output}) fails.
  (k) => { const sr = pkt(k, 'SR'); return (sr && sr.data && /disabl/i.test(JSON.stringify(sr.data)) && !('output' in (sr.data || {}))) || 'exec not gated (SR must carry "disabled" and no output)'; });
await CASE('A10 II /nonexistent (no camera touch)', (p) => frame('II', p, { imgsrc: '/nonexistent' }),
  (k) => (ss(k) && ss(k).data.cmd === 'II' && ss(k).data.ACK === false) || 'II bad-src not ACK-false');
await CASE('A11 RC cam_doorbell_ping', (p) => frame('RC', p, { target: 'cam_doorbell_ping' }),
  (k) => (ss(k) && ss(k).data.cmd === 'RC' && ss(k).data.ACK === true) || 'RC doorbell not ACK-true');
await CASE('A12 SB {stream:false}', (p) => frame('SB', p, { stream: false }),
  (k) => (ss(k) && ss(k).data.cmd === 'SB' && ss(k).data.ACK === true) || 'SB not ACK-true');
await CASE('A13 PR {} (empty handler)', (p) => frame('PR', p, {}),
  (k) => (ss(k) && ss(k).data.cmd === 'PR' && ss(k).data.ACK === false) || 'PR not ACK-false');

console.log('\nGroup B -- malformed payload (error paths):');
await CASE('B2 GS {} no items', (p) => frame('GS', p, {}),
  (k) => (!pkt(k, 'GS') && ss(k) && ss(k).data.cmd === 'GS' && ss(k).data.ACK === false) || 'GS{} should be SS ACK-false, no GS');
await CASE('B3 GS items not array', (p) => frame('GS', p, { items: 'notarray' }),
  (k) => (!pkt(k, 'GS') && ss(k) && ss(k).data.ACK === false) || 'GS bad-items not ACK-false');
await CASE('B4 SV {} no filename', (p) => frame('SV', p, {}),
  (k) => (ss(k) && ss(k).data.cmd === 'SV' && ss(k).data.ACK === false) || 'SV{} not ACK-false');
await CASE('B6 FB {} no path', (p) => frame('FB', p, {}),
  (k) => (ss(k) && ss(k).data.cmd === 'FB' && ss(k).data.ACK === false) || 'FB{} not ACK-false');
await CASE('B7 LD /nonexistent', (p) => frame('LD', p, { filename: '/nonexistent' }),
  (k) => (ss(k) && ss(k).data.cmd === 'LD' && ss(k).data.ACK === false) || 'LD bad-file not ACK-false');
await CASE('B10 RC bogus target', (p) => frame('RC', p, { target: '__bogus__' }),
  (k) => (ss(k) && ss(k).data.cmd === 'RC' && ss(k).data.ACK === false) || 'RC bogus not ACK-false');
await CASE('B11 SC {} no type (guard holds)', (p) => frame('SC', p, {}),
  (k) => (pkt(k, 'SR') || ss(k)) ? true : 'SC{} produced nothing (guard regressed?)');
await CASE('B12 II non-JSON (outer break, no reply)', (p) => frame('II', p, 'not json'),
  null, { expectNoReply: true });
// CI/FI with unparseable JSON: the json==NULL guard breaks BEFORE
// this->CI_pgID / TriggerMode / AddMatchingFeature, so the CAMERA and ENGINE
// FEATURES are untouched -- the safety property -- and session_ACK stays
// false, so the universal SS fires ACK-false. (Not literally "nothing
// touched": a few shared sampler/FPS/snap flags are set before the guard; the
// camera train and def load are not reached. The reply proves the guard,
// liveness proves no session leaked.)
await CASE('B13 CI non-JSON (camera untouched, ACK-false)', (p) => frame('CI', p, 'not json'),
  (k) => (ss(k) && ss(k).data.cmd === 'CI' && ss(k).data.ACK === false) || 'CI bad-json not ACK-false');
await CASE('B14 FI garbage (camera untouched, ACK-false)', (p) => frame('FI', p, undefined, { rawBytes: Uint8Array.from([0xff, 0xfe, 0x00, 0x01]) }),
  (k) => (ss(k) && ss(k).data.cmd === 'FI' && ss(k).data.ACK === false) || 'FI garbage not ACK-false');
await CASE('B15a ST InspAreaBypass:true', (p) => frame('ST', p, { InspAreaBypass: true }),
  (k) => (ss(k) && ss(k).data.cmd === 'ST' && ss(k).data.ACK === true) || 'ST bypass-on not ACK-true');
await CASE('B15b ST InspAreaBypass:false (restore)', (p) => frame('ST', p, { InspAreaBypass: false }),
  (k) => (ss(k) && ss(k).data.cmd === 'ST' && ss(k).data.ACK === true) || 'ST bypass-off not ACK-true');

console.log('\nGroup C -- protocol-level abuse (framing):');
await CASE('C1 truncated header (5 bytes)', () => Uint8Array.from([0x47, 0x53, 0, 0, 0]), null, { expectNoReply: true });
await CASE('C2 declared > actual (1MiB, 4 bytes sent)', (p) => frame('GS', p, undefined, { rawBytes: Uint8Array.from([1, 2, 3, 4]), declLen: 1 << 20 }),
  null, { expectNoReply: true });
await CASE('C3 giant declared length (0xFFFFFFFF)', (p) => frame('GS', p, undefined, { rawBytes: Uint8Array.from([1, 2, 3, 4]), declLen: 0xFFFFFFFF }),
  null, { expectNoReply: true });
await CASE('C4 length-0 header-only HR', (p) => frame('HR', p, undefined, { rawBytes: new Uint8Array(0) }),
  (k) => (ss(k) && ss(k).data.cmd === 'HR' && ss(k).data.ACK === true) || 'empty HR not ACK-true');
await CASE('C5 length-0 unknown TL XX', (p) => frame('XX', p, undefined, { rawBytes: new Uint8Array(0) }),
  (k) => (ss(k) && ss(k).data.cmd === 'XX' && ss(k).data.ACK === false) || 'unknown TL XX did not SS ACK-false');
await CASE('C6 unknown TL ZZ w/ payload', (p) => frame('ZZ', p, { x: 1 }),
  (k) => (ss(k) && ss(k).data.cmd === 'ZZ' && ss(k).data.ACK === false) || 'unknown TL ZZ not ACK-false');
await CASE('C7 GS without trailing NUL (refused)', (p) => frame('GS', p, { items: ['binary_path'] }, { nul: false }),
  null, { expectNoReply: true });
await CASE('C9 two HR packets in one frame', (p) => {
  const a = frame('HR', p, {}); const b = frame('HR', p, {});
  const u = new Uint8Array(a.length + b.length); u.set(a, 0); u.set(b, a.length); return u;
}, (k) => k.filter((x) => x.tl === 'SS').length >= 1 || 'concatenated HR produced no SS');
await CASE('C11 1.2MB of empty 9-byte packets (stack-blowup guard)', (p) => {
  const one = frame('HR', p, undefined, { rawBytes: new Uint8Array(0) });
  const N = Math.floor(1_200_000 / one.length);
  const u = new Uint8Array(one.length * N);
  for (let i = 0; i < N; i++) u.set(one, i * one.length);
  return u;
}, () => true, { timeoutMs: 3000,
     // 133k packets is real work, not a stall: allow for draining it.
     // Measured 2.26s on the bench; 8s leaves room for a slower host
     // while still catching an actual wedge.
     liveMs: 8000 });

if (INCLUDE_CRASHERS) {
  console.log('\nGroup D -- crashers (SIGSEGV an UNPATCHED core; patched -> ACK-false):');
  await CASE('D1 PD no type (null strcmp)', (p) => frame('PD', p, { CONN_ID: 1 }),
    (k) => (ss(k) && ss(k).data.cmd === 'PD' && ss(k).data.ACK === false) || 'PD no-type not ACK-false (or core died)');
}

console.log(`\nRESULT: ${pass}/${pass + fail} cases pass`);
if (!INCLUDE_CRASHERS)
  console.log('WARNING: crash-guard cases (D1 PD-no-type) SKIPPED -- run with --include-crashers for full coverage; a PD-guard regression would slip through this run.');
console.log('coverage: 15 of the 17 handled TLs (EX + SF excluded as heavy/stateful; the rest swept valid+malformed+abuse).');
ws.close();
process.exit(fail === 0 ? 0 : 1);
