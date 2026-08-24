// stream_lab -- a 5 MP live-view test rig that runs in the real Electron.
//
//   node tools/stream_lab/run.mjs --help
//
// It stands in for the core: a WebSocket server on 127.0.0.1 that pushes 5 MP
// frames at a chosen rate, in a chosen wire format, and a page that renders
// them through a chosen pipeline. Every combination is measured the same way,
// so the comparison is like for like:
//
//   wire      jpeg <quality> | raw          (bytes actually sent)
//   pipeline  bitmap2d | decoder2d | decoderGL | workerGL | rawGL
//
// Frames are REAL: 2448x2048 8-bit grey captures from this machine's camera
// (data/BMP_carousel_test). Synthetic images compress far better than real
// ones, which makes every wire-size number optimistic and every conclusion
// about bandwidth wrong.
//
// Each frame carries a 16-byte header (magic, sequence, send time) ahead of the
// payload, so the page can report END TO END latency and detect a dropped or
// reordered frame. A pipeline that keeps up on average while quietly dropping
// frames is not keeping up, and an average-only measurement cannot tell.
'use strict';

const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('node:path');
const fs = require('node:fs');
const http = require('node:http');
const crypto = require('node:crypto');
const zlib = require('node:zlib');

const args = {};
for (const a of process.argv.slice(2)) {
  const m = /^--([^=]+)(?:=(.*))?$/.exec(a);
  if (m) args[m[1]] = m[2] === undefined ? true : m[2];
}

const WIRE = String(args.wire || 'jpeg');
const QUALITY = Number(args.quality || 85);
const PIPELINE = String(args.pipeline || 'decoderGL');
const FPS = Number(args.fps || 20);
const SECONDS = Number(args.seconds || 8);
const FRAMES_DIR = String(args.frames || '');
const SHOW = !!args.show;
const DEVTOOLS = !!args.devtools;
const PORT = Number(args.port || 4311);
const CONC = Number(args.concurrency || 1);

const HEADER = 16;                       // magic u32 | seq u32 | sendMs f64

app.commandLine.appendSwitch('enable-features', 'SharedArrayBuffer');
// A 5 MP texture per frame plus decode buffers; the default renderer heap is
// fine, but the GPU process wants headroom on integrated graphics.
app.disableHardwareAcceleration === undefined; // (documented no-op: we WANT the GPU)

// --- frame source -------------------------------------------------------------
//
// PNG decoding without a dependency: the grey 8-bit PNGs from the camera are
// zlib-deflated scanlines with a filter byte per row. That is little enough to
// unfilter here, and it keeps this tool installable with nothing but Electron.
function readGreyPng(file) {
  const buf = fs.readFileSync(file);
  if (buf.readUInt32BE(0) !== 0x89504e47) throw new Error(`${file}: not a PNG`);
  let off = 8, w = 0, h = 0, bitDepth = 0, colorType = -1;
  const idat = [];
  while (off < buf.length) {
    const len = buf.readUInt32BE(off);
    const type = buf.toString('latin1', off + 4, off + 8);
    const data = buf.subarray(off + 8, off + 8 + len);
    if (type === 'IHDR') {
      w = data.readUInt32BE(0); h = data.readUInt32BE(4);
      bitDepth = data[8]; colorType = data[9];
      if (data[12] !== 0) throw new Error(`${file}: interlaced PNG not supported`);
    } else if (type === 'IDAT') idat.push(data);
    else if (type === 'IEND') break;
    off += 12 + len;
  }
  if (bitDepth !== 8 || colorType !== 0) {
    throw new Error(`${file}: expected 8-bit greyscale (bitDepth 8, colorType 0), got ${bitDepth}/${colorType}`);
  }
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const out = Buffer.alloc(w * h);
  let src = 0;
  for (let y = 0; y < h; y++) {
    const filter = raw[src++];
    const row = out.subarray(y * w, (y + 1) * w);
    const prev = y ? out.subarray((y - 1) * w, y * w) : null;
    for (let x = 0; x < w; x++) {
      const rv = raw[src++];
      const a = x ? row[x - 1] : 0;
      const b = prev ? prev[x] : 0;
      const c = (x && prev) ? prev[x - 1] : 0;
      let v;
      switch (filter) {
        case 0: v = rv; break;
        case 1: v = rv + a; break;
        case 2: v = rv + b; break;
        case 3: v = rv + ((a + b) >> 1); break;
        case 4: {                                  // Paeth
          const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
          v = rv + (pa <= pb && pa <= pc ? a : pb <= pc ? b : c);
          break;
        }
        default: throw new Error(`${file}: unknown row filter ${filter}`);
      }
      row[x] = v & 0xff;
    }
  }
  return { w, h, grey: out };
}

// --- websocket ------------------------------------------------------------------
function startWs(port) {
  const server = http.createServer();
  let sock = null;
  server.on('upgrade', (req, socket) => {
    const accept = crypto.createHash('sha1')
      .update(req.headers['sec-websocket-key'] + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11')
      .digest('base64');
    socket.write('HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n'
               + 'Connection: Upgrade\r\nSec-WebSocket-Accept: ' + accept + '\r\n\r\n');
    socket.setNoDelay(true);
    socket.on('error', () => { sock = null; });
    sock = socket;
  });
  server.listen(port, '127.0.0.1');
  return {
    connected: () => sock !== null,
    send(payload) {
      if (!sock) return false;
      const head = Buffer.alloc(10);
      head[0] = 0x82;
      head[1] = 127;
      head.writeBigUInt64BE(BigInt(payload.length), 2);
      sock.write(head);
      return sock.write(payload);
    },
    close() { try { server.close(); sock && sock.destroy(); } catch { /* going away anyway */ } },
  };
}

app.whenReady().then(async () => {
  const dir = FRAMES_DIR || path.join(__dirname, '..', '..', '..', '..',
                                      'InspectionCore', 'Core0_1', 'data', 'BMP_carousel_test');
  const files = fs.readdirSync(dir).filter((f) => f.endsWith('.png')).sort()
                  .map((f) => path.join(dir, f));
  if (!files.length) { console.error('no PNG frames in ' + dir); app.exit(2); return; }
  const frames = files.map(readGreyPng);
  const { w, h } = frames[0];
  console.error(`source: ${files.length} frame(s) ${w}x${h} grey from ${dir}`);

  const win = new BrowserWindow({
    width: 1500, height: 950, show: SHOW,
    backgroundColor: '#101418',
    webPreferences: { contextIsolation: false, nodeIntegration: true, backgroundThrottling: false },
  });
  // Renderer console -> this terminal. Without it every log() in the page is
  // invisible to a headless run, and the tool becomes debuggable only with a
  // window open -- which defeats using it in a script.
  // console.error, not process.stderr.write: it supplies the newline itself,
  // and every attempt to hand-write one through this project's tooling has
  // arrived with the backslash eaten.
  win.webContents.on('console-message', (_e, level, message) => {
    console.error('[page]', message);
  });
  if (DEVTOOLS) win.webContents.openDevTools({ mode: 'right' });
  await win.loadFile(path.join(__dirname, 'index.html'));

  // JPEG encoding happens in the PAGE (canvas.toBlob) because Electron's main
  // process has no image encoder without a dependency. It is done ONCE, up
  // front, outside the timed run -- the core would encode per frame, and that
  // cost is measured separately by jpeg_bench, not here. What this rig measures
  // is the wire and the receive side.
  const encoded = await win.webContents.executeJavaScript(
    `window.__prepare(${JSON.stringify({ w, h, count: frames.length, wire: WIRE, quality: QUALITY })})`
      .replace('__prepare(', '__prepare('),
    true,
  ).catch((e) => { console.error('prepare failed:', e.message); app.exit(3); });

  // Hand the raw greyscale up to the page for encoding, one frame at a time, so
  // a 5 MB buffer is never serialised more often than necessary.
  const payloads = [];
  for (let i = 0; i < frames.length; i++) {
    if (WIRE === 'raw') {
      payloads.push(Buffer.from(frames[i].grey));
    } else {
      const b64 = frames[i].grey.toString('base64');
      const jpeg = await win.webContents.executeJavaScript(
        `window.__encode(${i}, ${JSON.stringify(b64)})`, true);
      payloads.push(Buffer.from(jpeg, 'base64'));
    }
    console.error(`  frame ${i}: ${(payloads[i].length / 1024).toFixed(1)} KB on the wire`);
  }

  const wsrv = startWs(PORT);
  const total = Math.round(FPS * SECONDS);

  // Main-thread-free is not CPU-free: ImageDecoder and the GPU process both
  // burn cycles somewhere, and the target machine is a 2-core Surface Go 3 that
  // is also running the inspection core. app.getAppMetrics() reports per-process
  // CPU as a percentage of ONE core, so the sum across Electron's processes is
  // what actually competes with the core.
  const cpuSamples = [];
  const cpuTimer = setInterval(() => {
    let sum = 0;
    const per = {};
    for (const m of app.getAppMetrics()) {
      const pc = (m.cpu && m.cpu.percentCPUUsage) || 0;
      sum += pc;
      per[m.type] = (per[m.type] || 0) + pc;
    }
    let rss = 0;
    for (const m of app.getAppMetrics()) rss += (m.memory && m.memory.workingSetSize) || 0;
    cpuSamples.push({ sum, per, rssMB: rss / 1024 });
  }, 400);

  // --- trace capture ------------------------------------------------------
  //
  // The 120 s run recorded a single event that blocked the main thread 306 ms,
  // pushed end-to-end to 326 ms and opened a 320 ms hole in arrivals, one or
  // two times per run. The note written at the time said "most likely GC of
  // the large buffers", which was a guess with nothing behind it -- every one
  // of those three numbers is measured from the main thread, so anything that
  // stops the main thread produces all three, GC or not.
  //
  // A Chromium trace names the event instead of inferring it. v8 and v8.gc
  // cover collection; devtools.timeline covers the task, the decode and the
  // upload; blink.user_timing carries the lab's own marks. The post-pass keeps
  // only what ran longer than LAB_TRACE_MS so the output is the handful of
  // events that matter rather than a million routine ones.
  const TRACE = process.env.LAB_TRACE === '1';
  const TRACE_MS = Number(process.env.LAB_TRACE_MS || 50);
  const traceEvents = [];
  let dbg = null;
  if (TRACE) {
    dbg = win.webContents.debugger;
    try {
      dbg.attach('1.3');
      dbg.on('message', (_e, method, params) => {
        if (method === 'Tracing.dataCollected' && params && params.value) {
          for (const ev of params.value) traceEvents.push(ev);
        }
      });
      await dbg.sendCommand('Tracing.start', {
        transferMode: 'ReportEvents',
        traceConfig: {
          recordMode: 'recordContinuously',
          includedCategories: [
            'v8', 'v8.gc', 'v8.execute', 'devtools.timeline',
            'disabled-by-default-devtools.timeline', 'blink.user_timing',
            'toplevel', 'latency',
            // The stall is on CrGpuMain, and none of the categories above name
            // what that thread is doing -- they only show the task wrapper.
            'gpu', 'viz', 'cc', 'disabled-by-default-gpu.device',
            'disabled-by-default-gpu.service', 'gpu.capture',
          ],
        },
      });
      console.error('trace: recording');
    } catch (e) {
      console.error('trace: could not start -- ' + e.message);
      dbg = null;
    }
  }

  const done = new Promise((resolve) => ipcMain.once('lab-done', (_e, d) => resolve(d)));
  win.webContents.send('lab-start', {
    port: PORT, pipeline: PIPELINE, wire: WIRE, w, h, expect: total, header: HEADER,
    concurrency: CONC,
  });
  for (let i = 0; i < 200 && !wsrv.connected(); i++) await new Promise((r) => setTimeout(r, 25));
  if (!wsrv.connected()) { console.error('page never connected'); app.exit(4); return; }

  // Paced at the requested rate against a fixed schedule, NOT with a fixed
  // sleep between sends: a sleep-based loop silently slows down by however long
  // each send takes, and then reports the rate it achieved as though it were
  // the rate requested.
  const t0 = Date.now();
  let sent = 0, backpressure = 0;
  for (let i = 0; i < total; i++) {
    const due = t0 + (i * 1000) / FPS;
    const wait = due - Date.now();
    if (wait > 0) await new Promise((r) => setTimeout(r, wait));
    const body = payloads[i % payloads.length];
    const msg = Buffer.alloc(HEADER + body.length);
    msg.writeUInt32LE(0x5641_4c42 >>> 0, 0);
    msg.writeUInt32LE(i, 4);
    msg.writeDoubleLE(Date.now(), 8);
    body.copy(msg, HEADER);
    if (!wsrv.send(msg)) backpressure++;
    sent++;
  }
  win.webContents.send('lab-end');
  const r = await done;
  clearInterval(cpuTimer);

  if (dbg) {
    const flushed = new Promise((resolve) => {
      const h = (_e, method) => { if (method === 'Tracing.tracingComplete') {
        dbg.removeListener('message', h); resolve(); } };
      dbg.on('message', h);
    });
    try {
      await dbg.sendCommand('Tracing.end');
      await Promise.race([flushed, new Promise((r2) => setTimeout(r2, 20000))]);
    } catch (e) { console.error('trace: end failed -- ' + e.message); }
    try { dbg.detach(); } catch {}

    // Complete events carry `dur`; the async and instant ones do not, and a
    // zero there would sort to the top as though nothing was slow.
    const slow = traceEvents
      .filter((ev) => typeof ev.dur === 'number' && ev.dur >= TRACE_MS * 1000)
      .sort((x, y) => y.dur - x.dur);

    // Without the thread's NAME a pid/tid pair says nothing: a 375 ms task on
    // the GPU process's main thread stalls presentation, the same task on a
    // watchdog or memory-dump thread stalls nothing. Chromium emits both names
    // as metadata events, so keep them.
    const names = {};
    for (const ev of traceEvents) {
      if (ev.ph !== 'M') continue;
      if (ev.name === 'thread_name') names[ev.pid + '/' + ev.tid] = ev.args && ev.args.name;
      if (ev.name === 'process_name') names[ev.pid + '/*'] = ev.args && ev.args.name;
    }
    // The slow list alone names the CONTAINER ("GPUTask, 430 ms") and not the
    // work. The children were filtered out by the threshold, so for the single
    // worst task keep every event on its thread inside its window -- that is
    // where the actual operation is named.
    let inside = [];
    if (slow.length) {
      const w = slow[0];
      inside = traceEvents
        .filter((ev) => ev.pid === w.pid && ev.tid === w.tid
                     && typeof ev.ts === 'number'
                     && ev.ts >= w.ts && ev.ts <= w.ts + w.dur)
        .sort((x, y) => x.ts - y.ts)
        .map((ev) => ({
          rel_ms: +(((ev.ts - w.ts) / 1000)).toFixed(3),
          ms: typeof ev.dur === 'number' ? +(ev.dur / 1000).toFixed(3) : null,
          name: ev.name, cat: ev.cat, ph: ev.ph, args: ev.args,
        }));
    }

    const out = path.join(process.env.LAB_OUT || __dirname, 'trace_slow.json');
    fs.writeFileSync(out, JSON.stringify({
      totalEvents: traceEvents.length, thresholdMs: TRACE_MS,
      worstWindow: inside,
      slow: slow.slice(0, 200).map((ev) => ({
        ms: +(ev.dur / 1000).toFixed(2), name: ev.name, cat: ev.cat, ph: ev.ph,
        ts: ev.ts, pid: ev.pid, tid: ev.tid,
        proc: names[ev.pid + '/*'], thread: names[ev.pid + '/' + ev.tid],
        args: ev.args,
      })),
    }, null, 1));
    console.error(`trace: ${traceEvents.length} events, `
                + `${slow.length} over ${TRACE_MS} ms -> ${out}`);
    for (const ev of slow.slice(0, 12)) {
      console.error(`   ${String((ev.dur / 1000).toFixed(1)).padStart(7)} ms  `
                  + `${ev.name}   [${names[ev.pid + '/*'] || ev.pid}`
                  + ` / ${names[ev.pid + '/' + ev.tid] || ev.tid}]`);
    }
  }

  // Drop the first two samples: they cover startup and the JPEG encoding, not
  // the streaming run.
  const useful = cpuSamples.slice(2);
  const cpuSum = useful.map((s2) => s2.sum).sort((a, b) => a - b);
  const byType = {};
  for (const s2 of useful) for (const k of Object.keys(s2.per)) (byType[k] = byType[k] || []).push(s2.per[k]);
  const cpu = {
    total_pct_of_one_core_p50: cpuSum.length ? +cpuSum[Math.floor(cpuSum.length * .5)].toFixed(1) : null,
    total_pct_of_one_core_max: cpuSum.length ? +cpuSum[cpuSum.length - 1].toFixed(1) : null,
    by_process: Object.fromEntries(Object.entries(byType).map(([k, v]) => {
      const s3 = v.slice().sort((a, b) => a - b);
      return [k, +s3[Math.floor(s3.length * .5)].toFixed(1)];
    })),
  };

  // Memory across the whole run, not just at the end: a leak that is flat for
  // the first half and climbs in the second half looks fine in a single
  // end-of-run reading.
  const rssSeries = useful.map((s2) => Math.round(s2.rssMB));
  cpu.rss_MB_first = rssSeries[0] ?? null;
  cpu.rss_MB_last = rssSeries[rssSeries.length - 1] ?? null;
  cpu.rss_MB_max = rssSeries.length ? Math.max(...rssSeries) : null;

  const wallS = (Date.now() - t0) / 1000;
  process.stdout.write('\n@@RESULT@@' + JSON.stringify({
    wire: WIRE, quality: WIRE === 'jpeg' ? QUALITY : null, pipeline: PIPELINE,
    requested_fps: FPS, seconds: +wallS.toFixed(2),
    frame_KB: +(payloads[0].length / 1024).toFixed(1),
    sent, backpressure_events: backpressure, cpu,
    ...r,
  }) + '\n');

  if (args.capture) {
    // capturePage() renders the window offscreen too, so a picture of the
    // result can be taken without a visible desktop session.
    const img = await win.webContents.capturePage();
    fs.writeFileSync(String(args.capture), img.toPNG());
    console.error('captured ' + args.capture);
  }

  if (SHOW || DEVTOOLS) {
    console.error('\nwindow left open for inspection -- close it to exit');
    win.on('closed', () => app.exit(0));
  } else {
    wsrv.close();
    app.exit(0);
  }
});
