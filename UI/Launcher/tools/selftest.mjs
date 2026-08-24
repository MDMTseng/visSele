// End-to-end exercise of everything in src/ that does not need a window.
//
//   node tools/selftest.mjs <update.zip> <workingDir>
//
// <workingDir> is the folder the application RUNS IN -- the parent of data/,
// not data/ itself. Nothing in this test writes to it.
//
// Runs against a REAL package and a REAL application, in a throwaway app root,
// so it covers the parts that only break in contact with the filesystem and the
// operating system: extraction, hashing, the rename, the pointer write, reading
// the version's own boot.js, pipe draining, the control channel, and the
// graceful-stop path.
//
// Deliberately not a mock. Every failure this design exists to prevent -- an
// undrained pipe, a hard kill of a soft shutdown, a half-installed version --
// only shows up against the real thing.
import { createRequire } from 'node:module';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const require = createRequire(import.meta.url);
const { Config } = require('../src/config');
const { AppStore } = require('../src/apps');
const { Updater } = require('../src/updater');
const { Supervisor } = require('../src/supervisor');
const boot = require('../src/boot');

const ZIP = process.argv[2];
const WORKING_DIR = process.argv[3];
if (!ZIP || !WORKING_DIR) {
  console.error('usage: node tools/selftest.mjs <update.zip> <workingDir>   (the PARENT of data/)');
  process.exit(2);
}

let failures = 0;
const check = (name, cond, detail) => {
  console.log(`${cond ? 'PASS' : 'FAIL'}  ${name}${detail ? '  -- ' + detail : ''}`);
  if (!cond) failures++;
};

// A core already running on this machine holds the control port, and the core
// binds it exclusively -- so the child this test starts would come up with no
// control channel at all, and every health, readiness and shutdown check below
// would fail for a reason that has nothing to do with what they test.
//
// Found this the hard way: with a stray core up, the suite reported four
// failures and none of them named the actual cause. Say it here, once, before
// anything runs.
{
  const control = require('../src/control');
  const r = await control.ping(4098, 800);
  if (r.ok) {
    console.error('A core is ALREADY RUNNING and holding the control port '
      + `(pid ${r.reply && r.reply.pid}, up ${r.reply && r.reply.uptime_s}s).`);
    console.error('Stop it first -- this test starts its own core and the two cannot share the port.');
    process.exit(2);
  }
}

const root = fs.mkdtempSync(path.join(os.tmpdir(), 'launcher-selftest-'));
console.log('app root:', root);
console.log('working dir:', path.resolve(WORKING_DIR), '\n');

const cfg = new Config(root);
cfg.values.workingDir = path.resolve(WORKING_DIR);
const apps = new AppStore(cfg);
const updater = new Updater(cfg, apps);

// A snapshot of the working directory, so the promise that nothing touches it
// is checked rather than asserted. This is the constraint the whole layout
// exists to honour: the machine's calibration and recipe are never copied,
// moved, seeded or written by the launcher.
const snapshotWorkingDir = () => {
  const out = [];
  const walk = (d, rel = '') => {
    for (const e of fs.readdirSync(d, { withFileTypes: true })) {
      const r = rel ? rel + '/' + e.name : e.name;
      if (e.isDirectory()) walk(path.join(d, e.name), r);
      else { const st = fs.statSync(path.join(d, e.name)); out.push(`${r}:${st.size}:${st.mtimeMs}`); }
    }
  };
  try { walk(path.resolve(WORKING_DIR)); } catch { /* reported by the check */ }
  return out.sort().join('\n');
};
// Snapshotted around the LAUNCHER's operations only. The application itself
// writes in there constantly -- that is what the directory is for -- so
// comparing across a run would prove nothing. What must be true is that
// installing, selecting and pruning do not touch it.
let before = snapshotWorkingDir();

// --- 1. install ---------------------------------------------------------------
console.log('--- install');
const installed = await updater.install(ZIP, (m) => console.log('   ', m));
check('installs and reports a version', typeof installed.version === 'string', installed.version);
check('nothing is current yet', apps.currentVersion() === null,
      'an install must not silently switch the running version');
check('version validates', apps.validate(installed.version).ok);
check('staging is gone', !fs.existsSync(path.join(apps.dir, '.staging')));

// --- 2. tamper detection ------------------------------------------------------
console.log('\n--- tamper / corruption');
{
  const bad = path.join(root, 'corrupt.zip');
  const buf = fs.readFileSync(ZIP);
  buf[Math.floor(buf.length * 0.6)] ^= 0xff;
  fs.writeFileSync(bad, buf);
  let err = null;
  try { await updater.install(bad, () => {}); } catch (e) { err = e.message; }
  check('a corrupted package is refused', err !== null, err || 'IT WAS ACCEPTED');
  check('staging cleaned after a rejection', !fs.existsSync(path.join(apps.dir, '.staging')));
}

// --- 3. select ----------------------------------------------------------------
console.log('\n--- select');
apps.setCurrent(installed.version);
check('current.json names the version', apps.currentVersion() === installed.version);
const resolved = apps.resolve();
check('resolve finds the version', !!resolved, resolved && resolved.dir);

{
  fs.writeFileSync(apps.currentFile, JSON.stringify({ version: 'not-installed' }));
  const r = apps.resolve();
  check('a dangling pointer falls back to a valid version',
        r && r.version === installed.version && r.fellBackFrom === 'not-installed');
  apps.setCurrent(installed.version);
}

// --- 4. the version's own boot description -------------------------------------
console.log('\n--- boot.js');
let plan = null;
try {
  plan = boot.load(resolved.dir, cfg.workingDir, (m) => console.log('   ', m));
} catch (e) {
  check('boot.js loads', false, e.message);
}
if (plan) {
  check('boot.js loads', true, plan.name || '(unnamed)');
  check('declares at least one process', plan.processes.length > 0,
        plan.processes.map((p) => p.id).join(', '));
  check('exactly one process is primary', plan.processes.filter((p) => p.primary).length === 1);
  check('the primary executable exists', fs.existsSync(plan.primary.exe), plan.primary.exe);
  check('the primary declares a control channel', !!plan.primary.control,
        plan.primary.control ? `${plan.primary.control.host}:${plan.primary.control.port}`
                             : 'none -- every stop would be a force kill');
  check('the plan points its cwd at the working directory',
        plan.primary.args.some((a) => a.includes(cfg.workingDir))
        || plan.primary.cwd === cfg.workingDir,
        `cwd=${plan.primary.cwd} args=${plan.primary.args.join(' ')}`);

  const services = boot.makeServices(plan, () => {}, () => []);
  const unmet = await boot.callHook(plan, 'checkRequirements', services,
                                    boot.defaults.checkRequirements, () => {});
  check('the version\'s stated requirements are met', unmet.length === 0,
        unmet.map((u) => u.path).join(', ') || 'all present');

  // An unknown key must be refused, not ignored. This is what makes a newer
  // package fail loudly on an older launcher instead of half-applying.
  let rejected = null;
  try {
    const tmpApp = fs.mkdtempSync(path.join(os.tmpdir(), 'bootcheck-'));
    fs.mkdirSync(path.join(tmpApp, 'scripts'), { recursive: true });
    fs.writeFileSync(path.join(tmpApp, 'scripts', 'boot.js'),
      'module.exports={apiVersion:1,describe:()=>({core:{exe:"x"},somethingNew:1})};');
    boot.load(tmpApp, cfg.workingDir, () => {});
  } catch (e) { rejected = e.message; }
  check('an unknown plan key is refused, not ignored',
        rejected !== null && /unknown key/.test(rejected), rejected || 'IT WAS IGNORED');
}

// --- 4b. the working directory is untouched by install/select/boot ------------
console.log('\n--- the machine\'s working directory (after install, select, boot.js)');
check('install, select and reading boot.js changed NOTHING in it',
      before === snapshotWorkingDir(),
      before === snapshotWorkingDir() ? 'byte-for-byte identical'
                                      : 'IT CHANGED -- the launcher must never write here');

// --- 5. run it -----------------------------------------------------------------
console.log('\n--- supervise');
const sup = new Supervisor(cfg);
let sawStderrTag = false;
sup.on('line', (l) => { if (l.includes('[err] ')) sawStderrTag = true; });

const exited = new Promise((res) => sup.once('exit', res));
sup.start(plan);
check('child has a pid', sup.status().pid > 0, String(sup.status().pid));

const ready = await sup.waitUntilReady((m) => console.log('   ', m));
check('the application reported itself ready', !!ready,
      ready && ready.info ? JSON.stringify(ready.info).slice(0, 90) : 'no answer');
// The reply must come from the child we started. Without this a stray core
// left on the same port answers, and the supervisor reports a stranger's
// health and then sends it the shutdown.
check('the health reply came from OUR process',
      !!(ready && ready.info && ready.info.pid === sup.status().pid),
      `reply pid ${ready && ready.info ? ready.info.pid : '?'} vs child pid ${sup.status().pid}`);
check('both streams are drained', sup.tail(9999).length > 0,
      `${sup.tail(9999).length} lines captured, stderr seen=${sawStderrTag}`);

// --- 6. graceful stop -----------------------------------------------------------
console.log('\n--- stop');
const t0 = Date.now();
const stopped = await sup.stop();
await exited;
check('stopped', stopped.stopped);
check('stopped GRACEFULLY (no force kill)', stopped.forced === false,
      `took ${((Date.now() - t0) / 1000).toFixed(1)} s`);
check('the application ran its own teardown', /graceful/i.test(sup.tail(400).join('\n')),
      'looked for the core\'s "graceful shutdown" line in captured output');

// --- 7. prune -------------------------------------------------------------------
console.log('\n--- prune');
before = snapshotWorkingDir();     // the application has been running; re-baseline
{
  for (const v of ['0.0.1', '0.0.2', '0.0.3']) {
    fs.mkdirSync(path.join(apps.versionDir(v), 'scripts'), { recursive: true });
  }
  const removed = apps.prune(cfg.values.keepVersions);
  check('prune never removes the current version',
        apps.currentVersion() === installed.version && fs.existsSync(apps.versionDir(installed.version)),
        'removed: ' + (removed.join(', ') || 'none'));
}

// --- 8. prune did not reach outside the app root ----------------------------------
check('prune changed NOTHING in the working directory', before === snapshotWorkingDir(),
      'the machine\'s calibration and recipe must survive every cleanup');

console.log(`\n${failures === 0 ? 'ALL PASS' : failures + ' FAILURE(S)'}`);
console.log('leaving app root for inspection:', root);
process.exit(failures ? 1 : 0);
