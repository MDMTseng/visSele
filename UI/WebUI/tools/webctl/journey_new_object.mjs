// One object, end to end, on a fake camera: TAKE -> SBM -> 快速驗證 -> InspectionUI.
//
//   node journey_new_object.mjs
//   needs: a core started with FORCE_BMP_CAROUSEL=<folder of frames>,
//          `npm run dev` on 8081, webctld on 8765
//
// WHAT THIS ADDS OVER regress_features.
//
// That suite checks features one at a time, and drives the canvas by dispatching
// redux actions where a person would draw. Both are deliberate -- it is fast and
// it isolates -- but it means two things go unchecked: whether the CANVAS TOOLS
// actually author anything when a human drags them, and whether the steps
// COMPOSE. A def can pass every isolated check and still be unusable because
// step 3 was reading what step 1 wrote in a form step 2 replaced.
//
// So this one draws. Every registration line, every polygon vertex here is a
// real press-move-release through the browser's input pipeline, and the def it
// produces is carried all the way into an inspection.
//
// WHY A FAKE CAMERA, AND WHY THAT IS NOT A COMPROMISE.
//
// FORCE_BMP_CAROUSEL replays a folder of frames through the whole live pipeline
// -- the same ST/CI path, the same data view, the same caches. The frames are
// identical every run, so a measurement that moves is the code moving, not the
// light. It also means this can run on a machine with no camera attached, which
// is the difference between a test that runs and a test that is described.
//
// The frames are the synthetic part from fixtures/ -- drawn, not photographed,
// because this repository is public.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { makeCtl, toMain, dismissCamModal, loadRecipe, freshPage } from './lib_enter.mjs';
import { makeProbe, makeTally, sleep } from './_rf_lib.mjs';

const MODEL = process.argv[2] || process.env.WEBCTL_MODEL || 'data/test1';
const APP = process.env.WEBCTL_APP || 'http://127.0.0.1:8081/';
const NAME = process.env.JOURNEY_NAME || 'JOURNEY-PART';
// JOURNEY_BENCH=<folder under fixtures/benches>: the instrument setup the CORE
// was started with. The profile says what a def captured on that machine must
// measure in, and the check below reads what the studio actually reports -- the
// live half of unit_bench_profiles, which exercises the rule but not the wiring
// that feeds it. Set it only when the core's data/lens_calib.json really is that
// profile's, or this is asserting the wrong machine.
const BENCH = process.env.JOURNEY_BENCH || null;
const BENCH_DIR = path.join(path.dirname(fileURLToPath(import.meta.url)), 'fixtures', 'benches');
const bench = BENCH
  ? JSON.parse(fs.readFileSync(path.join(BENCH_DIR, BENCH, 'expect.json'), 'utf8'))
  : null;

// The carousel the core is replaying, and what it says the part IS. With it,
// the region below is drawn around a 4 mm part in millimetres -- the same
// instruction on every bench -- instead of a fraction of whatever the frame
// happens to span, which is a different piece of the part each time the lens
// changes. Falls back to fractions of the frame when no fixture is named.
// One carousel, whatever the bench: the frames carry the scene's own mm/px and
// the CORE resamples them to the instrument's (FAKE_CAM_MMPP), so a bench is a
// pair of settings rather than a second copy of the pictures. A pre-rendered
// per-bench folder is still honoured when one exists (make_bench_carousel.py),
// for a run with no env var to set.
const CAROUSEL = process.env.JOURNEY_CAROUSEL
  || (BENCH && fs.existsSync(path.join(path.dirname(BENCH_DIR), 'journey_carousel_' + BENCH))
      ? path.join(path.dirname(BENCH_DIR), 'journey_carousel_' + BENCH)
      : path.join(path.dirname(BENCH_DIR), 'journey_carousel'));
const fixture = (() => {
  try { return JSON.parse(fs.readFileSync(path.join(CAROUSEL, 'frames.json'), 'utf8')); }
  catch { return null; }
})();

const ctl = makeCtl(`http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`);
const { api, ev } = ctl;
const P = makeProbe(ev);
const T = makeTally();
const { ok, section } = T;
const EI = 'window.__GP_STORE__.getState().UIData.edit_info';
const SM = 'window.__GP_STORE__.getState().UIData.c_state.value';

const drag = (x1, y1, x2, y2) => api('/drag', { x1, y1, x2, y2, steps: 14 });
const clickAt = (x, y) => api('/mouse', { x, y });

// AIM IN WORLD UNITS, NOT PAGE PIXELS.
//
// The studio canvas pans and zooms. A test that presses fixed page coordinates
// is really asserting where the camera happened to be when it ran -- it draws
// the registration line across whatever the viewport shows today, and starts
// drawing it across the background the moment the fit changes, the window is a
// different shape, or somebody nudges the view. It would also fail differently
// on the Surface Go than on this bench, which is the opposite of what a
// regression test is for.
//
// So the studio exposes its own world->page transform in dev (__SBM2_TO_PAGE__,
// the same matrix the renderer draws through) and the test names points the way
// a person thinks about them: the middle of the part, a box around it. Where
// that lands on screen is the app's business.
//
// World is image-mm while the locline tool is active (the raw frame), and the
// object frame -- origin at the registration -- for every other tool.
const imgWorld = () => ev(`JSON.stringify(window.__SBM2_IMG__||null)`)
  .then((s) => { try { return JSON.parse(s); } catch { return null; } });

const toPage = (wx, wy) => ev(
  `JSON.stringify(window.__SBM2_TO_PAGE__ ? window.__SBM2_TO_PAGE__(${wx}, ${wy}) : null)`)
  .then((s) => { try { return JSON.parse(s); } catch { return null; } });

// Wait until the view has stopped moving before aiming at it.
//
// The studio fits the image to the canvas when the frame arrives, and the modal
// itself is still being laid out for a frame or two after it opens. Sampling
// the transform during that produces a press at a point that has moved by the
// time the release happens -- which showed up as a registration landing metres
// away from where the test asked for it, in one run out of five.
const viewSettled = async ({ timeout = 20000 } = {}) => {
  const t0 = Date.now(); let prev = null;
  for (;;) {
    const im = await imgWorld();
    // Where the picture's two opposite corners land. Both conditions matter:
    // it has to be FITTED (the fit runs a beat after the frame arrives, and
    // before it does the image is a 40-pixel stamp in the corner -- a stable
    // state, so waiting only for "stopped moving" returns while the view is
    // still wrong, and a drag measured against it is too short to count as a
    // drag at all), and it has to have STOPPED, or the release lands somewhere
    // the press was never aimed at.
    const a = im ? await toPage(0, 0) : null;
    const b = im ? await toPage(im.wmm, im.hmm) : null;
    const rw = await ev(`(function(){var c=window.__SBM2_CANVAS__;
      return c?c.canvas.getBoundingClientRect().width:0;})()`);
    if (a && b && rw) {
      const span = Math.abs(b.x - a.x);
      // A LOW bar, deliberately. This guard exists to reject one specific
      // state -- the pre-fit view, where the picture is a ~40 px stamp near the
      // corner and a drag across the part is too short to count as a drag. It
      // is not a judgement on how well the fit fills the canvas: that lands at
      // about a fifth of the width here (img_info.scale, see the note in
      // DrawHook_CanvasComponent.SetImg) and moves with the instrument's mmpp,
      // so a tighter bar fails on a finer lens while nothing is wrong: the
      // same capture spans 300 px at 13.9 um/px and 112 px at 5.2 um/px, both
      // fitted. 80 px sits between that and the 41 px pre-fit stamp.
      const fitted = span > 80;
      if (fitted && prev && Math.abs(span - prev) < 1) return true;
      prev = fitted ? span : null;
    }
    if (Date.now() - t0 > timeout) return false;
    await sleep(250);
  }
};

// A press-drag-release between two WORLD points.
const dragWorld = async (w1, w2) => {
  const a = await toPage(w1.x, w1.y), b = await toPage(w2.x, w2.y);
  if (!a || !b) return false;
  await api('/drag', { x1: Math.round(a.x), y1: Math.round(a.y),
                       x2: Math.round(b.x), y2: Math.round(b.y), steps: 14 });
  return true;
};

// Whether the studio is actually ON SCREEN. antd leaves a closed modal's DOM in
// the document (no destroyOnClose), so presence proves nothing either way.
const studioVisible = () => ev(`(function(){
  var e = document.querySelector('[data-testid="sbm2"]');
  if (!e) return false;
  var w = e.closest('.ant-modal-wrap');
  if (w && (w.style.display === 'none' || getComputedStyle(w).display === 'none')) return false;
  return !!(e.offsetParent || e.getClientRects().length);
})()`);

// A click at a WORLD point. Re-reads the transform every time: the tools that
// switch between the raw and object frames change it underneath us.
const clickWorld = async (w) => {
  const p = await toPage(w.x, w.y);
  if (!p) return false;
  await api('/mouse', { x: Math.round(p.x), y: Math.round(p.y) });
  return true;
};

(async () => {
  console.log(`app ${APP}   model ${MODEL}   new object ${NAME}`);
  if (fixture)
    console.log(`fixture ${path.basename(CAROUSEL)}  mmpp=${fixture.source_mmpp}  `
              + `part r=${fixture.part.base_radius_mm}mm`);
  await freshPage(ctl, APP);
  // Any uncaught page error is a failure of the journey, not noise: the bug
  // that made 套用並離開 stop closing the modal was visible ONLY here.
  await ev(`window.__CERR__=[];var _ce=console.error;console.error=function(){try{window.__CERR__.push(Array.from(arguments).map(String).join(' ').slice(0,300));}catch(e){};return _ce.apply(console,arguments);}`);
  await ev(`window.addEventListener('error',function(e){window.__LASTERR__=String(e.message)+' | '+String((e.error&&e.error.stack||'')).split(String.fromCharCode(10)).slice(0,5).join(' <- ');})`);
  await P.waitFor('app mounted', async () =>
    (await ev(`typeof window.__GP_STORE__`)) === 'object', { timeout: 40000 });
   await toMain(ctl);
  await dismissCamModal(ctl);
  await loadRecipe(ctl, MODEL);
  await ev(`window.__GP_STORE__.dispatch({ type: 'Edit_Mode' })`);
  ok('the def editor opens', await P.waitFor('DEFCONF', async () =>
    JSON.stringify(await P.store(SM)).indexOf('DEFCONF_MODE') >= 0, { timeout: 25000 }));

  // ---------------------------------------------------------------------------
  section('1. TAKE a frame off the (fake) camera');
  {
    await P.click('take');
    await P.waitFor('dialog or confirm', async () =>
      (await P.confirmShowing('還沒存檔')) || (await P.exists('take-name')), { timeout: 10000 });
    if (await P.confirmShowing('還沒存檔')) await P.confirmClick('丟掉變更');
    ok('the take dialog opens', await P.waitExists('take-name', { timeout: 10000 }));
    await P.setInput('take-name', NAME);
    await P.waitAttr('take-next', 'data-enabled', '1', { timeout: 6000 });
    await P.click('take-next');
    ok('the viewfinder opens', await P.waitExists('take-capture', { timeout: 10000 }));

    await P.click('take-stream-start');
    const streaming = await P.waitAttr('take-capture', 'data-phase', 'streaming', { timeout: 12000 });
    ok('the carousel streams', streaming,
       `phase=${await P.attr('take-capture', 'data-phase')}`);
    if (streaming) {
      await ev(`window.__J_IMG__ = ${EI}.img;`);
      ok('frames arrive', await P.waitFor('a new frame', () => ev(
        `(function(){var n=${EI}.img; if(n===window.__J_IMG__) return false;
           window.__J_IMG__=n; return true;})()`), { timeout: 12000 }));
      await P.click('take-stream-stop');
      await P.waitAttr('take-capture', 'data-phase', 'idle', { timeout: 10000 });
    }
    await P.click('take-use-frame');
    ok('the studio opens on the captured frame',
       await P.waitExists('sbm2', { timeout: 30000 }));
    ok('the def is a new object', (await P.store(`${EI}.DefFileName`)) === NAME);
    ok('with nothing carried over', (await P.attr('sbm2', 'data-done')) === '0000',
       `data-done=${await P.attr('sbm2', 'data-done')}`);
  }

  // ---------------------------------------------------------------------------
  section('2. draw the registration line with the tool, as a person would');
  {
    ok('the locline tool selects', await P.click('sbm2-tool-locline'));
    ok('and reports itself active',
       await P.waitAttr('sbm2', 'data-tool', 'locline', { timeout: 4000 }));
    // locline shows the RAW frame, so world is image-mm and the picture spans
    // (0,0)..(wmm,hmm). Press at its centre, release to the right: origin in the
    // middle of the part, 0 degrees pointing +x.
    ok('the view stops moving before we aim at it', await viewSettled());
    // JOURNEY_SHOT=<file>: a PNG of the studio the moment the fit has settled
    // and before anything is drawn on it. This is the only way to answer "does
    // the new object come up looking right?" -- an assertion can say the
    // numbers are in range and still miss a picture the size of a postage stamp.
    if (process.env.JOURNEY_SHOT) {
      const u = `${process.env.WEBCTL_BASE || 'http://127.0.0.1:8765'}/shot`
              + `?path=${encodeURIComponent(process.env.JOURNEY_SHOT)}`
              + `&selector=${encodeURIComponent('[data-testid="sbm2"]')}`;
      console.log('  shot ->', await (await fetch(u)).text());
    }
    const im = await imgWorld();
    ok('the studio reports the image it is showing', !!im, JSON.stringify(im));
    // The frame came off the camera, so the scale must be the MACHINE's, not
    // whatever the def being edited happened to carry. Only asserted when the
    // bench profile is named, because it is a statement about the core's
    // instrument settings rather than about the UI.
    // AND THE PART IS THE RIGHT PHYSICAL SIZE.
    //
    // The scale field being right is not the same as the picture being right.
    // The fake camera is told the instrument's mm/px (FAKE_CAM_MMPP) and each
    // frame carries the scene's own (frames.json), and it resamples by the
    // ratio -- so the same 4 mm part lands on 288 px through this bench's lens
    // and 769 px through a 5.2 um one. This measures the bright area actually
    // delivered, converts it with the machine's mmpp, and compares it against
    // the area the fixture says the part has. A camera that ignored the scale
    // passes the mmpp check above and fails this one.
    if (fixture && fixture.part && fixture.part.area_mm2 && im) {
      // The exposure the camera was given, if any -- both measurements below
      // have to know it: at half the light the part's own level halves, so a
      // fixed brightness threshold stops finding the part and a fixed expected
      // level is simply the wrong number.
      const expoMs = Number(process.env.JOURNEY_EXPO_MS);
      const ratio = (Number.isFinite(expoMs) && expoMs > 0)
        ? expoMs / (fixture.source_exposure_ms || 1) : 1;
      const lv = fixture.levels || { field: 26, part: 232 };
      const partLevel = Math.min(255, lv.part * ratio);
      const thr = Math.round((Math.min(255, lv.field * ratio) + partLevel) / 2);

      const shot = await P.store(`(function(){
        var c = window.__SBM2_CANVAS__.secCanvas;
        var o = document.createElement('canvas'); o.width = c.width; o.height = c.height;
        var x = o.getContext('2d'); x.drawImage(c, 0, 0);
        var d = x.getImageData(0, 0, c.width, c.height).data;
        var n = 0, step = 4;                       // every 4th pixel, both axes
        for (var y = 0; y < c.height; y += step)
          for (var xx = 0; xx < c.width; xx += step)
            if (d[(y * c.width + xx) * 4] > ${thr}) n++;
        // The MEDIAN of a patch at the part's centre, not the peak of the whole
        // frame: the fake camera adds +-20% brightness jitter and +-15 of noise,
        // so the frame's maximum is the top of that spread and always reads
        // high. The middle of the part is flat, so its median is the level.
        var cx = (c.width / 2) | 0, cy = (c.height / 2) | 0, v = [];
        for (var yy = cy - 30; yy < cy + 30; yy++)
          for (var xx2 = cx - 30; xx2 < cx + 30; xx2++)
            v.push(d[(yy * c.width + xx2) * 4]);
        v.sort(function(a, b){ return a - b; });
        return { px: n * step * step, centre: v[v.length >> 1], w: c.width, h: c.height };
      })()`);

      const areaMm2 = shot && shot.px * im.mmpp * im.mmpp;
      const want = fixture.part.area_mm2;
      ok('and the part arrives at its true physical size',
         areaMm2 && Math.abs(areaMm2 - want) / want < 0.15,
         `measured ${areaMm2 && areaMm2.toFixed(2)} mm2 vs ${want.toFixed(2)} `
       + `(${shot && shot.px} px over ${thr} at ${im.mmpp} mm/px)`);

      // EXPOSURE, when the camera was given one.
      //
      // Same contract as the scale: with FAKE_CAM_EXPO_MS unset the file is
      // loaded exactly as it is, and a frame with no source_exposure_ms is a
      // 1 ms frame -- full brightness. Set it and the part's own level moves
      // with it, clipping at 255 the way a sensor does.
      //
      // 25%: the fake camera's own brightness jitter is +-20% by design, so a
      // tighter band would fail on the augmentation rather than on the scale.
      if (ratio !== 1)
        ok(`the part comes back at the ${expoMs} ms exposure level`,
           shot && Math.abs(shot.centre - partLevel) / partLevel < 0.25,
           `centre ${shot && shot.centre} vs ${partLevel.toFixed(0)} `
         + `(${lv.part} at ${fixture.source_exposure_ms || 1} ms)`);
    }
    ok(`the capture measures at this bench's scale${BENCH ? ' (' + BENCH + ')' : ''}`,
       !bench ? null
       : (bench.editorMmpp_noSignature == null
          ? im && im.mmpp !== 1
          : im && Math.abs(im.mmpp - bench.editorMmpp_noSignature) < 1e-9),
       `mmpp=${im && im.mmpp}  expected=${bench && bench.editorMmpp_noSignature}`);
    if (im) {
      const c = { x: im.wmm / 2, y: im.hmm / 2 };
      // JOURNEY_REG_DEG: the direction of the registration drag. The default is
      // horizontal, which is the one angle that hides every rotation bug -- so
      // it is worth being able to set up the same object at 90 degrees, the way
      // a part actually lands on the plate.
      const degs = Number(process.env.JOURNEY_REG_DEG) || 0;
      const rad = degs * Math.PI / 180, len = im.wmm * 0.18;
      ok(`the drag reaches the canvas${degs ? ' (registration at ' + degs + ' deg)' : ''}`,
         await dragWorld(c, { x: c.x + len * Math.cos(rad),
                              y: c.y + len * Math.sin(rad) }));
      ok('and authors a registration',
         await P.waitFor('step 1 done', async () =>
           ((await P.attr('sbm2', 'data-done')) || '').charAt(0) === '1', { timeout: 8000 }),
         `data-done=${await P.attr('sbm2', 'data-done')}  reg=${JSON.stringify(await P.store(`${EI}.def_image_reg`))}`);
    }
  }

  // ---------------------------------------------------------------------------
  section('3. set the include region through the commit path the tool uses');
  {
    // WATCH FOR THE REGISTRATION DISAPPEARING.
    //
    // Everything drawn from here on -- the region, the features, every
    // measurement afterwards -- is written relative to def_image_reg, and a
    // reducer clearing it mid-session is invisible on screen: the studio simply
    // shows step 1 as undone again. Wrapping dispatch is the only way to name
    // the action that did it. Asserted at the end of section 5.
    await ev(`(function(){var st=window.__GP_STORE__; if(st.__wrapped) return; st.__wrapped=1;
      window.__ACTLOG__=[]; var d=st.dispatch.bind(st);
      st.dispatch=function(a){ var before=(st.getState().UIData.edit_info||{}).def_image_reg;
        var r=d(a); var after=(st.getState().UIData.edit_info||{}).def_image_reg;
        if(before&&!after) window.__ACTLOG__.push('REG CLEARED BY '+(a&&a.type));
        return r; }; return true;})()`);

    // GEOMETRY BY API, not by four timed clicks.
    //
    // The polygon tool needs vertices spaced further apart than its 200 ms
    // double-release guard, inside a view that has finished settling, closed
    // within 12 px of the first one. Every one of those is a way for the test
    // to fail while the app is fine. __SBM2_TEST__.poly calls the same onPoly
    // the mouse path calls, so the app's commit logic still runs -- only the
    // pointer choreography is skipped. The drag in section 2 stays a real
    // press-move-release, which is the coverage that matters: it proves the
    // canvas tools respond to a human, and it is what caught the fit bug.
    ok('the studio exposes its authoring API', await ev(`!!window.__SBM2_TEST__`));
    const im = await imgWorld();
    if (im) {
      // World is the object frame now: the box is written around the origin the
      // last step put in the middle of the part.
      // In MILLIMETRES when the fixture says how big the part is: a box a
      // little outside its longest radius, which is the same physical box on
      // every instrument. Only the frame-fraction fallback moves with the lens.
      const pr = fixture && fixture.part && fixture.part.max_radius_mm;
      const rx = pr ? pr * 1.15 : im.wmm * 0.22;
      const ry = pr ? pr * 1.15 : im.hmm * 0.30;
      await ev(`window.__SBM2_TEST__.poly('include', ${JSON.stringify(
        [{ x: -1, y: -1 }, { x: 1, y: -1 }, { x: 1, y: 1 }, { x: -1, y: 1 }]
          .map((p) => ({ x: p.x * rx, y: p.y * ry })))})`);
      ok('the region lands in the def',
         await P.waitAttr('sbm2', 'data-regions', '1/0', { timeout: 8000 }),
         `data-regions=${await P.attr('sbm2', 'data-regions')}`);
      // In @__SBM_INFO__ and in mm -- never in shapeList, whose closed feature
      // vocabulary rejects unknown types and fails the whole def on load.
      const inc = await P.store(`${EI}.__loc_include`);
      ok('as object-frame mm in the SBM info', Array.isArray(inc) && inc.length === 1
         && Math.abs(inc[0][0].x) < im.wmm, JSON.stringify(inc && inc[0] && inc[0][0]));
      const shapes = await P.store(`${EI}.shapeList||[]`);
      ok('and nothing was written into shapeList',
         !shapes.some((x) => String(x && x.type).indexOf('loc_') === 0));
    }
  }

  // ---------------------------------------------------------------------------
  section('4. generate the features the def will actually locate with');
  {
    // Through the PINNED next button, which is how the studio is meant to be
    // driven -- it names the next action so nobody has to read the progress
    // bar. Poking the step-3 block directly toggles it CLOSED when the studio
    // has already advanced there on its own, which is a test failing on its own
    // navigation rather than on the app.
    ok('the studio offers generate as the next action',
       await P.waitAttr('sbm2-next', 'data-action', 'generate', { timeout: 8000 }),
       `data-action=${await P.attr('sbm2-next', 'data-action')}`);
    await P.click('sbm2-next');
    // The core trains from the scratch sidecar TAKE wrote. This is the check
    // that the whole capture chain joined up: a wrong path here produces zero
    // features and the def silently falls back to sig360 -- which still
    // locates, so nothing on screen looks wrong.
    ok('the core returns features',
       await P.waitFor('features', async () =>
         Number(await P.attr('sbm2', 'data-features')) > 0, { timeout: 30000 }),
       `data-features=${await P.attr('sbm2', 'data-features')}`);
    ok('the def now carries a feature cache', !!(await P.store(`${EI}.__shape_cache`)));
    ok('and the cache is not stale', !(await P.store(`${EI}.__shape_stale`)));
    // Four now: ROI joined the sequence. It mirrors step 3 rather than gating,
    // because "no explicit points" is a complete configuration, not an unfinished one.
    ok('every step is done', (await P.attr('sbm2', 'data-done')) === '1111',
       `data-done=${await P.attr('sbm2', 'data-done')}`);
  }

  // ---------------------------------------------------------------------------
  section('5. leave the studio and check the def survived it');
  {
    ok('the next action becomes 套用並離開',
       await P.waitAttr('sbm2-next', 'data-action', 'close', { timeout: 8000 }),
       `data-action=${await P.attr('sbm2-next', 'data-action')}`);
    await P.click('sbm2-next');
    // NOT waitGone: antd keeps a closed modal's DOM mounted, so "the node is
    // absent" would be waiting for something that never happens even when the
    // studio has closed correctly. What the operator sees is the visibility.
    ok('the studio closes', await P.waitFor('studio hidden', async () =>
      !(await studioVisible()), { timeout: 12000 }));
    ok('the registration survived', !!(await P.store(`${EI}.def_image_reg`)));
    ok('the features survived', !!(await P.store(`${EI}.__shape_cache`)));
    ok('the region survived',
       ((await P.store(`${EI}.__loc_include`)) || []).length === 1);
    ok('the engine is shape_based', (await P.store(`${EI}.locating_engine`)) === 'shape_based');
    // The registration is the one thing everything else is written against, and
    // it has been seen to vanish mid-session. The dispatch wrapper installed
    // above names the action that cleared it, which is the only useful thing to
    // know when it happens.
    const cleared = await P.store('window.__ACTLOG__||[]');
    ok('nothing silently cleared the registration', cleared.length === 0,
       JSON.stringify(cleared));
  }

  // ---------------------------------------------------------------------------
  section('6. 快速驗證 in CI, against the same frames');
  {
    ok('快速驗證 opens', await P.click('quick-verify'));
    const ciThere = await P.waitExists('quick-verify-ci', { timeout: 8000 });
    ok('the mode choice appears', ciThere);
    if (ciThere) {
      await P.click('quick-verify-ci');
      // The station overlay and the bypass switch are what make an off-station
      // drop distinguishable from a def that cannot locate.
      ok('the inspection view opens',
         await P.waitFor('report view', () => ev(
           `!!document.querySelector('.ant-modal-body canvas')`), { timeout: 20000 }));
      await sleep(2500);
      ok('it produces inspection reports', await P.waitFor('a report', () => ev(
        `(function(){var s=window.__GP_STORE__.getState().UIData;
           return !!(s.edit_info && s.edit_info.inspReport); })()`), { timeout: 20000 }),
        'no inspReport in the store');
    }
  }

  ok('no uncaught page errors along the way',
     (await ev(`window.__LASTERR__||''`)) === '',
     await ev(`window.__LASTERR__||''`));

  // LEAVE THE APP AT MAIN. The next suite can then reuse this page instead of
  // reloading, and a reload costs ~7s of camera-reconnect modal that nothing
  // can hurry. Best-effort: a suite that already failed must still report.
  await toMain(ctl).catch(() => {});

  process.exit(T.done() ? 1 : 0);
})().catch((e) => {
  console.error('\nHARNESS ERROR: ' + (e && e.stack ? e.stack : e));
  T.done();
  process.exit(2);
});
