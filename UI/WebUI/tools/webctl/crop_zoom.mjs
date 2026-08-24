// crop_zoom -- magnify a region of a PNG, because the thing we need to judge on
// the inspection canvas is ~60 px across and the question ("is that a part or
// an overlay box?") cannot be answered at that size.
//
//   node crop_zoom.mjs <in.png> <out.png> <x> <y> <w> <h> [zoom]
//
// Uses Chromium (already a dependency for the bringup scripts) rather than
// adding an image library to the bench machine.
import { chromium } from 'playwright';
import fs from 'node:fs';

const [inp, outp, X, Y, W, H, Z = 6] = process.argv.slice(2);
if (!outp) { console.error('usage: crop_zoom.mjs in.png out.png x y w h [zoom]'); process.exit(2); }
const b64 = fs.readFileSync(inp).toString('base64');

const br = await chromium.launch();
const pg = await br.newPage();
const png = await pg.evaluate(async ({ b64, x, y, w, h, z }) => {
  const img = new Image();
  img.src = 'data:image/png;base64,' + b64;
  await img.decode();
  const c = document.createElement('canvas');
  c.width = w * z; c.height = h * z;
  const g = c.getContext('2d');
  g.imageSmoothingEnabled = false;          // show the real pixels, not a blur
  g.drawImage(img, x, y, w, h, 0, 0, w * z, h * z);
  return c.toDataURL('image/png').split(',')[1];
}, { b64, x: +X, y: +Y, w: +W, h: +H, z: +Z });
fs.writeFileSync(outp, Buffer.from(png, 'base64'));
await br.close();
console.log(`${outp}: ${X},${Y} ${W}x${H} at ${Z}x`);
