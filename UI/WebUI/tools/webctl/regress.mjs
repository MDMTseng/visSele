#!/usr/bin/env node
// One-shot regression run: golden (serialized def) + flows (behavioral) against the
// running app. Wired to `npm test`. Requires the webctl daemon up (node webctl.mjs start)
// with the dev app loaded (Vite :8081) and the core on :4090.
//
// Override the def/image fixture with WEBCTL_MODEL (path without extension).
import { spawnSync } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const dir = path.dirname(fileURLToPath(import.meta.url));
const PORT = Number(process.env.WEBCTL_PORT || 8765);
// Same fixture flows.mjs defaults to -- the def this pointed at lives only on
// one developer machine, so the baselines were unreproducible anywhere else.
const MODEL = process.env.WEBCTL_MODEL ||
  path.join(dir, 'fixtures', 'caliper_verify_tagged');

const health = await fetch(`http://127.0.0.1:${PORT}/health`).then((r) => r.json()).catch(() => null);
if (!health) {
  console.error(
    'webctl daemon not reachable on :' + PORT + '.\n' +
    'Start it first:  cd tools/webctl && node webctl.mjs start\n' +
    '(dev app must be running: npm run dev  → http://localhost:8081, core on :4090)'
  );
  process.exit(2);
}

const run = (args, label) => {
  console.log(`\n=== ${label} ===`);
  return spawnSync('node', [path.join(dir, args[0]), ...args.slice(1)], { stdio: 'inherit', cwd: dir }).status || 0;
};

let fail = 0;
fail += run(['golden.mjs', 'verify', 'caliper_verify', MODEL + '.hydef'], 'golden (serialized def)') ? 1 : 0;
fail += run(['flows.mjs', 'verify'], 'flows (behavioral)') ? 1 : 0;

console.log('\n' + (fail ? 'REGRESSION FAILURES ✗' : 'all regression checks passed ✓'));
process.exit(fail ? 1 : 0);
