// Every way Chromium will turn a 5 MP JPEG into something drawable, timed.
//
//   node decode_paths_bench.mjs [--headed]
//
// The question this answers: can JPEG stay on the wire (93 KB/frame instead of
// 4.8 MB) if the decode goes somewhere cheaper than createImageBitmap's 30.8 ms?
//
//   1  createImageBitmap(Blob)              the current path
//   2  ImageDecoder (WebCodecs)             may reach a hardware JPEG decoder
//   3  ImageDecoder -> VideoFrame -> WebGL  skips the RGBA conversion entirely
//   4  <img>.decode()                       the media pipeline, as MJPEG uses
//   5  createImageBitmap in a Worker        same cost, but off the main thread
//
// 5 is the one to read carefully. If decode cannot be made cheaper it can still
// be made someone else's problem -- but only if there is a spare core, and the
// target machine is a 2-core Surface Go 3 with an inspection core on it.
//
// --headed for a real GPU: headless falls back to SwiftShader and any
// hardware-decode question becomes meaningless.
import { chromium } from 'playwright';

const HEADED = process.argv.includes('--headed');
const browser = await chromium.launch({
  headless: !HEADED,
  args: ['--enable-features=SharedArrayBuffer'],
});
const page = await (await browser.newContext()).newPage();
page.on('console', (m) => { if (m.type() === 'error') console.log('[page]', m.text()); });

const out = await page.evaluate(async () => {
  const W = 2448, H = 2048, DW = 1600, DH = 950, N = 20;
  const res = { support: {}, results: {} };

  res.support.ImageDecoder = typeof ImageDecoder !== 'undefined';
  res.support.VideoFrame = typeof VideoFrame !== 'undefined';
  res.support.SharedArrayBuffer = typeof SharedArrayBuffer !== 'undefined';
  res.support.crossOriginIsolated = self.crossOriginIsolated;

  // --- source ---------------------------------------------------------------
  const src = document.createElement('canvas');
  src.width = W; src.height = H;
  const sx = src.getContext('2d');
  sx.fillStyle = '#fff'; sx.fillRect(0, 0, W, H);
  sx.fillStyle = '#111';
  sx.beginPath(); sx.ellipse(W * .5, H * .5, W * .17, H * .22, .3, 0, Math.PI * 2); sx.fill();
  const id = sx.getImageData(0, 0, W, H);
  for (let i = 0; i < id.data.length; i += 4) {
    const n = (Math.imul(i, 2654435761) >>> 24) % 7 - 3;
    id.data[i] += n; id.data[i + 1] += n; id.data[i + 2] += n;
  }
  sx.putImageData(id, 0, 0);
  const blob = await new Promise((r) => src.toBlob(r, 'image/jpeg', 0.85));
  const bytes = new Uint8Array(await blob.arrayBuffer());
  res.jpeg_KB = +(bytes.byteLength / 1024).toFixed(1);

  const dst = document.createElement('canvas');
  dst.width = DW; dst.height = DH;
  const dx = dst.getContext('2d');

  const avg = async (fn) => {
    await fn();                       // warm
    const t0 = performance.now();
    for (let i = 0; i < N; i++) await fn();
    return +((performance.now() - t0) / N).toFixed(2);
  };

  // --- 1 createImageBitmap ----------------------------------------------------
  res.results.createImageBitmap = await avg(async () => {
    const b = await createImageBitmap(blob);
    dx.drawImage(b, 0, 0, DW, DH);
    b.close();
  });

  // --- 2 / 3 WebCodecs ImageDecoder -------------------------------------------
  if (res.support.ImageDecoder) {
    try {
      const sup = await ImageDecoder.isTypeSupported('image/jpeg');
      res.support.jpegDecoderSupported = sup;
      if (sup) {
        res.results.imageDecoder_to2d = await avg(async () => {
          const dec = new ImageDecoder({ data: bytes, type: 'image/jpeg' });
          const { image } = await dec.decode();
          dx.drawImage(image, 0, 0, DW, DH);
          image.close();
          dec.close();
        });

        // A VideoFrame goes to the GPU as a texture without ever being
        // converted to RGBA in JS -- the part that makes path C in the other
        // bench so slow.
        const gc = document.createElement('canvas');
        gc.width = DW; gc.height = DH;
        const gl = gc.getContext('webgl2');
        if (gl) {
          const tex = gl.createTexture();
          gl.bindTexture(gl.TEXTURE_2D, tex);
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
          const dbg = gl.getExtension('WEBGL_debug_renderer_info');
          res.renderer = dbg ? gl.getParameter(dbg.UNMASKED_RENDERER_WEBGL) : gl.getParameter(gl.RENDERER);
          const px = new Uint8Array(4);
          res.results.imageDecoder_toWebGL = await avg(async () => {
            const dec = new ImageDecoder({ data: bytes, type: 'image/jpeg' });
            const { image } = await dec.decode();
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, image);
            gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
            image.close();
            dec.close();
          });
          gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, px);
        }
      }
    } catch (e) {
      res.results.imageDecoder_error = String(e.message || e);
    }
  }

  // --- 4 <img>.decode() --------------------------------------------------------
  {
    const url = URL.createObjectURL(blob);
    res.results.imgDecode = await avg(async () => {
      const im = new Image();
      im.src = url;
      await im.decode();
      dx.drawImage(im, 0, 0, DW, DH);
    });
    URL.revokeObjectURL(url);
  }

  // --- 5 createImageBitmap inside a Worker -------------------------------------
  //
  // Measured as WALL CLOCK from the main thread, including the round trip and
  // the ImageBitmap transfer, because that is what the UI actually waits for.
  // The point is not that it is faster; it is whether the main thread is free
  // during it.
  {
    const code = `
      onmessage = async (e) => {
        const t0 = performance.now();
        const bmp = await createImageBitmap(e.data);
        postMessage({ bmp, workerMs: performance.now() - t0 }, [bmp]);
      };`;
    const wurl = URL.createObjectURL(new Blob([code], { type: 'text/javascript' }));
    const w = new Worker(wurl);
    const once = () => new Promise((resolve) => {
      w.onmessage = (e) => resolve(e.data);
      w.postMessage(blob);
    });
    let workerMs = 0;
    const wall = await avg(async () => {
      const r = await once();
      workerMs += r.workerMs;
      dx.drawImage(r.bmp, 0, 0, DW, DH);
      r.bmp.close();
    });
    res.results.worker_wallclock = wall;
    res.results.worker_decodeOnly = +(workerMs / (N + 1)).toFixed(2);
    w.terminate();
    URL.revokeObjectURL(wurl);
  }

  dx.getImageData(DW >> 1, DH >> 1, 1, 1);
  return res;
});

console.log(JSON.stringify(out, null, 1));
await browser.close();
