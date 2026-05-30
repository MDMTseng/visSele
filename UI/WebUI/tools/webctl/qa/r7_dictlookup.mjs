#!/usr/bin/env node
/*
 * QA R7 — i18n / dictLookUp robustness (core-INDEPENDENT).
 *
 * VIEWPOINT (round 7):
 *   The editor's i18n surface is the `dictLookUp(key, dict, theme)` helper in
 *   src/UTIL/MISC_Util.js plus a sprinkling of direct `DICT._.x` accesses.
 *   The helper falls back to the key (string-key path) or to the last segment
 *   (array-key path) when a translation is missing — direct property access
 *   does NOT. Round-8 backlog flagged hardcoded CJK strings in JSX as a smell.
 *
 * DEV HOOKS USED (already wired in src/script.jsx ~line 103):
 *   window.__GP_UTIL__.dictLookUp   - the live helper (post-bundle)
 *   window.__GP_STORE__             - Redux store; state.UIData.DICT is the
 *                                     loaded zh_TW dictionary
 *
 * TEST PLAN:
 *   Phase A — REACHABILITY: probe __GP_UTIL__.dictLookUp + store DICT.
 *   Phase B — LIVE-DICT assertions (browser eval against real loaded DICT):
 *     dict.loaded            DICT has top-level "_" section with content
 *     dict.count             DICT._ has > 50 keys (smoke check)
 *     live.hit.string        dictLookUp("type", DICT) === "類型" (known key)
 *     live.hit.line          dictLookUp("line", DICT) returns translation
 *     live.miss.string       dictLookUp("__definitely_missing__", DICT)
 *                            returns "__definitely_missing__" (NOT undefined)
 *     live.miss.notThrow     missing-key path does not throw
 *     direct.hit             DICT._.type === "類型"
 *     direct.miss.undefined  DICT._.__missing__ === undefined  (OPENQUESTION:
 *                            direct access has NO fallback — documented gap)
 *     arrayKey.hit           dictLookUp(["_","type"], DICT) === "類型"
 *     arrayKey.miss          dictLookUp(["_","__missing__"], DICT) ===
 *                            "__missing__" (last segment fallback)
 *     theme.fallback         dictLookUp("type", DICT, "nonexistent_theme")
 *                            === "type"  (no such theme -> key fallback)
 *     theme.alt              dictLookUp("calc_add_measure", DICT, "defConf")
 *                            returns "增加量測變數" (defConf theme exists)
 *   Phase C — SMELL DOCUMENTATION (always passes, prints metrics):
 *     smell.hardcodedCJK     count of unique CJK strings outside DICT in
 *                            src/InspectionUI.js (round-8 backlog).
 *                            Sourced via repo grep (this script runs from
 *                            tools/webctl/qa so it pulls the file directly).
 *
 * NO core needed. Daemon at http://127.0.0.1:8765 (WEBCTL_PORT override).
 * Idempotent. Exit non-zero on real failure.
 * Run: node tools/webctl/qa/r7_dictlookup.mjs
 */

import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';

const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;
const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const REPO_SRC = resolve(__dirname, '../../../src');

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
      "typeof window.__GP_STORE__==='object' && typeof window.__GP_UTIL__==='object'"
    );
    if (ok === true) return true;
  }
  throw new Error('dev hooks never appeared — is app running with WEBCTL daemon?');
}

// ---------------------------------------------------------------------------
// Phase A — reachability
// ---------------------------------------------------------------------------
async function probe() {
  return ev(`(function(){
    var U = window.__GP_UTIL__;
    var S = window.__GP_STORE__;
    var st = S && S.getState && S.getState();
    var D = st && st.UIData && st.UIData.DICT;
    return {
      hasUtil: !!U,
      hasDictLookUp: !!(U && typeof U.dictLookUp === 'function'),
      hasDict: !!D,
      hasUnderscore: !!(D && D._),
      underscoreKeyCount: (D && D._) ? Object.keys(D._).length : 0,
      themeKeys: D ? Object.keys(D) : [],
    };
  })()`);
}

// ---------------------------------------------------------------------------
// Phase B — live-DICT assertions (in-browser)
// ---------------------------------------------------------------------------
async function liveTests(p) {
  if (!p.hasDictLookUp) { skip('live.*', '__GP_UTIL__.dictLookUp missing'); return; }
  if (!p.hasUnderscore) { skip('live.*', 'DICT._ missing'); return; }

  report('dict.loaded', p.hasUnderscore, `themes=[${p.themeKeys.join(',')}]`);
  report('dict.count', p.underscoreKeyCount > 50,
    `DICT._ has ${p.underscoreKeyCount} keys`);

  // live.hit.string  — "type" -> "類型" per zh_TW.js
  const hitType = await ev(`(function(){
    var U=window.__GP_UTIL__, D=window.__GP_STORE__.getState().UIData.DICT;
    return U.dictLookUp("type", D);
  })()`);
  report('live.hit.string', hitType === '類型', `got ${JSON.stringify(hitType)}`);

  // live.hit.line
  const hitLine = await ev(`(function(){
    var U=window.__GP_UTIL__, D=window.__GP_STORE__.getState().UIData.DICT;
    return U.dictLookUp("line", D);
  })()`);
  report('live.hit.line', typeof hitLine === 'string' && hitLine !== 'line' && hitLine.length > 0,
    `got ${JSON.stringify(hitLine)}`);

  // live.miss.string — fallback to key
  const miss = await ev(`(function(){
    var U=window.__GP_UTIL__, D=window.__GP_STORE__.getState().UIData.DICT;
    return U.dictLookUp("__definitely_missing_xyz__", D);
  })()`);
  report('live.miss.string', miss === '__definitely_missing_xyz__',
    `expected key fallback, got ${JSON.stringify(miss)}`);

  // live.miss.notThrow — sweeping a few oddball keys
  const sweep = await ev(`(function(){
    var U=window.__GP_UTIL__, D=window.__GP_STORE__.getState().UIData.DICT;
    var ks=["","   ","\\n","__none__","NULL","undefined"];
    var caught=null, vals=[];
    try { ks.forEach(function(k){ vals.push(U.dictLookUp(k,D)); }); }
    catch(e){ caught=String(e); }
    return {caught:caught, vals:vals};
  })()`);
  report('live.miss.notThrow', sweep.caught === null,
    sweep.caught ? `threw: ${sweep.caught}` : `vals=${JSON.stringify(sweep.vals)}`);

  // direct.hit
  const directHit = await ev(`window.__GP_STORE__.getState().UIData.DICT._.type`);
  report('direct.hit', directHit === '類型', `got ${JSON.stringify(directHit)}`);

  // direct.miss.undefined — documented gap
  const directMiss = await ev(`window.__GP_STORE__.getState().UIData.DICT._.__definitely_missing_xyz__`);
  report('direct.miss.undefined', directMiss === undefined,
    `OPENQUESTION: direct DICT._.x has no fallback (got ${JSON.stringify(directMiss)})`);

  // arrayKey.hit
  const arrHit = await ev(`(function(){
    var U=window.__GP_UTIL__, D=window.__GP_STORE__.getState().UIData.DICT;
    return U.dictLookUp(["_","type"], D);
  })()`);
  report('arrayKey.hit', arrHit === '類型', `got ${JSON.stringify(arrHit)}`);

  // arrayKey.miss — last segment fallback
  const arrMiss = await ev(`(function(){
    var U=window.__GP_UTIL__, D=window.__GP_STORE__.getState().UIData.DICT;
    return U.dictLookUp(["_","__no_such_key__"], D);
  })()`);
  report('arrayKey.miss', arrMiss === '__no_such_key__',
    `expected last-segment fallback, got ${JSON.stringify(arrMiss)}`);

  // theme.fallback — nonexistent theme should fall through to key
  const themeMiss = await ev(`(function(){
    var U=window.__GP_UTIL__, D=window.__GP_STORE__.getState().UIData.DICT;
    return U.dictLookUp("type", D, "nonexistent_theme_xyz");
  })()`);
  report('theme.fallback', themeMiss === 'type',
    `expected key fallback, got ${JSON.stringify(themeMiss)}`);

  // theme.alt — defConf theme exists in zh_TW.js
  const themeAlt = await ev(`(function(){
    var U=window.__GP_UTIL__, D=window.__GP_STORE__.getState().UIData.DICT;
    return U.dictLookUp("calc_add_measure", D, "defConf");
  })()`);
  report('theme.alt', themeAlt === '增加量測變數',
    `got ${JSON.stringify(themeAlt)}`);
}

// ---------------------------------------------------------------------------
// Phase C — smell: hardcoded CJK strings in JSX (round-8 backlog)
// ---------------------------------------------------------------------------
function smellHardcodedCJK() {
  const files = [
    'InspectionUI.js',
    'InstInspUI.js',
    'DefConfUI.js',
    'MAINUI.js',
    'RepDisplayUI.js',
    'EverCheckCanvasComponent.js',
  ];
  const cjkRe = /[一-鿿]+/g;
  const perFile = {};
  let totalUnique = 0;
  const allUnique = new Set();
  for (const f of files) {
    try {
      const src = readFileSync(resolve(REPO_SRC, f), 'utf8');
      const matches = src.match(cjkRe) || [];
      const uniq = new Set(matches);
      perFile[f] = { occurrences: matches.length, unique: uniq.size };
      for (const s of uniq) allUnique.add(s);
    } catch (e) {
      perFile[f] = { error: e.message };
    }
  }
  totalUnique = allUnique.size;
  console.log(`[smell.hardcodedCJK] DOC — ${totalUnique} unique CJK strings outside DICT across ${files.length} JSX files`);
  for (const [f, m] of Object.entries(perFile)) {
    if (m.error) console.log(`  ${f}: ERROR ${m.error}`);
    else console.log(`  ${f}: ${m.occurrences} occurrences, ${m.unique} unique`);
  }
  // sample first 8
  const sample = [...allUnique].slice(0, 8);
  console.log(`  sample: ${JSON.stringify(sample)}`);
  return totalUnique;
}

// ---------------------------------------------------------------------------
async function main() {
  let p = { hasUtil: false };
  try {
    await waitReady();
    p = await probe();
    console.log(`[reachability] __GP_UTIL__=${p.hasUtil} dictLookUp=${p.hasDictLookUp} ` +
      `DICT=${p.hasDict} DICT._=${p.hasUnderscore} keyCount=${p.underscoreKeyCount} ` +
      `themes=[${p.themeKeys.join(',')}]`);
  } catch (e) {
    console.log(`[reachability] daemon/app unavailable — skipping browser arm (${e.message})`);
  }

  if (p.hasUtil) {
    await liveTests(p);
  } else {
    skip('live.*', 'browser hooks not reachable');
  }

  console.log('--- smell documentation ---');
  const cjkCount = smellHardcodedCJK();

  console.log(`\nSummary: ${failures} FAILED, ${skipped} skipped, hardcoded-CJK-unique=${cjkCount}`);
  process.exit(failures ? 1 : 0);
}

main().catch((e) => { console.error('FATAL', e); process.exit(1); });
