// D1 / worklist 3.5 -- does the fail-to-reject threshold actually hold?
//
//   node fw_tolerance.mjs [--port 4099] [--stop-after 5]
//
// UNANSWERED_STOP_AFTER is not a tuning knob, it is the whole safety argument
// for not faulting on the first unjudged part (LegacyFirmware.cpp:3215):
//
//     "One unjudged part is normal loss -- it recirculates and gets another
//      pass, costing a lap. Several in a row is not loss, it is a system that
//      has stopped working."
//
// Nothing tested it. The UNSET arm right below that comment was missing its
// `if(UNANSWERED_POLICY!=1) break;` and therefore faulted on the FIRST unjudged
// part regardless of policy -- found by a 5-hour soak that ran 51,161 objects
// and stopped on 51,162. A test that costs 30 seconds would have found it on
// day one.
//
// THE LEVER IS trig_report. `{"type":"trig_report","on":false}` makes the board
// keep triggering the camera and keep sorting, and simply not tell the core.
// The core therefore never answers, every part arrives at SWITCH unjudged, and
// the threshold is exercised with no wiring change, no camera, and no plate --
// the firmware deliberately still consumes the queue in this mode.
//
// TWO CASES, and they must disagree:
//   stop_only + stop_after=N   ->  runs N-1 unjudged, faults on the Nth
//   none                       ->  never faults, however many go unjudged
//
// A run where both cases behave the same is a failure even if neither crashes:
// it means the policy is not being read.
import net from 'node:net';

const argv = process.argv.slice(2);
const arg = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? argv[i + 1] : d; };
const PORT = Number(arg('port', 4099));
const STOP_AFTER = Number(arg('stop-after', 5));
const PERIOD_US = Number(arg('period-us', 300000));   // ~3.3/s, slow enough to watch
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const s = net.connect(PORT, '127.0.0.1');
let buf = '', lines = [];
s.on('data', (d) => {
  buf += d.toString('latin1');
  let nl;
  while ((nl = buf.indexOf('\n')) >= 0) { lines.push(buf.slice(0, nl)); buf = buf.slice(nl + 1); }
});
// Once we are finished the core closes its end, and an ECONNRESET printed
// under a PASS reads like the test failed. Only shout while we still care.
let done = false;
s.on('error', (e) => {
  if (done) return;
  console.error(`console ${PORT}: ${e.message}`);
  process.exit(1);
});
await new Promise((r) => s.once('connect', r));

let id = 84000;
async function ask(obj, ms = 2000) {
  const myId = id++;
  lines = [];
  s.write(JSON.stringify({ ...obj, id: myId }) + '\n');
  await sleep(ms);
  const hit = lines.find((l) => l.includes(`"id":${myId}`));
  if (!hit) return null;
  try { return JSON.parse(hit.slice(hit.indexOf('{'))); } catch { return null; }
}
const stat = () => ask({ type: 'get_running_stat' }, 2200);

const READY = 101;
async function bringUp() {
  let st = await stat();
  if (!st) { console.log('board not answering -- is the core up and the channel held?'); return false; }
  if (st.state === READY) return true;
  console.log(`  bring-up: state ${st.state} -> READY`);
  await ask({ type: 'clear_error' }, 1200);
  await ask({ type: 'set_dry_run', on: true }, 1200);
  // Nested, not top-level. A flat {"type":"plate","freq":...} is not a command
  // and the board answers it with nothing at all (defect 1.3).
  await ask({ type: 'set_setup', plate: { freq: 15000 } }, 1500);
  await ask({ type: 'enter_insp_mode' }, 1500);
  for (let i = 0; i < 30; i++) {
    st = await stat();
    if (st && st.state === READY) { console.log(`  bring-up: READY after ${i + 1}s`); return true; }
    await sleep(1000);
  }
  console.log(`  bring-up FAILED, stuck at state ${st && st.state}`);
  return false;
}

const unanswered = (st) => (st && st.yield && st.yield.verdict && st.yield.verdict.unanswered) ?? 0;
const consec = (st) => (st && st.health && st.health.consec_unanswered) ?? 0;

// Feed parts and watch for the line to stop. Returns how many went unjudged by
// the time it left READY, or -1 if it never did.
async function feedUntilStop(count) {
  await ask({ type: 'trig_phantom_train', count, period_us: PERIOD_US }, 1200);
  const budgetMs = count * (PERIOD_US / 1000) + 8000;
  const t0 = Date.now();
  let last = null;
  while (Date.now() - t0 < budgetMs) {
    const st = await stat();
    if (!st) { await sleep(500); continue; }
    last = st;
    if (st.state !== READY) {
      return { stoppedAt: unanswered(st), consec: consec(st),
               state: st.state, errs: st.error_hist || [] };
    }
    await sleep(700);
  }
  return { stoppedAt: -1, consec: consec(last), state: last && last.state,
           errs: (last && last.error_hist) || [] };
}

async function runCase(mode, expectStop) {
  console.log(`\n=== skip_policy mode="${mode}" stop_after=${STOP_AFTER} ===`);
  await ask({ type: 'clear_error' }, 1200);
  await ask({ type: 'reset_running_stat' }, 1200);
  if (!(await bringUp())) return false;

  const sp = await ask({ type: 'set_setup', skip_policy: { mode, stop_after: STOP_AFTER } }, 1500);
  if (!sp || sp.ack !== true) { console.log(`  FAIL: set_setup skip_policy not acked (${JSON.stringify(sp)})`); return false; }

  const tr = await ask({ type: 'trig_report', on: false }, 1500);
  console.log(`  trig_report -> ${JSON.stringify(tr)}`);
  if (!tr || tr.on !== false) { console.log('  FAIL: could not suppress reporting'); return false; }

  // Feed comfortably more than the threshold so "never stops" is a real
  // observation rather than "ran out of parts".
  const feed = STOP_AFTER * 3;
  const r = await feedUntilStop(feed);

  // Put reporting back before judging anything, so a failure here does not
  // leave the bench in a state that poisons the next case.
  await ask({ type: 'trig_report', on: true }, 1200);

  if (expectStop) {
    if (r.stoppedAt < 0) {
      console.log(`  FAIL: fed ${feed} unjudged parts and the line NEVER stopped`);
      console.log(`        (consec_unanswered=${r.consec}, state=${r.state}) -- the`);
      console.log('        threshold is not being enforced');
      return false;
    }
    console.log(`  stopped: state=${r.state} unanswered=${r.stoppedAt} consec=${r.consec} errors=${JSON.stringify(r.errs)}`);
    // The point is that it tolerates SOME. Stopping on the first part is the
    // exact regression this test exists for.
    if (r.stoppedAt < 2 && STOP_AFTER > 1) {
      console.log(`  FAIL: stopped on part ${r.stoppedAt} with stop_after=${STOP_AFTER}`);
      console.log('        -- that is the fault-on-first-part regression');
      return false;
    }
    console.log('  PASS: tolerated several, then stopped');
    return true;
  }

  if (r.stoppedAt >= 0) {
    console.log(`  FAIL: mode "none" stopped anyway at unanswered=${r.stoppedAt}`);
    console.log(`        state=${r.state} errors=${JSON.stringify(r.errs)}`);
    return false;
  }
  console.log(`  PASS: fed ${feed} unjudged parts, never stopped (consec=${r.consec})`);
  return true;
}

let ok = true;
ok = (await runCase('stop_only', true)) && ok;
ok = (await runCase('none', false)) && ok;

// Leave the bench as we found it: reporting on, a policy that stops, no error.
await ask({ type: 'trig_report', on: true }, 1000);
await ask({ type: 'set_setup', skip_policy: { mode: 'stop_only', stop_after: STOP_AFTER } }, 1200);
await ask({ type: 'clear_error' }, 1200);

console.log(`\n${ok ? 'PASS' : 'FAIL'}`);
// end(), not destroy() + exit(): tearing the socket down and exiting in the
// same tick makes libuv assert on Windows ("!(handle->flags & UV_HANDLE_CLOSING)")
// and prints a crash line underneath a PASS, which reads like a failure.
done = true;
process.exitCode = ok ? 0 : 1;
s.end();
