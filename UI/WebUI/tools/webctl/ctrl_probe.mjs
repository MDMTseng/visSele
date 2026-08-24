// Exercise the core's launcher control socket (127.0.0.1:4098).
//
//   node ctrl_probe.mjs              -- ping, a junk command, and a long line
//   node ctrl_probe.mjs shutdown     -- ping, then ask the core to stop
//
// The shutdown case is the one worth checking by hand: the ack must arrive
// BEFORE the socket dies, because a supervisor that sees the connection drop
// without an ack cannot tell "stopping cleanly" from "did not understand" and
// would escalate to a hard kill of a graceful shutdown in progress.
import net from 'node:net';

const PORT = Number(process.env.INSP_CONTROL_PORT || 4098);
const DO_SHUTDOWN = process.argv[2] === 'shutdown';

function ask(line, waitMs = 800) {
  return new Promise((resolve) => {
    const s = net.connect(PORT, '127.0.0.1');
    let buf = '';
    let closed = false;
    const done = (note) => {
      if (closed) return;
      closed = true;
      try { s.destroy(); } catch {}
      resolve({ reply: buf.trim() || null, note });
    };
    s.on('connect', () => s.write(line + '\n'));
    s.on('data', (d) => { buf += d.toString('latin1'); if (buf.includes('\n')) done('reply'); });
    s.on('error', (e) => done('socket error: ' + e.code));
    s.on('close', () => done(buf ? 'reply then close' : 'closed with no reply'));
    setTimeout(() => done('timeout'), waitMs);
  });
}

const show = async (label, line, waitMs) => {
  const r = await ask(line, waitMs);
  console.log(`${label.padEnd(22)} ${r.note.padEnd(22)} ${r.reply ?? ''}`);
  return r;
};

await show('ping', '{"type":"ping"}');
await show('unknown', '{"type":"nonsense"}');
await show('over-long line', '{"type":"' + 'x'.repeat(400) + '"}');
// Two pings on separate connections back to back: the dev console refuses a
// second client, this one must not.
await Promise.all([show('concurrent ping A', '{"type":"ping"}'),
                   show('concurrent ping B', '{"type":"ping"}')]);

if (DO_SHUTDOWN) {
  console.log('');
  const t0 = Date.now();
  await show('shutdown', '{"type":"shutdown"}');

  // The control PORT closes as soon as the control thread leaves its accept
  // loop, which is well before the core has finished tearing down -- measured
  // at 0.0 s versus ~1 s to actual exit. So the port is the wrong observable:
  // a supervisor that waited on it would start its force-kill timer against a
  // shutdown that had barely begun. Wait on the PROCESS.
  //
  // The real launcher does not need this poll at all: it owns the child and
  // gets an 'exit' event. This is here so the probe measures the same thing.
  const { execSync } = await import('node:child_process');
  const alive = () => {
    try {
      return execSync('tasklist /FI "IMAGENAME eq visSele.exe" /NH', { encoding: 'latin1' })
        .toLowerCase().includes('vissele.exe');
    } catch { return false; }
  };
  let portClosedAt = null;
  for (let i = 0; i < 120; i++) {
    if (portClosedAt === null) {
      const r = await ask('{"type":"ping"}', 300);
      if (r.note.startsWith('socket error')) portClosedAt = Date.now() - t0;
    }
    if (!alive()) {
      console.log(`control port closed after ${((portClosedAt ?? 0) / 1000).toFixed(1)} s`);
      console.log(`process exited after   ${((Date.now() - t0) / 1000).toFixed(1)} s`);
      process.exit(0);
    }
    await new Promise((r2) => setTimeout(r2, 250));
  }
  console.log('STILL RUNNING after 30 s -- graceful shutdown did not complete');
  process.exit(1);
}
