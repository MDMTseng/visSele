// Functional regression for the def-authoring features, against a real core.
//
//   node regress_features.mjs [modelPath]
//   needs: core on 4090 (a camera for the streaming section -- it skips without
//          one), `npm run dev` on 8081, webctld on 8765
//
// WHY THIS LAYER EXISTS, stated plainly because it decides what belongs here.
//
// The unit suites cover the parts with an answer -- paths, scales, key sets, the
// candidate rule, the auto-exit rule -- and they have caught real defects. What
// they cannot see is WIRING, and each of these shipped, compiled, bundled and
// was installed before a person pressed a button and found it:
//
//   * a selector declared in the wrong component -- ReferenceError on click
//   * a counter reading a list the data had already left -- progress stuck
//   * a modal rendering a snapshot of state -- preview frozen
//   * a "stop" that kept the subscription -- the stream never stopped
//   * a field that has never existed -- viewfinder with no scale
//   * an editor lock silently discarding the reset -- TAKE kept the old def
//
// None is a logic error. Each is two correct pieces not joined, and the only
// instrument that sees them is the running app. So every check here asserts
// OBSERVABLE STATE after a real interaction.
//
// Two rules the checks obey:
//   - read a data-* attribute, never text or geometry. On this UI a wrong pick
//     CLICKS instead of failing, so a bad selector produces a GREEN run.
//   - wait for a CONDITION, never a duration. A fixed sleep encodes the speed of
//     the machine it was written on: wasteful here, a false failure on a slower
//     one, and it has to be re-tuned every time the app gets heavier.
import { makeCtl, toMain, dismissCamModal, loadRecipe, freshPage } from './lib_enter.mjs';
import { makeProbe, makeTally } from './_rf_lib.mjs';

const MODEL = process.argv[2] || process.env.WEBCTL_MODEL || 'data/test1';
const APP = process.env.WEBCTL_APP || 'http://127.0.0.1:8081/';
const ctl = makeCtl(`http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`);
const { api, ev } = ctl;
const P = makeProbe(ev);
const T = makeTally();
const { ok, section } = T;
const EI = 'window.__GP_STORE__.getState().UIData.edit_info';
const SM = 'window.__GP_STORE__.getState().UIData.c_state.value';

// Put the editor back on a known def. Every section starts here, so a section
// that fails cannot cascade into the next one reporting nonsense about a def it
// did not expect.
async function freshDef() {
  // toMain, NOT a bare EXIT dispatch. lib_enter's trap 2: EXIT lands on MAIN,
  // and MAIN + EXIT lands on SPLASH, which is a DEAD END while the socket is up
  // -- only a reconnect leaves it. Dispatching EXIT from wherever we happen to
  // be therefore works from the editor and destroys the run from the menu, and
  // the first call is always from the menu. toMain knows all of that.
  await toMain(ctl);
  await loadRecipe(ctl, MODEL);
  await ev(`window.__GP_STORE__.dispatch({ type: 'Edit_Mode' })`);
  return P.waitFor('def editor', async () =>
    JSON.stringify(await P.store(SM)).indexOf('DEFCONF_MODE') >= 0, { timeout: 25000 });
}

// Open the take dialog and get as far as the named viewfinder.
async function toViewfinder(name) {
  if (!(await P.click('take'))) return 'no take button';
  // A dirty def is asked about; a clean one goes straight through. Both are
  // legitimate, so wait for EITHER and act on whichever arrived -- branching on
  // a sleep would make the suite depend on which one it happened to hit.
  await P.waitFor('dialog or confirm', async () =>
    (await P.confirmShowing('還沒存檔')) || (await P.exists('take-name')), { timeout: 10000 });
  if (await P.confirmShowing('還沒存檔')) await P.confirmClick('丟掉變更');
  if (!(await P.waitExists('take-name', { timeout: 10000 }))) return 'name phase never appeared';
  await P.setInput('take-name', name);
  if (!(await P.waitAttr('take-next', 'data-enabled', '1', { timeout: 6000 })))
    return 'next never enabled';
  await P.click('take-next');
  if (!(await P.waitExists('take-capture', { timeout: 10000 }))) return 'viewfinder never appeared';
  return null;
}

(async () => {
  console.log(`app ${APP}   model ${MODEL}`);
  await freshPage(ctl, APP);
  await P.waitFor('app mounted', async () =>
    (await ev(`typeof window.__GP_STORE__`)) === 'object', { timeout: 40000 });
  await toMain(ctl);
  await dismissCamModal(ctl);
  ok('the editor opens on a known def', await freshDef(), JSON.stringify(await P.store(SM)));

  section('the SBM entry is gated on the locator AND on the lock');
  {
    // Two gates, one button. The locator gate is old: the entry used to be
    // unconditional AND switched the engine on click, so a sig360 recipe was
    // converted by being looked at.
    //
    // The lock gate is new, and it is the same defConf_lock_level that drops
    // DefConf actions silently -- the studio would open, take a registration
    // line, redraw itself as though it had kept it, and change nothing.
    //
    // The gate is asserted by DRIVING the lock, not by assuming the state the
    // editor was entered in. Entering leaves it at 1, but a suite that reuses
    // an open page inherits whatever the previous section left -- an assertion
    // that depends on that is testing the order of the file.
    const engine = await P.store(`${EI}.locating_engine`);
    const shown = () => P.exists('sbm-studio-v2');

    if (engine !== 'shape_based') {
      ok('a sig360 def offers no SBM entry', !(await shown()), `engine=${engine}`);
    } else {
      await P.dispatch({ type: 'DefConf_Lock_Level_Update', data: 1 });
      ok('locked, so the studio is not offered', await P.waitFor('sbm entry gone',
        async () => !(await shown()), { timeout: 5000 }));

      await P.dispatch({ type: 'DefConf_Lock_Level_Update', data: 0 });
      ok('unlocking reveals it', await P.waitFor('sbm entry', shown, { timeout: 5000 }));

      await P.dispatch({ type: 'DefConf_Lock_Level_Update', data: 1 });
      ok('locking again hides it', await P.waitFor('sbm entry gone',
        async () => !(await shown()), { timeout: 5000 }));
      await P.dispatch({ type: 'DefConf_Lock_Level_Update', data: 0 });
    }

    // v1 is gone. Asserted rather than assumed: two studios reading the same
    // def is two places for the same bug, and a leftover button is how one
    // comes back.
    ok('the old studio is gone', !(await P.exists('sbm-studio-v1')));
  }

  section('TAKE: unsaved work is asked about, and the safe answer is safe');
  {
    await P.dispatch({ type: 'Matching_Angle_Margin_Deg_Update', data: 137 });
    const hash = await P.store(`${EI}.DefFileHash`);
    await P.click('take');
    ok('a dirty def is asked about',
       await P.waitFor('confirm', () => P.confirmShowing('還沒存檔'), { timeout: 10000 }));
    // Checking the DEFAULT branch is the half that matters: a confirm whose safe
    // answer is not safe is worse than no confirm at all.
    await P.confirmClick('先回去存檔');
    await P.waitFor('confirm closed', async () => !(await P.confirmShowing('還沒存檔')),
                    { timeout: 6000 });
    ok('answering "go and save" opens nothing', !(await P.exists('take-name')));
    ok('and changes nothing', (await P.store(`${EI}.DefFileHash`)) === hash);
  }

  section('TAKE: the viewfinder starts on the def, not on the camera');
  {
    const why = await toViewfinder('RF-REUSE');
    ok('the viewfinder opens', why === null, why || '');
    if (!why) {
      ok('it starts idle', (await P.attr('take-capture', 'data-phase')) === 'idle');
      ok('on the def scale', (await P.attr('take-capture', 'data-from-camera')) === '0');
      ok('on __CACHE_IMG__', (await P.attr('take-capture', 'data-src')) === 'cache');
      ok('with the keep switch clear', (await P.attr('take-capture', 'data-keep')) === '0');
      ok('and use-frame available, because the def has an image',
         (await P.disabled('take-use-frame')) === false,
         `has-image=${await P.attr('take-capture', 'data-has-image')}`);
    }
  }

  section('TAKE: 使用現有圖像 builds a new object from the def image');
  {
    const before = await P.store(`${EI}.defModelPath`);
    await P.click('take-use-frame');
    ok('the studio opens by itself', await P.waitExists('sbm2', { timeout: 30000 }));
    ok('the name is applied', (await P.store(`${EI}.DefFileName`)) === 'RF-REUSE',
       JSON.stringify(await P.store(`${EI}.DefFileName`)));
    ok('the engine becomes shape_based',
       (await P.store(`${EI}.locating_engine`)) === 'shape_based');
    const after = await P.store(`${EI}.defModelPath`);
    ok('it claims a new file, so the source def cannot be overwritten',
       after !== before && String(after).indexOf('RF-REUSE') >= 0, `${before} -> ${after}`);
    // The editor-lock defect lived here: Def_Retake was discarded and the "new
    // object" kept the previous registration, cache and name.
    ok('the registration is cleared', !(await P.store(`${EI}.def_image_reg`)));
    ok('the feature cache is cleared', !(await P.store(`${EI}.__shape_cache`)));
    ok('it is marked a fresh capture', (await P.store(`${EI}.__img_fresh_capture`)) === true);
  }

  section('the studio reflects the def, not a visited flag');
  if (await P.exists('sbm2')) {
    ok('progress starts at step 1', (await P.attr('sbm2', 'data-step')) === '1');
    // Four steps since ROI joined the sequence, and none of them is done on a
    // fresh capture -- including ROI, which mirrors the feature step rather
    // than gating on points of its own.
    ok('nothing is marked done', (await P.attr('sbm2', 'data-done')) === '0000',
       `data-done=${await P.attr('sbm2', 'data-done')}`);

    // Step 3's block is collapsed until it is current, so its button is not in
    // the DOM. Asserting on an absent element reads as "not disabled" -- the
    // wrong answer for the right reason, and it would pass a broken guard.
    await P.click('sbm2-block-step-2');
    await P.waitExists('sbm2-generate', { timeout: 6000 });
    ok('generate is refused before there is an object frame',
       (await P.disabled('sbm2-generate')) === true);

    // Regions live in @__SBM_INFO__ / edit_info.__loc_*, NOT in shapeList. The
    // counter read shapeList and was therefore always 0, pinning progress on
    // step 2 with the region visibly drawn beside the message.
    const poly = [{ x: -2, y: -2 }, { x: 2, y: -2 }, { x: 2, y: 2 }, { x: -2, y: 2 }];
    await P.dispatch({ type: 'EditInfo_Patch', data: { __loc_include: [poly] } });
    ok('an include region is counted',
       await P.waitAttr('sbm2', 'data-regions', '1/0', { timeout: 6000 }),
       `data-regions=${await P.attr('sbm2', 'data-regions')}`);

    await P.dispatch({ type: 'EditInfo_Patch',
      data: { def_image_reg: { cx: 15, cy: 12, angle: 0, isFlipped: false } } });
    ok('a registration completes step 1',
       await P.waitFor('step1 done', async () =>
         ((await P.attr('sbm2', 'data-done')) || '').charAt(0) === '1', { timeout: 6000 }),
       `data-done=${await P.attr('sbm2', 'data-done')}`);
    ok('generate is then offered', (await P.disabled('sbm2-generate')) === false);
    ok('progress moves on', (await P.attr('sbm2', 'data-step')) === '3',
       `data-step=${await P.attr('sbm2', 'data-step')}`);
    // The params block is collapsed too, so open it and read the VALUE. The
    // first version of this check only asked whether the element existed, which
    // it never did, and "is it there" would not have caught a default of 0 --
    // and 0 turns the position filter off entirely.
    await P.click('sbm2-block-opt-103');
    const gotTol = await P.waitExists('sbm2-postol', { timeout: 6000 });
    const tol = gotTol ? await ev(
      `(function(){var e=document.querySelector('[data-testid="sbm2-postol"]');
         var i=e&&(e.tagName==='INPUT'?e:e.querySelector('input'));
         return i?i.value:'__NOINPUT__'; })()`) : '__MISSING__';
    ok('the position tolerance is exposed and defaults to 20 px', tol === '20', `value=${tol}`);
  } else {
    ok('studio checks', null, 'the studio did not open');
  }

  section('TAKE: streaming takes the camera frame, and the machine scale with it');
  {
    await freshDef();
    const why = await toViewfinder('RF-STREAM');
    if (why) { ok('the viewfinder opens', false, why); }
    else {
      await P.click('take-stream-start');
      const streaming = await P.waitAttr('take-capture', 'data-phase', 'streaming', { timeout: 10000 });
      if (!streaming) {
        ok('streaming starts', null,
           `phase=${await P.attr('take-capture', 'data-phase')} -- no camera?`);
      } else {
        ok('streaming starts', true);
        ok('the stop control appears', await P.exists('take-stream-stop'));
        ok('the other capture controls are withdrawn',
           !(await P.exists('take-stream-start')) && !(await P.exists('take-wait-trigger')));
        ok('use-frame is refused while streaming', (await P.disabled('take-use-frame')) === true);
        ok('the scale switches to the machine',
           (await P.attr('take-capture', 'data-from-camera')) === '1');
        ok('the frame source switches to the streamed cache',
           (await P.attr('take-capture', 'data-src')) === 'lastview');

        // Identity, not size: edit_info.img is an object whose shape differs by
        // codec, and reading byteLength off the wrapper gives 0 for both samples
        // -- reporting a working stream as broken. This also covers the editor
        // lock, which discarded IMAGE actions exactly as it discarded the reset.
        await ev(`window.__RF_IMG__ = ${EI}.img;`);
        ok('frames actually arrive',
           await P.waitFor('a new frame', () => ev(
             `(function(){ var n = ${EI}.img;
                if (n === window.__RF_IMG__) return false;
                window.__RF_IMG__ = n; return true; })()`), { timeout: 12000 }));

        await P.click('take-stream-stop');
        ok('stopping returns it to idle',
           await P.waitAttr('take-capture', 'data-phase', 'idle', { timeout: 10000 }));
        ok('use-frame is then offered', (await P.disabled('take-use-frame')) === false);
        ok('and the frame is still the streamed one',
           (await P.attr('take-capture', 'data-src')) === 'lastview');

        await P.click('take-use-frame');
        ok('the studio opens on the captured frame',
           await P.waitExists('sbm2', { timeout: 30000 }));
        ok('the name is applied', (await P.store(`${EI}.DefFileName`)) === 'RF-STREAM');
        ok('the scratch template was stamped',
           String(await P.store(`${EI}.__tmp_ref_image_path`)).indexOf('__retake_ref') >= 0);
      }
    }
  }

  section('TAKE: 保留量測設定 keeps the measurements and still clears the localizer');
  {
    await freshDef();
    const featsBefore = await P.store(`(${EI}._obj.shapeList||[]).length`);
    const why = await toViewfinder('RF-KEEP');
    if (why) { ok('the viewfinder opens', false, why); }
    else {
      await P.click('take-keep');
      ok('the switch reads kept',
         await P.waitAttr('take-capture', 'data-keep', '1', { timeout: 5000 }));
      // Watch for the registration coming BACK, and name what did it. This is
      // how the ATBundle race was caught: a def-load reply landing after the
      // retake re-applied the old def, about one run in three, and the only
      // visible trace was this assertion failing with the old values in place.
      await ev(`(function(){var st=window.__GP_STORE__; if(st.__kw) return; st.__kw=1;
        window.__KLOG__=[]; var d=st.dispatch.bind(st);
        st.dispatch=function(a){ var b=(st.getState().UIData.edit_info||{}).def_image_reg;
          var r=d(a); var c=(st.getState().UIData.edit_info||{}).def_image_reg;
          if(!!b!==!!c) window.__KLOG__.push((c?'RESTORED by ':'cleared by ')+(a&&a.type));
          return r; }; })()`);
      await P.click('take-use-frame');
      await P.waitExists('sbm2', { timeout: 30000 });
      const featsAfter = await P.store(`(${EI}._obj.shapeList||[]).length`);
      ok('the measurement features survive',
         featsAfter === featsBefore && featsBefore > 0, `${featsBefore} -> ${featsAfter}`);
      // The other half, and the one nothing can restore: these describe a frame
      // that no longer exists, so keep-mode has to clear them too.
      // Polled, not read once. The studio opens as soon as the capture lands,
      // and the patch that clears the old localizer arrives on its own tick --
      // so reading immediately caught the pre-clear state about one run in
      // three and reported a working app as broken.
      const _regCleared = await P.waitFor('reg cleared', async () =>
        !(await P.store(`${EI}.def_image_reg`)), { timeout: 8000 });
      ok('the registration is still cleared', _regCleared,
         `reg=${JSON.stringify(await P.store(`${EI}.def_image_reg`))} `
       + `log=${JSON.stringify(await P.store('window.__KLOG__||[]'))} `
       + `keep=${await P.attr('take-capture','data-keep')} `
       + `fresh=${JSON.stringify(await P.store(`${EI}.__img_fresh_capture`))}`);
      ok('the feature cache is still cleared',
         await P.waitFor('cache cleared', async () =>
           !(await P.store(`${EI}.__shape_cache`)), { timeout: 8000 }));
    }
  }

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
