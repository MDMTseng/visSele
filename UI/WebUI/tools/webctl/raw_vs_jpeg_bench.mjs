// Is a raw-image transport + WebGL cheaper than JPEG + 2D canvas, for THIS
// station's frames?
//
//   node raw_vs_jpeg_bench.mjs [--headed] [--full]
//
// --full uses the sensor's own 2448x2048 (5.01 MP) instead of the 816x528
// preview crop -- the "cancel the ROI crop and stream the whole frame" case.
// Decode cost tracks PIXELS, so the two regimes are 11.6x apart and the answer
// is not the same in both.
//
// Compares, per frame, at the size the preview actually uses:
//
//   A  JPEG blob -> createImageBitmap -> drawImage into a 2D canvas   (today)
//   B  raw bytes -> texSubImage2D(R8) -> one textured quad in WebGL   (proposed)
//   C  raw bytes -> putImageData into a 2D canvas                     (control:
//        raw transport WITHOUT WebGL, to separate the two changes)
//
// Both B and C get the raw buffer handed to them as an ArrayBuffer, which is
// what a binary WebSocket frame delivers, so the comparison is decode+draw --
// the part that differs. Wire bytes are reported separately.
//
// READ THE RENDERER LINE BEFORE BELIEVING B. Headless Chromium falls back to
// SwiftShader, a SOFTWARE GL implementation; a WebGL number measured on it is
// not the number a Surface Go 3's Intel UHD would produce, it is a pessimistic
// stand-in. --headed tries for a real GPU.
import { chromium } from 'playwright';

const HEADED = process.argv.includes('--headed');
const FULL = process.argv.includes('--full');
const browser = await chromium.launch({ headless: !HEADED });
const page = await (await browser.newContext()).newPage();

const out = await page.evaluate(async (FULL) => {
  const W = FULL ? 2448 : 816, H = FULL ? 2048 : 528;
  const DW = 1600, DH = 950;       // what it is scaled up to on screen
  const N = FULL ? 20 : 60;
  const res = { notes: [] };

  // --- the source frame: backlit, so mostly saturated with a dark part ------
  const src = document.createElement('canvas');
  src.width = W; src.height = H;
  const sx = src.getContext('2d');
  sx.fillStyle = '#fff'; sx.fillRect(0, 0, W, H);
  sx.fillStyle = '#111';
  sx.beginPath();
  sx.ellipse(W * 0.5, H * 0.5, W * 0.17, H * 0.22, 0.3, 0, Math.PI * 2);
  sx.fill();
  const idata = sx.getImageData(0, 0, W, H);
  for (let i = 0; i < idata.data.length; i += 4) {
    const n = (Math.imul(i, 2654435761) >>> 24) % 7 - 3;
    idata.data[i] += n; idata.data[i + 1] += n; idata.data[i + 2] += n;
  }
  sx.putImageData(idata, 0, 0);

  // Mono raw, which is what the camera gives and what a raw transport would
  // actually carry -- one byte per pixel, not four.
  const mono = new Uint8Array(W * H);
  for (let p = 0; p < W * H; p++) mono[p] = idata.data[p * 4];

  const jpegBlob = await new Promise((r) => src.toBlob(r, 'image/jpeg', 0.85));
  res.wire = {
    jpeg_KB: +(jpegBlob.size / 1024).toFixed(1),
    raw_mono_KB: +(mono.byteLength / 1024).toFixed(1),
    ratio: +(mono.byteLength / jpegBlob.size).toFixed(1),
  };

  const time = async (fn) => {
    await fn();                     // warm up, so first-call setup is excluded
    const t0 = performance.now();
    for (let i = 0; i < N; i++) await fn();
    return (performance.now() - t0) / N;
  };

  // --- A: JPEG -> createImageBitmap -> drawImage ---------------------------
  {
    const dst = document.createElement('canvas');
    dst.width = DW; dst.height = DH;
    const dx = dst.getContext('2d');
    let decode = 0, draw = 0;
    const t0 = performance.now();
    for (let i = 0; i < N; i++) {
      const a = performance.now();
      const bmp = await createImageBitmap(jpegBlob);
      const b = performance.now();
      dx.drawImage(bmp, 0, 0, DW, DH);
      bmp.close();
      draw += performance.now() - b;
      decode += b - a;
    }
    dx.getImageData(DW >> 1, DH >> 1, 1, 1);      // force the work to land
    res.A_jpeg_2d = {
      total_ms: +((performance.now() - t0) / N).toFixed(3),
      decode_ms: +(decode / N).toFixed(3),
      draw_ms: +(draw / N).toFixed(3),
    };
  }

  // --- B: raw -> WebGL R8 texture -> quad ----------------------------------
  try {
    const gl_c = document.createElement('canvas');
    gl_c.width = DW; gl_c.height = DH;
    const gl = gl_c.getContext('webgl2', { antialias: false, preserveDrawingBuffer: false });
    if (!gl) throw new Error('no webgl2');

    const dbg = gl.getExtension('WEBGL_debug_renderer_info');
    res.renderer = dbg ? gl.getParameter(dbg.UNMASKED_RENDERER_WEBGL) : gl.getParameter(gl.RENDERER);

    const vs = gl.createShader(gl.VERTEX_SHADER);
    gl.shaderSource(vs, `#version 300 es
      const vec2 P[4] = vec2[4](vec2(-1.,-1.), vec2(1.,-1.), vec2(-1.,1.), vec2(1.,1.));
      out vec2 uv;
      void main(){ vec2 p = P[gl_VertexID]; uv = vec2(p.x*.5+.5, .5-p.y*.5); gl_Position = vec4(p,0.,1.); }`);
    gl.compileShader(vs);
    const fs = gl.createShader(gl.FRAGMENT_SHADER);
    gl.shaderSource(fs, `#version 300 es
      precision mediump float; uniform sampler2D T; in vec2 uv; out vec4 o;
      void main(){ float v = texture(T, uv).r; o = vec4(v,v,v,1.); }`);
    gl.compileShader(fs);
    const pr = gl.createProgram();
    gl.attachShader(pr, vs); gl.attachShader(pr, fs); gl.linkProgram(pr);
    if (!gl.getProgramParameter(pr, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(pr));
    gl.useProgram(pr);

    const tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    // Allocated ONCE. Re-allocating storage every frame is the classic way to
    // make a texture upload look expensive; a live view uploads into the same
    // storage each time.
    gl.texStorage2D(gl.TEXTURE_2D, 1, gl.R8, W, H);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
    gl.viewport(0, 0, DW, DH);

    let upload = 0;
    const px = new Uint8Array(4);
    const t0 = performance.now();
    for (let i = 0; i < N; i++) {
      const a = performance.now();
      gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, W, H, gl.RED, gl.UNSIGNED_BYTE, mono);
      upload += performance.now() - a;
      gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
    }
    // readPixels blocks until the GPU is done, so the loop above cannot be
    // credited with work that has not happened yet.
    gl.readPixels(DW >> 1, DH >> 1, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, px);
    res.B_raw_webgl = {
      total_ms: +((performance.now() - t0) / N).toFixed(3),
      upload_ms: +(upload / N).toFixed(3),
    };
  } catch (e) {
    res.B_raw_webgl = { error: String(e.message || e) };
  }

  // --- C: raw -> putImageData (raw transport, no WebGL) ---------------------
  {
    const dst = document.createElement('canvas');
    dst.width = W; dst.height = H;
    const dx = dst.getContext('2d');
    const img = dx.createImageData(W, H);
    const disp = document.createElement('canvas');
    disp.width = DW; disp.height = DH;
    const px = disp.getContext('2d');
    res.C_raw_2d_ms = +(await time(async () => {
      // mono -> RGBA expansion, which a 2D canvas cannot avoid
      const d = img.data;
      for (let p = 0, q = 0; p < mono.length; p++, q += 4) {
        d[q] = d[q + 1] = d[q + 2] = mono[p]; d[q + 3] = 255;
      }
      dx.putImageData(img, 0, 0);
      px.drawImage(dst, 0, 0, DW, DH);
    })).toFixed(3);
    px.getImageData(DW >> 1, DH >> 1, 1, 1);
  }

  return res;
}, FULL);

console.log(FULL ? '=== FULL FRAME 2448x2048' : '=== PREVIEW 816x528');
console.log(JSON.stringify(out, null, 1));
console.log('');
const fps = Number(process.env.FPS || 23);
const pct = (ms) => (ms * fps / 10).toFixed(1) + '% of one core at ' + fps + ' fps';
if (out.A_jpeg_2d) console.log('A jpeg + 2d   ', out.A_jpeg_2d.total_ms, 'ms ->', pct(out.A_jpeg_2d.total_ms));
if (out.B_raw_webgl?.total_ms) console.log('B raw + webgl ', out.B_raw_webgl.total_ms, 'ms ->', pct(out.B_raw_webgl.total_ms));
if (out.C_raw_2d_ms) console.log('C raw + 2d    ', out.C_raw_2d_ms, 'ms ->', pct(Number(out.C_raw_2d_ms)));
await browser.close();
