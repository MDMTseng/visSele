// How many device protocol errors does the core survive?
//
//   node latch_loop.mjs [--cycles 40] [--port 4099]
//
// F1 (AUDIT_BACKLOG P1) is that PerifChannel::recv_ERROR compiles to a single
// `ud2`, so the core dies the moment its data layer sees a malformed frame from
// the device. Proving it existed was easy -- the disassembly is one
// instruction. Saying how OFTEN it bites needed a reliable trigger, and there
// now is one: latch the DEVICE's parser, let it recover, and the traffic that
// recovery produces is what the core cannot frame.
//
// The cheapest reliable latch is an empty container (A1-1):
//
//     {"type":"ping","x":[]}      -> device SERIAL_PROTOCOL_ERROR, deaf
//     {"type":"clear_error"}      -> device recovers
//
// One cycle is one protocol error on a real link. This counts how many cycles
// the core survives, which converts "it crashed once during testing" into a
// number an operator can reason about: a machine whose cable raises one framing
// error a day has this long before the core process dies.
//
// It leaves the core dead on purpose when it reproduces -- the minidump is the
// evidence. Restart with the bench recipe afterwards.
import net from 'node:net';
import { execSync } from 'node:child_process';

const argv = process.argv.slice(2);
const num = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? Number(argv[i + 1]) : d; };
const CYCLES = num('cycles', 40);
const PORT = num('port', 4099);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const coreAlive = () => { try { return /visSele\.exe/.test(execSync('tasklist', { encoding: 'utf8' })); } catch { return null; } };

let s = null, buf = '', lines = [];
function connect() {
  return new Promise((resolve) => {
    s = net.connect(PORT, '127.0.0.1');
    buf = ''; lines = [];
    s.on('data', (d) => {
      buf += d.toString('latin1');
      let nl;
      while ((nl = buf.indexOf('\n')) >= 0) { lines.push(buf.slice(0, nl)); buf = buf.slice(nl + 1); }
    });
    s.on('error', () => resolve(false));
    s.once('connect', () => resolve(true));
  });
}
let id = 30000;
async function answered(obj, ms = 1500) {
  const myId = id++;
  lines = [];
  try { s.write(JSON.stringify({ ...obj, id: myId }) + '\n'); } catch { return false; }
  await sleep(ms);
  return lines.some((l) => l.includes(`"id":${myId}`));
}

if (!(await connect())) { console.error('console not reachable'); process.exit(1); }
if (!coreAlive()) { console.error('core not running'); process.exit(1); }
console.log(`latch/recover cycles, up to ${CYCLES}\n`);

let latched = 0, recovered = 0;
for (let i = 1; i <= CYCLES; i++) {
  // latch
  try { s.write('{"type":"ping","x":[]}\n'); } catch {}
  await sleep(900);
  const deaf = !(await answered({ type: 'get_running_stat' }, 1500));
  if (deaf) latched++;
  // recover
  try { s.write('{"type":"clear_error"}\n'); } catch {}
  await sleep(1200);
  const back = await answered({ type: 'get_running_stat' }, 2000);
  if (back) recovered++;

  if (!coreAlive()) {
    console.log(`\ncycle ${i}: CORE PROCESS GONE`);
    console.log(`latched ${latched}/${i} cycles, device recovered ${recovered} times`);
    try {
      const dmp = execSync('ls -t InspectionCore/Core0_1/insp_crash_*.dmp | head -1',
        { encoding: 'utf8', shell: 'C:/msys64/usr/bin/bash.exe', cwd: 'C:/Users/w2110/Documents/workspace/visSele' }).trim();
      if (dmp) console.log(`minidump: ${dmp}`);
    } catch {}
    console.log(`\nMEAN CYCLES TO CORE DEATH (this run): ${i}`);
    process.exit(0);
  }
  if (i % 5 === 0) console.log(`  cycle ${i}: latched ${latched}, recovered ${recovered}, core alive`);
  await sleep(300);
}
console.log(`\ncore survived all ${CYCLES} cycles (latched ${latched}, recovered ${recovered})`);
process.exit(1);
