// Sweep every device command in three shapes and assert the three things a
// command surface must always do.
//
//   node cmd_sweep.mjs [--port 4099] [--phase 1|2|3] [--only cmd]
//
// THE THREE ASSERTIONS
//
//   1. Something comes back. A command that is silently swallowed is worse
//      than one that fails: the caller waits, then guesses.
//   2. The reply carries `ack`, and `ack` agrees with what actually happened.
//   3. Afterwards `get_running_stat` still answers and SYSTIME has not gone
//      backwards -- i.e. the board neither wedged nor rebooted.
//
// WHY THREE SHAPES
//
// Valid input only proves the happy path, which is the path that already
// works. The interesting answers are at "you forgot an argument" and "your
// argument is the wrong type", because that is where a hand-written parser
// either returns a clear refusal or quietly does half the job.
//
// `get_schema` cannot generate the argument table -- it documents `io_on_level`
// and says "every other key in get_setup" and nothing about the ~70 commands.
// So phase 1 sends every command BARE and lets the device's own refusals say
// what each one wants. That is slower to read than a schema but it is the
// device's actual contract rather than a document about it.
//
// THE CLASS THIS IS REALLY HUNTING
//
// `{"type":"plate","freq":15000}` parses, is acked, and does nothing: the real
// key is `set_setup` -> `plate` -> `freq`. A setting that did not take looks
// exactly like one that did. That cost half a day on 2026-08-19 and produced a
// batch of timing data taken at the wrong plate speed. Phase 3 walks the whole
// get_setup tree and checks, for every group, whether the flat spelling is
// accepted-and-ignored.
//
// EXCLUDED, deliberately: reboot_bootloader, save_setup, clear_saved_setup
// (NVS -- this machine's is good and is active-low), crash_test, wdt_test
// (destructive; they belong to A4).
import net from 'node:net';

const argv = process.argv.slice(2);
const num = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? Number(argv[i + 1]) : d; };
const str = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? argv[i + 1] : d; };
const PORT = num('port', 4099);
const PHASE = num('phase', 0);
const ONLY = str('only', null);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// The REAL command surface, extracted from the firmware's own dispatch:
//
//   grep -oP 'strcmp\(type,\s*"\K[a-z0-9_]+' src/app/LegacyFirmware.cpp | sort -u
//
// 56 types. The 70-name list in the bare-board plan was wrong: it mixed in
// config GROUP names (plate, gate, cam, skip_policy, io_on_level,
// stage_pulse_offset/width_us/center) and enum VALUES (center, centre,
// slow_only, stop_only, slow_and_stop, none, alt, mode, abort, prbs). Those are
// not commands, and asking the device proved it -- see UNKNOWN below.
const CMDS = [
  'aux_test', 'clear_error', 'clear_error_history', 'clear_verdict_log',
  'comm_lost_backup', 'enter_insp_mode', 'enter_insp_test_mode',
  'exit_insp_mode', 'fault', 'get_backup_stat', 'get_running_stat',
  'get_schema', 'get_sel1_cd', 'get_setup', 'get_spikes', 'get_state_names',
  'get_verdict_log', 'get_version', 'get_width_hist', 'io_trace_arm',
  'io_trace_dump', 'io_trace_stop', 'jog', 'jog_arm', 'jog_end', 'light',
  'pin_mode', 'pin_off', 'pin_on', 'pin_read', 'ping', 'poll', 'pushlog',
  'report', 'reset_latency_stat', 'reset_running_stat', 'sel_act',
  'set_dry_run', 'set_gate_disable', 'set_sel1_cd', 'set_setup',
  'stepper_disable', 'stepper_enable', 'trig_cam_burst', 'trig_cam_pulse',
  'trig_phantom_pulse', 'trig_phantom_train', 'trig_report', 'virt_pulse',
];
// Left out on purpose: reboot_bootloader / save_setup / clear_saved_setup
// (NVS), crash_test / wdt_test (destructive -- A4), bye (ends the dialogue this
// sweep runs over), rsp_ (a reply prefix, not a command), RESET (the framing
// layer's own packet).

// Names that LOOK like commands and are not. Every one of these is silently
// swallowed -- no reply, no ack:false, nothing -- which is the whole mechanism
// behind the plate.freq trap.
const UNKNOWN = [
  'plate', 'gate', 'cam', 'skip_policy', 'io_on_level', 'stage_pulse_offset',
  'stage_pulse_width_us', 'stage_pulse_center', 'abort', 'alt', 'mode', 'none',
  'prbs', 'center', 'centre', 'slow_only', 'stop_only', 'slow_and_stop',
  'get_versionn',                      // a one-character typo of a real command
  'definitely_not_a_command',
];

// ---------------------------------------------------------------------------

const sock = net.connect(PORT, '127.0.0.1');
let buf = '';
const waiters = new Map();          // id -> resolve
let lastSystime = 0, sawStat = null, versionWaiter = null;

sock.on('data', (d) => {
  buf += d.toString('latin1');
  let nl;
  while ((nl = buf.indexOf('\n')) >= 0) {
    const line = buf.slice(0, nl).trim();
    buf = buf.slice(nl + 1);
    if (!line || line[0] !== '{') continue;
    let j; try { j = JSON.parse(line); } catch { continue; }
    if (typeof j.dbg === 'string') {
      const m = /SYSTIME: (\d+) ms/.exec(j.dbg);
      if (m) lastSystime = Number(m[1]);
    }
    if (j.health) sawStat = j;
    if (j.id != null && waiters.has(j.id)) { waiters.get(j.id)(j); waiters.delete(j.id); continue; }
    // get_version is answered by rsp_JsonRaw_version() on the FRAMING layer,
    // which stamps its own id and drops the caller's. Nothing else on this
    // surface does that, and a client correlating on id waits for a reply that
    // is already sitting in its buffer. Match it by type instead.
    if (j.type === 'rsp_JsonRaw_version' && versionWaiter) { versionWaiter(j); versionWaiter = null; }
  }
});
sock.on('error', (e) => { console.error(`console ${PORT}: ${e.message}`); process.exit(1); });
await new Promise((r) => sock.once('connect', r));

let nextId = 1000;
// Every probe carries an id and is matched on the echoed id. Without that,
// a reply cannot be told from the unprompted SYSTIME/pong background and
// "did it answer?" degenerates into "did anything arrive?".
async function ask(obj, timeoutMs = 2500) {
  const id = nextId++;
  if (obj.type === 'get_version') {
    const t0 = Date.now();
    const p = new Promise((r) => { versionWaiter = r; setTimeout(() => { if (versionWaiter) { versionWaiter = null; r(null); } }, timeoutMs); });
    sock.write(JSON.stringify({ ...obj, id }) + String.fromCharCode(10));
    const reply = await p;
    return { reply: reply ? { ...reply, ack: undefined, _offChannel: true } : null, ms: Date.now() - t0 };
  }
  const p = new Promise((r) => {
    waiters.set(id, r);
    setTimeout(() => { if (waiters.has(id)) { waiters.delete(id); r(null); } }, timeoutMs);
  });
  const t0 = Date.now();
  sock.write(JSON.stringify({ ...obj, id }) + '\n');
  const reply = await p;
  return { reply, ms: Date.now() - t0 };
}

async function health(tag) {
  sawStat = null;
  const before = lastSystime;
  const { reply } = await ask({ type: 'get_running_stat' }, 4000);
  if (!reply) return { ok: false, why: `no get_running_stat reply after ${tag}` };
  await sleep(1200);                       // let one unprompted SYSTIME land
  if (lastSystime && before && lastSystime < before)
    return { ok: false, why: `SYSTIME went backwards ${before} -> ${lastSystime}: rebooted` };
  return { ok: true, state: reply.state, errHist: (reply.error_hist || []).join(',') };
}

const rows = [];
function row(cmd, shape, r) {
  rows.push({ cmd, shape, ...r });
}

// --- phase 1: every command bare -------------------------------------------
async function phase1() {
  console.log('\n=== phase 1: bare  {"type":cmd}  ===');
  console.log('cmd                     ms  ack    reply');
  for (const cmd of CMDS) {
    if (ONLY && cmd !== ONLY) continue;
    const { reply, ms } = await ask({ type: cmd });
    const ackv = reply ? reply.ack : undefined;
    const summary = reply
      ? JSON.stringify(reply).replace(/,"id":\d+/, '').slice(0, 90)
      : 'NO REPLY';
    row(cmd, 'bare', { ms, replied: !!reply, ack: ackv, summary });
    console.log(`${cmd.padEnd(22)} ${String(ms).padStart(4)}  ${String(ackv).padEnd(6)} ${summary}`);
    // Commands that move the state machine get the board put back afterwards
    // by the caller; the health gate here only asks whether it is still alive.
    const h = await health(cmd);
    if (!h.ok) { console.log(`   !! ${h.why}`); row(cmd, 'health', { fail: h.why }); break; }
    await sleep(120);
  }
}

async function phaseUnknown() {
  console.log(String.fromCharCode(10) + '=== phase 1b: names that are NOT commands ===');
  let silent = 0;
  for (const cmd of UNKNOWN) {
    const { reply, ms } = await ask({ type: cmd }, 1500);
    if (!reply) silent++;
    row(cmd, 'unknown', { ms, replied: !!reply, ack: reply ? reply.ack : undefined,
                          summary: reply ? JSON.stringify(reply).slice(0, 80) : 'silence' });
  }
  console.log(`${silent}/${UNKNOWN.length} unknown types produced NO REPLY AT ALL.`);
  // Only when it is still true. The paragraph below describes defect 1.3,
  // fixed in firmware on 2026-08-21; printing it under a "0/20" line states
  // the opposite of what the run just measured, and a report that argues with
  // its own numbers is worse than one that says nothing.
  if (silent > 0) {
    console.log('An unknown type is indistinguishable from a dead board. This is');
    console.log('the plate.freq trap: {"type":"plate","freq":N} is a typo the');
    console.log('device does not have any way to tell you about.');
  } else {
    console.log('Every unknown type answered -- a typo is now distinguishable');
    console.log('from a dead board (defect 1.3, fixed in firmware 2026-08-21).');
  }
}

// --- phase 2: wrong types ---------------------------------------------------
// One junk object covering the argument names the surface actually uses, so a
// command that reads any of them gets a string where it wants a number and a
// number where it wants a bool. A parser that coerces instead of refusing
// shows up as ack:true with nothing changed.
const JUNK = { on: 'yes', n: 'many', freq: 'fast', idx: 'one', level: 'high',
               pin: 'D13', us: 'soon', ms: [], mode: 7, width: {}, count: -1,
               value: null, enable: 'maybe', ch: 'A', cat: 'three' };
async function phase2() {
  console.log('\n=== phase 2: wrong types ===');
  console.log('cmd                     ms  ack    reply');
  for (const cmd of CMDS) {
    if (ONLY && cmd !== ONLY) continue;
    const { reply, ms } = await ask({ type: cmd, ...JUNK });
    const summary = reply ? JSON.stringify(reply).replace(/,"id":\d+/, '').slice(0, 90) : 'NO REPLY';
    row(cmd, 'wrongtype', { ms, replied: !!reply, ack: reply ? reply.ack : undefined, summary });
    console.log(`${cmd.padEnd(22)} ${String(ms).padStart(4)}  ${String(reply ? reply.ack : undefined).padEnd(6)} ${summary}`);
    const h = await health(cmd);
    if (!h.ok) { console.log(`   !! ${h.why}`); row(cmd, 'health', { fail: h.why }); break; }
    await sleep(120);
  }
}

// --- phase 3: accepted-and-ignored ------------------------------------------
// For every group in get_setup, try the FLAT spelling that looks right and
// isn't. If the device acks it and get_setup is unchanged, that is one more
// instance of the plate.freq trap.
async function phase3() {
  console.log('\n=== phase 3: accepted-and-ignored (the plate.freq class) ===');
  const { reply: setup } = await ask({ type: 'get_setup' }, 4000);
  if (!setup) { console.log('get_setup did not answer -- skipping'); return; }
  const groups = Object.entries(setup).filter(([, v]) => v && typeof v === 'object' && !Array.isArray(v));
  console.log(`${groups.length} nested groups in get_setup\n`);
  console.log('probe                                  ack    changed  verdict');
  for (const [g, obj] of groups) {
    const keys = Object.keys(obj).filter((k) => typeof obj[k] === 'number');
    if (!keys.length) continue;
    const k = keys[0];
    const orig = obj[k];
    // A value that is different but plainly legal: same magnitude, +1.
    const probe = orig + 1;
    // The WRONG spelling: the group name as a command type, the key flat.
    const { reply } = await ask({ type: g, [k]: probe });
    await sleep(400);
    const { reply: after } = await ask({ type: 'get_setup' }, 4000);
    const now = after && after[g] ? after[g][k] : undefined;
    const changed = now === probe;
    const ackv = reply ? reply.ack : undefined;
    let verdict;
    if (!reply) verdict = 'no reply (safe: caller learns nothing worked)';
    else if (ackv === true && !changed) verdict = 'ACCEPTED AND IGNORED';
    else if (ackv === true && changed) verdict = 'took effect (flat form is real)';
    else verdict = 'refused (correct)';
    console.log(`{"type":"${g}","${k}":${probe}}`.padEnd(38) +
                ` ${String(ackv).padEnd(6)} ${String(changed).padEnd(8)} ${verdict}`);
    row(`${g}.${k}`, 'flat', { ack: ackv, changed, verdict });
    if (changed) {                                   // put it back
      await ask({ type: 'set_setup', [g]: { [k]: orig } });
      await sleep(300);
    }
  }
}

// ---------------------------------------------------------------------------
const pre = await health('baseline');
if (!pre.ok) { console.error(`BASELINE FAILED: ${pre.why}`); process.exit(1); }
console.log(`baseline: state=${pre.state} error_hist=[${pre.errHist}] SYSTIME=${lastSystime}`);

if (!PHASE || PHASE === 1) { await phase1(); await phaseUnknown(); }
if (!PHASE || PHASE === 2) await phase2();
if (!PHASE || PHASE === 3) await phase3();

console.log('\n---- summary ----');
const noReply = rows.filter((r) => r.replied === false);
const noAck = rows.filter((r) => r.replied && r.ack === undefined);
const ignored = rows.filter((r) => r.verdict === 'ACCEPTED AND IGNORED');
const broke = rows.filter((r) => r.fail);
console.log(`${rows.length} probes`);
console.log(`no reply at all      : ${noReply.length}${noReply.length ? ' -> ' + noReply.map((r) => `${r.cmd}/${r.shape}`).join(' ') : ''}`);
console.log(`replied without ack  : ${noAck.length}${noAck.length ? ' -> ' + noAck.map((r) => `${r.cmd}/${r.shape}`).join(' ') : ''}`);
console.log(`accepted and ignored : ${ignored.length}${ignored.length ? ' -> ' + ignored.map((r) => r.cmd).join(' ') : ''}`);
console.log(`health gate failures : ${broke.length}${broke.length ? ' -> ' + broke.map((r) => r.cmd + ': ' + r.fail).join('; ') : ''}`);
sock.end();
process.exit(broke.length ? 1 : 0);
