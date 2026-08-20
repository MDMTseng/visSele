// Which device replies are bigger than the CORE can frame?
//
//   node big_reply_probe.mjs [--port 4099]
//
// The core's peripheral data layer assembles one frame in `uint8_t
// dataBuff[2048]` (common_lib/include/Data_Layer_Protocol.hpp). A device reply
// that does not close its outermost object within 2048 bytes trips
// RECV_BUFFER_FULL, and the core's handler for that is
// PerifChannel::recv_ERROR() -- an `int` function with an empty body, which
// gcc -O2 compiles to a bare `ud2`. So "reply too long" is not a truncated
// line, it is an illegal instruction and a dead core process.
//
// This walks the read-only query commands one at a time, measures the largest
// single line each produces, and checks the core is still alive afterwards.
// Anything approaching 2048 is a live grenade regardless of whether it went
// off today: reply sizes grow with the config, the verdict log and the command
// table, and nothing anywhere warns when one crosses the line.
import net from 'node:net';
import { execSync } from 'node:child_process';

const argv = process.argv.slice(2);
const num = (n, d) => { const i = argv.indexOf(`--${n}`); return i >= 0 ? Number(argv[i + 1]) : d; };
const PORT = num('port', 4099);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const coreAlive = () => { try { return /visSele\.exe/.test(execSync('tasklist', { encoding: 'utf8' })); } catch { return null; } };

// Read-only. Nothing here changes a setting, moves a part or touches NVS.
const CMDS = [
  'get_version', 'get_state_names', 'get_setup', 'get_running_stat',
  'get_backup_stat', 'get_verdict_log', 'get_spikes', 'get_width_hist',
  'get_schema', 'report', 'ping',
];

(async () => {
  if (!coreAlive()) { console.error('core is not running'); process.exit(1); }
  console.log(`big_reply_probe -- 127.0.0.1:${PORT}\n`);
  console.log('command            max_line  lines  core');
  console.log('------------------ --------  -----  ----');

  for (const cmd of CMDS) {
    const s = net.connect(PORT, '127.0.0.1');
    let buf = '';
    s.on('data', (d) => { buf += d.toString('latin1'); });
    s.on('error', () => {});
    await new Promise((r) => { s.once('connect', r); s.once('error', r); });
    await sleep(600);
    buf = '';                                  // drop the unprompted background chatter
    s.write(JSON.stringify({ type: cmd }) + '\n');
    await sleep(2500);
    const lines = buf.split('\n').filter((l) => l.trim());
    // Ignore the once-a-second SYSTIME/pong background so the number means the reply.
    const own = lines.filter((l) => !/SYSTIME|"pong"|"cam_trig"|comm_lost_backup/.test(l));
    const max = own.reduce((m, l) => Math.max(m, l.length), 0);
    s.destroy();
    const up = coreAlive();
    const flag = !up ? 'DEAD' : max > 1800 ? 'near' : 'ok';
    console.log(`${cmd.padEnd(18)} ${String(max).padStart(8)}  ${String(own.length).padStart(5)}  ${flag}`);
    if (!up) {
      console.log(`\n${cmd} KILLED THE CORE -- its reply exceeded the core's 2048-byte frame buffer`);
      process.exit(2);
    }
    await sleep(400);
  }
  console.log('\nheadroom is against dataBuff[2048] in the core; "near" is >1800.');
})();
