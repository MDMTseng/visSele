#!/usr/bin/env node
// QA suite runner: executes every qa/*.mjs test file serially (they share one
// browser via the webctld daemon, so they must NOT run in parallel) and reports
// a pass/fail summary. Each test file exits 0 = pass, non-zero = fail.
//
//   node tools/webctl/qa/run.mjs            run all qa/*.mjs (except this runner)
//   WEBCTL_MODEL=<path> node .../run.mjs    override the def model path
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const files = fs.readdirSync(__dirname)
  .filter((f) => f.endsWith('.mjs') && f !== 'run.mjs')
  .sort();

let pass = 0, fail = 0;
const failed = [];
for (const f of files) {
  process.stdout.write(`\n========== ${f} ==========\n`);
  const r = spawnSync('node', [path.join(__dirname, f)], { stdio: 'inherit', env: process.env });
  if (r.status === 0) pass++;
  else { fail++; failed.push(f); }
}
console.log(`\n=== QA SUITE: ${pass} passed, ${fail} failed${failed.length ? ' (' + failed.join(', ') + ')' : ''} ===`);
process.exit(fail ? 1 : 0);
