#!/usr/bin/env node
// What happens to an inspection report, asserted end to end.
//
//   node tools/pipeline_contract.mjs
//
// WHY THIS EXISTS, BEFORE ANY RESTRUCTURING
// -----------------------------------------
// The inspection path lives in a single 674-line switch case in
// UICtrlReducer.js -- half that file. Everything fixed there on 2026-08-26 (S1,
// S2, B4) had to be found by reading it line by line, because a switch case
// cannot be called with an input and checked.
//
// The obvious next move is to pull the computation out into named functions.
// The objection to that is real and today supplied the evidence for it: the
// worst bugs of the day were DUPLICATION, not size --
//
//   arc span            three copies, one correct, all in one file
//   stale-hit fingerprint  two copies, in two files; changing one alone
//                          would have hidden every caliper hit
//   caliper resolution  four copies across two files
//   "an empty statistic"   two copies, the second created that same morning
//
// Extraction does not prevent that. Nothing stops a second copy appearing in
// another file. What prevents it is a test that asserts the ANSWER, wherever
// the code lives -- which is what the geometry contract does, and what this
// does for the pipeline.
//
// So: this comes FIRST. Restructuring after it is a mechanical move with a
// check behind it. Restructuring before it is the process that produced the
// four bugs above.
//
// HOW IT RUNS
// -----------
// The reducer imports through vite aliases, so it is bundled with esbuild
// (same aliases, from vite.config.mjs) into a temp file and imported. That is
// deliberate: it exercises the REAL reducer, not a transcription of it. A
// transcription would agree with itself and with nothing else -- the same trap
// the geometry emitter avoids by calling the core's own function.
import { build } from 'esbuild';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '..');
const r = (p) => path.resolve(root, p);

const ENTRY = path.join(os.tmpdir(), `pipeline-entry-${process.pid}.mjs`);
const OUT = path.join(os.tmpdir(), `pipeline-bundle-${process.pid}.mjs`);

// Only the pieces the pipeline is made of. Importing the reducer itself pulls
// in the whole app (antd, react, the canvas); these are the units the case
// calls, and asserting them together is what makes the sequence checkable.
fs.writeFileSync(ENTRY, `
export { InspectionEditorLogic, effectiveLimits, MEASURERSULTRESION,
         MEASURERSULTRESION_reducer } from 'UTIL/InspectionEditorLogic';
export { statReducer, initMeasureStatistic } from 'REDUX_STORE_SRC/reducer/spcStats';
export { pickCtrlMargin } from 'UTIL/ctrlMarginPick';
export { shapeDefFingerprint, shapeDefProjection, defFileGeneration } from 'UTIL/MISC_Util';
export { INSPECTION_STATUS } from 'UTIL/BPG_Protocol';
`);

await build({
  entryPoints: [ENTRY],
  bundle: true,
  format: 'esm',
  platform: 'node',
  outfile: OUT,
  logLevel: 'silent',
  alias: {
    UTIL: r('src/UTIL'),
    JSSRCROOT: r('src'),
    LANG: r('src/languages'),
    RES: r('resource'),
    REDUX_STORE_SRC: r('src/redux'),
    STYLE: r('style'),
  },
  loader: { '.js': 'jsx', '.jsx': 'jsx', '.png': 'empty', '.svg': 'empty',
            '.css': 'empty', '.less': 'empty', '.ttf': 'empty' },
  define: { 'process.env.NODE_ENV': '"test"' },
  // jsum does a CJS require('crypto') and esbuild's ESM output has no require.
  // Hand it node's, rather than stubbing the hash -- the fingerprint under test
  // has to be the real one.
  banner: { js: "import { createRequire as __cr } from 'node:module';"
              + " const require = __cr(import.meta.url);" },
  external: ['node:crypto'],
});

const M = await import('file:///' + OUT.split(path.sep).join('/'));

let fails = 0;
const ok = (c, m, d = '') => {
  console.log((c ? 'PASS  ' : 'FAIL  ') + m + (d ? '  -- ' + d : ''));
  if (!c) fails++;
};

// --- the fixture ----------------------------------------------------------
// One measure, one 製程 that tightens it. Small on purpose: a vector nobody can
// read is a vector nobody maintains.
const ROOT_MEASURE = { id: 7, type: 'measure', name: 'width',
                       value: 10, USL: 11, LSL: 9, UCL: 10.8, LCL: 9.2,
                       quality_essential: true };
const TIGHT = { id: 7, value: 10, USL: 10.4, LSL: 9.6, UCL: 10.3, LCL: 9.7 };
const CTRL_MARGIN = { 'PRESS': [TIGHT] };

const judge = (value, status) => ({ id: 7, value, status });

console.log('=== the 製程 override reaches BOTH the verdict and the statistics ===');
{
  const picked = M.pickCtrlMargin(['PRESS'], CTRL_MARGIN);
  ok(picked.info === CTRL_MARGIN.PRESS, 'the tag selects its override', picked.tag);
  ok(picked.ambiguous.length === 0, 'and reports no ambiguity for a single match');

  // statistics: the same override, not the root
  const statistic = { measureList: [{ ...ROOT_MEASURE,
                        statistic: M.initMeasureStatistic(ROOT_MEASURE) }] };
  const report = { judgeReports: [judge(10.5, M.INSPECTION_STATUS.SUCCESS)] };
  M.statReducer(statistic, report, picked.info);
  const st = statistic.measureList[0].statistic;
  ok(st.count === 1, 'the sample was counted', String(st.count));
  // 10.5 is inside the ROOT limits (9..11) and outside the 製程 limits (9.6..10.4)
  ok(st.sp.SNG_count === 1,
     'a value inside the ROOT limits but outside the 製程 counts as NG',
     `SNG_count=${st.sp.SNG_count} (root would have said 0)`);
}

console.log('\n=== a 製程 change restarts the bucket rather than mixing ===');
{
  const measure = { ...ROOT_MEASURE, statistic: M.initMeasureStatistic(ROOT_MEASURE) };
  const statistic = { measureList: [measure] };
  const rep = (v) => ({ judgeReports: [judge(v, M.INSPECTION_STATUS.SUCCESS)] });
  M.statReducer(statistic, rep(10.0), undefined);          // root limits
  M.statReducer(statistic, rep(10.1), undefined);
  ok(measure.statistic.count === 2, 'two samples under the root limits',
     String(measure.statistic.count));
  const rangeBefore = measure.statistic.histogram.xmax;
  M.statReducer(statistic, rep(10.0), [TIGHT]);            // 製程 changes
  ok(measure.statistic.count === 1,
     'the 製程 change restarted the bucket instead of mixing limit sets',
     `count ${measure.statistic.count} (2 would mean mixed)`);
  ok(measure.statistic.histogram.xmax < rangeBefore,
     'and the histogram range followed the new limits',
     `${rangeBefore.toFixed(2)} -> ${measure.statistic.histogram.xmax.toFixed(2)}`);
}

console.log('\n=== a def edit invalidates hits; an inspection does not ===');
{
  const shape = { id: 3, type: 'search_point', locating: 'caliper',
                  pt1: { x: 1, y: 2 }, margin: 1, width: 2, angleDeg: 0,
                  caliper: { count: 10, width: 0.5 }, edge: { min_strength: 30 } };
  const stamped = M.shapeDefFingerprint(shape);
  ok(M.shapeDefFingerprint({ ...shape, angleDeg: 90 }) !== stamped,
     'rotating the search point invalidates its hits');
  ok(M.shapeDefFingerprint({ ...shape, cal_hits: [{ x: 1, y: 1, st: 2 }],
                             inspection_status: 0, reported_pt: { x: 5, y: 5 },
                             na_reason: 'x' }) === stamped,
     'but an inspection RESULT does not -- otherwise every run throws them away');
  ok(!('cal_hits' in M.shapeDefProjection({ ...shape, cal_hits: [1] })),
     'and the def projection drops the results, so they never reach the def file');
}

console.log('\n=== the verdict roll-up keeps an NG ===');
{
  const R = M.MEASURERSULTRESION;
  const worse = M.MEASURERSULTRESION_reducer;
  ok(worse(R.NG, R.UOK) === R.NG,
     'a later OK does not overwrite an earlier NG',
     `${worse(R.NG, R.UOK)} (UOK=${R.UOK}, NG=${R.NG})`);
  ok(worse(R.UOK, R.NA) === R.NA, 'an NA still wins over an OK');
}

console.log('\n=== limits, and the disabled back side ===');
{
  const def = { value: 10, USL: 11, LSL: 9, USL_b: 20, LSL_b: 1 };
  ok(M.effectiveLimits(def, false).USL === 11, 'front limits for an unflipped part');
  ok(M.effectiveLimits(def, true).USL === 11,
     'and for a FLIPPED part too, because back-side limits are disabled',
     'if this fails, BACK_SIDE_LIMITS_ENABLED was turned on -- see backSideLimits.js');
}

console.log('\n=== the measurement fence survives a def round trip ===');
{
  // The fence is the ONE region that nothing re-derives: the localization
  // polygons get re-baked from the sig360 signature at save, so losing them on
  // load is invisible, but a fence exists only as the shapes the operator drew.
  // Drop it anywhere along def -> shapes -> def and their work is silently gone.
  const FENCE = [[{ x: -5, y: -3 }, { x: 5, y: -3 }, { x: 5, y: 3 }, { x: -5, y: 3 }]];
  const HOLE  = [[{ x: -1, y: -1 }, { x: 1, y: -1 }, { x: 1, y: 1 }]];

  const roundTrip = (locatingEngine) => {
    const ed = new M.InspectionEditorLogic();
    ed.SetDefInfo({
      features: [{ id: 1, type: 'line', pt1: { x: 0, y: 0 }, pt2: { x: 1, y: 1 } }],
      measure_fence_include: FENCE,
      measure_fence_exclude: HOLE,
    });
    const out = M.defFileGeneration({
      _obj: ed, DefFileName: 'x', DefFileTag: '', locating_engine: locatingEngine,
    });
    return { ed, fs0: out.featureSet[0] };
  };

  const { ed, fs0 } = roundTrip('shape_based');
  ok(ed.shapeList.filter((s) => s.type === 'fence_include').length === 1,
     'loading a def rebuilds the fence as an editable shape',
     `${ed.shapeList.filter((s) => s.type === 'fence_include').length} include shape(s)`);
  ok(JSON.stringify(fs0.measure_fence_include) === JSON.stringify(FENCE),
     'and saving emits the same polygon back, unchanged',
     JSON.stringify(fs0.measure_fence_include));
  ok(JSON.stringify(fs0.measure_fence_exclude) === JSON.stringify(HOLE),
     'exclude polygons too -- a fence of only holes is a legal fence');
  ok(!fs0.features.some((s) => s && String(s.type).startsWith('fence_')),
     'the fence never ships inside features[] -- the core rejects an unknown '
     + 'feature type and takes the WHOLE def down with it');

  // The difference from localization_*, asserted rather than commented.
  const legacy = roundTrip('sig360').fs0;
  ok(JSON.stringify(legacy.measure_fence_include) === JSON.stringify(FENCE),
     'the fence is emitted for a NON-shape_based def too',
     'a caliper measures whichever locator found the part');
  ok(legacy.localization_include === undefined,
     'while localization_include is not -- it only means something to shape_based');
}

try { fs.unlinkSync(ENTRY); fs.unlinkSync(OUT); } catch { /* best effort */ }
console.log(fails ? `\n${fails} FAILURES` : '\n--- the pipeline behaves as specified ---');
process.exit(fails ? 1 : 0);
