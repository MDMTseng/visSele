// Render the visSele plate deck to a folder of JPGs.
//
// The point is NOT a pretty export -- it is that a fresh agent session can
// `Read` a plate and see the whole diagram, so every shot is a full section at
// 2x, light theme (dark ground JPEGs read badly when a tool downscales them),
// and wide enough that nothing inside wraps to a phone layout.
//
//   node tools/webctl/plates_export.mjs <page.html> <outDir>
import { chromium } from 'playwright';
import { pathToFileURL } from 'node:url';
import { mkdirSync, writeFileSync } from 'node:fs';
import { resolve, join } from 'node:path';

const src = resolve(process.argv[2]);
const out = resolve(process.argv[3]);
mkdirSync(out, { recursive: true });

const browser = await chromium.launch();
const page = await browser.newPage({
  viewport: { width: 1180, height: 1200 },
  deviceScaleFactor: 2,
  colorScheme: 'light',
});

await page.goto(pathToFileURL(src).href, { waitUntil: 'load' });
// Webfonts come from Google; if the bench is offline the fallback stack is
// fine, so this is a wait with a floor, not a requirement.
await page.evaluate(() => document.fonts.ready).catch(() => {});
await page.waitForTimeout(1200);

// Force light: a JPEG has no theme, and the light palette is the one drawn for
// paper. Also drop the sticky index -- it would overlay the top of each shot.
await page.evaluate(() => {
  document.documentElement.setAttribute('data-theme', 'light');
  const nav = document.querySelector('nav.idx');
  if (nav) nav.style.display = 'none';
});

// id -> ASCII slug. Kept explicit rather than transliterated so the names stay
// stable when a plate's heading is reworded, and so nothing in this folder
// carries CJK punctuation -- the reader here is a shell or an agent's file
// tool, and the Chinese title survives in INDEX.tsv where it is read anyway.
const SLUG = {
  p1: 'machine', p2: 'five-layers', p3: 'ports', p4: 'boot-and-update',
  p5: 'frame-journey', p6: 'bpg-contract', p7: 'pairing', p8: 'board-state',
  p9: 'threads-and-locks', p10: 'config-ownership', p11: 'production-safety',
  p12: 'code-webui', p13: 'code-core', p14: 'code-firmware',
};

const shots = [];

// The masthead becomes plate 00 so the set opens with what the machine is.
const mast = await page.$('header.mast');
if (mast) {
  shots.push({
    el: mast, name: '00-cover', title: 'visSele 圖解機台（封面）',
    lede: '這一疊圖是什麼、來源與校準日期。',
  });
}

const plateEls = await page.$$('section.plate');
for (const el of plateEls) {
  const id = await el.getAttribute('id');                    // p1 .. p11
  const n = String(id || '').replace(/^p/, '').padStart(2, '0');
  const title = (await el.$eval('h2', (h) => h.textContent.trim()).catch(() => '')) || id;
  const lede = await el.$eval('.lede', (p) => p.textContent.replace(/\s+/g, ' ').trim())
    .catch(() => '');
  shots.push({ el, name: `${n}-${SLUG[id] || id}`, title, lede });
}

const index = [];
for (const s of shots) {
  const file = `${s.name}.jpg`;
  await s.el.screenshot({
    path: join(out, file),
    type: 'jpeg',
    quality: 92,
    // A section's own background is transparent; paint the page ground under
    // it or the JPEG comes out black where the CSS never painted.
    style: 'section.plate, header.mast { background: #f2f4f6; padding: 22px 26px; }',
  });
  index.push(`${file}\t${s.title}\t${s.lede || ''}`);
  console.log('  ', file, '--', s.title);
}

writeFileSync(join(out, 'INDEX.tsv'), index.join('\n') + '\n', 'utf8');
await browser.close();
console.log(`\n${shots.length} plates -> ${out}`);
