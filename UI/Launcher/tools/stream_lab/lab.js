// stream_lab renderer: receive frames, run one pipeline, measure honestly.
'use strict';
const { ipcRenderer } = require('electron');

const log = (m) => { const p = document.getElementById('log'); p.textContent += m + '\n'; console.log(m); };

// --- one-time frame preparation ------------------------------------------------
let PREP = null;
window.__prepare = (cfg) => { PREP = cfg; return true; };

// Grey bytes in (base64, because that is what survives executeJavaScript
// cheaply), JPEG bytes out. Runs before the timed section.
window.__encode = async (idx, b64) => {
  const bin = atob(b64);
  const grey = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) grey[i] = bin.charCodeAt(i);
  const c = document.createElement('canvas');
  c.width = PREP.w; c.height = PREP.h;
  const x = c.getContext('2d');
  const img = x.createImageData(PREP.w, PREP.h);
  const d = img.data;
  for (let p = 0, q = 0; p < grey.length; p++, q += 4) {
    d[q] = d[q + 1] = d[q + 2] = grey[p]; d[q + 3] = 255;
  }
  x.putImageData(img, 0, 0);
  const blob = await new Promise((r) => c.toBlob(r, 'image/jpeg', PREP.quality / 100));
  const buf = new Uint8Array(await blob.arrayBuffer());
  let s = '';
  for (let i = 0; i < buf.length; i += 0x8000) s += String.fromCharCode.apply(null, buf.subarray(i, i + 0x8000));
  return btoa(s);
};

// --- GL ---------------------------------------------------------------------------
let gl = null, texR8 = null, texRGBA = null;

function initGL(w, h) {
  const c = document.getElementById('view');
  gl = c.getContext('webgl2', { antialias: false, desynchronized: true });
  if (!gl) throw new Error('no webgl2');
  const dbg = gl.getExtension('WEBGL_debug_renderer_info');
  log('renderer: ' + (dbg ? gl.getParameter(dbg.UNMASKED_RENDERER_WEBGL) : gl.getParameter(gl.RENDERER)));

  const mk = (t, s) => {
    const sh = gl.createShader(t); gl.shaderSource(sh, s); gl.compileShader(sh);
    if (!gl.getShaderParameter(sh, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(sh));
    return sh;
  };
  const prog = gl.createProgram();
  gl.attachShader(prog, mk(gl.VERTEX_SHADER, `#version 300 es
    const vec2 P[4] = vec2[4](vec2(-1.,-1.), vec2(1.,-1.), vec2(-1.,1.), vec2(1.,1.));
    out vec2 uv;
    void main(){ vec2 p = P[gl_VertexID]; uv = vec2(p.x*.5+.5, .5-p.y*.5); gl_Position = vec4(p,0.,1.); }`));
  // One shader, two source formats: R8 for a raw mono upload, RGBA for a
  // decoded VideoFrame. `mono` picks which channel layout to read so the two
  // paths cannot differ by anything except the upload itself.
  gl.attachShader(prog, mk(gl.FRAGMENT_SHADER, `#version 300 es
    precision mediump float;
    uniform sampler2D T; uniform bool MONO; in vec2 uv; out vec4 o;
    void main(){ vec4 t = texture(T, uv); o = MONO ? vec4(vec3(t.r),1.) : vec4(t.rgb,1.); }`));
  gl.linkProgram(prog);
  if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(prog));
  gl.useProgram(prog);
  gl.uniform1i(gl.getUniformLocation(prog, 'T'), 0);
  window.__mono = gl.getUniformLocation(prog, 'MONO');

  const mkTex = () => {
    const t = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, t);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    return t;
  };
  texR8 = mkTex();
  // Storage allocated ONCE. texImage2D per frame reallocates, which shows up as
  // a slower "upload" that is really an allocation.
  gl.texStorage2D(gl.TEXTURE_2D, 1, gl.R8, w, h);
  gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
  texRGBA = mkTex();
  gl.texStorage2D(gl.TEXTURE_2D, 1, gl.RGBA8, w, h);
  gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
}

function resizeCanvasTo(w, h) {
  const c = document.getElementById('view');
  const box = c.parentElement.getBoundingClientRect();
  const s = Math.min(box.width / w, box.height / h);
  c.width = Math.max(1, Math.round(w * s));
  c.height = Math.max(1, Math.round(h * s));
}

// --- measurement -------------------------------------------------------------------
const M = { e2e: [], busy: [], gaps: [], seqs: [], recv: 0, decodeFail: 0, late: [] };
let lastArrive = 0;

// --- main-thread responsiveness ------------------------------------------------
//
// `busy` above is wall time from the start of handling a frame to the end of
// it, and an `await` in the middle means the main thread was IDLE for part of
// that -- both ImageDecoder and a Worker decode elsewhere and resolve a promise
// here. So `busy` cannot tell "the main thread was occupied" from "the main
// thread was waiting", which is exactly the difference between the pipelines.
//
// This heartbeat can. A timer asks to run every 4 ms; whatever it is LATE by is
// time the main thread was not free to run it. That is what a click, a scroll
// or an overlay redraw would have waited for, so it is the number an operator
// actually feels.
let hbTimer = null;
function startHeartbeat() {
  const PERIOD = 4;
  let due = performance.now() + PERIOD;
  hbTimer = setInterval(() => {
    const now = performance.now();
    M.late.push(Math.max(0, now - due));
    due = now + PERIOD;
  }, PERIOD);
}
function stopHeartbeat() { if (hbTimer) { clearInterval(hbTimer); hbTimer = null; } }

function stat(a) {
  if (!a.length) return null;
  const s = [...a].sort((x, y) => x - y);
  return {
    avg: +(a.reduce((p, c) => p + c, 0) / a.length).toFixed(2),
    p50: +s[Math.floor(s.length * .5)].toFixed(2),
    p95: +s[Math.floor(s.length * .95)].toFixed(2),
    max: +s[s.length - 1].toFixed(2),
  };
}

// --- pipelines ------------------------------------------------------------------------
let worker = null;

// `seq` is passed so a pipeline can refuse to paint a frame that a newer one
// has already overtaken. draw() below is the only place that checks it.
function newerThanScreen(seq) {
  if (seq < shownSeq) { stale++; return false; }
  shownSeq = seq;
  return true;
}

async function handle(pipeline, body, w, h, seq) {
  switch (pipeline) {
    case 'bitmap2d': {
      const bmp = await createImageBitmap(new Blob([body], { type: 'image/jpeg' }));
      const c2 = document.getElementById('view2d');
      c2.getContext('2d').drawImage(bmp, 0, 0, c2.width, c2.height);
      bmp.close();
      return;
    }
    case 'decoder2d': {
      const dec = new ImageDecoder({ data: body, type: 'image/jpeg' });
      const { image } = await dec.decode();
      const c2 = document.getElementById('view2d');
      c2.getContext('2d').drawImage(image, 0, 0, c2.width, c2.height);
      image.close(); dec.close();
      return;
    }
    case 'decoderGL': {
      const dec = new ImageDecoder({ data: body, type: 'image/jpeg' });
      const { image } = await dec.decode();
      if (!window.__fmtLogged) {
        window.__fmtLogged = true;
        log(`VideoFrame format=${image.format} coded=${image.codedWidth}x${image.codedHeight} `
          + `visible=${JSON.stringify(image.visibleRect)} `
          + `allocSize(default)=${image.allocationSize ? image.allocationSize() : 'n/a'}`);
        try {
          log('  plane 0 alone: ' + image.allocationSize({ rect: { x: 0, y: 0, width: image.codedWidth, height: image.codedHeight } }));
        } catch (e) { log('  allocationSize(rect) failed: ' + e.message); }
      }
      gl.bindTexture(gl.TEXTURE_2D, texRGBA);
      gl.uniform1i(window.__mono, 0);
      if (newerThanScreen(seq)) {
        gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, image);
        gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
      }
      image.close(); dec.close();
      return;
    }
    case 'decoderYGL': {
      // The decoded frame is I420: a full-resolution Y plane followed by
      // quarter-size U and V. The source is a GREY camera, so U and V are
      // constant 128 and carry nothing -- yet uploading the VideoFrame as RGBA
      // makes Chromium convert all three planes and push 20 MB to the GPU for
      // an image whose information is 4.8 MB.
      //
      // copyTo() cannot select a single plane, so this copies all 7.5 MB and
      // uploads only the Y prefix. A 7.5 MB memcpy costs about 1 ms; it buys
      // back 15 MB of GPU traffic per frame.
      const dec = new ImageDecoder({ data: body, type: 'image/jpeg' });
      const { image } = await dec.decode();
      const need = image.allocationSize();
      if (!window.__yBuf || window.__yBuf.byteLength < need) window.__yBuf = new Uint8Array(need);
      await image.copyTo(window.__yBuf);
      gl.bindTexture(gl.TEXTURE_2D, texR8);
      gl.uniform1i(window.__mono, 1);
      if (newerThanScreen(seq)) {
        gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, w, h, gl.RED, gl.UNSIGNED_BYTE,
                         window.__yBuf.subarray(0, w * h));
        gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
      }
      image.close(); dec.close();
      return;
    }
    case 'decoderRedGL': {
      // The same idea without the copy: hand the VideoFrame straight to
      // texSubImage2D but ask for a single-channel destination. If the driver
      // path allows it, this is 4.8 MB of upload and no CPU copy at all. If it
      // does not, it throws -- which is a fine answer, and better than a silent
      // fallback that would make the measurement meaningless.
      const dec = new ImageDecoder({ data: body, type: 'image/jpeg' });
      const { image } = await dec.decode();
      gl.bindTexture(gl.TEXTURE_2D, texR8);
      gl.uniform1i(window.__mono, 1);
      gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, w, h, gl.RED, gl.UNSIGNED_BYTE, image);
      gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
      image.close(); dec.close();
      return;
    }
    case 'workerGL': {
      // Decode off the main thread, then TRANSFER the VideoFrame back. A
      // VideoFrame is transferable, so the pixels do not cross the thread
      // boundary -- only the handle does. This is the configuration that should
      // leave the main thread with nothing but the GPU upload.
      const frame = await new Promise((resolve, reject) => {
        const id = Math.random();
        const onMsg = (e) => {
          if (e.data.id !== id) return;
          worker.removeEventListener('message', onMsg);
          e.data.error ? reject(new Error(e.data.error)) : resolve(e.data.frame);
        };
        worker.addEventListener('message', onMsg);
        worker.postMessage({ id, bytes: body }, [body.buffer]);
      });
      gl.bindTexture(gl.TEXTURE_2D, texRGBA);
      gl.uniform1i(window.__mono, 0);
      gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, frame);
      gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
      frame.close();
      return;
    }
    case 'rawGL': {
      gl.bindTexture(gl.TEXTURE_2D, texR8);
      gl.uniform1i(window.__mono, 1);
      gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, w, h, gl.RED, gl.UNSIGNED_BYTE, body);
      gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
      return;
    }
    default:
      throw new Error('unknown pipeline ' + pipeline);
  }
}

// --- run -----------------------------------------------------------------------------
let CFG = null, ws = null, inFlight = 0, ended = false, CONC = 1, shownSeq = -1;
let stale = 0;

ipcRenderer.on('lab-start', async (_e, cfg) => {
  CFG = cfg;
  CONC = cfg.concurrency || 1;
  document.getElementById('hdr').textContent =
    `${cfg.wire} -> ${cfg.pipeline}   ${cfg.w}x${cfg.h}`;
  const use2d = cfg.pipeline === 'bitmap2d' || cfg.pipeline === 'decoder2d';
  document.getElementById('view').style.display = use2d ? 'none' : '';
  document.getElementById('view2d').style.display = use2d ? '' : 'none';
  resizeCanvasTo(cfg.w, cfg.h);
  const c2 = document.getElementById('view2d');
  { const box = c2.parentElement.getBoundingClientRect();
    const s = Math.min(box.width / cfg.w, box.height / cfg.h);
    c2.width = Math.round(cfg.w * s); c2.height = Math.round(cfg.h * s); }

  if (!use2d) { try { initGL(cfg.w, cfg.h); } catch (e) { log('GL init failed: ' + e.message); } }

  if (cfg.pipeline === 'workerGL') {
    worker = new Worker('worker.js');
  }
  if (cfg.pipeline.startsWith('decoder') || cfg.pipeline === 'workerGL') {
    if (typeof ImageDecoder === 'undefined') { log('ImageDecoder UNAVAILABLE in this build'); }
    else log('ImageDecoder jpeg supported: ' + await ImageDecoder.isTypeSupported('image/jpeg'));
  }

  startHeartbeat();
  ws = new WebSocket(`ws://127.0.0.1:${cfg.port}/`);
  ws.binaryType = 'arraybuffer';
  ws.onmessage = async (ev) => {
    const arrive = performance.now();
    const dv = new DataView(ev.data);
    const seq = dv.getUint32(4, true);
    const sentMs = dv.getFloat64(8, true);
    const body = new Uint8Array(ev.data, cfg.header);

    M.seqs.push(seq);
    if (lastArrive) M.gaps.push(arrive - lastArrive);
    lastArrive = arrive;

    // A frame that arrives while the previous one is still being processed is
    // DROPPED, exactly as a live view must drop it -- queueing would turn a
    // throughput shortfall into unbounded latency, and the picture would drift
    // further behind the machine the longer it ran.
    // Concurrency policy.
    //
    // With CONC = 1 a frame arriving while another is being decoded is dropped.
    // That is correct but wasteful here: ImageDecoder does its work off the
    // main thread, so while one frame is decoding the main thread is idle and a
    // second decode could already be under way.
    //
    // Allowing more in flight raises throughput but reintroduces ORDERING as a
    // problem -- two decodes can finish out of order, and a live view that
    // paints an older frame over a newer one is worse than one that drops.
    // So the draw is gated on the sequence number: a frame older than what is
    // already on screen is decoded and thrown away, never painted.
    if (inFlight >= CONC) return;
    inFlight++;
    const t0 = performance.now();
    try {
      await handle(cfg.pipeline, body, cfg.w, cfg.h, seq);
    } catch (e) {
      M.decodeFail++;
      if (M.decodeFail < 3) log('pipeline error: ' + e.message);
    }
    const t1 = performance.now();
    inFlight--;
    M.recv++;
    M.busy.push(t1 - t0);
    M.e2e.push(Date.now() - sentMs);
  };
  ws.onerror = () => log('websocket error');
});

ipcRenderer.on('lab-end', () => {
  if (ended) return;
  ended = true;
  // Let anything still in flight finish before reporting.
  stopHeartbeat();
  setTimeout(() => {
    let ordered = true;
    for (let i = 1; i < M.seqs.length; i++) if (M.seqs[i] <= M.seqs[i - 1]) ordered = false;
    const span = M.gaps.reduce((p, c) => p + c, 0) / 1000;
    const out = {
      arrived: M.seqs.length,
      rendered: M.recv,
      dropped_by_backlog: M.seqs.length - M.recv,
      pipeline_errors: M.decodeFail,
      concurrency: CONC,
      stale_discarded: stale,
      arrivals_in_order: ordered,
      arrived_fps: span > 0 ? +(M.seqs.length / span).toFixed(1) : null,
      rendered_fps: span > 0 ? +(M.recv / span).toFixed(1) : null,
      handle_wall_ms: stat(M.busy),
      main_thread_blocked_ms: stat(M.late),
      end_to_end_ms: stat(M.e2e),
      arrival_gap_ms: stat(M.gaps),
    };
    log(JSON.stringify(out, null, 1));
    ipcRenderer.send('lab-done', out);
  }, 400);
});
