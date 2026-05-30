#!/usr/bin/env node
/*
 * QA META-RUNNER (round-10) — supersedes the previous simple runner.
 *
 * Discovers tools/webctl/qa/r*_*.mjs test files, runs them SERIALLY (the
 * webctld daemon owns ONE shared browser — parallel runs would collide on the
 * single window/store), captures stdout+stderr per suite, categorizes
 * PASS / SKIP / FAIL, mines well-known markers ("REAL PRODUCT BUG", "GAP:",
 * "NOTE:") from each suite's output, and prints a structured summary.
 *
 *   node tools/webctl/qa/run.mjs                      run every r*_*.mjs
 *   node tools/webctl/qa/run.mjs r3_diag r5_wslife    run a chosen subset
 *                                                     (basename w/ or w/o .mjs)
 *
 * Categorization:
 *   exit 0 + no "SKIP" marker in output         -> PASS
 *   exit 0 + output contains "SKIP" (line-anchored / "SKIP (core down)") -> SKIP
 *   exit != 0                                   -> FAIL
 *
 * Exits 0 iff every suite is PASS or SKIP, else 1.
 */
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawn } from 'node:child_process';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

const SUITE_RE = /^r\d+_[a-z0-9_]+\.mjs$/;

function discover() {
  return fs.readdirSync(__dirname)
    .filter((f) => SUITE_RE.test(f))
    .filter((f) => f !== 'run.mjs')
    .sort((a, b) => {
      const na = parseInt(a.match(/^r(\d+)/)[1], 10);
      const nb = parseInt(b.match(/^r(\d+)/)[1], 10);
      if (na !== nb) return na - nb;
      return a.localeCompare(b);
    });
}

function resolveRequested(args) {
  const all = discover();
  if (args.length === 0) return all;
  const matched = [];
  const missing = [];
  for (const a of args) {
    const base = a.endsWith('.mjs') ? a : a + '.mjs';
    if (all.includes(base)) matched.push(base);
    else missing.push(a);
  }
  if (missing.length) {
    console.error(`unknown suite(s): ${missing.join(', ')}`);
    console.error(`available: ${all.join(', ')}`);
    process.exit(2);
  }
  return matched;
}

function runOne(file) {
  return new Promise((resolve) => {
    const t0 = Date.now();
    const child = spawn('node', [path.join(__dirname, file)], {
      env: process.env,
      stdio: ['ignore', 'pipe', 'pipe'],
    });
    let out = '';
    let err = '';
    child.stdout.on('data', (b) => {
      const s = b.toString();
      out += s;
      process.stdout.write(s);
    });
    child.stderr.on('data', (b) => {
      const s = b.toString();
      err += s;
      process.stderr.write(s);
    });
    child.on('close', (code) => {
      resolve({ file, code: code ?? -1, ms: Date.now() - t0, out, err });
    });
    child.on('error', (e) => {
      resolve({ file, code: -1, ms: Date.now() - t0, out, err: err + '\n' + e.message });
    });
  });
}

function categorize(r) {
  const text = r.out + '\n' + r.err;
  if (r.code !== 0) return 'FAIL';
  // SKIP marker: standalone "SKIP" word at line-start, "SKIP:", or "SKIP (core down)".
  if (/(^|\n)\s*SKIP\b/.test(text) || /SKIP \(core down\)/.test(text)) return 'SKIP';
  return 'PASS';
}

function oneLineSummary(r) {
  // last non-empty stdout line, trimmed; cap length.
  const lines = r.out.split(/\r?\n/).map((l) => l.trim()).filter(Boolean);
  const last = lines[lines.length - 1] || '';
  return last.length > 140 ? last.slice(0, 137) + '...' : last;
}

const BUG_PATTERNS = [
  /REAL PRODUCT BUG[^\n]*/g,
  /\bGAP:[^\n]*/g,
  /\bNOTE:[^\n]*/g,
  /TypeError[^\n]*/g,
];

function extractBugs(r) {
  const text = r.out + '\n' + r.err;
  const hits = new Set();
  for (const re of BUG_PATTERNS) {
    const m = text.match(re);
    if (m) for (const s of m) hits.add(s.trim().slice(0, 200));
  }
  return [...hits];
}

function roundOf(file) {
  const m = file.match(/^r(\d+)_/);
  return m ? parseInt(m[1], 10) : 0;
}

async function main() {
  const args = process.argv.slice(2);
  const suites = resolveRequested(args);
  if (suites.length === 0) {
    console.log('no qa suites discovered');
    process.exit(0);
  }
  console.log(`META-RUNNER: ${suites.length} suite(s)`);
  for (const s of suites) console.log(`  - ${s}`);
  console.log('');

  const results = [];
  const t0 = Date.now();
  for (const file of suites) {
    process.stdout.write(`\n========== ${file} ==========\n`);
    const r = await runOne(file);
    r.status = categorize(r);
    r.summary = oneLineSummary(r);
    r.bugs = extractBugs(r);
    results.push(r);
  }
  const totalMs = Date.now() - t0;

  console.log('\n========== QA META-SUMMARY ==========');
  for (const r of results) {
    console.log(`[round ${roundOf(r.file)}] ${r.file}: ${r.status} (${r.ms}ms) - ${r.summary}`);
  }

  const pass = results.filter((r) => r.status === 'PASS').length;
  const skip = results.filter((r) => r.status === 'SKIP').length;
  const fail = results.filter((r) => r.status === 'FAIL');
  console.log('');
  console.log(`TOTALS: ${pass} PASS, ${skip} SKIP, ${fail.length} FAIL — total ${totalMs}ms`);
  if (fail.length) {
    console.log(`FAILED: ${fail.map((r) => r.file).join(', ')}`);
  }

  const allBugs = [];
  for (const r of results) {
    for (const b of r.bugs) allBugs.push(`${r.file}: ${b}`);
  }
  if (allBugs.length) {
    console.log('');
    console.log(`BUGS LOGGED (${allBugs.length}):`);
    for (const b of allBugs.slice(0, 40)) console.log(`  - ${b}`);
    if (allBugs.length > 40) console.log(`  ... (+${allBugs.length - 40} more)`);
  } else {
    console.log('BUGS LOGGED: (none observed)');
  }

  process.exit(fail.length ? 1 : 0);
}

main().catch((e) => {
  console.error('meta-runner crashed:', e);
  process.exit(2);
});
