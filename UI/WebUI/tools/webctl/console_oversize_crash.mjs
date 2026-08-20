// Minimal reproduction: one oversized dev-console line kills the CORE PROCESS.
//
//   node console_oversize_crash.mjs [--bytes 2100] [--port 4099]
//
// Chain (each link verified 2026-08-19 on the bare board):
//
//   1. PerifConsoleThread accepts lines up to 4096 bytes
//      (wiringPanel.cpp, `if (line.size() < 4096)`), checks only that the
//      first non-space character is '{', and forwards verbatim.
//   2. The DEVICE's frame buffer is 2048 bytes (Data_Layer_Protocol.hpp,
//      `uint8_t dataBuff[2048]`, sized "to comfortably hold the largest
//      legitimate command"). At 2048 it raises RECV_BUFFER_FULL and LATCHES:
//      SERIAL_PROTOCOL_ERROR (11), machine stopped.
//   3. The device reports that error by echoing its whole frame buffer back
//      (`dbg_printf("recv_ERROR:%d %s ...", errorcode, dataBuff)`) -- ~2kB.
//   4. That reply overruns the CORE's own 2048-byte dataBuff, so the core's
//      data layer raises its own RECV_BUFFER_FULL and calls
//      PerifChannel::recv_ERROR().
//   5. PerifChannel::recv_ERROR is declared `int` and has an EMPTY BODY
//      (wiringPanel.cpp:1500). Falling off the end of a non-void function is
//      undefined behaviour; gcc -O2 emits the function as a single `ud2`.
//      The process takes ILLEGAL_INSTRUCTION and dies.
//
//        0000000140151ff0 <_ZN12PerifChannel10recv_ERROREN18Data_JsonRaw_Layer10ERROR_TYPEE>:
//           140151ff0:  0f 0b    ud2
//
// Step 5 is the one that matters beyond this console: recv_ERROR is the core's
// handler for ANY malformed frame arriving from the device. Line noise, a
// truncated frame, a device reboot mid-packet -- every one of them reaches an
// illegal instruction. The console is merely the cheapest way to prove it.
//
// PerifChannel::recv_RESET() (wiringPanel.cpp:1496) is the same shape and is on
// the RECOVERY path: the data layer calls it when it finds a RESET_PACKET while
// its own parser is latched. Both the fault and its recovery are ud2.
import net from 'node:net';
import { execSync } from 'node:child_process';

const argv = process.argv.slice(2);
const num = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? Number(argv[i + 1]) : d; };
const BYTES = num('bytes', 2100);
const PORT = num('port', 4099);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const alive = () => {
  try { return /visSele\.exe/.test(execSync('tasklist', { encoding: 'utf8' })); }
  catch { return null; }
};

(async () => {
  console.log(`core process before: ${alive() ? 'RUNNING' : 'not running'}`);
  const head = '{"type":"get_running_stat","pad":"', tail = '"}';
  const line = head + 'A'.repeat(Math.max(0, BYTES - head.length - tail.length)) + tail;

  const s = net.connect(PORT, '127.0.0.1');
  let got = '';
  s.on('data', (d) => { got += d.toString('latin1'); });
  s.on('error', (e) => console.log(`socket: ${e.message}`));
  await new Promise((r) => s.once('connect', r));
  console.log(`connected; sending one ${line.length}-byte line`);
  s.write(line + '\n');

  for (let t = 1; t <= 15; t++) {
    await sleep(1000);
    const up = alive();
    if (!up) {
      console.log(`\nt+${t}s  CORE PROCESS GONE -- reproduced`);
      try {
        const dmp = execSync('ls -t InspectionCore/Core0_1/insp_crash_*.dmp 2>/dev/null | head -1',
          { encoding: 'utf8', shell: 'C:/msys64/usr/bin/bash.exe' }).trim();
        if (dmp) console.log(`minidump: ${dmp}`);
      } catch {}
      process.exit(0);
    }
  }
  console.log(`\ncore survived 15s; ${got.length} bytes echoed back -- NOT reproduced at ${BYTES} bytes`);
  process.exit(1);
})();
