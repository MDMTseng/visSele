// Rescale a PNG by a factor, for magnification-portability tests.
//
//   node img_scale.mjs <in.png> <out.png> <factor>
//
// Chromium rather than an image library: this bench machine has neither PIL nor
// sharp, and crop_zoom.mjs already established the idiom.
//
// The resample is the browser's default (smooth), which is what a real optical
// magnification change looks like far better than nearest-neighbour would --
// the point of the test is that the LOCATOR copes with a genuinely smaller
// part, not that it copes with aliasing artefacts.
import { chromium } from 'playwright';
import fs from 'node:fs';

const [inp, outp, F] = process.argv.slice(2);
if (!outp) { console.error('usage: img_scale.mjs in.png out.png factor'); process.exit(2); }
const f = Number(F);
const b64 = fs.readFileSync(inp).toString('base64');

const br = await chromium.launch();
const pg = await br.newPage();
const png = await pg.evaluate(async ({ b64, f }) => {
  const img = new Image();
  img.src = 'data:image/png;base64,' + b64;
  await img.decode();
  const w = Math.max(1, Math.round(img.naturalWidth * f));
  const h = Math.max(1, Math.round(img.naturalHeight * f));
  const c = document.createElement('canvas');
  c.width = w; c.height = h;
  const x = c.getContext('2d');
  x.imageSmoothingEnabled = true;
  x.imageSmoothingQuality = 'high';
  x.drawImage(img, 0, 0, w, h);
  return { d: c.toDataURL('image/png').split(',')[1], w, h,
           sw: img.naturalWidth, sh: img.naturalHeight };
}, { b64, f });
fs.writeFileSync(outp, Buffer.from(png.d, 'base64'));
console.log(`${inp} ${png.sw}x${png.sh}  --x${f}-->  ${outp} ${png.w}x${png.h}`);
await br.close();
