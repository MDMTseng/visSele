// Dump the running core's log ring from a terminal.
//
//   node tools/corelog.mjs            # dump, then print the newest dump file
//   node tools/corelog.mjs --path     # dump, print only the file path
//
// The ring is in memory and used to have exactly one exit: SC {type:"log_dump"}
// over BPG, wired to a button in the WebUI. That put the log behind the UI --
// which is the wrong place for it when the UI is what is misbehaving, or when
// the machine will not leave calibration and the question is why.
//
// Goes through the CONTROL port (4098), not BPG: it answers a bare JSON line,
// needs no handshake, and is already the channel the supervisor uses to ping
// and stop the core. Sending a plain BPG frame to 4090 gets no reply at all --
// measured -- so the WebUI's route is not usable from outside the app.
import net from 'node:net';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const REPO = path.resolve(here, '..', '..', '..');
const WORKING_DIR = process.env.SOAK_WORKING_DIR
  || path.join(REPO, 'InspectionCore', 'Core0_1');
const PORT = Number(process.env.INSP_CONTROL_PORT || 4098);
const pathOnly = process.argv.includes('--path');

const ask = (obj) => new Promise((resolve, reject) => {
  const sock = net.createConnection({ port: PORT, host: '127.0.0.1' }, () => {
    sock.write(JSON.stringify(obj) + '\n');
  });
  let buf = '';
  const done = (err, val) => { try { sock.destroy(); } catch {} err ? reject(err) : resolve(val); };
  sock.on('data', (d) => { buf += d.toString(); if (buf.includes('\n')) done(null, buf.trim()); });
  sock.on('error', (e) => done(e));
  setTimeout(() => done(new Error('control port timeout')), 4000);
});

// Newest dump BEFORE the request, so the one that appears can be identified
// rather than assumed -- a stale crash_*.dump from an earlier session sitting
// in the same folder is exactly the wrong file to read and believe.
const newest = () => {
  let best = null;
  for (const f of fs.readdirSync(WORKING_DIR)) {
    if (!/^crash_.*\.dump$/.test(f)) continue;
    const p = path.join(WORKING_DIR, f);
    const m = fs.statSync(p).mtimeMs;
    if (!best || m > best.m) best = { p, m };
  }
  return best;
};

const before = newest();
const reply = await ask({ type: 'log_dump' }).catch((e) => { console.log('ERR ' + e.message); process.exit(1); });
if (!/"ack"\s*:\s*true/.test(reply)) {
  console.log('core did not ack log_dump -- is it older than this command?');
  console.log(reply);
  process.exit(1);
}

// The drainer writes asynchronously; wait for a file that is actually new.
let got = null;
for (let i = 0; i < 40 && !got; i++) {
  await new Promise((r) => setTimeout(r, 100));
  const n = newest();
  if (n && (!before || n.p !== before.p || n.m > before.m)) got = n;
}
if (!got) { console.log('acked, but no new dump appeared in ' + WORKING_DIR); process.exit(1); }
if (pathOnly) { console.log(got.p); process.exit(0); }
console.log('==> ' + got.p);
process.stdout.write(fs.readFileSync(got.p, 'utf8'));
