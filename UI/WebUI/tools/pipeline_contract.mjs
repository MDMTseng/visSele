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
export { inspectSummary, objFromImage, angleDelta } from 'JSSRCROOT/sbmInspectResult';
export { SWEEP_AXES, sweepValues, perturbFor, sweepRow, sweepVerdict } from 'JSSRCROOT/sbmSweep';
export { acceptanceFloor, headroom, CORE_SIG_MATCH_THRES_DEFAULT, CORE_SHAPE_MIN_SCORE_DEFAULT } from 'UTIL/matchThreshold';
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

console.log('\n=== a result is drawn in the frame the IMAGE is in ===');
{
  // objFromImage inverts the studio canvas transform. Whatever pose you hand
  // it, a point comes back in the world that transform defines -- so handing it
  // the AUTHORED reg (what drawImage uses) puts a reported point on the pixel
  // it came from, and handing it the found pose does not.
  //
  // The arithmetic below uses the found pose deliberately, because that is the
  // case with a known answer: the numbers are the real ones off the bench
  // fixture (caliper_verify_tagged), where search point id 3 has pt1
  // (4.176, 3.435) in object-frame mm, the core placed the object at
  // (11.992, 7.580) and reported the measured point at (16.150, 11.087) in
  // image-mm. Through the FOUND pose that must land back on pt1 -- which is
  // what proves the inverse is right. Which pose the STUDIO feeds it is a
  // separate question, answered further down.
  const POSE = { cx: 11.992, cy: 7.580, rotate: 0, isFlipped: false, similarity: 0.9859 };
  const toObj = M.objFromImage(POSE);
  const got = toObj({ x: 16.150, y: 11.087 });
  ok(Math.hypot(got.x - 4.176, got.y - 3.435) < 0.1,
     'through the FOUND pose, a reported point inverts back onto its own def shape',
     `(${got.x.toFixed(3)}, ${got.y.toFixed(3)}) vs def pt1 (4.176, 3.435)`);

  // A rotated + flipped pose has to invert in the SAME order the canvas
  // composes it: scale(1,-1) then rotate then translate, i.e. flip LAST.
  const P2 = { cx: 5, cy: 5, rotate: Math.PI / 2, isFlipped: true };
  const back = M.objFromImage(P2)({ x: 5 + 2, y: 5 + 0 });   // +x in image
  ok(Math.abs(back.x - 0) < 1e-9 && Math.abs(back.y + 2) < 1e-9,
     'a 90deg flipped pose inverts in the canvas order (flip last)',
     `(${back.x.toFixed(6)}, ${back.y.toFixed(6)}) expected (0, -2)`);

  ok(Math.abs(M.angleDelta(-3.13, 3.13) - 0.0231853) < 1e-4,
     'a pose delta across +/-pi is the SHORT way round',
     'else a part sitting at 180deg reads as 359deg out');
}

console.log('\n=== the summary answers what a test run is for ===');
{
  const rp = { reports: [{ reports: [{
    cx: 10, cy: 4, rotate: 0, isFlipped: false, similarity: 0.97,
    detectedLines: [{ id: 1, name: 'L', status: 0, cx: 12, cy: 5 }],
    searchPoints: [
      { id: 3, name: 'a', status: 0, x: 11, y: 6 },
      { id: 4, name: 'b', status: -128, na_reason: 'edge.min_strength unset' },
      { id: 5, name: 'c', status: -128 },
    ],
  }] }] };
  const sum = M.inspectSummary(rp, { cx: 10, cy: 4, angle: 0, isFlipped: false });
  ok(sum.located && sum.counts.ok === 2 && sum.counts.na === 2,
     'every primitive is counted, passing and failing alike',
     `ok=${sum.counts.ok} na=${sum.counts.na} rows=${sum.rows.length}`);
  ok(sum.rows.length === 4,
     'a failed row still APPEARS -- dropping it makes a broken def look short');
  ok(sum.rows.find((r) => r.id === 4).reason === 'edge.min_strength unset',
     "and carries the core's own reason, not a guess");
  ok(/[^]/.test(sum.rows.find((r) => r.id === 5).reason)
     && sum.rows.find((r) => r.id === 5).reason !== 'undefined',
     'a reason-less NA says so rather than rendering "undefined" at an operator',
     sum.rows.find((r) => r.id === 5).reason);
  ok(sum.rows.find((r) => r.id === 5).at === null,
     'and a row with no position is not drawn at the origin');
  ok(sum.poseDelta && sum.poseDelta.dist === 0,
     'a pose matching the authored reg reports zero offset');

  const off = M.inspectSummary(rp, { cx: 10.5, cy: 4, angle: 0, isFlipped: true });
  ok(Math.abs(off.poseDelta.dist - 0.5) < 1e-9 && off.poseDelta.flipDiffers,
     'and a pose that does NOT match reports how far, including a flip',
     `${off.poseDelta.dist.toFixed(3)}mm, flipDiffers=${off.poseDelta.flipDiffers}`);
  // AND IT IS WHAT THE OVERLAY FOLLOWS. This assertion used to say the exact
  // opposite -- that positions come from the FOUND pose -- and that was the
  // bug: the studio canvas draws the picture through the AUTHORED reg, so a
  // point placed by the found pose lands in a frame the photograph is not in.
  // On the bench it drew every ring neatly on the def's own ROI points, over
  // blank background, while the parts sat elsewhere in the frame.
  const a3 = sum.rows.find((r) => r.id === 3).at;
  const b3 = off.rows.find((r) => r.id === 3).at;
  ok(a3.x !== b3.x || a3.y !== b3.y,
     'and the overlay follows it, because that is the frame the IMAGE is in',
     `(${a3.x}, ${a3.y}) vs (${b3.x}, ${b3.y})`);
  // A reg of nothing must be the identity, not a silent 0-rotation about (0,0)
  // that happens to look plausible.
  const raw = M.inspectSummary(rp, undefined).rows.find((r) => r.id === 3).at;
  ok(raw.x === 11 && raw.y === 6,
     'with no reg at all a point is left in image-mm, untransformed',
     `(${raw.x}, ${raw.y})`);

  // EVERY object, not just the first. The bench frame that exposed this has
  // three parts in it; reporting reports[0] alone silently drops two, and if
  // the def locks onto the wrong one the panel describes a part nobody is
  // looking at.
  const two = M.inspectSummary({ reports: [{ reports: [
    { cx: 10, cy: 4, rotate: 0, isFlipped: false, similarity: 0.97,
      searchPoints: [{ id: 1, status: 0, x: 11, y: 5 }] },
    { cx: 40, cy: 4, rotate: 0, isFlipped: false, similarity: 0.91,
      searchPoints: [{ id: 1, status: 0, x: 41, y: 5 }] },
  ] }] }, { cx: 0, cy: 0, angle: 0 });
  ok(two.poses.length === 2 && two.rows.length === 2,
     'two located objects give two poses and both their measurements',
     `${two.poses.length} poses, ${two.rows.length} rows`);
  ok(two.rows[0].obj === 0 && two.rows[1].obj === 1,
     'and every row says which object it belongs to');
  ok(two.rows[0].at.x === 11 && two.rows[1].at.x === 41,
     'placed by the SAME canvas transform -- one image, one frame, N objects',
     'rectifying to a found pose could only ever straighten one of them');

  // THE ORIENTATION STUB. It is drawn from two transformed points, not from an
  // angle recomposed by hand -- composing the canvas rotation with the found
  // rotation needs the sign of ctx.rotate, where the flip lands in the order,
  // and whether the reg angle adds or subtracts. Three chances to be wrong, and
  // every wrong answer still draws a plausible line, so it is not
  // self-checking. It was wrong on the bench.
  const axisOf = (found, reg) => {
    const P = M.inspectSummary({ reports: [{ reports: [
      { cx: 0, cy: 0, rotate: found, isFlipped: !!(reg && reg.isFlipped), similarity: 1,
        searchPoints: [] } ] }] }, reg).poses[0];
    return Math.atan2(P.axis.y - P.at.y, P.axis.x - P.at.x) * 180 / Math.PI;
  };
  const near = (a, b) => Math.abs(M.angleDelta(a * Math.PI / 180, b * Math.PI / 180)) < 1e-6;

  ok(near(axisOf(0, { cx: 0, cy: 0, angle: 0 }), 0),
     'an unrotated object on an unrotated reg points along +x');
  ok(near(axisOf(Math.PI / 2, { cx: 0, cy: 0, angle: 0 }), -90),
     'a rotate of +90 points at -90 in image terms -- rotate is not an image angle',
     'measured: --insp {"rot_deg":5} -> rotate +5, and +5 rot_deg moves content to -5');
  // THE ONE THAT WAS FAILING, and is now the point. An object sitting exactly
  // AT the authored reg must point along the world +x axis, because rectifying
  // is what a reg is for. It came out at 2x the reg angle while the stub used
  // +rotate; it is 0 now that it uses the IMAGE angle, -rotate.
  //
  // Both facts behind that were measured rather than argued:
  //   `--insp img def out '{"rot_deg":5}'` reports rotate = +5.0000, and
  //   test_perturb pins +5 rot_deg as moving image content to -5 in image atan2.
  // And def_image_reg.angle is in ROTATE space -- DefConfUI writes it as
  // `angle: reg.rotate` off a report -- which is why the canvas rotating by
  // +reg.angle rectifies at all.
  const A = 0.7, D = 0.2;
  // "AT the authored reg" means rotate === reg.angle, because BOTH are in
  // rotate space -- that is the whole content of the note above, and writing
  // this assertion with a negated rotate (as if reg.angle were an image angle)
  // is how it failed at 2x while the code was right.
  ok(near(axisOf(A, { cx: 0, cy: 0, angle: A }), 0),
     'an object AT the authored reg points along the world +x axis',
     `${axisOf(A, { cx: 0, cy: 0, angle: A }).toFixed(4)} deg`);
  ok(near(axisOf(A + D, { cx: 0, cy: 0, angle: A }), -D * 180 / Math.PI),
     'and one 0.2 rad further round in ROTATE reads as 0.2 rad the other way on screen',
     `${axisOf(A + D, { cx: 0, cy: 0, angle: A }).toFixed(4)} deg -- rotate and image angle run opposite`);

  ok(near(axisOf(0.3, { cx: 0, cy: 0, angle: 0, isFlipped: true }), 0.3 * 180 / Math.PI),
     'and a flipped reg mirrors the direction rather than ignoring the flip');

  const none = M.inspectSummary({ reports: [] }, undefined);
  ok(!none.located && none.rows.length === 0,
     'no object found is a stated verdict, not a crash', none.why);
}

console.log('\n=== a locate FAILURE says which kind of failure it was ===');
{
  // Three nothings that look identical on screen and are fixed three different
  // ways. Before the core reported `locate`, all three said the same sentence.
  const fail = (top) => M.inspectSummary({ reports: [{ reports: [], ...top }] }, undefined);

  const near = fail({ locate: { reason: 'signature below match threshold',
                                candidates: 4, best: 0.87, thres: 0.9 } });
  ok(/0\.8700/.test(near.why) && /0\.90/.test(near.why) && /4/.test(near.why),
     'it saw the part and scored it too low -> the score, the floor and the gap',
     near.why);

  const nada = fail({ locate: { reason: 'shape matcher returned no candidate',
                                code: 'no_candidate', candidates: 0 } });
  ok(!/0\.00/.test(nada.why) && nada.why !== near.why,
     'nothing scored at all -> NOT reported as a score of zero',
     nada.why);

  const dropped = fail({ region_dropped: 3 });
  ok(/3/.test(dropped.why) && !/沒跑到/.test(dropped.why),
     'the working region rejected what the locator FOUND -> say that, not that it never ran',
     dropped.why);

  const silent = fail({});
  ok(silent.why && !/undefined/.test(silent.why),
     'and a core with no comment still produces a sentence, not "undefined"',
     silent.why);
}

console.log('\n=== the robustness sweep measures against the RIGHT reference ===');
{
  const vals = M.sweepValues('rot', -4, 4, 5);
  ok(vals[0] === 0, 'the baseline runs FIRST -- every other step is read against it',
     JSON.stringify(vals));
  ok(vals.filter((v) => v === 0).length === 1,
     'and exactly once, not twice because the range happens to cross it',
     JSON.stringify(vals));
  ok(M.perturbFor('rot', 0, 9) === null,
     'the baseline step sends NO perturb at all, not a zero one');
  ok(M.perturbFor('rot', 3, 9).rot_deg === 3 && M.perturbFor('rot', 3, 9).seed === 9,
     'and a real step carries the axis field TestPerturb parses, plus the seed');
  ok(M.perturbFor('gain', 1.2, 9).gain === 1.2,
     'each axis maps to its own core field', 'gain, not rot_deg');

  // THE BUG THIS EXISTS TO CATCH. A part does not sit at exactly 0 degrees, so
  // a residual computed against absolute zero reports its mounting angle as
  // localization error on EVERY step of every sweep -- a confident, constant,
  // completely fake finding.
  const at = (deg) => ({ located: true, pose: { rotate: deg * Math.PI / 180, similarity: 0.98 },
                         counts: { ok: 4, na: 0, ng: 0 }, rows: [], why: '' });
  const base = at(31.4);                       // the part is mounted crooked
  const step = M.sweepRow('rot', 5, at(36.4), base);   // and the scene rotated 5 more
  ok(Math.abs(step.residual) < 1e-6,
     'a crooked part gives ZERO residual -- the baseline is the reference, not 0',
     `residual ${step.residual.toFixed(6)} deg (moved ${step.moved.toFixed(3)})`);

  const bad = M.sweepRow('rot', 5, at(36.0), base);
  ok(Math.abs(bad.residual + 0.4) < 1e-6,
     'and a locator that lags by 0.4 deg reports exactly that',
     `residual ${bad.residual.toFixed(3)}`);

  // An inverted sign convention looks like "twice as bad", which is a finding
  // people act on. It gets flagged instead.
  const flipped = M.sweepRow('rot', 5, at(31.4 - 5), base);
  ok(flipped.signSuspect,
     'a residual of 2x the applied value is flagged as a SIGN error, not a result',
     `residual ${flipped.residual.toFixed(3)} vs applied 5`);
  ok(!bad.signSuspect, 'while an ordinary small residual is not');

  // Wrapping: a part near +/-180 must not read as 360 out.
  const nearPi = M.sweepRow('rot', 4, at(-176), at(180));
  ok(Math.abs(nearPi.residual) < 1e-4,
     'and a sweep across the +/-180 boundary does not report 356 degrees of error',
     `residual ${nearPi.residual.toFixed(4)}`);

  const rows = [M.sweepRow('rot', 0, at(0), at(0)),
                M.sweepRow('rot', 3, at(3), at(0)),
                M.sweepRow('rot', 6, { located: false, counts: { ok: 0, na: 0, ng: 0 },
                                       rows: [], why: 'x' }, at(0))];
  const v = M.sweepVerdict('rot', rows);
  ok(/0/.test(v) && /3/.test(v) && !/6/.test(v.split('，')[0]),
     'the verdict names the CONTIGUOUS range that located, stopping at the failure',
     v);

  const dead = M.sweepVerdict('rot', [M.sweepRow('rot', 0,
    { located: false, counts: { ok: 0, na: 0, ng: 0 }, rows: [], why: 'x' }, null)]);
  ok(/單次檢驗|原圖/.test(dead),
     'and a baseline that never located says so instead of reporting a range', dead);
}

console.log('\n=== nothing imports a React the project does not have ===');
{
  // This one is not about the pipeline; it is here because it is the only
  // check that runs. esbuild resolves `useSyncExternalStore` off the React
  // namespace object happily and the bundle builds CLEAN -- it then throws
  // "reactExports.useSyncExternalStore is not a function" the moment the
  // screen opens. A green build is not evidence, so the evidence goes here.
  //
  // The project is on React 16. usePerifLink in perif/PerifAPI.js is the
  // pattern to copy: subscribe + setState, with an immediate read to cover
  // the gap between render and effect.
  const REACT18 = ['useSyncExternalStore', 'useInsertionEffect', 'useId',
                   'useTransition', 'useDeferredValue', 'createRoot', 'hydrateRoot'];
  const major = parseInt(JSON.parse(
    fs.readFileSync(r('node_modules/react/package.json'), 'utf8')).version, 10);
  const walk = (dir, out = []) => {
    for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
      const f = path.join(dir, e.name);
      if (e.isDirectory()) { if (e.name !== 'node_modules') walk(f, out); }
      else if (/\.(js|jsx)$/.test(e.name)) out.push(f);
    }
    return out;
  };
  const hits = [];
  if (major < 18) {
    for (const f of walk(r('src'))) {
      const src = fs.readFileSync(f, 'utf8');
      for (const api of REACT18) {
        // Only a CALL or a named import counts -- the comments explaining why
        // these are unavailable must not trip their own check.
        const re = new RegExp('(^|[^A-Za-z0-9_.])' + api + '\s*[(,]|\b' + api + '\s*}', 'm');
        if (re.test(src)) hits.push(path.relative(r('src'), f) + ': ' + api);
      }
    }
  }
  ok(hits.length === 0,
     `React ${major}: no source uses a React 18+ API`,
     hits.length ? hits.join('; ') : 'checked ' + walk(r('src')).length + ' files');
}

console.log('\n=== a match score is read against the floor its OWN locator uses ===');
{
  // The two locators gate at wildly different places, and the screen shows one
  // number. Reading a shape_based score against a sig360 floor -- or against a
  // guessed 0.9 -- makes a def with enormous headroom look marginal, or a
  // marginal one look fine.
  const shape = M.acceptanceFloor({ locating_engine: 'shape_based' });
  ok(shape.floor === M.CORE_SHAPE_MIN_SCORE_DEFAULT && shape.floor === 0.5,
     'a shape_based def is gated by line2Dup at 0.50, not by sig_match_sim_thres',
     `floor ${shape.floor}, key ${shape.key}`);
  ok(M.acceptanceFloor({ locating_engine: 'shape_based', shape_min_score: 72 }).floor === 0.72,
     'and shape_min_score is 0-100 in the def, converted here exactly once');

  const sig = M.acceptanceFloor({});
  ok(sig.floor === M.CORE_SIG_MATCH_THRES_DEFAULT && sig.floor === 0.7,
     "a sig360 def with no key gets the CORE's 0.7 -- not the editor's 0.9 seed",
     `floor ${sig.floor}`);
  ok(M.acceptanceFloor({ sig_match_sim_thres: 0.93 }).floor === 0.93,
     'and an explicit value wins over both');

  // THE BAR. It draws headroom, and the bug it replaced is worth pinning: a
  // real sweep spanning 0.986..0.998 over a 0.50 gate was auto-scaled to its
  // own min/max and drew a bar swinging empty-to-full across twelve
  // thousandths -- a dramatic curve over nothing, on a run with no problem.
  const lo = M.headroom(0.986, 0.5), hi = M.headroom(0.998, 0.5);
  ok(lo > 0.95 && hi > 0.95 && Math.abs(hi - lo) < 0.03,
     'a comfortable sweep draws a column of nearly-full bars, all alike',
     `0.986 -> ${(lo * 100).toFixed(1)}%, 0.998 -> ${(hi * 100).toFixed(1)}%`);
  ok(M.headroom(0.55, 0.5) < 0.15,
     'while a score actually near the gate is visibly short',
     `${(M.headroom(0.55, 0.5) * 100).toFixed(1)}%`);
  ok(M.headroom(0.4, 0.5) === 0, 'below the gate clamps to empty, never negative');
  ok(!Number.isFinite(M.headroom(NaN, 0.5)) && !Number.isFinite(M.headroom(0.9, 1)),
     'and an absent score or a degenerate floor draws nothing, not a full bar');
}

console.log('\n=== a namespace import only reads what the module NAMED ===');
{
  // The second runtime-only bug of the day from one family: it builds clean and
  // throws when the screen opens.
  //
  // `import * as BPG from './BPG_Protocol'` then `BPG.map_BPG_Packet2Act(...)`
  // is undefined when the function lives only on the DEFAULT export. Inside a
  // protocol callback the TypeError reaches nothing an operator sees: the image
  // switch loaded in the core, so the inspection moved to the new picture, and
  // the canvas kept the old one. Two screens disagreeing, neither complaining.
  const ALIASES = { UTIL: 'src/UTIL', JSSRCROOT: 'src', REDUX_STORE_SRC: 'src/redux' };
  const walk = (dir, out = []) => {
    for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
      const f = path.join(dir, e.name);
      if (e.isDirectory()) { if (e.name !== 'node_modules') walk(f, out); }
      else if (/\.(js|jsx)$/.test(e.name)) out.push(f);
    }
    return out;
  };
  const resolve = (spec, from) => {
    let base;
    const alias = Object.keys(ALIASES).find((a) => spec === a || spec.startsWith(a + '/'));
    if (alias) base = r(ALIASES[alias] + spec.slice(alias.length));
    else if (spec.startsWith('.')) base = path.resolve(path.dirname(from), spec);
    else return null;                       // a package: not ours to check
    for (const cand of [base, base + '.js', base + '.jsx',
                        path.join(base, 'index.js'), path.join(base, 'index.jsx')])
      if (fs.existsSync(cand) && fs.statSync(cand).isFile()) return cand;
    return null;
  };
  const namedExports = (file) => {
    const src = fs.readFileSync(file, 'utf8');
    const names = new Set();
    for (const m of src.matchAll(/export\s+(?:async\s+)?(?:function|class|const|let|var)\s+([A-Za-z0-9_$]+)/g))
      names.add(m[1]);
    for (const m of src.matchAll(/export\s*{([^}]*)}/g))
      for (const part of m[1].split(','))
        names.add(part.trim().split(/\s+as\s+/).pop().trim());
    return names;
  };
  const bad = [];
  for (const f of walk(r('src'))) {
    const src = fs.readFileSync(f, 'utf8');
    for (const m of src.matchAll(/import\s*\*\s*as\s+([A-Za-z0-9_$]+)\s+from\s*['"]([^'"]+)['"]/g)) {
      const [, ns, spec] = m;
      const target = resolve(spec, f);
      if (!target) continue;
      const named = namedExports(target);
      const used = new Set();
      for (const u of src.matchAll(new RegExp('\\b' + ns + '\\.([A-Za-z0-9_$]+)', 'g')))
        used.add(u[1]);
      for (const name of used)
        if (name !== 'default' && !named.has(name))
          bad.push(`${path.relative(r('src'), f)}: ${ns}.${name} is not a named export of ${spec}`);
    }
  }
  ok(bad.length === 0, 'every NS.member resolves to a real named export',
     bad.length ? bad.slice(0, 6).join('; ') : 'checked ' + walk(r('src')).length + ' files');
}

console.log('\n=== the trained SBM features survive a save ===');
{
  // __shape_cache is the trained line2Dup FeatureSet. Without it the core
  // re-extracts on every def PARSE -- which is every def load and every II, so
  // once per step of a robustness sweep. It was written on save and never read
  // on load, so it survived exactly one save and every later open-and-save
  // dropped it in silence.
  const CACHE = { fp: 'v1|100x100|1|nf128|T4,8,|w30.00|s60.00|roi0:0|ao0.0000',
                  n: 2, x: [1, 2], y: [3, 4] };
  const ed = new M.InspectionEditorLogic();
  ed.SetDefInfo({ features: [{ id: 1, type: 'line', pt1: { x: 0, y: 0 }, pt2: { x: 1, y: 1 } }] });
  const out = M.defFileGeneration({
    _obj: ed, DefFileName: 'x', DefFileTag: '',
    locating_engine: 'shape_based', __shape_cache: CACHE,
  });
  const _e = (out.featureSet[0].inherentfeatures || [])
    .find((e) => e && e.name === '@__SBM_INFO__');
  ok(_e && JSON.stringify(_e.shape_cache) === JSON.stringify(CACHE),
     'a cache in the editor state is emitted into the def unchanged');

  // It DOES change the def identity, as of the move into inherentfeatures.
  // The previous assertion here said the opposite -- that a def with trained
  // features and one without must hash alike -- and it was inverted
  // deliberately, not accidentally: see the note at the write site.
  const bare = M.defFileGeneration({
    _obj: ed, DefFileName: 'x', DefFileTag: '', locating_engine: 'shape_based',
  });
  ok(out.featureSet_sha1 !== bare.featureSet_sha1,
     'and a def WITH trained features hashes differently from one without',
     `${String(out.featureSet_sha1).slice(0, 12)} vs ${String(bare.featureSet_sha1).slice(0, 12)}`);
  ok(!(bare.featureSet[0].inherentfeatures || [])
       .some((e) => e && e.name === '@__SBM_INFO__'),
     'no features in, no entry out -- it is never invented');
}

console.log('\n=== def_image_reg lives in featureSet[0], and the hash says so ===');
{
  const REG = { cx: 15.02, cy: 9.30, angle: -0.0022, isFlipped: false };
  const ed = new M.InspectionEditorLogic();
  ed.SetDefInfo({ features: [{ id: 1, type: 'line', pt1: { x: 0, y: 0 }, pt2: { x: 1, y: 1 } }] });
  const gen = (ei) => M.defFileGeneration({ _obj: ed, DefFileName: 'x', DefFileTag: '', ...ei });

  const out = gen({ def_image_reg: REG });
  ok(JSON.stringify(out.featureSet[0].def_image_reg) === JSON.stringify(REG),
     'it is written into featureSet[0]');
  ok(out.def_image_reg === undefined,
     'and the top-level key is REMOVED -- two places holding one value is how '
     + 'they come to disagree');

  // THE POINT OF MOVING IT. featureSet_sha1 hashes featureSet only, so while
  // this sat at the top level a registration change did not change the def
  // hash: the save-conflict check could not see a reg-only edit, and
  // subFeatureDefSha1 -- which rides every report into the database -- was
  // identical either side of a change of coordinate system.
  const moved = gen({ def_image_reg: { ...REG, angle: 0 } });
  ok(out.featureSet_sha1 !== moved.featureSet_sha1,
     'changing the registration now CHANGES featureSet_sha1',
     `${String(out.featureSet_sha1).slice(0, 10)} vs ${String(moved.featureSet_sha1).slice(0, 10)}`);
  ok(gen({ def_image_reg: REG }).featureSet_sha1 === out.featureSet_sha1,
     'and the same registration still hashes the same');

  // THE TRAINED FEATURES moved into inherentfeatures beside the signature, and
  // are hashed with it. That is a deliberate reversal -- they used to be added
  // after the digest so a def with a cache and one without hashed alike.
  const withF = gen({ def_image_reg: REG, __shape_cache: { fp: 'z', n: 1 } });
  const entry = withF.featureSet[0].inherentfeatures
    .find((e) => e && e.name === '@__SBM_INFO__');
  ok(entry && entry.type === 'sbm_info' && entry.shape_cache.fp === 'z',
     'the trained features are an inherentfeatures entry, beside the signature');
  ok(withF.featureSet[0].__shape_cache === undefined,
     'and the legacy top-level key is gone, not mirrored');
  ok(withF.featureSet_sha1 !== out.featureSet_sha1,
     'they are INSIDE the hash now -- what the machine matches against is recipe',
     'regenerating features therefore counts as a def revision');
  // Saving twice must not accumulate entries: inherentfeatures is the live
  // list off the editor object, so appending in place would grow it every save.
  const twice = gen({ def_image_reg: REG, __shape_cache: { fp: 'z', n: 1 } });
  ok(twice.featureSet[0].inherentfeatures
       .filter((e) => e && e.name === '@__SBM_INFO__').length === 1,
     'and a second save does not append a second copy');
  ok(twice.featureSet_sha1 === withF.featureSet_sha1,
     'so saving an unchanged def twice still hashes the same');
}

try { fs.unlinkSync(ENTRY); fs.unlinkSync(OUT); } catch { /* best effort */ }
console.log(fails ? `\n${fails} FAILURES` : '\n--- the pipeline behaves as specified ---');
process.exit(fails ? 1 : 0);
