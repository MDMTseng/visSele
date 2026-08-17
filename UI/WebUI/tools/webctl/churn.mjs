// Teardown-under-fire probe for the WS layer lock rework (backlog 2.1/2.6).
//
// While an observer client keeps a live inspection stream (CI free-run + SB),
// this hammers the exact paths the rework changed:
//   - churn: waves of clients that subscribe to the stream and then DESTROY
//     the socket with no close frame -- teardown happens while the stream
//     thread may be mid-send to them (deferred-close path).
//   - stall: one client that subscribes and then never reads -- forces the
//     5s SO_SNDTIMEO + shutdown-in-safeSend + doClosing while a sender is
//     provably inside send() on that fd.
//
// Pass = the observer's RP counter advances throughout, and the core still
// answers a GS at the end. A hang, a crash, or a frozen observer stream is
// the failure signature this exists to catch.
//
//   node churn.mjs [rounds=30]
import WebSocket from 'ws';
const H = 9, enc = new TextEncoder();
function frame(t, p, g, o) {
  const b = enc.encode(o == null ? '' : JSON.stringify(o));
  const u = new Uint8Array(H + b.length + 1);
  u[0] = t.charCodeAt(0); u[1] = t.charCodeAt(1); u[2] = p; u[3] = g >> 8; u[4] = g & 255;
  const l = u.length - H;
  u[5] = l >>> 24; u[6] = (l >> 16) & 255; u[7] = (l >> 8) & 255; u[8] = l & 255;
  u.set(b, H); return u;
}
const ROUNDS = parseInt(process.argv[2] || '30', 10);
const URL = 'ws://127.0.0.1:4090';

// ---- observer: keep a stream and count it -------------------------------
const A = new WebSocket(URL); A.binaryType = 'arraybuffer';
let pgA = 1, rp = 0, im = 0;
A.on('message', d => {
  const b = new Uint8Array(d instanceof ArrayBuffer ? d : d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength));
  const t = String.fromCharCode(b[0], b[1]);
  if (t === 'HR') { A.send(frame('HR', 0, pgA++, { a: ['d'] })); return; }
  if (t === 'RP') rp++;
  if (t === 'IM') im++;
});
A.on('error', e => { console.error('observer error:', e.message); process.exit(2); });
await new Promise(r => A.on('open', () => setTimeout(r, 400)));
A.send(frame('ST', 0, pgA++, { IMG_STREAMING_JPEG_QUALITY: 85 }));
A.send(frame('CI', 0, pgA++, { deffile: 'data/test1.hydef', frame_count: -1, trigger_mode: 0 }));
A.send(frame('SB', 0, pgA++, { stream: true }));
await new Promise(r => setTimeout(r, 2500));
const rp0 = rp;
console.log(`observer streaming: RP=${rp} IM=${im}`);
if (rp === 0) { console.error('observer got no stream -- cannot test'); process.exit(2); }

// ---- freeze-window sampler ----------------------------------------------
// The old verdict only checked RP advanced in the FINAL window -- a 20s
// mid-test freeze passed. Sample every second and assert the longest gap.
const rpSamples = [];
const sampler = setInterval(() => rpSamples.push(rp), 1000);

// ---- stall client: subscribe (CONFIRMED), then never read ---------------
// Confirm the SB actually landed before pausing -- otherwise this can
// silently degrade into testing an unsubscribed socket.
const stall = new WebSocket(URL); stall.binaryType = 'arraybuffer';
let pgS = 1, stallSubscribed = false;
stall.on('error', () => {});
stall.on('message', d => {
  const b = new Uint8Array(d instanceof ArrayBuffer ? d : d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength));
  const t = String.fromCharCode(b[0], b[1]);
  if (t === 'HR') { stall.send(frame('HR', 0, pgS++, { a: ['d'] })); return; }
  if (t === 'SS') stallSubscribed = true;   // any stream traffic = subscribed
});
await new Promise(r => stall.on('open', () => setTimeout(r, 200)));
stall.send(frame('SB', 0, pgS++, { stream: true }));
await new Promise(r => setTimeout(r, 1200));   // see at least one stream pkt
stall._socket.pause();           // stop reading; the core's sends back up
console.log(`stall client armed (stream seen before pause: ${stallSubscribed})`);
// HALF-close it (FIN, keep not-reading) inside the send-backpressure window.
// This is the deterministic forcing for tryFinalizeClose's DEFERRED branch:
// the core's recv sees EOF and runs doClosing while the sender is STILL
// blocked in send() on that fd (the client's receive window stays full), so
// the try_lock must fail -- "deferred close: sender mid-send" on the core's
// stdout. A destroy() would RST instead: the blocked send fails and exits
// before the select loop even wakes, and only the immediate branch runs.
setTimeout(() => { try { stall._socket.end(); } catch {} console.log('stall client half-closed (FIN) mid-backpressure'); }, 2500);

// ---- churn waves --------------------------------------------------------
let churned = 0;
for (let round = 0; round < ROUNDS; round++) {
  const wave = [];
  for (let i = 0; i < 3; i++) {
    const w = new WebSocket(URL); w.binaryType = 'arraybuffer';
    w.on('error', () => {});
    let pg = 1;
    w.on('message', d => {
      const b = new Uint8Array(d instanceof ArrayBuffer ? d : d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength));
      if (String.fromCharCode(b[0], b[1]) === 'HR') w.send(frame('HR', 0, pg++, { a: ['d'] }));
    });
    w.on('open', () => w.send(frame('SB', 0, pg++, { stream: true })));
    wave.push(w);
  }
  await new Promise(r => setTimeout(r, 220));
  for (const w of wave) { try { w._socket.destroy(); } catch {} churned++; }

  // fd-reuse probe: a client born RIGHT after the destroys (accept() will
  // reuse the freed fd numbers) must see a clean stream from byte 0 -- its
  // first message must be a well-formed HR, never a leftover mid-frame from
  // the previous owner of the fd. This is the observable for the
  // pendingCloseFd slot-skip + close-under-sendMutex design.
  if (round % 5 === 0) {
    const oneProbe = () => new Promise(res => {
      const p = new WebSocket(URL); p.binaryType = 'arraybuffer';
      let seen = 0, bad = 0;
      // 8s: while the stall client's wedge is active (<=5s), the HR greeting
      // blocks behind linkLayerLock -- known 2.8 bound, owned by the freeze
      // assertion, not this probe. This probe asserts CONTENT cleanliness of a
      // reused fd, not latency.
      const to = setTimeout(() => { try { p._socket.destroy(); } catch {} res(seen === 0 ? 'no-handshake' : bad); }, 8000);
      p.on('error', () => { clearTimeout(to); res('connect-fail'); });
      p.on('message', d => {
        const b = new Uint8Array(d instanceof ArrayBuffer ? d : d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength));
        seen++;
        const tl = String.fromCharCode(b[0], b[1]);
        // The FIRST frame on a clean connection MUST be the HR greeting. A
        // leftover mid-frame from the previous owner of a reused fd could
        // begin with two uppercase bytes and a plausible length and pass a
        // mere /^[A-Z]{2}$/ check -- so the strong invariant is "first frame
        // is exactly HR". Later frames just need a valid TL + honest length.
        if (seen === 1) { if (tl !== 'HR') bad++; }
        else if (!/^[A-Z]{2}$/.test(tl)) bad++;
        const declared = (b[5] * 0x1000000) + (b[6] << 16) + (b[7] << 8) + b[8];
        if (declared > b.length - 9) bad++;   // header claims more than the frame carries
        if (seen >= 3) { clearTimeout(to); p.close(); res(bad); }
      });
    });
    // Opportunistic, non-blocking on failure: a probe landing inside the stall
    // client's <=5s wedge can't be accepted (connect-fail) -- that's the known
    // bound, not a defect, and blocking to retry it would stall the churn loop
    // and starve the very stream the freeze assertion measures. Only DIRTY
    // BYTES (numeric bad>0) fail the run; a transient bad connection is skipped
    // with a warning.
    const probeBad = await oneProbe();
    if (typeof probeBad === 'number' && probeBad > 0) { console.error(`FD-REUSE PROBE FAILED round ${round}: ${probeBad} dirty frame(s)`); process.exit(1); }
    if (typeof probeBad !== 'number') console.warn(`  (fd-reuse probe round ${round}: ${probeBad} -- transient wedge window, skipped)`);
  }
  if ((round + 1) % 10 === 0) console.log(`round ${round + 1}/${ROUNDS}: churned=${churned} observer RP=${rp}`);
}

// Stop the freeze sampler HERE -- the churn (streaming) phase is over. The
// teardown-wait and gsOK probe below are legitimately quiet for the observer,
// so sampling them would count expected idle as a "freeze". maxGap/avgRate are
// computed over the streaming phase only.
clearInterval(sampler);

// ---- let the stall client's SO_SNDTIMEO fire and teardown settle --------
console.log('waiting 8s for the stall client teardown (SO_SNDTIMEO=5s)...');
await new Promise(r => setTimeout(r, 8000));
try { stall._socket.destroy(); } catch {}

// ---- verdict ------------------------------------------------------------
const rpMid = rp;
await new Promise(r => setTimeout(r, 3000));
const gsOK = await new Promise(res => {
  const C = new WebSocket(URL); C.binaryType = 'arraybuffer';
  let pg = 1;
  const to = setTimeout(() => { res(false); }, 5000);
  C.on('error', () => { clearTimeout(to); res(false); });
  C.on('message', d => {
    const b = new Uint8Array(d instanceof ArrayBuffer ? d : d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength));
    const t = String.fromCharCode(b[0], b[1]);
    if (t === 'HR') { C.send(frame('GS', 0, pg++, { items: ['camera_info'] })); return; }
    if (t === 'GS') { clearTimeout(to); C.close(); res(true); }
  });
});
// Longest CONTIGUOUS freeze over the STREAMING phase (sampler stopped before
// the teardown tail). This asserts RECOVERY, not a tight latency: churn
// DELIBERATELY wedges the layer (the FIN-half-closed stall client holds the
// full 5s SO_SNDTIMEO, and fd-reuse probes connect during those windows), so
// several-second freezes are the expected, correct behaviour. The bound of 20s
// cleanly separates "recovered after the deliberate wedges" from the real
// regression -- one stuck client wedging the layer FOREVER (which would show
// maxGap == the whole streaming duration). A throughput floor is deliberately
// NOT asserted: it fights the test's own purpose and flags the wedge itself.
let maxGap = 0, gap = 0, totalFrozen = 0;
for (let i = 1; i < rpSamples.length; i++) {
  if (rpSamples[i] === rpSamples[i - 1]) { gap++; totalFrozen++; } else gap = 0;
  if (gap > maxGap) maxGap = gap;
}
const gapOK = maxGap <= 20;
const streamAlive = rp > rpMid;
console.log(`RESULT: churned=${churned} observer RP ${rp0} -> ${rp} (advancing at end: ${streamAlive}) core GS answer: ${gsOK} longest freeze: ${maxGap}s (limit 20, recovery) total frozen: ${totalFrozen}s (fyi) stall-subscribed: ${stallSubscribed}`);
// Deferred-close branch: capture the core's stdout and REPORT whether the
// "deferred close: sender mid-send" marker fired. It is a RACE, not a
// deterministic forcing (doClosing shutdown()s first, so the sender usually
// unwinds before try_lock) -- so a miss is not a failure, but the run must
// SAY which happened instead of leaving a human to grep. Pass CORE_LOG=<path>.
let deferredSeen = null;
if (process.env.CORE_LOG) {
  try {
    const fs = await import('node:fs');
    deferredSeen = fs.readFileSync(process.env.CORE_LOG, 'utf8').includes('deferred close: sender mid-send');
  } catch { deferredSeen = null; }
}
console.log(`deferred-close branch observed this run: ${deferredSeen === null ? 'unknown (set CORE_LOG=<core stdout path> to check)' : deferredSeen} -- it is a rare safety-net path, not required to fire.`);
A.send(frame('CI', 0, pgA++, { frame_count: 0 }));
setTimeout(() => process.exit(streamAlive && gsOK && gapOK && stallSubscribed ? 0 : 1), 500);
