// Convert a def's contour-mode line/arc features to caliper mode, offline.
//
//   node tools/def_convert.mjs --in <dir> --out <dir> [--min-sagitta-px 3]
//
// WHY THIS IS IN THE WEBUI AND NOT THE CORE. Rewriting a recipe is the
// editor's job; the core reads defs, it does not author them. But a migration
// that cannot be measured before it is made is a migration nobody should run,
// so the rule is extracted into src/shapes/_caliperSeed.js and this tool
// imports it. It runs the SAME function the editor runs when somebody switches
// `locating` to 'caliper' -- a def converted here is byte-identical to one
// converted by hand, and there is no second implementation to drift.
//
// Pair it with InspectionCore/test_suite/def_compat.py: convert a corpus here,
// run that against the pre-conversion baseline, and read what the migration
// costs in millimetres before touching a machine.
//
// Originals are never modified.
import fs from 'node:fs';
import path from 'node:path';
import { seedCaliper, seedEdge, arcSagittaPx, geomLengthOf, ARC_MIN_SAGITTA_PX }
  from '../src/shapes/_caliperSeed.js';

// EDGE_SEED / ARC_POLARITY now live in _caliperSeed beside the caliper seed,
// because the WebUI save path needs the same two values. See seedEdge there for
// what each was measured against.

function parseArgs(argv) {
  const a = { minSagittaPx: ARC_MIN_SAGITTA_PX };
  for (let i = 2; i < argv.length; i++) {
    const k = argv[i];
    if (k === '--in') a.src = argv[++i];
    else if (k === '--out') a.dst = argv[++i];
    else if (k === '--min-sagitta-px') a.minSagittaPx = parseFloat(argv[++i]);
    else if (k === '--engine') a.engine = argv[++i];
    else { console.error('unknown argument: ' + k); process.exit(2); }
  }
  if (!a.src || !a.dst) {
    console.error('usage: node def_convert.mjs --in <dir> --out <dir> ' +
                  '[--min-sagitta-px 3] [--engine shape_based]');
    process.exit(2);
  }
  return a;
}

function convertFeature(f, mmpp, minSagittaPx) {
  if (f.type !== 'line' && f.type !== 'arc') return { changed: false };
  if (f.locating === 'caliper' || f.locating === 1) return { changed: false };

  const L = geomLengthOf(f);
  if (!(L > 0)) return { changed: false, note: 'no usable geometry -- left in contour mode' };

  if (f.type === 'arc') {
    const sag = arcSagittaPx(f, mmpp);
    if (sag !== null && sag < minSagittaPx) {
      return { changed: false, skipped: true, note:
        `SKIPPED -- taught sagitta ${sag.toFixed(1)}px (< ${minSagittaPx}): the three ` +
        'points are nearly collinear, so the caliper boxes would run nearly parallel ' +
        'instead of fanning around the feature. Have the arc re-taught; do not convert.' };
    }
  }

  f.locating = 'caliper';
  f.caliper = { ...(f.caliper || {}), ...seedCaliper(f) };
  f.edge = { ...seedEdge(f), ...(f.edge || {}) };
  return { changed: true, note:
    `${f.type} len=${L.toFixed(3)}mm -> count=${f.caliper.count} ` +
    `width=${f.caliper.width.toFixed(4)}mm` };
}

const a = parseArgs(process.argv);
fs.mkdirSync(a.dst, { recursive: true });
const names = fs.readdirSync(a.src).filter((n) => n.endsWith('.hydef')).sort();
if (!names.length) { console.error('no .hydef in ' + a.src); process.exit(2); }

let total = 0, totalSkipped = 0;
for (const name of names) {
  const base = name.slice(0, -6);
  const def = JSON.parse(fs.readFileSync(path.join(a.src, name), 'utf8'));
  const fs0 = (def.featureSet || [{}])[0];
  const mmpp = fs0.mmpp;
  let changed = 0, skipped = 0;
  console.log('== ' + base);
  for (const feat of fs0.features || []) {
    const r = convertFeature(feat, mmpp, a.minSagittaPx);
    if (r.note) console.log('   ' + String(feat.name ?? '').padEnd(18) + r.note);
    if (r.changed) changed++;
    if (r.skipped) skipped++;
  }
  if (a.engine) { fs0.locating_engine = a.engine; console.log('   locating_engine -> ' + a.engine); }
  console.log(`   ${changed} converted` + (skipped ? `, ${skipped} SKIPPED (needs re-teaching)` : ''));
  total += changed; totalSkipped += skipped;

  fs.writeFileSync(path.join(a.dst, name), JSON.stringify(def));
  // The image travels with the def: --insp needs the pair, and so does the
  // shape trainer.
  const png = path.join(a.src, base + '.png');
  if (fs.existsSync(png)) fs.copyFileSync(png, path.join(a.dst, base + '.png'));
}
console.log('');
console.log(`${total} converted, ${totalSkipped} skipped, across ${names.length} def(s) -> ${a.dst}`);
console.log('originals untouched. Next: def_compat.py --corpus <out> --ignore-def-sha1 ' +
            'against the pre-conversion baseline.');
