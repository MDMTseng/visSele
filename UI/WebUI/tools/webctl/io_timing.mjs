// Verify that stage_pulse_offset / stage_pulse_width_us actually reach the
// output pin timing -- on a bare board, with no scope.
//
//   node io_timing.mjs [--port 4099] [--parts 30]
//
// WHY THIS MATTERS MORE THAN IT LOOKS
//
// stage_pulse_offset and stage_pulse_width_us decide WHEN each light fires,
// when each camera is triggered, and when each selector valve opens. Getting
// them wrong does not crash anything: it blows a good part down the reject
// chute, or a bad one into the good bin, silently. That whole block has had no
// automated test of any kind.
//
// It was previously assumed untestable without hardware, because every
// IO_TRACE_LOG call site is inside the inspection pipeline and carries
// `task->src->tid`. That is true, and it stopped being an obstacle when the
// bare-board loop closed: a phantom object train puts real tasks through the
// pipeline, the timer ISR schedules the pins exactly as it would for real
// parts, and io_trace_arm/dump reads the result back.
//
// UNITS. Trace events are [pulse, pin, val, tid] where `pulse` is the plate
// step counter, not microseconds. The timer ticks at 2 * plate.freq, so at the
// production freq of 15000 one pulse is 1/30000 s = 33.333us. Both settings are
// expressed in the units their name says -- offsets in pulses, widths in
// microseconds -- and the conversion between them is the thing worth checking.
//
// THE TEST IS THE CHANGE, NOT THE AGREEMENT. Reading the trace and finding it
// matches get_setup proves little: the defaults could agree by construction.
// So this changes a width, re-traces, and requires the trace to follow -- then
// puts it back.
import net from 'node:net';

const argv = process.argv.slice(2);
const num = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? Number(argv[i + 1]) : d; };
const PORT = num('port', 4099);
const PARTS = num('parts', 30);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const PIN = { 0: 'SWITCH', 16: 'L1A', 17: 'CAM1', 18: 'L2A', 19: 'CAM2', 25: 'SEL1', 26: 'SEL2', 32: 'SEL3' };

const s = net.connect(PORT, '127.0.0.1');
let buf = '', lines = [];
s.on('data', (d) => {
  buf += d.toString('latin1');
  let nl;
  while ((nl = buf.indexOf('\n')) >= 0) { lines.push(buf.slice(0, nl)); buf = buf.slice(nl + 1); }
});
s.on('error', (e) => { console.error(`console ${PORT}: ${e.message}`); process.exit(1); });
await new Promise((r) => s.once('connect', r));

let id = 60000;
async function ask(obj, ms = 2500) {
  const myId = id++;
  lines = [];
  s.write(JSON.stringify({ ...obj, id: myId }) + '\n');
  const t0 = Date.now();
  while (Date.now() - t0 < ms) {
    await sleep(50);
    const hit = lines.find((l) => l.includes(`"id":${myId}`));
    if (hit) { try { return JSON.parse(hit); } catch { return null; } }
  }
  return null;
}

// One armed window: run the train, arm, wait, dump.
async function capture(seconds = 5) {
  await ask({ type: 'io_trace_arm' });
  await sleep(seconds * 1000);
  const d = await ask({ type: 'io_trace_dump' }, 4000);
  await ask({ type: 'io_trace_stop' });
  return d && Array.isArray(d.ev) ? d.ev : [];
}

// Pull per-tid edge times out of the raw event list.
function byTid(ev) {
  const m = new Map();
  for (const [pulse, pin, val, tid] of ev) {
    if (!m.has(tid)) m.set(tid, []);
    m.get(tid).push({ pulse, pin, val });
  }
  return m;
}
function widthPulses(edges, pin) {
  const on = edges.find((e) => e.pin === pin && e.val === 1);
  const off = edges.find((e) => e.pin === pin && e.val === 0 && on && e.pulse >= on.pulse);
  return on && off ? off.pulse - on.pulse : null;
}
// The median across parts, so one clipped part at a buffer boundary cannot
// decide the answer.
function median(xs) {
  const v = xs.filter((x) => x != null).sort((a, b) => a - b);
  return v.length ? v[Math.floor(v.length / 2)] : null;
}

const setup = await ask({ type: 'get_setup' }, 4000);
if (!setup) { console.error('get_setup did not answer'); process.exit(1); }
const freq = setup.plate.freq;
const usPerPulse = 1e6 / (2 * freq);
console.log(`plate.freq=${freq}  ->  tick 2*freq = ${2 * freq}/s, 1 pulse = ${usPerPulse.toFixed(3)}us\n`);

console.log(`starting a ${PARTS}-part phantom train at 300ms`);
await ask({ type: 'trig_phantom_train', count: PARTS, period_us: 300000 });
await sleep(1500);

// ---- 1. widths as configured -----------------------------------------------
const ev1 = await capture(5);
if (!ev1.length) { console.error('no trace events -- is the train running and the board in 101?'); process.exit(1); }
console.log(`captured ${ev1.length} events\n`);
const t1 = byTid(ev1);

console.log('pin    configured_us  expected_pulses  measured_pulses  parts  verdict');
const W = setup.stage_pulse_width_us;
for (const [pinNo, name] of Object.entries(PIN)) {
  if (!(name in W)) continue;
  const exp = Math.round(W[name] / usPerPulse);
  const meas = median([...t1.values()].map((e) => widthPulses(e, Number(pinNo))));
  const n = [...t1.values()].filter((e) => widthPulses(e, Number(pinNo)) != null).length;
  if (meas == null) { console.log(`${name.padEnd(6)} ${String(W[name]).padStart(13)}  ${String(exp).padStart(15)}  ${'-'.padStart(15)}  ${String(n).padStart(5)}  not seen`); continue; }
  // One pulse of slack: the scheduler rounds a fractional width up.
  const ok = Math.abs(meas - exp) <= 1;
  console.log(`${name.padEnd(6)} ${String(W[name]).padStart(13)}  ${String(exp).padStart(15)}  ${String(meas).padStart(15)}  ${String(n).padStart(5)}  ${ok ? 'match' : 'MISMATCH'}`);
}

// ---- 2. offsets, as differences within one part ----------------------------
// Absolute offsets are positions in a revolution; what a trace can check is the
// DISTANCE between two stages for the same tid, which is the difference of
// their configured offsets and is what actually decides where a part lands.
console.log('\npair                     configured_delta  measured_delta  parts  verdict');
const O = setup.stage_pulse_offset;
const PAIRS = [
  ['CAM2_on -> CAM1_on', 19, 1, 17, 1, O.CAM1_on - O.CAM2_on],
  ['CAM1_on -> SWITCH',  17, 1, 0, null, O.SWITCH - O.CAM1_on],
  ['SWITCH -> SEL3_off', 0, null, 32, 0, O.SEL3_off - O.SWITCH],
];
for (const [label, pinA, valA, pinB, valB, expected] of PAIRS) {
  const deltas = [];
  for (const edges of t1.values()) {
    const a = edges.find((e) => e.pin === pinA && (valA === null || e.val === valA));
    const b = edges.find((e) => e.pin === pinB && (valB === null || e.val === valB));
    if (a && b) deltas.push(b.pulse - a.pulse);
  }
  const meas = median(deltas);
  const ok = meas != null && Math.abs(meas - expected) <= 1;
  console.log(`${label.padEnd(24)} ${String(expected).padStart(16)}  ${String(meas ?? '-').padStart(14)}  ${String(deltas.length).padStart(5)}  ${ok ? 'match' : meas == null ? 'not seen' : 'MISMATCH'}`);
}

// ---- 3. the actual test: change it and see the trace move ------------------
const origCam1 = W.CAM1;
const halved = Math.round(origCam1 / 2);
console.log(`\nchanging stage_pulse_width_us.CAM1  ${origCam1} -> ${halved}us and re-tracing`);
await ask({ type: 'set_setup', stage_pulse_width_us: { CAM1: halved } });
await sleep(500);
const check = await ask({ type: 'get_setup' }, 4000);
console.log(`  get_setup now reports CAM1 = ${check.stage_pulse_width_us.CAM1}`);
await ask({ type: 'trig_phantom_train', count: PARTS, period_us: 300000 });
await sleep(1500);
const ev2 = await capture(5);
const t2 = byTid(ev2);
const meas2 = median([...t2.values()].map((e) => widthPulses(e, 17)));
const exp2 = Math.round(halved / usPerPulse);
console.log(`  expected ${exp2} pulses, measured ${meas2}  ->  ${meas2 != null && Math.abs(meas2 - exp2) <= 1 ? 'THE SETTING REACHES THE PIN' : 'NO -- the trace did not follow'}`);

console.log(`\nrestoring stage_pulse_width_us.CAM1 = ${origCam1}`);
await ask({ type: 'set_setup', stage_pulse_width_us: { CAM1: origCam1 } });
await sleep(500);
const back = await ask({ type: 'get_setup' }, 4000);
console.log(`  restored: ${back.stage_pulse_width_us.CAM1}`);
await ask({ type: 'trig_phantom_train', count: 0 });     // stop the train
s.end();
process.exit(0);
