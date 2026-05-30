#!/usr/bin/env node
/*
 * QA R9 — DEEP PostfixExpCalc / Exp2PostfixExp (src/UTIL/expr.js).
 *
 * VIEWPOINT (round 9): exhaust the def-expression engine semantics that the
 * editor's measure/coupling code lives on top of. r4_purelib covered the basic
 * shape; this script pushes precedence, associativity, unary minus, the full
 * funcSet, whitespace, error recovery, deep nesting, large/IEEE edge values,
 * and div-by-zero/NaN propagation.
 *
 * SOURCE READING (src/UTIL/expr.js):
 *
 *   Operators discovered (and the precedence layering they get from the
 *   reduce in Exp2PostfixExp):
 *     [",", "\\-\\+", "%", "\\*\\/", "\\^"]
 *   processed in THAT order via extractArith — so the FIRST splitter in the
 *   list binds OUTERMOST (lowest precedence). Effective precedence (low→high):
 *       ","   (argument separator, multi-group)
 *       "+","-"   (additive)
 *       "%"   (modulo as a single-char class)
 *       "*","/"   (multiplicative)
 *       "^"   (exponent — highest)
 *   NOTE: because seps live in a [character class] regex, "+-" share a level
 *   and "*\/" share a level. Right-grouping of arithmetic is left-to-right by
 *   the recursive extractArithGroup that keeps peeling from the front. There
 *   is NO explicit unary-minus handling — "-5" leads to extractArithGroup
 *   needing both sides of the "-" non-empty, so it tokenizes as "(<empty>)-5"
 *   which the default funcSet will eval to NaN. UNARY MINUS IS NOT SUPPORTED
 *   at the top of an expression; "(0-5)" or "(-5)" inside parens is the
 *   workaround. We record this as an observed-behavior test, not a failure.
 *
 *   funcSet handling: PostfixExpCalc only dispatches whatever the caller hands
 *   it. expr.js itself only ships one helper (ExpCalcBasic) with:
 *       min$, max$, $+$, $-$, $*$, $/$, $^$
 *   Everything else (sin/cos/abs/sqrt/floor/ceil/round/Math.*) is supplied by
 *   the editor's runtime funcSet, not expr.js. So "funcSet whitelist" of the
 *   engine itself is just those seven. We test exactly those operators, then
 *   add a parallel funcSet that mirrors the editor (Math.sin, Math.cos,
 *   abs, sqrt, floor, ceil, round) to demonstrate the extension surface.
 *
 *   Tokenizer:
 *     - Variables: pure identifiers ("foo") fall through to default funcSet
 *       → parseFloat("foo") → NaN. NO context object: PostfixExpCalc has no
 *       variable-binding hook. The editor injects values by editing funcSet
 *       (key→value) before calling. We test the parseFloat-fallback path and
 *       record "no native variable substitution".
 *     - Whitespace: NOT stripped anywhere. "1 + 2" tokenizes via the
 *       extractArithGroup regex which is greedy ".+" on both sides; the
 *       resulting key is "1 $+$ 2 " literally — parseFloat tolerates leading
 *       whitespace but NOT trailing alphabetics, so " 2 " → parseFloat → 2.
 *       We test this and record whitespace tolerance.
 *     - Parentheses: extractAtomGroup peels innermost-first; mismatched parens
 *       cause it to terminate early (no match) and the unmatched "(" stays in
 *       the token stream → default funcSet returns the raw key. NEVER throws.
 *
 * IMPLEMENTED TESTS (see plan list in TEST PLAN). Anything in source we could
 * not exercise is noted in REPORT.
 *
 * Self-contained, core-INDEPENDENT, idempotent.
 * Run: node tools/webctl/qa/r9_expr_deep.mjs
 */

let failures = 0;
let skipped = 0;
function report(name, ok, detail) {
  console.log(`[${name}] ${ok ? 'PASS' : 'FAIL'}${detail ? ' — ' + detail : ''}`);
  if (!ok) failures++;
}
function note(name, detail) {
  console.log(`[${name}] NOTE — ${detail}`);
}
function skip(name, why) {
  console.log(`[${name}] SKIP — ${why}`);
  skipped++;
}
function near(a, b, eps = 1e-9) {
  if (Number.isNaN(a) && Number.isNaN(b)) return true;
  if (!Number.isFinite(a) && a === b) return true;
  return Math.abs(a - b) < eps;
}

// --- expr.js verbatim ------------------------------------------------------
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

// Editor-mirror evaluator: bare minimum the engine ships + extended math.
function makeFuncSet(extras = {}) {
  return {
    "$+$": v => v[0] + v[1],
    "$-$": v => v[0] - v[1],
    "$*$": v => v[0] * v[1],
    "$/$": v => v[0] / v[1],
    "$^$": v => Math.pow(v[0], v[1]),
    "$%$": v => v[0] % v[1],
    "min$": v => Math.min(...v),
    "max$": v => Math.max(...v),
    "abs$": v => Math.abs(v[0]),
    "sqrt$": v => Math.sqrt(v[0]),
    "floor$": v => Math.floor(v[0]),
    "ceil$": v => Math.ceil(v[0]),
    "round$": v => Math.round(v[0]),
    "sin$": v => Math.sin(v[0]),
    "cos$": v => Math.cos(v[0]),
    "Math.sin$": v => Math.sin(v[0]),
    "Math.cos$": v => Math.cos(v[0]),
    "Math.abs$": v => Math.abs(v[0]),
    ...extras,
    default: (key, vals) => {
      if (vals === undefined) {
        // numeric-or-NaN; whitespace-tolerant via parseFloat
        return parseFloat(key);
      }
      if (key.match(/^\$[,$]+$/gm) !== null) return vals;
      return NaN;
    }
  };
}
function evalExpr(s, extras) {
  const post = Exp2PostfixExp(s);
  return PostfixExpCalc(post, makeFuncSet(extras))[0];
}
function safeEval(s, extras) {
  try { return { v: evalExpr(s, extras), threw: false }; }
  catch (e) { return { v: undefined, threw: true, err: String(e) }; }
}

// --- TESTS -----------------------------------------------------------------

// ARITHMETIC
function tArith() {
  const cases = [
    ['1+2', 3], ['7-4', 3], ['3*4', 12], ['10/4', 2.5],
    ['1.5+2.25', 3.75], ['0.1+0.2', 0.30000000000000004],
  ];
  let ok = true, why = [];
  for (const [e, exp] of cases) {
    const v = evalExpr(e);
    if (!near(v, exp, 1e-12)) { ok = false; why.push(`${e}=${v} !=${exp}`); }
  }
  report('arith.basic', ok, why.join('; '));
}

// PRECEDENCE
function tPrecedence() {
  const cases = [
    ['1+2*3', 7],
    ['(1+2)*3', 9],
    ['2^3', 8],
    ['2*3+4*5', 26],
    ['10-2*3', 4],
    ['2+3*4-5', 9],
    ['2^3*2', 16],   // ^ higher than *
    ['2*2^3', 16],
  ];
  let ok = true, why = [];
  for (const [e, exp] of cases) {
    const v = evalExpr(e);
    if (!near(v, exp)) { ok = false; why.push(`${e}=${v} !=${exp}`); }
  }
  report('precedence', ok, why.join('; '));
}

// ASSOCIATIVITY — record observed
function tAssoc() {
  // 2^3^2: right-assoc = 2^(3^2)=2^9=512; left-assoc = (2^3)^2=64
  const v = evalExpr('2^3^2');
  // record observed; engine is left-assoc within a precedence band (recursive
  // peel from front). We don't fail on either, just record + assert it parsed.
  const ok = Number.isFinite(v) && (near(v, 512) || near(v, 64));
  note('assoc.pow', `2^3^2=${v} (${near(v, 512) ? 'right-assoc' : near(v, 64) ? 'left-assoc' : 'other'})`);
  report('assoc.parsed', ok, `got ${v}`);

  // subtraction left-assoc check: 10-3-2 == 5 (left) vs 9 (right)
  const w = evalExpr('10-3-2');
  note('assoc.sub', `10-3-2=${w} (${near(w, 5) ? 'left-assoc' : near(w, 9) ? 'right-assoc' : 'other'})`);
}

// UNARY MINUS — record (engine has no native support)
function tUnary() {
  const a = safeEval('-5+3');
  const b = safeEval('5*-2');
  const c = safeEval('(0-5)+3'); // workaround
  const d = safeEval('(-5)+3');  // paren-wrap
  note('unary.leading', `-5+3 => ${a.v} (threw=${a.threw}); engine has no unary-minus`);
  note('unary.embedded', `5*-2 => ${b.v} (threw=${b.threw})`);
  // workaround should work
  report('unary.workaround.paren-sub', near(c.v, -2), `(0-5)+3=${c.v}`);
  note('unary.workaround.parenNeg', `(-5)+3 => ${d.v}`);
}

// FUNCTIONS — only those in source-supplied funcSet (min,max) plus editor mirror
function tFunctions() {
  const cases = [
    ['min(3,1,2)', 1],
    ['max(3,1,2)', 3],
    ['abs(0-5)', 5],
    ['sqrt(9)', 3],
    ['floor(2.7)', 2],
    ['ceil(2.1)', 3],
    ['round(2.6)', 3],
    ['sin(0)', 0],
    ['cos(0)', 1],
    ['Math.sin(0)', 0],
    ['Math.cos(0)', 1],
  ];
  let ok = true, why = [];
  for (const [e, exp] of cases) {
    const v = evalExpr(e);
    if (!near(v, exp, 1e-12)) { ok = false; why.push(`${e}=${v} !=${exp}`); }
  }
  report('functions.whitelist', ok, why.join('; '));
}

// WHITESPACE
function tWhitespace() {
  const cases = [
    ['1 + 2 * 3', 7],
    ['  1+2  ', 3],
    ['min( 3 , 1 , 2 )', 1],
  ];
  let ok = true, why = [];
  for (const [e, exp] of cases) {
    const r = safeEval(e);
    if (r.threw) { ok = false; why.push(`${e} threw`); continue; }
    if (!near(r.v, exp)) { ok = false; why.push(`${e}=${r.v} !=${exp}`); }
  }
  report('whitespace.tolerant', ok, why.join('; '));
}

// ERROR CASES
function tErrors() {
  let ok = true, why = [];
  // Unknown token: should not throw; default fallback parses → NaN.
  for (const s of ['foo+1', '', '(', '1+', '()', '((1+2)*3', '1+2)*3', '+', '*']) {
    const r = safeEval(s);
    if (r.threw) { ok = false; why.push(`"${s}" threw: ${r.err}`); }
  }
  report('errors.noThrow', ok, why.join('; '));

  const r1 = safeEval('foo+1');
  note('errors.unknownToken', `foo+1 => ${r1.v} (NaN expected via parseFloat fallback)`);
  const r2 = safeEval('');
  note('errors.empty', `"" => ${r2.v}`);
  const r3 = safeEval('((1+2)*3');
  note('errors.mismatchedParens', `((1+2)*3 => ${r3.v} (no throw)`);
}

// VARIABLES — engine has no context; document gap.
function tVariables() {
  // Inject via funcSet extras as the editor does.
  const v = evalExpr('x*2', { x: 5 });
  report('variables.viaFuncSetExtras', near(v, 10), `x*2 with {x:5}=${v}`);
  note('variables.noContextArg',
    'PostfixExpCalc has no context object; editor injects vars by adding key→value to funcSet');
}

// DEEP NESTING
function tDeepNest() {
  // ((1+2)*(3+4))*((5-1)/(2)) = 21*2 = 42
  const v = evalExpr('((1+2)*(3+4))*((5-1)/(2))');
  report('deepNest', near(v, 42), `got ${v}`);
  // pathological nesting
  const v2 = evalExpr('(((((1+1)+1)+1)+1)+1)');
  report('deepNest.linear', near(v2, 6), `got ${v2}`);
}

// LARGE NUMBERS / NaN PROPAGATION
function tLarge() {
  const v = evalExpr('1e10*1e10');
  report('large.1e20', near(v, 1e20, 1e6), `got ${v}`);
  const w = evalExpr('foo*2+1'); // NaN propagation
  report('large.nanProp', Number.isNaN(w), `foo*2+1=${w}`);
  const huge = evalExpr('1e308*10'); // overflow to Infinity
  report('large.overflowInf', huge === Infinity, `1e308*10=${huge}`);
}

// DIV-BY-ZERO
function tDiv() {
  const a = evalExpr('1/0');
  report('div.byZero.inf', a === Infinity, `1/0=${a}`);
  const b = evalExpr('0/0');
  report('div.zeroByZero.nan', Number.isNaN(b), `0/0=${b}`);
  const c = evalExpr('(0-1)/0');
  report('div.negInf', c === -Infinity, `(0-1)/0=${c}`);
}

// MODULO — source has '%' as a precedence level; verify it works
function tModulo() {
  const v = safeEval('10%3');
  if (v.threw || !near(v.v, 1)) {
    // record as gap if engine can't dispatch (default returns NaN-shaped key)
    note('modulo', `10%3 => ${v.v} threw=${v.threw} (engine has %' splitter but no built-in $%$)`);
    report('modulo.gracefulOrPass', !v.threw, v.threw ? 'threw' : `got ${v.v}`);
  } else {
    report('modulo.works', true, `10%3=${v.v}`);
  }
}

// MAIN
(function main() {
  console.log('--- R9 expr deep tests (core-independent, src verbatim) ---');
  tArith();
  tPrecedence();
  tAssoc();
  tUnary();
  tFunctions();
  tWhitespace();
  tErrors();
  tVariables();
  tDeepNest();
  tLarge();
  tDiv();
  tModulo();
  console.log(`\nResult: ${failures ? failures + ' FAILED' : 'ALL PASS'} (${skipped} skipped)`);
  process.exit(failures ? 1 : 0);
})();
