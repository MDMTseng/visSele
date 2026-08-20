// Abuse the peripheral dev console's own protocol surface.
//
//   node console_abuse.mjs [--port 4099] [--only caseName] [--skip-risky]
//                          [--no-latch]      # skip every case that stops the machine
//
// WHY THIS TEST EXISTS
//
// The console (wiringPanel.cpp, PerifConsoleThread) was ported to Windows on
// 2026-08-19 and until now only its happy path had been exercised: send a line
// of JSON, read the reply. The port itself already produced one real bug --
// the socket was made non-blocking so the ECHO path could not stall the
// peripheral RX thread, and the READ path then saw EWOULDBLOCK on every idle
// client and treated it as a disconnect, kicking everyone who stopped typing.
// A defect found while writing the code is rarely the only one in it.
//
// What makes this surface worth abusing rather than trusting: every byte here
// is one hop from a machine that moves physical parts. The console forwards
// verbatim to a device whose parser LATCHES on bytes it cannot frame -- one
// stray frame and the machine stops. The code knows this and carries a guard
// that rejects non-JSON, but the guard, the line assembler, the accept loop
// and the echo path are all hand-written C against a socket API that differs
// between the two platforms this file supports.
//
// EVERY CASE ENDS THE SAME WAY: a FRESH connection must answer
// get_running_stat, and the device's SYSTIME must not go backwards (a reboot).
// The gate waits long enough to see the core's link RESYNC recover a latched
// device, and REPORTS how long that took -- because "it came back" and "it
// came back in nine seconds with the plate turning" are different facts.
import net from 'node:net';

const argv = process.argv.slice(2);
const num = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? Number(argv[i + 1]) : d; };
const str = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? argv[i + 1] : d; };
const PORT = num('port', 4099);
const ONLY = str('only', null);
const SKIP_RISKY = argv.includes('--skip-risky');
const NO_LATCH = argv.includes('--no-latch');

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// A connection that keeps every byte, so a case can look at the wire and not
// at a parsed view of it. One of the findings below is only visible in bytes.
function open() {
  const c = { chunks: [], closed: false, err: null };
  c.sock = net.connect(PORT, '127.0.0.1');
  c.sock.on('data', (d) => c.chunks.push(d));
  c.sock.on('close', () => { c.closed = true; });
  c.sock.on('error', (e) => { c.err = e.message; c.closed = true; });
  c.raw = () => Buffer.concat(c.chunks);
  c.text = () => c.raw().toString('latin1');
  c.send = (s) => c.sock.write(s);
  c.sendJson = (o) => c.sock.write(JSON.stringify(o) + '\n');
  c.end = () => { try { c.sock.destroy(); } catch {} };
  return c;
}

async function waitConnect(c, ms = 3000) {
  const t0 = Date.now();
  while (!c.sock.readyState || c.sock.readyState === 'opening') {
    if (c.err) return false;
    if (Date.now() - t0 > ms) return false;
    await sleep(20);
  }
  return true;
}

// The device prints SYSTIME once a second unprompted. It is the cheapest
// reboot detector there is: it only ever goes up, and a reboot resets it.
function systimeOf(text) {
  let last = null, m;
  const re = /SYSTIME: (\d+) ms/g;
  while ((m = re.exec(text)) !== null) last = Number(m[1]);
  return last;
}
function errorHistOf(text) {
  let last = null, m;
  const re = /"error_hist":\[([0-9,\s]*)\]/g;
  while ((m = re.exec(text)) !== null) last = m[1].trim();
  return last;
}
function stateOf(text) {
  let last = null, m;
  const re = /"state":(\d+)/g;
  while ((m = re.exec(text)) !== null) last = Number(m[1]);
  return last;
}

let lastSystime = 0;
let lastErrHist = null;

// The liveness gate. Deliberately a NEW connection every time: if a case
// leaked the client slot or wedged the accept loop, this is what notices.
// The wait is long because a latched device is recovered by the core's link
// RESYNC, not instantly -- and that recovery latency is data, not noise.
async function liveness(tag, waitMs = 30000) {
  const t0 = Date.now();
  const c = open();
  if (!(await waitConnect(c))) { c.end(); return { ok: false, why: `connect failed: ${c.err}` }; }
  let stat = false;
  while (Date.now() - t0 < waitMs) {
    c.sendJson({ type: 'get_running_stat' });
    const tq = Date.now();
    while (Date.now() - tq < 1500) {
      await sleep(50);
      if (/"health"/.test(c.text())) { stat = true; break; }
    }
    if (stat) break;
  }
  const ms = Date.now() - t0;
  let st = systimeOf(c.text());
  const tw = Date.now();
  while (st === null && Date.now() - tw < 2500) { await sleep(100); st = systimeOf(c.text()); }
  const eh = errorHistOf(c.text());
  const state = stateOf(c.text());
  c.end();
  if (!stat) return { ok: false, why: `no get_running_stat reply in ${(waitMs / 1000) | 0}s (${tag})`, ms };
  if (st !== null) {
    if (st < lastSystime) { const p = lastSystime; lastSystime = st; return { ok: false, why: `SYSTIME went BACKWARDS ${p} -> ${st}: the board rebooted`, ms }; }
    lastSystime = st;
  }
  const newErr = eh !== null && eh !== lastErrHist;
  const prevErr = lastErrHist;
  if (eh !== null) lastErrHist = eh;
  return { ok: true, ms, systime: st, state, errHist: eh, newErr, prevErr };
}

const results = [];
function record(name, verdict, note) {
  results.push({ name, verdict, note });
  console.log(`  ${verdict.padEnd(7)} ${name}${note ? ' -- ' + note : ''}`);
}

// ---------------------------------------------------------------------------
// CASES
// ---------------------------------------------------------------------------

// The non-JSON guard writes a fixed string literal with a HARDCODED byte
// count. If that count is not the literal's length, the client is sent either
// a truncated line (no newline: a line-oriented reader hangs) or bytes from
// past the end of the string. Only the wire can say which, so read the wire --
// INCLUDING what follows the newline, which is where an over-count lands.
async function caseNonJson() {
  const c = open();
  if (!(await waitConnect(c))) { c.end(); return record('nonjson_reply', 'FAIL', 'connect failed'); }
  c.send('hello\n');
  await sleep(1500);
  const raw = c.raw();
  c.end();
  const i = raw.indexOf(Buffer.from('{"err":"not JSON'));
  if (i < 0) return record('nonjson_reply', 'FAIL', 'no error reply to a non-JSON line');
  const nl = raw.indexOf(0x0a, i);
  if (nl < 0) return record('nonjson_reply', 'FAIL', 'error reply never terminated by a newline');
  const lineLen = nl - i + 1;
  // Anything the console appended after that newline in the SAME write is
  // over-count: bytes read from past the end of the literal.
  const after = raw.slice(nl + 1, nl + 1 + 16);
  const nul = after.indexOf(0x00);
  if (nul === 0) {
    const junk = after.slice(0, 16);
    return record('nonjson_reply', 'FINDING',
      `literal is ${lineLen} bytes; a NUL and ${junk.length - 1}+ bytes follow it in the same write ` +
      `(hex ${junk.toString('hex')}) -- the hardcoded length overruns the string literal`);
  }
  record('nonjson_reply', 'PASS', `reply ${lineLen} bytes, nothing appended after the newline`);
}

// The line assembler stops appending at 4096 and forwards what it has when the
// newline arrives. The DEVICE's frame buffer is 2048 (Data_Layer_Protocol.hpp,
// sized "to comfortably hold the largest legitimate command"). So the console
// will accept, and put on the wire, lines up to twice what the device can
// frame. Bisect for the real threshold rather than assuming either number.
async function caseLatchThreshold() {
  const mk = (n) => {
    const head = '{"type":"get_running_stat","pad":"', tail = '"}';
    return head + 'A'.repeat(Math.max(0, n - head.length - tail.length)) + tail;
  };
  // Below the device buffer, at it, and above it. Each probe that latches
  // costs one machine stop plus the RESYNC recovery, so keep the list short.
  const probes = [1500, 2040, 2100];
  const seen = [];
  for (const n of probes) {
    const c = open();
    if (!(await waitConnect(c))) { c.end(); return record('latch_threshold', 'FAIL', 'connect failed'); }
    c.send(mk(n) + '\n');
    await sleep(2500);
    const answered = /"health"/.test(c.text());
    c.end();
    const lv = await liveness(`line ${n}`);
    const latched = !answered && lv.newErr && /(^|,)11(,|$)/.test(lv.errHist || '');
    seen.push({ n, answered, latched, recoverMs: lv.ms, errHist: lv.errHist, ok: lv.ok });
    console.log(`         ${n} bytes: device answered=${answered}, error_hist=${lv.errHist}, ` +
      `next stat took ${lv.ms}ms, state=${lv.state}`);
    if (!lv.ok) { record('latch_threshold', 'FAIL', `did not recover after ${n} bytes: ${lv.why}`); return; }
  }
  const firstLatch = seen.find((s) => s.latched);
  if (!firstLatch) return record('latch_threshold', 'INFO', `no probe latched: ${seen.map((s) => s.n).join(',')}`);
  record('latch_threshold', 'FINDING',
    `a ${firstLatch.n}-byte console line latches the device parser ` +
    `(SERIAL_PROTOCOL_ERROR 11) and stops the machine; the core's link RESYNC ` +
    `recovered it in ~${(firstLatch.recoverMs / 1000).toFixed(1)}s. The console caps lines at ` +
    `4096, the device frame buffer is 2048 -- the console accepts and transmits ` +
    `lines it knows the device cannot frame.`);
}

// Half a line, then gone. The assembler holds the partial in a std::string
// scoped to the connection; nothing should reach the device.
async function casePartialClose() {
  const c = open();
  if (!(await waitConnect(c))) { c.end(); return record('partial_then_close', 'FAIL', 'connect failed'); }
  c.send('{"type":"get_run');
  await sleep(300);
  c.end();
  await sleep(500);
  record('partial_then_close', 'INFO', 'half a line, no newline, then closed');
}

// A whole command with no terminator. Must NOT reach the device: an unframed
// object is exactly what the device parser latches on.
async function caseNoNewline() {
  const c = open();
  if (!(await waitConnect(c))) { c.end(); return record('no_newline_close', 'FAIL', 'connect failed'); }
  c.send(JSON.stringify({ type: 'get_version' }));
  await sleep(1000);
  const sawVersion = /"version"|"fw"/.test(c.text());
  c.end();
  record('no_newline_close', sawVersion ? 'FINDING' : 'PASS',
    sawVersion ? 'an unterminated line reached the device anyway' : 'nothing forwarded, as intended');
}

// The accept loop is single-threaded: accept(), then an inner read loop that
// runs until THAT client leaves. A second client therefore is not "the new
// client" -- it sits in the listen backlog, fully connected as far as TCP and
// the operator are concerned, and nothing it sends is read until the first
// client goes. The code comment says the opposite ("one client at a time", old
// client closed on accept); that branch cannot be reached from here.
async function caseSecondClient() {
  const a = open();
  if (!(await waitConnect(a))) { a.end(); return record('second_client', 'FAIL', 'client A connect failed'); }
  a.sendJson({ type: 'ping' });
  await sleep(800);
  const aAlive = /"pong"/.test(a.text());

  const b = open();
  const bConnected = await waitConnect(b);
  b.sendJson({ type: 'get_running_stat' });
  await sleep(2500);
  const whileAHeld = b.raw().length;

  a.end();                       // let A go: B's queued bytes are read NOW
  await sleep(3000);
  const afterALeft = b.raw().length;
  const bLate = /"health"/.test(b.text());
  b.end();

  const note = `A attached=${aAlive}; B connected=${bConnected}, bytes while A held=${whileAHeld}, ` +
    `after A left=${afterALeft}, B's command executed late=${bLate}`;
  record('second_client', whileAHeld === 0 && bLate ? 'FINDING' : 'INFO', note);
}

// '!TL' injects a BPG packet with an arbitrary two-letter type straight into
// the core's upper layer. Nothing validates TL.
async function caseUnknownTl() {
  const c = open();
  if (!(await waitConnect(c))) { c.end(); return record('unknown_tl', 'FAIL', 'connect failed'); }
  c.send('!xx {"hello":1}\n');
  await sleep(1200);
  const acked = /"core":"XX injected"/.test(c.text());
  c.end();
  record('unknown_tl', 'INFO', `unknown two-letter type accepted and acked=${acked}`);
}

// '!TL' needs size>4 AND line[3]==' '. Probe the edges of that test, including
// the ones that decide whether a malformed injection falls through to the
// device instead of being rejected.
async function caseTlEdges() {
  const c = open();
  if (!(await waitConnect(c))) { c.end(); return record('tl_edges', 'FAIL', 'connect failed'); }
  const probes = ['!pd', '!pd ', '!p {}', '!pdx{}', '!!! {}'];
  for (const p of probes) { c.send(p + '\n'); await sleep(500); }
  await sleep(800);
  const t = c.text();
  c.end();
  record('tl_edges', 'INFO',
    `${probes.length} edge probes: injected=${(t.match(/injected/g) || []).length}, ` +
    `rejected_as_not_json=${(t.match(/not JSON/g) || []).length}`);
}

// The slow reader. This is the trade the code documents: a client that does
// not read must lose ECHO LINES and must NOT stall the peripheral RX thread.
// The failure being tested for actually happened once and wedged the core
// mid-experiment, which is why the non-blocking write exists at all.
async function caseSlowReader(seconds = 20) {
  const c = open();
  if (!(await waitConnect(c))) { c.end(); return record('slow_reader', 'FAIL', 'connect failed'); }
  c.sock.pause();                       // never read a byte from here on
  const t0 = Date.now();
  let sent = 0;
  while (Date.now() - t0 < seconds * 1000) {   // ~3.7kB of echo per request
    c.sendJson({ type: 'get_running_stat' });
    sent++;
    await sleep(200);
  }
  const heldMs = Date.now() - t0;
  c.end();
  await sleep(400);
  const lv = await liveness('after slow reader');
  record('slow_reader', lv.ok ? 'PASS' : 'FAIL',
    `held ${(heldMs / 1000).toFixed(0)}s without reading, ${sent} stat requests; ` +
    (lv.ok ? `a fresh client got a stat in ${lv.ms}ms` : lv.why));
}

// RISKY: malformed payload behind '!pd'. The PD handler owns the peripheral
// channel; a payload that tears it down costs a CONNECT to restore, and a
// CONNECT toggles DTR and reboots the board.
async function casePdBad() {
  const c = open();
  if (!(await waitConnect(c))) { c.end(); return record('pd_bad_payload', 'FAIL', 'connect failed'); }
  c.send('!pd {oops\n');
  await sleep(1500);
  c.send('!pd {}\n');
  await sleep(1500);
  const t = c.text();
  c.end();
  record('pd_bad_payload', 'INFO', `acks seen: ${(t.match(/injected/g) || []).length}`);
}

const CASES = [
  ['nonjson_reply', caseNonJson],
  ['partial_then_close', casePartialClose],
  ['no_newline_close', caseNoNewline],
  ['second_client', caseSecondClient],
  ['unknown_tl', caseUnknownTl],
  ['tl_edges', caseTlEdges],
  ['slow_reader', () => caseSlowReader(20)],
  ['latch_threshold', caseLatchThreshold, { latch: true }],
  ['pd_bad_payload', casePdBad, { risky: true }],
];

(async () => {
  console.log(`console_abuse -- 127.0.0.1:${PORT}`);
  const pre = await liveness('baseline');
  if (!pre.ok) { console.error(`BASELINE FAILED: ${pre.why}`); process.exit(1); }
  console.log(`  baseline OK, stat in ${pre.ms}ms, SYSTIME ${pre.systime}, state ${pre.state}, error_hist [${pre.errHist}]\n`);

  for (const [name, fn, flags = {}] of CASES) {
    if (ONLY && name !== ONLY) continue;
    if (flags.risky && SKIP_RISKY) { console.log(`  SKIP    ${name} (risky)`); continue; }
    if (flags.latch && NO_LATCH) { console.log(`  SKIP    ${name} (stops the machine)`); continue; }
    try { await fn(); } catch (e) { record(name, 'FAIL', `threw: ${e.message}`); }
    // These two run their own gates inside.
    if (name !== 'slow_reader' && name !== 'latch_threshold') {
      const lv = await liveness(name);
      if (!lv.ok) {
        console.error(`  HALT    liveness gate failed after ${name}: ${lv.why}`);
        results.push({ name: `${name}:liveness`, verdict: 'FAIL', note: lv.why });
        break;
      }
      if (lv.newErr) console.log(`         note: error_hist changed [${lv.prevErr}] -> [${lv.errHist}]`);
    }
    await sleep(300);
  }

  console.log('\n---- summary ----');
  for (const r of results) console.log(`${r.verdict.padEnd(8)} ${r.name}${r.note ? ' -- ' + r.note : ''}`);
  const bad = results.filter((r) => r.verdict === 'FAIL');
  const finds = results.filter((r) => r.verdict === 'FINDING');
  console.log(`\n${results.length} cases, ${bad.length} FAIL, ${finds.length} FINDING`);
  process.exit(bad.length ? 1 : 0);
})();
