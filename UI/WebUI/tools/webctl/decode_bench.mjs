// What does a frame actually cost to decode, and where does the time go?
//
//   node decode_bench.mjs
//
// Three things the earlier 8.32 ms number could not separate:
//   * content -- a gradient is near worst case for JPEG; this station is
//     BACKLIT, so a real frame is mostly saturated white with a dark part, and
//     compresses very differently
//   * size -- decode cost tracks PIXELS, so downsampling is the direct lever
//   * wrapper -- createImageBitmap is decode PLUS blob handling and an RGBA
//     conversion; blaming all of it on "decode" overstates the codec
//
// No machine interaction: the images are synthesised in the page.
import { chromium } from 'playwright';

const browser = await chromium.launch({ headless: true });
const page = await (await browser.newContext()).newPage();

const rows = await page.evaluate(async () => {
  const mk = (w, h, kind) => {
    const c = document.createElement('canvas');
    c.width = w; c.height = h;
    const x = c.getContext('2d');
    if (kind === 'gradient') {
      const img = x.createImageData(w, h);
      for (let i = 0; i < img.data.length; i += 4) {
        const v = (i / 4) % 255;
        img.data[i] = v; img.data[i + 1] = (v * 3) % 255; img.data[i + 2] = 255 - v; img.data[i + 3] = 255;
      }
      x.putImageData(img, 0, 0);
    } else {
      // Backlit: saturated field, one dark part, a little sensor noise so the
      // encoder cannot collapse it to nothing.
      x.fillStyle = '#fff'; x.fillRect(0, 0, w, h);
      x.fillStyle = '#111';
      x.beginPath();
      x.ellipse(w * 0.5, h * 0.5, w * 0.17, h * 0.22, 0.3, 0, Math.PI * 2);
      x.fill();
      const img = x.getImageData(0, 0, w, h);
      for (let i = 0; i < img.data.length; i += 4) {
        const n = (Math.imul(i, 2654435761) >>> 24) % 7 - 3;
        img.data[i] += n; img.data[i + 1] += n; img.data[i + 2] += n;
      }
      x.putImageData(img, 0, 0);
    }
    return c;
  };

  const bench = async (w, h, kind, q) => {
    const c = mk(w, h, kind);
    const blob = await new Promise((r) => c.toBlob(r, 'image/jpeg', q));
    const buf = await blob.arrayBuffer();
    const N = 40;

    // createImageBitmap from a Blob: decode + wrapper, what the app does today
    let t0 = performance.now(); let bmp = null;
    for (let i = 0; i < N; i++) { bmp = await createImageBitmap(blob); if (i < N - 1) bmp.close(); }
    const full = (performance.now() - t0) / N;

    // Same bytes through an <img>: the media pipeline's decode, no Blob
    // plumbing in JS. MJPEG in an <img> is this path, one frame after another.
    const url = URL.createObjectURL(blob);
    t0 = performance.now();
    for (let i = 0; i < N; i++) {
      const im = new Image();
      im.src = url;
      await im.decode();
    }
    const viaImg = (performance.now() - t0) / N;
    URL.revokeObjectURL(url);

    // Draw cost, separated from decode
    const dst = document.createElement('canvas');
    dst.width = 1600; dst.height = 950;
    const dcx = dst.getContext('2d');
    t0 = performance.now();
    for (let i = 0; i < N; i++) dcx.drawImage(bmp, 0, 0, 1600, 950);
    dcx.getImageData(800, 475, 1, 1);          // force the work to happen
    const blit = (performance.now() - t0) / N;
    bmp.close();

    return { size: `${w}x${h}`, kind, q, kb: +(buf.byteLength / 1024).toFixed(1),
             bitmap_ms: +full.toFixed(2), img_decode_ms: +viaImg.toFixed(2),
             blit_ms: +blit.toFixed(3) };
  };

  const out = [];
  out.push(await bench(816, 528, 'gradient', 0.85));
  out.push(await bench(816, 528, 'backlit', 0.85));
  out.push(await bench(408, 264, 'backlit', 0.85));
  out.push(await bench(204, 132, 'backlit', 0.85));
  return out;
});

console.log('size      kind      q     KB   createImageBitmap  <img>.decode   drawImage');
for (const r of rows)
  console.log(`${r.size.padEnd(9)} ${r.kind.padEnd(9)} ${r.q}  ${String(r.kb).padStart(6)}  ` +
              `${String(r.bitmap_ms).padStart(13)} ms ${String(r.img_decode_ms).padStart(10)} ms ` +
              `${String(r.blit_ms).padStart(9)} ms`);
await browser.close();
