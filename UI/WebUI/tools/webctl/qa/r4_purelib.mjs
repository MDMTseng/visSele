#!/usr/bin/env node
/*
 * QA R4 — PURE UTILITY MODULES (expr.js / structures.js / MISC_Util.js helpers).
 *
 * VIEWPOINT: the small pure functions every editor path depends on:
 *   - PostfixExpCalc / Exp2PostfixExp (def-expression engine)
 *   - round (nearest-multiple, used by measure coupling and many sheets)
 *   - GetObjElement (safe deep get on path[])
 *   - dictLookUp (i18n fallback semantics)
 *   - CircularCounter / ConsumeQueue (structures)
 *
 * REACHABILITY FINDINGS (investigated against src/script.jsx):
 *   The following dev hooks ARE exposed on window:
 *     __GP_STORE__, __GP_DEF__, __GP_LOAD_BY_PATH__, __GP_DIAG__,
 *     __GP_DB_QUEUE__, __GP_BPG__, __GP_MEASURE__
 *   NO __GP_UTIL__ hook exists. PostfixExpCalc / round / GetObjElement /
 *   dictLookUp / CircularCounter / ConsumeQueue are NOT reachable from the
 *   browser eval surface today. MISC_Util re-exports them only for ES-module
 *   imports inside the bundle; they are tree-shaken / closed-over and there is
 *   no `window.PostfixExpCalc`, `window.round`, etc.
 *
 *   FOLLOW-UP PROPOSAL FOR PARENT: add to src/script.jsx (near line 101):
 *     import { round, GetObjElement, dictLookUp } from './UTIL/MISC_Util';
 *     import { PostfixExpCalc, Exp2PostfixExp } from './UTIL/expr';
 *     import { CircularCounter, ConsumeQueue } from './UTIL/structures';
 *     window.__GP_UTIL__ = {
 *       PostfixExpCalc, Exp2PostfixExp,
 *       round, GetObjElement, dictLookUp,
 *       CircularCounter, ConsumeQueue,
 *     };
 *   That single line unlocks in-browser verification of these pure modules.
 *
 * TEST PLAN (this script, core-INDEPENDENT):
 *  Phase A — REACHABILITY PROBE (live browser via webctld):
 *    - /reload, waitReady, then probe window for __GP_UTIL__ and friends.
 *    - Report which pure utils are actually reachable. If __GP_UTIL__ exists,
 *      run the in-browser variants of every test below; otherwise skip the
 *      in-browser arm cleanly and continue with the reference arm.
 *  Phase B — REFERENCE-IMPLEMENTATION TESTS (Node, mirrors src exactly):
 *    Pure-formula transcriptions verbatim from src/UTIL/expr.js and
 *    src/UTIL/MISC_Util.js — same approach r1_purelogic uses for
 *    measure.coupling. Validates the semantics the editor depends on:
 *      expr.basic           "1+2*3"  -> 7
 *      expr.precedence      "(1+2)*3"-> 9
 *      expr.unknownTokenNaN "foo+1"  -> NaN  (default funcSet parseFloat fallthrough)
 *      expr.malformedSafe   ""       -> does not throw
 *      expr.minMax          "min(3,1,2)"  -> 1   (multi-group flag)
 *      expr.mathSin         "Math.sin(0)" -> 0
 *      round.basic          round(1.2345, 0.001) -> 1.234 (or .235; nearest-multiple)
 *      round.coarse         round(12.34, 1)      -> 12
 *      round.NaNSafe        round(NaN, 0.001)    -> NaN, no throw
 *      GetObjElement.present  {a:{b:{c:7}}}, [a,b,c] -> 7
 *      GetObjElement.missing  {a:{}}, [a,b,c] -> undefined, no throw
 *      GetObjElement.undefRoot undefined,[x]  -> undefined
 *      dictLookUp.hit       {_: {hello:'hi'}} hello -> 'hi'
 *      dictLookUp.miss      {_: {}} hello -> 'hello'   (returns key as fallback)
 *      dictLookUp.arrayKey  GetObjElement-style path; missing -> last key
 *      CircularCounter.fifoWrap   enQ/deQ wraps cleanly at total_size
 *
 * Idempotent, no core needed. Exits non-zero on any real failure.
 * Run: node tools/webctl/qa/r4_purelib.mjs
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
let skipped = 0;
function report(name, ok, detail) {
  console.log(`[${name}] ${ok ? 'PASS' : 'FAIL'}${detail ? ' — ' + detail : ''}`);
  if (!ok) failures++;
}
function skip(name, why) {
  console.log(`[${name}] SKIP — ${why}`);
  skipped++;
}

async function waitReady() {
  await api('/reload', {});
  for (let i = 0; i < 60; i++) {
    await sleep(150);
    const ok = await ev(
      "typeof window.__GP_STORE__==='object' && typeof window.__GP_DIAG__==='object'"
    );
    if (ok === true) return true;
  }
  throw new Error('dev hooks never appeared — is app running with WEBCTL daemon?');
}

// ---------------------------------------------------------------------------
// Phase B reference implementations — verbatim transcriptions from src.
// ---------------------------------------------------------------------------

// --- expr.js (verbatim) ----------------------------------------------------
function extractAtomGroup(exp_str_arr, mark = "$") {
  let lastH = exp_str_arr[exp_str_arr.length - 1];
  let matchD = lastH.match(/\(([^\(\)]+)\)/);
  if (matchD !== null && matchD.length > 0) {
    exp_str_arr[exp_str_arr.length - 1] = matchD[1];
    var newstr = lastH.replace(/\([^\(\)]+\)/, mark);
    exp_str_arr.push(newstr);
  }
  return exp_str_arr;
}
function extractArithGroup(expression, sep = "\\-\\+", mark = "$") {
  let regx_param, regx_opera;
  if (mark == "$") mark += "$";
  regx_param = new RegExp("(.+)[" + sep + "](.+)");
  regx_opera = new RegExp(".+([" + sep + "]).+");
  let matchD = expression.match(regx_param);
  if (matchD == null) return [expression];
  var rep = expression.replace(regx_opera, mark + "$1" + mark);
  return [matchD[1], matchD[2], rep];
}
function extractArith(exp_stack, operator_regexp = "\\-\\+", mark = "$") {
  return exp_stack.map(exp => {
    if (operator_regexp == "," && exp.includes(",")) {
      if (exp.includes(",")) {
        let splitArr = exp.split(",");
        let aggStr = mark;
        for (let i = 1; i < splitArr.length; i++) aggStr += "," + mark;
        splitArr.push(aggStr);
        return splitArr;
      }
      return exp;
    }
    let parse_target = operator_regexp;
    let arrayX = extractArithGroup(exp, parse_target, mark);
    while (true) {
      let arrayY = extractArithGroup(arrayX[0], parse_target, mark);
      if (arrayY.length == 1) break;
      arrayX.shift();
      arrayX = arrayY.concat(arrayX);
    }
    return arrayX;
  }).flat();
}
function Exp2PostfixExp(exp_str) {
  let retA = [exp_str];
  let p_Mark = "$";
  while (true) {
    let currentSize = retA.length;
    retA = extractAtomGroup(retA, p_Mark);
    if (currentSize == retA.length) break;
  }
  let postExpCalc_Func = {
    default: (key, vals) => {
      if (vals !== undefined) {
        let skey = key.split("$");
        let newKey = skey.reduce((nk, ele, idx) => {
          if (idx == 0) return ele;
          let repK = ((idx - 1) >= vals.length) ? "$" : vals[idx - 1];
          return nk + "(" + repK + ")" + ele;
        }, "");
        key = newKey;
      }
      return key;
    }
  };
  retA = retA.map(exp => {
    exp = exp.replace(/\$/g, "#");
    let a_post_exp = [",", "\\-\\+", "%", "\\*\\/", "\\^"].reduce(
      (pexp, exp) => extractArith(pexp, exp, "$"), [exp]);
    let XXY = PostfixExpCalc(a_post_exp, postExpCalc_Func, false);
    return XXY[0].replace(/\#/g, "$");
  });
  retA = PostfixExpCalc(retA, postExpCalc_Func);
  retA = retA.flat();
  while (true) {
    let currentSize = retA.length;
    retA = extractAtomGroup(retA, p_Mark);
    if (currentSize == retA.length) break;
  }
  return retA;
}
const regex_multi_group = /^\$(,\$)*$/gm;
function PostfixExpCalc(postExp, funcSet, handleMultiGroup = true) {
  let valStack = [];
  postExp.forEach(exp => {
    let valPush;
    if (exp.includes !== undefined && exp.includes("$")) {
      let groupFlagCount = exp.split("$").length - 1;
      let tmpArr = valStack.splice(valStack.length - groupFlagCount, 9999999);
      if (funcSet[exp] !== undefined) valPush = funcSet[exp](tmpArr.flat());
      else if (handleMultiGroup && exp.match(regex_multi_group) != null) valPush = tmpArr.flat();
      else valPush = funcSet.default(exp, tmpArr.flat());
    } else {
      if (funcSet[exp] !== undefined) valPush = funcSet[exp];
      else if (funcSet.default !== undefined) valPush = funcSet.default(exp);
      else valPush = NaN;
    }
    valStack.push(valPush);
  });
  return valStack;
}

// numeric eval helper using the same funcSet shape the editor uses
function evalExpr(exprStr) {
  const post = Exp2PostfixExp(exprStr);
  const funcSet = {
    "$+$": v => v[0] + v[1],
    "$-$": v => v[0] - v[1],
    "$*$": v => v[0] * v[1],
    "$/$": v => v[0] / v[1],
    "$^$": v => Math.pow(v[0], v[1]),
    "min$": v => Math.min(...v),
    "max$": v => Math.max(...v),
    "Math.sin$": v => Math.sin(v[0]),
    "Math.cos$": v => Math.cos(v[0]),
    default: (key, vals) => {
      if (vals === undefined) {
        const pv = parseFloat(key);
        return pv; // NaN on unknown token — no throw
      }
      if (key.match(/^\$[,$]+$/gm) !== null) return vals;
      return NaN;
    }
  };
  return PostfixExpCalc(post, funcSet)[0];
}

// --- MISC_Util.js (verbatim) -----------------------------------------------
function round(num, round_nor = 1) {
  let tmp = (1 / round_nor);
  let round_nor_inv = Math.round(tmp);
  if (round_nor_inv == 0) round_nor_inv = tmp;
  return Math.round(num * round_nor_inv) / round_nor_inv;
}
function GetObjElement(rootObj, keyTrace, traceIdxTo = keyTrace.length - 1) {
  let obj = rootObj;
  let traceIdxTLen = traceIdxTo + 1;
  if (rootObj === undefined) return;
  for (let i = 0; i < traceIdxTLen; i++) {
    let key = keyTrace[i];
    obj = obj[key];
    if (obj === undefined) return;
  }
  return obj;
}
function dictLookUp(key, dict, theme) {
  return Array.isArray(key)
    ? GetObjElement(dict, key) || key[key.length - 1]
    : GetObjElement(dict, [theme || "_", key]) || key;
}

// --- structures.js (verbatim, CircularCounter) -----------------------------
class CircularCounter {
  constructor(total_size) { this._total_size = total_size; this.clear(); }
  size() { return this._size; }
  clear() { this._sidx = 0; this._eidx = 0; this._size = 0; }
  tsize() { return this._total_size; }
  f(idx = -1) {
    idx += 1;
    if (idx > this._size) return -1;
    idx += this._sidx - idx + this._total_size;
    return idx % this._total_size;
  }
  r(idx = 0) {
    if (idx >= this._size) return -1;
    idx = this._eidx + idx;
    return idx % this._total_size;
  }
  enQ(force = false) {
    if (this._size == this._total_size) {
      if (!force) return false;
      this.deQ();
    }
    this._sidx += 1;
    this._sidx %= this._total_size;
    this._size++;
    return true;
  }
  deQ() {
    if (this._size <= 0) return false;
    this._eidx += 1;
    this._eidx %= this._total_size;
    this._size--;
    return true;
  }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
function near(a, b, eps = 1e-9) { return Math.abs(a - b) < eps; }

function testExprBasic() {
  let ok = true, why = [];
  try {
    const v = evalExpr("1+2*3");
    if (!near(v, 7)) { ok = false; why.push(`1+2*3=${v} !=7`); }
  } catch (e) { ok = false; why.push('threw: ' + e); }
  report('expr.basic', ok, why.join('; '));
}
function testExprPrecedence() {
  let ok = true, why = [];
  try {
    const v = evalExpr("(1+2)*3");
    if (!near(v, 9)) { ok = false; why.push(`(1+2)*3=${v} !=9`); }
  } catch (e) { ok = false; why.push('threw: ' + e); }
  report('expr.precedence', ok, why.join('; '));
}
function testExprUnknownTokenNaN() {
  // unknown identifier should fall through default => parseFloat(NaN) — but
  // not throw. Final value may be NaN; we only assert "does not throw, returns
  // a value (NaN allowed)".
  let ok = true, why = [];
  try {
    const v = evalExpr("foo+1");
    if (v !== v && typeof v !== 'number') { ok = false; why.push('non-number result'); }
    // explicitly NaN is acceptable per default funcSet semantics
  } catch (e) { ok = false; why.push('threw: ' + e); }
  report('expr.unknownTokenNaN', ok, why.join('; '));
}
function testExprMalformedSafe() {
  // empty / weird inputs should not throw uncaught
  let ok = true, why = [];
  for (const s of ["", "(", "1+", "()"]) {
    try { evalExpr(s); }
    catch (e) { ok = false; why.push(`"${s}" threw: ${e}`); }
  }
  report('expr.malformedSafe', ok, why.join('; '));
}
function testExprMinMax() {
  let ok = true, why = [];
  try {
    const v = evalExpr("min(3,1,2)");
    if (!near(v, 1)) { ok = false; why.push(`min(3,1,2)=${v} !=1`); }
    const w = evalExpr("max(3,1,2)");
    if (!near(w, 3)) { ok = false; why.push(`max=${w} !=3`); }
  } catch (e) { ok = false; why.push('threw: ' + e); }
  report('expr.minMax', ok, why.join('; '));
}
function testExprMathSin() {
  let ok = true, why = [];
  try {
    const v = evalExpr("Math.sin(0)");
    if (!near(v, 0)) { ok = false; why.push(`Math.sin(0)=${v}`); }
  } catch (e) { ok = false; why.push('threw: ' + e); }
  report('expr.mathSin', ok, why.join('; '));
}

function testRoundBasic() {
  let ok = true, why = [];
  const v = round(1.2345, 0.001);
  // nearest-multiple of 0.001 — either 1.234 or 1.235 acceptable depending on float
  if (!(near(v, 1.234, 1e-9) || near(v, 1.235, 1e-9))) { ok = false; why.push(`round(1.2345,0.001)=${v}`); }
  const w = round(1.4, 0.5);
  if (!near(w, 1.5)) { ok = false; why.push(`round(1.4,0.5)=${w} !=1.5`); }
  report('round.basic', ok, why.join('; '));
}
function testRoundCoarse() {
  let ok = true, why = [];
  const v = round(12.34, 1);
  if (!near(v, 12)) { ok = false; why.push(`round(12.34,1)=${v}`); }
  report('round.coarse', ok, why.join('; '));
}
function testRoundNaNSafe() {
  let ok = true, why = [];
  try {
    const v = round(NaN, 0.001);
    if (v === v) { ok = false; why.push(`expected NaN got ${v}`); }
  } catch (e) { ok = false; why.push('threw: ' + e); }
  report('round.NaNSafe', ok, why.join('; '));
}

function testGetObjPresent() {
  const v = GetObjElement({ a: { b: { c: 7 } } }, ['a', 'b', 'c']);
  report('GetObjElement.present', v === 7, `got ${v}`);
}
function testGetObjMissing() {
  let ok = true, why = [];
  try {
    const v = GetObjElement({ a: {} }, ['a', 'b', 'c']);
    if (v !== undefined) { ok = false; why.push(`got ${v}`); }
  } catch (e) { ok = false; why.push('threw: ' + e); }
  report('GetObjElement.missing', ok, why.join('; '));
}
function testGetObjUndefRoot() {
  let ok = true, why = [];
  try {
    const v = GetObjElement(undefined, ['x']);
    if (v !== undefined) { ok = false; why.push(`got ${v}`); }
  } catch (e) { ok = false; why.push('threw: ' + e); }
  report('GetObjElement.undefRoot', ok, why.join('; '));
}

function testDictHit() {
  const v = dictLookUp('hello', { _: { hello: 'hi' } });
  report('dictLookUp.hit', v === 'hi', `got ${v}`);
}
function testDictMiss() {
  const v = dictLookUp('hello', { _: {} });
  report('dictLookUp.miss', v === 'hello', `got ${v} (expected fallback to key)`);
}
function testDictArrayKey() {
  // array-key path; missing -> last key as fallback
  const v = dictLookUp(['ns', 'missing'], { ns: {} });
  report('dictLookUp.arrayKey', v === 'missing', `got ${v}`);
  const w = dictLookUp(['ns', 'k'], { ns: { k: 'V' } });
  report('dictLookUp.arrayKey.hit', w === 'V', `got ${w}`);
}

function testCircularCounterFifoWrap() {
  let ok = true, why = [];
  const cc = new CircularCounter(3);
  if (cc.size() !== 0) { ok = false; why.push('init size'); }
  cc.enQ(); cc.enQ(); cc.enQ();
  if (cc.size() !== 3) { ok = false; why.push(`size after 3 enQ=${cc.size()}`); }
  // 4th enQ without force should fail
  if (cc.enQ() !== false) { ok = false; why.push('4th enQ should be false'); }
  cc.deQ();
  if (cc.size() !== 2) { ok = false; why.push(`size after deQ=${cc.size()}`); }
  // now enQ should succeed and wrap
  if (cc.enQ() !== true) { ok = false; why.push('enQ after deQ should be true'); }
  report('CircularCounter.fifoWrap', ok, why.join('; '));
}

// ---------------------------------------------------------------------------
// Phase A — reachability probe
// ---------------------------------------------------------------------------
async function probeReachability() {
  let probe;
  try {
    probe = await ev(`(function(){
      return {
        hasUtil: typeof window.__GP_UTIL__ === 'object' && window.__GP_UTIL__!==null,
        utilKeys: (typeof window.__GP_UTIL__==='object' && window.__GP_UTIL__) ? Object.keys(window.__GP_UTIL__) : [],
        loosePostfix: typeof window.PostfixExpCalc,
        looseRound: typeof window.round,
        looseGetObjElement: typeof window.GetObjElement,
        looseDictLookUp: typeof window.dictLookUp,
      };
    })()`);
  } catch (e) {
    console.log(`[reachability] probe failed: ${e}`);
    return { hasUtil: false };
  }
  console.log(`[reachability] __GP_UTIL__=${probe.hasUtil} keys=[${probe.utilKeys.join(',')}] ` +
    `window.PostfixExpCalc=${probe.loosePostfix} window.round=${probe.looseRound} ` +
    `GetObjElement=${probe.looseGetObjElement} dictLookUp=${probe.looseDictLookUp}`);
  return probe;
}

// In-browser arm: re-run the canonical assertions against the live bundle if
// __GP_UTIL__ exists. Each test is self-contained — skips if its util missing.
async function inBrowserExprBasic(util) {
  if (!util.PostfixExpCalc || !util.Exp2PostfixExp) {
    skip('browser.expr.basic', 'PostfixExpCalc/Exp2PostfixExp not on __GP_UTIL__');
    return;
  }
  const v = await ev(`(function(){
    var U = window.__GP_UTIL__;
    var post = U.Exp2PostfixExp("1+2*3");
    var fs = {
      "$+$":v=>v[0]+v[1],"$-$":v=>v[0]-v[1],"$*$":v=>v[0]*v[1],"$/$":v=>v[0]/v[1],
      default:(k,v)=>v===undefined?parseFloat(k):(/^\\$[,$]+$/.test(k)?v:NaN)
    };
    return U.PostfixExpCalc(post,fs)[0];
  })()`);
  report('browser.expr.basic', near(v, 7), `got ${v}`);
}
async function inBrowserRound(util) {
  if (!util.round) { skip('browser.round.basic', 'round not on __GP_UTIL__'); return; }
  const v = await ev(`window.__GP_UTIL__.round(12.34, 1)`);
  report('browser.round.basic', near(v, 12), `got ${v}`);
}
async function inBrowserGetObj(util) {
  if (!util.GetObjElement) { skip('browser.GetObjElement', 'not on __GP_UTIL__'); return; }
  const v = await ev(`window.__GP_UTIL__.GetObjElement({a:{b:7}},['a','b'])`);
  report('browser.GetObjElement', v === 7, `got ${v}`);
}
async function inBrowserDictLookUp(util) {
  if (!util.dictLookUp) { skip('browser.dictLookUp', 'not on __GP_UTIL__'); return; }
  const v = await ev(`window.__GP_UTIL__.dictLookUp("missing",{_:{}})`);
  report('browser.dictLookUp.miss', v === 'missing', `got ${v}`);
}

async function main() {
  // Phase A: probe live browser for reachable utils.
  let probe = { hasUtil: false };
  try {
    await waitReady();
    probe = await probeReachability();
  } catch (e) {
    console.log(`[reachability] daemon/app not available — skipping browser arm (${e.message})`);
  }

  // Phase A continued: in-browser tests only if __GP_UTIL__ exposed.
  if (probe.hasUtil) {
    const util = probe.utilKeys.reduce((o, k) => (o[k] = true, o), {});
    await inBrowserExprBasic(util);
    await inBrowserRound(util);
    await inBrowserGetObj(util);
    await inBrowserDictLookUp(util);
  } else {
    skip('browser.*', 'window.__GP_UTIL__ not present — see follow-up proposal in header');
  }

  // Phase B: reference-implementation tests (verbatim transcriptions; always run).
  console.log('--- reference-implementation arm (mirrors src/UTIL verbatim) ---');
  testExprBasic();
  testExprPrecedence();
  testExprUnknownTokenNaN();
  testExprMalformedSafe();
  testExprMinMax();
  testExprMathSin();
  testRoundBasic();
  testRoundCoarse();
  testRoundNaNSafe();
  testGetObjPresent();
  testGetObjMissing();
  testGetObjUndefRoot();
  testDictHit();
  testDictMiss();
  testDictArrayKey();
  testCircularCounterFifoWrap();

  console.log(`\nReachability: __GP_UTIL__ ${probe.hasUtil ? 'PRESENT' : 'MISSING'} (parent should add hook to unlock browser arm)`);
  console.log(failures ? `${failures} test(s) FAILED, ${skipped} skipped` : `ALL PASS (${skipped} skipped)`);
  process.exit(failures ? 1 : 0);
}

main().catch((e) => { console.error('FATAL', e); process.exit(1); });
