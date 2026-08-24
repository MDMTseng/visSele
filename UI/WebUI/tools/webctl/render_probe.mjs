// Is the headless browser actually exercising GPU / decode / canvas, or is the
// soak measuring a software-rendered stand-in?
//
//   node render_probe.mjs [url]
//
// Deliberately does NOT enter the inspection view: whatever WS peer connects
// first gets the image stream, and a second browser could take it off a running
// soak. Everything here is a capability question about the rendering stack, so
// it needs no machine and no stream.
//
// Same launch options as soak6h.mjs, or the answer is about a different browser
// than the one being measured.
import { chromium } from 'playwright';

const URL = process.argv[2] || 'http://localhost:8082/';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const browser = await chromium.launch({ headless: true });
const page = await (await browser.newContext({ viewport: { width: 1600, height: 950 } })).newPage();
await page.goto(URL, { waitUntil: 'domcontentloaded' });
await sleep(4000);

const info = await page.evaluate(async () => {
  const out = {};

  // --- WebGL: the renderer string is the direct answer to "is this the GPU"
  try {
    const c = document.createElement('canvas');
    const gl = c.getContext('webgl2') || c.getContext('webgl');
    if (!gl) { out.gl = 'no webgl context'; }
    else {
      const dbg = gl.getExtension('WEBGL_debug_renderer_info');
      out.gl = {
        vendor: dbg ? gl.getParameter(dbg.UNMASKED_VENDOR_WEBGL) : gl.getParameter(gl.VENDOR),
        renderer: dbg ? gl.getParameter(dbg.UNMASKED_RENDERER_WEBGL) : gl.getParameter(gl.RENDERER),
        version: gl.getParameter(gl.VERSION),
      };
    }
  } catch (e) { out.gl = 'err ' + e.message; }

  // --- JPEG decode: the path the canvas component uses (createImageBitmap on a
  // Blob). Timed over several iterations because the first one includes setup.
  try {
    const cv = document.createElement('canvas');
    cv.width = 816; cv.height = 528;
    const cx = cv.getContext('2d');
    // Something with structure, so the encoder cannot trivially flatten it.
    const img = cx.createImageData(816, 528);
    for (let i = 0; i < img.data.length; i += 4) {
      const v = (i / 4) % 255;
      img.data[i] = v; img.data[i + 1] = (v * 3) % 255; img.data[i + 2] = 255 - v; img.data[i + 3] = 255;
    }
    cx.putImageData(img, 0, 0);
    const blob = await new Promise((r) => cv.toBlob(r, 'image/jpeg', 0.8));
    out.jpeg_bytes = blob.size;

    const N = 30;
    let t0 = performance.now();
    let bmp = null;
    for (let i = 0; i < N; i++) { bmp = await createImageBitmap(blob); if (i < N - 1) bmp.close(); }
    out.decode_ms_avg = +((performance.now() - t0) / N).toFixed(3);

    // --- blit: bitmap -> canvas, which is what the inspection view does
    const dst = document.createElement('canvas');
    dst.width = 1600; dst.height = 950;
    const dcx = dst.getContext('2d');
    t0 = performance.now();
    for (let i = 0; i < N; i++) dcx.drawImage(bmp, 0, 0, 1600, 950);
    out.blit_ms_avg = +((performance.now() - t0) / N).toFixed(3);
    // A readback forces the drawing to have actually happened -- without it the
    // loop above can be deferred and the timing means nothing.
    t0 = performance.now();
    const px = dcx.getImageData(800, 475, 1, 1).data;
    out.readback_ms = +(performance.now() - t0).toFixed(3);
    out.readback_px = Array.from(px);
    bmp.close();
  } catch (e) { out.decode = 'err ' + e.message; }

  // --- is the real inspection canvas present in this page at all?
  const cs = Array.from(document.querySelectorAll('canvas'));
  out.canvases = cs.map((c) => ({ w: c.width, h: c.height,
                                  cssW: Math.round(c.getBoundingClientRect().width) }));
  out.heapMB = performance.memory ? +(performance.memory.usedJSHeapSize / 1048576).toFixed(1) : null;
  return out;
});

console.log(JSON.stringify(info, null, 1));

// CDP gives what the page cannot see about itself.
const cdp = await page.context().newCDPSession(page);
try {
  await cdp.send('Performance.enable');
  const m = await cdp.send('Performance.getMetrics');
  const keep = ['Nodes', 'JSHeapUsedSize', 'JSHeapTotalSize', 'LayoutCount',
                'RecalcStyleCount', 'LayoutDuration', 'RecalcStyleDuration',
                'ScriptDuration', 'TaskDuration'];
  const got = {};
  for (const e of m.metrics) if (keep.includes(e.name)) got[e.name] = e.value;
  console.log('CDP Performance:', JSON.stringify(got));
} catch (e) { console.log('CDP Performance unavailable:', e.message); }
try {
  const d = await cdp.send('Memory.getDOMCounters');
  console.log('CDP DOMCounters:', JSON.stringify(d));
} catch (e) { console.log('CDP DOMCounters unavailable:', e.message); }

await browser.close();
