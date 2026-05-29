#!/usr/bin/env node
/*
 * QA R2 — MEASURE value <-> control-limit COUPLING (pure shape logic).
 *
 * VIEWPOINT: the pure coupling math in src/shapes/measure.js
 *   applyMeasureLimitCoupling(obj, changedKey, preVal) — MUTATES obj, deriving
 *   control limits from value/USL/LSL (+ _b back-value variants).
 *
 * Now DIRECTLY testable: a new __DEV_MODE__-gated hook exposes the REAL function
 * at window.__GP_MEASURE__.applyMeasureLimitCoupling. We construct an obj in the
 * browser, call the real function, and assert the mutated result against an
 * ORACLE transcribed verbatim from measure.js. CORE-INDEPENDENT: just /reload +
 * wait for the window hook; no reset()/LD/core backend needed.
 *
 * ORACLE (transcribed exactly from src/shapes/measure.js; round = round-to-0.001):
 *   - early return if obj.value === undefined (obj left unchanged).
 *   - "value":   LCL/UCL/LSL/USL each = round(orig - preVal + obj.value)  (delta shift)
 *   - "value_b": LCL_b/UCL_b/LSL_b/USL_b each = round(orig - preVal + obj.value)
 *                QUIRK: uses obj.value (NOT obj.value_b) — tested as-is.
 *   - "LSL":     LCL   = round(value   + (LSL   - value)  *2/3)
 *   - "USL":     UCL   = round(value   + (USL   - value)  *2/3)
 *   - "LSL_b":   LCL_b = round(value_b + (LSL_b - value_b)*2/3)
 *   - "USL_b":   UCL_b = round(value_b + (USL_b - value_b)*2/3)
 *
 * TEST PLAN (each: build obj -> call REAL fn in-browser -> compare to oracle):
 *   1. value shift: value 10->12, LCL/UCL/LSL/USL all shift +2.
 *   2. USL change:  value=10,USL=13 -> UCL=12.
 *   3. LSL change:  value=10,LSL=7  -> LCL=8.
 *   4. value_b shift: value_b 5->8, _b limits shift by (obj.value - preVal) using
 *      obj.value (the documented quirk) — pick obj.value != value_b to expose it.
 *   5. USL_b change: value_b=10,USL_b=13 -> UCL_b=12.
 *   6. LSL_b change: value_b=10,LSL_b=7  -> LCL_b=8.
 *   7. early-return: value undefined -> obj unchanged.
 *   8. rounding: values forcing 0.001 rounding (e.g. *2/3 non-terminating).
 *
 * Prints "[name] PASS/FAIL" with expected/got; exits non-zero on any real fail.
 * Run: node tools/webctl/qa/r2_measure.mjs
 */

const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;

async function api(p, body) {
  const r = await fetch(BASE + p, {
    method: body ? 'POST' : 'GET',
    headers: body ? { 'Content-Type': 'application/json' } : undefined,
    body: body ? JSON.stringify(body) : undefined,
  });
  const j = await r.json();
  if (!r.ok) throw new Error(JSON.stringify(j));
  return j;
}
const ev = (expr) => api('/eval', { expr }).then((r) => r.result);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

let failures = 0;
function report(name, ok, detail) {
  console.log(`[${name}] ${ok ? 'PASS' : 'FAIL'}${detail ? ' — ' + detail : ''}`);
  if (!ok) failures++;
}

// --- ORACLE (verbatim from src/shapes/measure.js) ---------------------------
function oRound(x, step) { return Math.round(x / step) * step; } // mirrors UTIL round(value,step)
function oracle(obj, changedKey, preVal) {
  const r = (x) => oRound(x, 0.001);
  if (obj.value === undefined) return;
  switch (changedKey) {
    case 'value':
      obj.LCL = r(obj.LCL - preVal + obj.value);
      obj.UCL = r(obj.UCL - preVal + obj.value);
      obj.LSL = r(obj.LSL - preVal + obj.value);
      obj.USL = r(obj.USL - preVal + obj.value);
      break;
    case 'value_b':
      obj.LCL_b = r(obj.LCL_b - preVal + obj.value); // quirk: obj.value
      obj.UCL_b = r(obj.UCL_b - preVal + obj.value);
      obj.LSL_b = r(obj.LSL_b - preVal + obj.value);
      obj.USL_b = r(obj.USL_b - preVal + obj.value);
      break;
    case 'LSL':   obj.LCL   = r(obj.value   + (obj.LSL   - obj.value)   * 2 / 3); break;
    case 'USL':   obj.UCL   = r(obj.value   + (obj.USL   - obj.value)   * 2 / 3); break;
    case 'LSL_b': obj.LCL_b = r(obj.value_b + (obj.LSL_b - obj.value_b) * 2 / 3); break;
    case 'USL_b': obj.UCL_b = r(obj.value_b + (obj.USL_b - obj.value_b) * 2 / 3); break;
  }
}

// Call the REAL in-browser function on a clone of `input`; return mutated obj.
async function callReal(input, changedKey, preVal) {
  const expr = `(function(){
    var fn = window.__GP_MEASURE__ && window.__GP_MEASURE__.applyMeasureLimitCoupling;
    if (typeof fn !== 'function') return {__err:'hook missing'};
    var obj = ${JSON.stringify(input)};
    fn(obj, ${JSON.stringify(changedKey)}, ${JSON.stringify(preVal)});
    return obj;
  })()`;
  return ev(expr);
}

function near(a, b) {
  if (a === undefined || b === undefined) return a === b;
  return Math.abs(a - b) < 1e-9;
}

// Compare real vs oracle across the keys that matter for this change.
async function runCase(name, input, changedKey, preVal, keys) {
  try {
    const got = await callReal(input, changedKey, preVal);
    if (got && got.__err) { report(name, false, got.__err); return; }
    const exp = JSON.parse(JSON.stringify(input));
    oracle(exp, changedKey, preVal);
    let ok = true;
    const diffs = [];
    for (const k of keys) {
      if (!near(got[k], exp[k])) { ok = false; diffs.push(`${k}: exp ${exp[k]} got ${got[k]}`); }
    }
    report(name, ok, ok ? keys.map((k) => `${k}=${got[k]}`).join(' ') : diffs.join('; '));
  } catch (e) {
    report(name, false, String(e));
  }
}

async function waitReady() {
  await api('/reload', {});
  for (let i = 0; i < 60; i++) {
    await sleep(150);
    const ok = await ev(
      "typeof window.__GP_MEASURE__==='object' && typeof window.__GP_MEASURE__.applyMeasureLimitCoupling==='function'"
    ).catch(() => false);
    if (ok === true) return true;
  }
  throw new Error('__GP_MEASURE__.applyMeasureLimitCoupling never appeared — is app on :8081 in __DEV_MODE__?');
}

async function main() {
  await waitReady();

  // 1. value shift: value 10->12 (+2). all four limits shift +2.
  await runCase('value.shift',
    { value: 12, LCL: 8, UCL: 12, LSL: 7, USL: 13 }, 'value', 10,
    ['LCL', 'UCL', 'LSL', 'USL']);

  // 2. USL change: value=10,USL=13 -> UCL=12.
  await runCase('USL.change',
    { value: 10, USL: 13, UCL: 0 }, 'USL', 13, ['UCL']);

  // 3. LSL change: value=10,LSL=7 -> LCL=8.
  await runCase('LSL.change',
    { value: 10, LSL: 7, LCL: 0 }, 'LSL', 7, ['LCL']);

  // 4. value_b shift QUIRK: uses obj.value, not value_b. value=12 (not value_b).
  //    preVal=5, obj.value=12 -> shift = 12-5 = +7 on each _b limit.
  await runCase('value_b.shift(quirk-uses-value)',
    { value: 12, value_b: 8, LCL_b: 8, UCL_b: 12, LSL_b: 7, USL_b: 13 }, 'value_b', 5,
    ['LCL_b', 'UCL_b', 'LSL_b', 'USL_b']);

  // 5. USL_b change: value_b=10,USL_b=13 -> UCL_b=12.
  await runCase('USL_b.change',
    { value: 1, value_b: 10, USL_b: 13, UCL_b: 0 }, 'USL_b', 13, ['UCL_b']);

  // 6. LSL_b change: value_b=10,LSL_b=7 -> LCL_b=8.
  await runCase('LSL_b.change',
    { value: 1, value_b: 10, LSL_b: 7, LCL_b: 0 }, 'LSL_b', 7, ['LCL_b']);

  // 7. early-return: value undefined -> obj unchanged.
  {
    const name = 'early-return(value-undefined)';
    try {
      const input = { LCL: 8, UCL: 12, LSL: 7, USL: 13 }; // no value
      const got = await callReal(input, 'value', 10);
      if (got && got.__err) { report(name, false, got.__err); }
      else {
        const ok = near(got.LCL, 8) && near(got.UCL, 12) && near(got.LSL, 7) && near(got.USL, 13)
          && got.value === undefined;
        report(name, ok, ok ? 'unchanged' : `mutated: ${JSON.stringify(got)}`);
      }
    } catch (e) { report(name, false, String(e)); }
  }

  // 8. rounding: USL change with value=10,USL=11 -> UCL = 10 + 1*2/3 = 10.6666.. -> round 0.001 = 10.667
  await runCase('rounding.USL(2/3)',
    { value: 10, USL: 11, UCL: 0 }, 'USL', 11, ['UCL']);
  // and a value shift forcing rounding: orig LCL 8.0005, +0.0001 -> round to 0.001
  await runCase('rounding.value-shift',
    { value: 10.0001, LCL: 8.0005, UCL: 12.0004, LSL: 7.0006, USL: 13.0007 }, 'value', 10,
    ['LCL', 'UCL', 'LSL', 'USL']);

  console.log(failures ? `\n${failures} test(s) FAILED` : '\nALL PASS');
  process.exit(failures ? 1 : 0);
}

main().catch((e) => { console.error('FATAL', e); process.exit(1); });
