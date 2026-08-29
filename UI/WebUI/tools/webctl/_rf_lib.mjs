// Shared primitives for the functional regression suites.
//
// SEPARATED FROM THE CHECKS so that the way we wait, click and read state has
// ONE definition. The suites that came before this one each grew their own, and
// two of them rotted; lib_enter.mjs exists for the same reason and says so.
//
// THE WAITING IS THE POINT.
//
// The first version of regress_features was a chain of sleep(800) and
// sleep(4000), chosen by trying numbers until a run went green. That is wrong in
// both directions at once: on this machine it wastes most of the run standing
// still, and on a slower one -- or a machine where a def parse takes longer than
// it did today -- it reports a working app as broken. A fixed sleep encodes the
// speed of the machine it was written on.
//
// So every wait here is a CONDITION with a deadline. It returns as soon as the
// thing is true, and when it is not true by the deadline it says what it was
// waiting for, which is the message somebody actually needs.
export const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

export function makeProbe(ev) {
  // Poll a predicate. Returns true, or false on timeout -- never throws, because
  // "it did not happen" is an assertion result, not a harness failure.
  const waitFor = async (what, fn, { timeout = 15000, interval = 120 } = {}) => {
    const t0 = Date.now();
    for (;;) {
      let v = false;
      try { v = await fn(); } catch { v = false; }
      if (v) return true;
      if (Date.now() - t0 > timeout) return false;
      await sleep(interval);
    }
  };

  // Read a data-* attribute off a testid.
  //
  // Three outcomes, deliberately distinguished: the element is missing, the
  // attribute is missing, or it has a value. A probe that collapses the first
  // two into "" reports a screen that never opened as a wrong value, which is
  // how a broken navigation gets diagnosed as a broken feature.
  const attr = (tid, name) => ev(
    `(function(){var e=document.querySelector('[data-testid="${tid}"]');
       if(!e) return '__MISSING__';
       var v=e.getAttribute('${name}'); return v===null?'__NOATTR__':v; })()`);

  const exists = (tid) => ev(`!!document.querySelector('[data-testid="${tid}"]')`);

  // Click by testid, in the page. NOT through the driver's click: Playwright
  // waits for a disabled control to become enabled and then times out, so
  // clicking something that should be refused HANGS the run instead of failing
  // it -- and half the assertions here are about things being refused.
  const click = (tid) => ev(
    `(function(){var e=document.querySelector('[data-testid="${tid}"]');
       if(!e) return false;
       var b=e.closest('button')||e.querySelector('button')||e;
       if(b.disabled) return 'disabled';
       b.click(); return true; })()`);

  // antd marks a disabled button with the property and a class; either alone
  // makes a click a no-op, so both count.
  const disabled = (tid) => ev(
    `(function(){var e=document.querySelector('[data-testid="${tid}"]');
       if(!e) return '__MISSING__';
       var b=e.closest('button')||e.querySelector('button')||e;
       return !!(b.disabled || (b.className||'').indexOf('disabled')>=0); })()`);

  // Wait until an attribute reaches a value. The common case, and the one that
  // replaces "sleep until the modal has probably re-rendered".
  const waitAttr = (tid, name, want, opts) =>
    waitFor(`${tid}[${name}]=${want}`, async () => (await attr(tid, name)) === want, opts);

  const waitExists = (tid, opts) => waitFor(`${tid} exists`, () => exists(tid), opts);
  const waitGone = (tid, opts) =>
    waitFor(`${tid} gone`, async () => !(await exists(tid)), opts);

  // Set a controlled React input. Assigning .value directly does not reach
  // React's onChange; the native setter plus an input event does.
  const setInput = (tid, value) => ev(
    `(function(){var e=document.querySelector('[data-testid="${tid}"]');
       if(!e) return false;
       var i = e.tagName==='INPUT' ? e : e.querySelector('input');
       if(!i) return false;
       var s=Object.getOwnPropertyDescriptor(window.HTMLInputElement.prototype,'value').set;
       s.call(i, ${JSON.stringify(String(value))});
       i.dispatchEvent(new Event('input',{bubbles:true}));
       return true; })()`);

  const store = (path) => ev(`JSON.stringify((function(){try{ return (${path}); }
      catch(e){ return '__ERR__'+e.message; }})())`)
    .then((s) => { try { return JSON.parse(s); } catch { return s; } });

  const dispatch = (obj) => ev(`window.__GP_STORE__.dispatch(${JSON.stringify(obj)})`);

  // Click a modal-confirm button by the text ON THAT BUTTON. The exception to
  // the no-text rule, and a narrow one: antd renders confirm buttons with no
  // stable hook, and the set is two buttons inside one .ant-modal-confirm-btns.
  const confirmClick = (text) => ev(
    `(function(){var b=Array.from(document.querySelectorAll('.ant-modal-confirm-btns button'))
       .find(function(x){return x.textContent.indexOf(${JSON.stringify(text)})>=0;});
       if(!b) return false; b.click(); return true; })()`);

  const confirmShowing = (text) => ev(
    `!!Array.from(document.querySelectorAll('.ant-modal-confirm-title'))
       .find(function(e){return e.textContent.indexOf(${JSON.stringify(text)})>=0;})`);

  return { waitFor, attr, exists, click, disabled, waitAttr, waitExists, waitGone,
           setInput, store, dispatch, confirmClick, confirmShowing };
}

// A tally that prints as it goes, so a run that hangs still shows what it got
// through -- the first thing anyone wants when a suite stops responding.
export function makeTally() {
  const rows = [];
  let pass = 0, fail = 0, skip = 0;
  const ok = (name, cond, detail) => {
    if (cond === null || cond === undefined) {
      skip++; rows.push(['SKIP', name, detail]);
      console.log(`  SKIP ${name}${detail ? '  -- ' + detail : ''}`);
    } else if (cond) {
      pass++; rows.push(['ok', name, detail]);
      console.log(`  ok   ${name}`);
    } else {
      fail++; rows.push(['FAIL', name, detail]);
      console.log(`  FAIL ${name}${detail ? '  -- ' + detail : ''}`);
    }
  };
  const section = (s) => console.log('\n' + s);
  const done = () => {
    console.log(`\n${pass} ok, ${fail} FAIL, ${skip} skipped`);
    return fail;
  };
  return { ok, section, done, get counts() { return { pass, fail, skip }; } };
}
