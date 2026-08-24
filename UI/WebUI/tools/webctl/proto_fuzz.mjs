// Protocol-shape fuzz against the REAL board, through the dev console.
//
//   node proto_fuzz.mjs [--port 4099] [--group json|line|all]
//
// The host-side fuzz (Peripheral/uInspESP32/tools/test_data_layer_overflow.cpp,
// 400 trials x 6000 bytes) exercises the data layer with no device attached.
// This is the other half: the same class of input arriving at a board that is
// in INSPECTION_MODE_READY with the plate turning.
//
// It is worth doing because the one shape already tried here found a P1: an
// empty `[]` or `{}` anywhere in a frame latches the device parser and the
// machine accepts no further command while still emitting SYSTIME (see
// CONSOLE_ABUSE_2026-08-19.md A1-1). Every probe below is a different way of
// asking "and what else is the parser wrong about".
//
// RECOVERY IS PART OF THE TEST. A latched board is recovered with clear_error,
// and whether that worked is recorded per probe -- "it latched" and "it latched
// and stayed latched" are very different answers. A probe that cannot be
// recovered stops the run rather than poisoning every probe after it.
//
// TWO GROUPS, because they are two different parsers:
//   json -- shapes the DEVICE's json_seg_parser has to survive
//   line -- shapes the CONSOLE's line assembler has to survive before the
//           device ever sees them (NUL truncation, fragmented writes)
import net from 'node:net';

const argv = process.argv.slice(2);
const num = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? Number(argv[i + 1]) : d; };
const str = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? argv[i + 1] : d; };
const PORT = num('port', 4099);
const GROUP = str('group', 'all');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const s = net.connect(PORT, '127.0.0.1');
let buf = '', lines = [];
s.on('data', (d) => {
  buf += d.toString('latin1');
  let nl;
  while ((nl = buf.indexOf('\n')) >= 0) { lines.push(buf.slice(0, nl)); buf = buf.slice(nl + 1); }
});
s.on('error', (e) => { console.error(`console ${PORT}: ${e.message}`); process.exit(1); });
await new Promise((r) => s.once('connect', r));

let id = 20000;
async function answered(obj, ms = 1500) {
  const myId = id++;
  lines = [];
  s.write(JSON.stringify({ ...obj, id: myId }) + '\n');
  await sleep(ms);
  return lines.some((l) => l.includes(`"id":${myId}`));
}
const healthy = () => answered({ type: 'get_running_stat' }, 2000);
// Recovery is not instant and the old version assumed it was: ONE clear_error,
// 1.5s, one health check -- then "UNRECOVERABLE", which aborts the whole run.
// On 2026-08-21 that verdict fired on `trailing garbage` and stopped the run at
// probe 6 of 18; checking the board by hand straight afterwards found it awake
// and answering. A wrong "the board did not come back" is the kind of
// conclusion that sends someone to pull the power.
//
// So: retry, and REPORT HOW LONG IT TOOK. Recovery latency is data -- "it
// latched and came back in 1.4s" and "it latched and needed 11s" are different
// answers about the same probe, and the old shape could express neither.
const RECOVER_TRIES = 6;
async function recover() {
  const t0 = Date.now();
  for (let i = 1; i <= RECOVER_TRIES; i++) {
    s.write(JSON.stringify({ type: 'clear_error' }) + '\n');
    await sleep(1200);
    if (await healthy()) return { ok: true, ms: Date.now() - t0, tries: i };
  }
  return { ok: false, ms: Date.now() - t0, tries: RECOVER_TRIES };
}

// A probe is (name, writer). The writer puts bytes on the socket; everything
// else -- health, recovery, bookkeeping -- is the same for all of them.
const JSON_PROBES = [
  ['deep nesting x8',        () => '{"type":"ping","a":' + '{"b":'.repeat(8) + '1' + '}'.repeat(8) + '}'],
  ['deep nesting x20',       () => '{"type":"ping","a":' + '{"b":'.repeat(20) + '1' + '}'.repeat(20) + '}'],
  ['deep nesting x60',       () => '{"type":"ping","a":' + '{"b":'.repeat(60) + '1' + '}'.repeat(60) + '}'],
  ['deep array x60',         () => '{"type":"ping","a":' + '['.repeat(60) + '1' + ']'.repeat(60) + '}'],
  ['unterminated string',    () => '{"type":"ping","a":"no end}'],
  ['trailing garbage',       () => '{"type":"ping"} trailing words'],
  ['two objects one line',   () => '{"type":"ping"}{"type":"ping"}'],
  ['duplicate keys',         () => '{"type":"ping","a":1,"a":2}'],
  ['very long key',          () => '{"type":"ping","' + 'k'.repeat(400) + '":1}'],
  ['very long string value', () => '{"type":"ping","a":"' + 'v'.repeat(900) + '"}'],
  ['unicode escape',         () => '{"type":"ping","a":"\\u00e4\\u4e2d\\u0000"}'],
  ['raw control char',       () => '{"type":"ping","a":"x\x01y"}'],
  ['huge number',            () => '{"type":"ping","a":123456789012345678901234567890}'],
  ['negative zero / exp',    () => '{"type":"ping","a":-0.0e+400}'],
  ['leading comma',          () => '{,"type":"ping"}'],
  ['bare true at top',       () => 'true'],           // caught by the console's JSON guard
  ['array as the frame',     () => '["type","ping"]'],
  ['whitespace flood',       () => '{' + ' '.repeat(500) + '"type":"ping"}'],
];

const LINE_PROBES = [
  // The console assembles bytes into a std::string and forwards it with
  // printfTo_perifCH(..., "%s", line.c_str()). c_str() stops at the first NUL,
  // so a NUL in the middle of a command silently truncates it -- the operator
  // sees the whole line echoed, the device gets half of it.
  ['NUL in the middle',      () => '{"type":"ping","a":1\x00,"b":2}'],
  ['NUL right after brace',  () => '{\x00"type":"ping"}'],
  ['CR inside the line',     () => '{"type":"ping",\r"a":1}'],   // \r is dropped by the assembler
  ['tab and vtab',           () => '{"type":"ping",\t"a":1}'],
];

async function runGroup(title, probes, fragment = false) {
  console.log(`\n=== ${title} ===`);
  console.log('probe                       answered  healthy_after  recovered');
  for (const [name, gen] of probes) {
    lines = [];
    const text = gen();
    if (fragment) {
      // Deliver one byte at a time with a gap, so the console's assembler sees
      // the line as ~N separate reads. The device must still get one frame.
      for (const ch of text) { s.write(ch); await sleep(2); }
      s.write('\n');
    } else {
      s.write(text + '\n');
    }
    await sleep(1200);
    const gotSomething = lines.filter((l) => !/SYSTIME|"pong"|comm_lost_backup|cam_trig/.test(l)).length;
    const ok = await healthy();
    let rec = '';
    if (!ok) {
      const r = await recover();
      rec = r.ok ? `clear_error OK (${(r.ms / 1000).toFixed(1)}s, ${r.tries}x)`
                 : `UNRECOVERABLE (gave up after ${(r.ms / 1000).toFixed(1)}s)`;
    }
    console.log(`${name.padEnd(27)} ${String(gotSomething).padEnd(9)} ${String(ok).padEnd(14)} ${rec}`);
    if (rec.startsWith('UNRECOVERABLE')) {
      console.log(`stopping: the board did not come back after ${RECOVER_TRIES} clear_error attempts`);
      return false;
    }
    await sleep(200);
  }
  return true;
}

if (!(await healthy())) { console.log('board not healthy at start; clearing'); await recover(); }
console.log('start: healthy');

if (GROUP === 'all' || GROUP === 'json') await runGroup('device JSON shapes', JSON_PROBES);
if (GROUP === 'all' || GROUP === 'line') {
  await runGroup('console line assembler', LINE_PROBES);
  // The same valid command, delivered one byte at a time. This is the console's
  // assembler under the delivery pattern a human typing into netcat produces,
  // and the one the non-blocking read path was rewritten for.
  await runGroup('fragmented delivery (1 byte per write)',
    [['valid ping, byte by byte', () => '{"type":"ping","a":1}']], true);
}

console.log(`\nfinal health: ${await healthy()}`);
s.end();
process.exit(0);
