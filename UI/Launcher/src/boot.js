// Loading an application's scripts/boot.js, and the contract it speaks.
//
// THE GOAL THIS FILE SERVES: the launcher's first version should be able to be
// its last. Anything specific to an application, a machine, or a protocol has
// to be expressible by the application, or one day it forces a new launcher
// into the field -- and a launcher update is the one update that cannot be
// delivered by the update mechanism it contains.
//
// So the split is not "what to run" versus "how to run it". It is:
//
//   THE APPLICATION DECIDES        how many processes there are, what they are
//                                  called, where they live, what arguments and
//                                  environment they get, how to ask one whether
//                                  it is healthy, how to ask it to stop, what
//                                  must exist on disk first, and where its UI
//                                  is.
//
//   THE LAUNCHER KEEPS             spawning, draining every stdio pipe, the log
//                                  ring and its rotation, the shutdown TIMEOUT,
//                                  the force kill, the crash screen, and the
//                                  refusal to auto-restart.
//
// The launcher's half is not a matter of taste. Every item in it is something
// the previous design delegated to the payload and that the payload then got
// wrong, once per version: an unread stderr pipe that could wedge the machine
// at 64 KB, a kill('SIGINT') that delivers no signal on Windows, and the
// taskkill /f on the line after it that made the core's careful teardown dead
// code. A hook may decide HOW to ask a process to stop; it may not decide
// whether there is a timeout behind it.
//
// ON EXECUTING PAYLOAD CODE. The old design had the shell require() a
// launcher.js out of a directory that had just been unzipped and never checked.
// Here the order is reversed: a package is rejected unless every file matches a
// SHA256 in its manifest, and only an installed, verified version is ever
// loaded. Verify-then-execute is defensible; execute-and-hope was not.
//
// `services` below is a CONVENIENCE, NOT A SANDBOX. A hook runs in the main
// process and can require() whatever it likes. The point of the object is that
// the common things have one obvious spelling that will not change, so hooks
// stay short and stay compatible.
'use strict';

const fs = require('node:fs');
const path = require('node:path');
const { execFile } = require('node:child_process');

const control = require('./control');

const BOOT_REL = path.join('scripts', 'boot.js');
const API_VERSION = 1;

// Every key the launcher understands, at each level. Anything else is an ERROR.
//
// Silently ignoring an unknown key is how a newer package half-runs on an older
// launcher: the parts it understands take effect, the parts it does not are
// dropped, and the machine ends up in a configuration nobody wrote. Refusing
// makes the version mismatch visible at install time, on the bench, instead of
// as strange behaviour on the line.
const PLAN_KEYS = new Set(['name', 'core', 'processes', 'ui', 'requires']);
const PROC_KEYS = new Set(['id', 'exe', 'args', 'cwd', 'env', 'control', 'readyTimeoutMs', 'primary']);
const UI_KEYS = new Set(['indexPath', 'url']);
const REQ_KEYS = new Set(['path', 'kind', 'why']);
const HOOKS = ['checkRequirements', 'isReady', 'health', 'requestShutdown'];

function rejectUnknown(obj, allowed, where) {
  for (const k of Object.keys(obj)) {
    if (!allowed.has(k)) {
      throw new Error(`boot.js: ${where} has an unknown key "${k}". `
        + `This launcher understands: ${[...allowed].join(', ')}. `
        + 'A newer package needs a newer launcher -- it is refused rather than half-applied.');
    }
  }
}

function bootPath(appDir) { return path.join(appDir, BOOT_REL); }
function hasBoot(appDir) {
  try { return fs.statSync(bootPath(appDir)).isFile(); } catch { return false; }
}

// Resolve a path the application gave us, and refuse anything that leaves its
// own directory. boot.js is verified content, not hostile, but a `..` in a path
// is far more likely to be a packaging mistake than an intention -- and
// silently running an executable from outside the version directory would make
// "which version is running?" unanswerable.
function resolveInApp(appDir, rel, what) {
  if (typeof rel !== 'string' || !rel.length) throw new Error(`boot.js: ${what} is missing`);
  if (path.isAbsolute(rel)) throw new Error(`boot.js: ${what} must be relative to the version directory, got "${rel}"`);
  const abs = path.resolve(appDir, rel);
  const base = path.resolve(appDir);
  if (abs !== base && !abs.startsWith(base + path.sep)) {
    throw new Error(`boot.js: ${what} "${rel}" escapes the version directory`);
  }
  return abs;
}

// What describe() is told. Small on purpose: if boot.js needs something new
// here, that is a conversation about the contract, not a discovery in the
// field.
function makeContext(appDir, workingDir, log) {
  return {
    apiVersion: API_VERSION,
    appDir,
    workingDir,
    platform: process.platform,
    log: (m) => log(String(m)),
  };
}

function load(appDir, workingDir, log = () => {}) {
  const file = bootPath(appDir);
  if (!fs.existsSync(file)) {
    throw new Error(`this version has no ${BOOT_REL} -- the launcher does not know how to start it`);
  }

  let mod;
  try {
    // Never cached: a version can be reinstalled in place during commissioning,
    // and a stale module would keep booting the old plan while every visible
    // sign said the new one was installed.
    delete require.cache[require.resolve(file)];
    mod = require(file);
  } catch (e) {
    throw new Error(`${BOOT_REL} failed to load: ${e.message}`);
  }

  if (!mod || typeof mod.describe !== 'function') throw new Error(`${BOOT_REL} must export describe(ctx)`);
  if (mod.apiVersion !== API_VERSION) {
    throw new Error(`${BOOT_REL} declares apiVersion ${mod.apiVersion}, this launcher speaks ${API_VERSION}`);
  }

  let raw;
  try {
    raw = mod.describe(makeContext(appDir, workingDir, log));
  } catch (e) {
    throw new Error(`${BOOT_REL} describe() threw: ${e.message}`);
  }
  if (!raw || typeof raw !== 'object') throw new Error(`${BOOT_REL} describe() returned nothing`);

  const plan = normalise(raw, appDir, workingDir);
  plan.hooks = {};
  for (const h of HOOKS) if (typeof mod[h] === 'function') plan.hooks[h] = mod[h].bind(mod);
  return plan;
}

function normaliseProcess(p, i, appDir, workingDir) {
  rejectUnknown(p, PROC_KEYS, `processes[${i}]`);
  const id = typeof p.id === 'string' && p.id ? p.id : `p${i}`;
  const exe = resolveInApp(appDir, p.exe, `processes[${i}].exe`);
  if (!fs.existsSync(exe)) throw new Error(`boot.js: ${id}: executable does not exist: ${exe}`);

  // cwd defaults to the working directory rather than the version directory,
  // because that is what an application with relative data paths needs -- but
  // it is the application's call, and one that keeps everything beside its
  // binary says so.
  let cwd;
  if (p.cwd === undefined || p.cwd === null || p.cwd === '@working') cwd = workingDir;
  else if (p.cwd === '@app') cwd = appDir;
  else cwd = resolveInApp(appDir, p.cwd, `${id}.cwd`);

  let ctl = null;
  if (p.control) {
    rejectUnknown(p.control, new Set(['host', 'port']), `${id}.control`);
    const port = Number(p.control.port);
    if (!Number.isInteger(port) || port <= 0 || port > 65535) {
      throw new Error(`boot.js: ${id}.control.port is not a port: ${p.control.port}`);
    }
    ctl = { host: typeof p.control.host === 'string' ? p.control.host : '127.0.0.1', port };
  }

  return {
    id,
    exe,
    args: Array.isArray(p.args) ? p.args.map(String) : [],
    cwd,
    env: (p.env && typeof p.env === 'object') ? p.env : {},
    control: ctl,
    readyTimeoutMs: Number(p.readyTimeoutMs) > 0 ? Number(p.readyTimeoutMs) : 20000,
    primary: !!p.primary,
  };
}

function normalise(raw, appDir, workingDir) {
  rejectUnknown(raw, PLAN_KEYS, 'the plan');

  // `core` is sugar for a single primary process, because one process is the
  // common case and making everyone write an array for it would be noise.
  let list;
  if (raw.processes && raw.core) throw new Error('boot.js: use either core or processes, not both');
  if (Array.isArray(raw.processes)) {
    if (!raw.processes.length) throw new Error('boot.js: processes is empty');
    list = raw.processes;
  } else if (raw.core) {
    list = [{ ...raw.core, id: raw.core.id || 'core', primary: true }];
  } else {
    throw new Error('boot.js: plan has no core and no processes');
  }

  const processes = list.map((p, i) => normaliseProcess(p, i, appDir, workingDir));

  // Exactly one primary. It is the one whose death means the application is
  // down; without a designated one, "the application exited" has no meaning.
  const primaries = processes.filter((p) => p.primary);
  if (primaries.length === 0) {
    if (processes.length === 1) processes[0].primary = true;
    else throw new Error('boot.js: with more than one process, exactly one must be primary: true');
  } else if (primaries.length > 1) {
    throw new Error(`boot.js: ${primaries.length} processes are marked primary; exactly one may be`);
  }
  const ids = new Set();
  for (const p of processes) {
    if (ids.has(p.id)) throw new Error(`boot.js: duplicate process id "${p.id}"`);
    ids.add(p.id);
  }

  let ui = null;
  if (raw.ui) {
    rejectUnknown(raw.ui, UI_KEYS, 'ui');
    if (raw.ui.indexPath && raw.ui.url) throw new Error('boot.js: ui may have indexPath or url, not both');
    if (raw.ui.indexPath) {
      const p = resolveInApp(appDir, raw.ui.indexPath, 'ui.indexPath');
      if (!fs.existsSync(p)) throw new Error(`boot.js: ui.indexPath does not exist: ${p}`);
      ui = { kind: 'file', target: p };
    } else if (typeof raw.ui.url === 'string' && raw.ui.url) {
      // A UI served over http by the application itself is a legitimate shape;
      // an arbitrary remote origin in a machine's window is not.
      if (!/^https?:\/\/(localhost|127\.0\.0\.1)(:\d+)?(\/|$)/i.test(raw.ui.url)) {
        throw new Error(`boot.js: ui.url must be a loopback http(s) URL, got "${raw.ui.url}"`);
      }
      ui = { kind: 'url', target: raw.ui.url };
    }
  }

  const requires = Array.isArray(raw.requires) ? raw.requires.map((r, i) => {
    if (!r || typeof r !== 'object') throw new Error(`boot.js: requires[${i}] is not an object`);
    rejectUnknown(r, REQ_KEYS, `requires[${i}]`);
    if (typeof r.path !== 'string' || !r.path) throw new Error(`boot.js: requires[${i}] has no path`);
    return {
      path: path.resolve(r.path),
      kind: r.kind === 'file' ? 'file' : 'dir',
      why: typeof r.why === 'string' ? r.why : '',
    };
  }) : [];

  return {
    name: typeof raw.name === 'string' ? raw.name : null,
    processes,
    primary: processes.find((p) => p.primary),
    ui,
    requires,
    appDir,
    workingDir,
    hooks: {},
  };
}

// --- the services a hook is handed -------------------------------------------
//
// Not a sandbox (see the file header) -- a stable spelling for the four things
// a hook realistically needs, so hooks stay short and keep working.
function makeServices(plan, log, liveProcesses) {
  return {
    plan,
    appDir: plan.appDir,
    workingDir: plan.workingDir,
    platform: process.platform,
    log: (m) => log(String(m)),

    // One request/response over a line-delimited JSON socket. The default
    // health and shutdown hooks are written in terms of this, so an application
    // that speaks the same shape on a different port needs no hook at all, and
    // one that speaks something else can still borrow the plumbing.
    lineJson: (port, host, obj, timeoutMs) => control.command(port, obj, timeoutMs, host),

    // 'file' | 'dir' | null.
    exists: (p) => {
      try { const st = fs.statSync(p); return st.isDirectory() ? 'dir' : st.isFile() ? 'file' : null; }
      catch { return null; }
    },

    // Run a helper that ships INSIDE this version. Confined to the version
    // directory for the same reason executables are: a supervisor that can be
    // told to run anything on the disk is not supervising anything.
    run: (relExe, args = [], opts = {}) => new Promise((resolve) => {
      let abs;
      try { abs = resolveInApp(plan.appDir, relExe, 'run()'); }
      catch (e) { return resolve({ code: -1, stdout: '', stderr: e.message }); }
      execFile(abs, args.map(String),
        { cwd: opts.cwd || plan.workingDir, timeout: Number(opts.timeoutMs) || 10000, windowsHide: true },
        (err, stdout, stderr) => resolve({ code: err ? (err.code ?? -1) : 0, stdout, stderr }));
    }),

    // Read-only view. Deliberately not the ChildProcess objects: killing and
    // stream handling stay with the supervisor.
    processes: () => liveProcesses(),
  };
}

// --- defaults -----------------------------------------------------------------
//
// Each of these is what happens when the application supplies no hook. They are
// the behaviour this core needs, expressed once, so the common case ships no
// hook at all -- and so an application that differs overrides one function
// rather than reimplementing supervision.

async function defaultCheckRequirements(services) {
  const unmet = [];
  for (const r of services.plan.requires) {
    const got = services.exists(r.path);
    if (got !== r.kind) unmet.push(r);
  }
  return unmet;
}

async function defaultHealth(services) {
  const checks = services.plan.processes.filter((p) => p.control);
  if (!checks.length) return { ok: true, info: null, note: 'no control channel declared' };

  // Who is supposed to be answering. A control channel identified only by a
  // port is identified only by a port: a stray process from an earlier session,
  // or a second launcher, answers on the same loopback port and is
  // indistinguishable from the child we started.
  //
  // That is not hypothetical -- it happened on the bench. A test spawned its
  // own core, health-checked a stranger with an hour of uptime, reported itself
  // healthy, and then sent the shutdown to the stranger: it stopped the wrong
  // process while its own child ran on until the force-kill timer.
  //
  // So if the reply names a pid, it must be OUR pid. If it names none, the
  // check is skipped rather than guessed at -- an application that does not
  // report a pid is not a broken one, it just cannot offer this protection.
  const live = new Map((services.processes() || []).map((p) => [p.id, p.pid]));

  const results = [];
  for (const p of checks) {
    const r = await services.lineJson(p.control.port, p.control.host, { type: 'ping' }, 1200);
    let ok = r.ok;
    let error = r.error || null;
    const reply = r.reply || null;
    if (ok && reply && typeof reply.pid === 'number') {
      const mine = live.get(p.id);
      if (mine && reply.pid !== mine) {
        ok = false;
        error = `${p.control.host}:${p.control.port} answered by pid ${reply.pid}, `
              + `but ${p.id} is pid ${mine} -- another instance is holding that port`;
      }
    }
    results.push({ id: p.id, ok, reply, error });
  }
  const bad = results.find((r) => !r.ok);
  return { ok: !bad, info: results.length === 1 ? results[0].reply : results, error: bad ? bad.error : null };
}

async function defaultIsReady(services) {
  const h = await defaultHealth(services);
  return h.ok ? h : null;
}

// Ask nicely, in reverse start order. Returns which processes acknowledged; the
// launcher owns what happens if they do not go, and no hook can opt out of it.
async function defaultRequestShutdown(services) {
  const out = [];
  const list = [...services.plan.processes].reverse();
  const live = new Map((services.processes() || []).map((p) => [p.id, p.pid]));

  for (const p of list) {
    if (!p.control) { out.push({ id: p.id, acked: false, error: 'no control channel' }); continue; }

    // Ask WHO is there before telling anyone to stop. Sending a shutdown to a
    // port without checking is how a supervisor kills a process it does not
    // own -- and on a production line the process on the other end of a
    // mistaken shutdown is a machine that was running.
    const who = await services.lineJson(p.control.port, p.control.host, { type: 'ping' }, 1200);
    const mine = live.get(p.id);
    if (who.ok && who.reply && typeof who.reply.pid === 'number' && mine && who.reply.pid !== mine) {
      out.push({
        id: p.id, acked: false,
        error: `refusing to shut down ${p.control.host}:${p.control.port}: it is pid `
             + `${who.reply.pid}, not our ${mine}. Our process will be force killed instead.`,
      });
      continue;
    }

    const r = await services.lineJson(p.control.port, p.control.host, { type: 'shutdown' }, 2000);
    out.push({ id: p.id, acked: !!r.ok, error: r.error || null });
  }
  return out;
}

// Call a hook if the application supplied one, otherwise the default. Errors
// from a hook are caught and reported, never thrown into the supervisor: a
// broken health check must not be able to take down the machine it watches.
async function callHook(plan, name, services, fallback, log) {
  const fn = plan.hooks[name];
  if (!fn) return fallback(services);
  try {
    return await fn(services);
  } catch (e) {
    log(`[launcher] ${BOOT_REL} ${name}() threw: ${e.message} -- falling back to the built-in`);
    return fallback(services);
  }
}

module.exports = {
  load, hasBoot, bootPath, BOOT_REL, API_VERSION,
  makeServices, callHook,
  defaults: {
    checkRequirements: defaultCheckRequirements,
    health: defaultHealth,
    isReady: defaultIsReady,
    requestShutdown: defaultRequestShutdown,
  },
};
