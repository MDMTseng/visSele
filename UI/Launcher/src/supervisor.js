// Runs whatever the application's boot.js described.
//
// It is told WHAT to run and knows nothing about what that is -- no executable
// name, no arguments, no port, no protocol. Even the health check and the
// shutdown request are the application's to define; this file calls a hook and
// takes what it gets.
//
// WHAT IS NOT NEGOTIABLE, and why each one is here:
//
//   every pipe has a reader     The old launcher opened stderr as a pipe and
//                               attached no handler (the line was there,
//                               commented out). A Windows anonymous pipe holds
//                               about 64 KB; once full the child's next write
//                               to stderr blocks forever. Not a crash -- the
//                               process stays in the task list, its sockets
//                               stay bound, and the machine just stops.
//
//   a timeout behind the ask    A hook decides HOW to ask a process to stop. It
//                               cannot decide that there is no deadline. An
//                               application that hangs on shutdown must not be
//                               able to hang the launcher with it.
//
//   the force kill              What the deadline runs into, and it is logged
//                               loudly, because an unclean stop is the first
//                               thing to suspect when the next run finds
//                               damaged state.
//
//   no automatic restart        The core decides whether a part passes. A
//                               supervisor that quietly brings it back after an
//                               unexplained death lets bad parts through while
//                               the line keeps running.
//
// Those four were all delegated to the payload in the previous design, and the
// payload got three of them wrong -- once, in a file that every version
// inherited.
'use strict';

const { spawn, execFile } = require('node:child_process');
const { EventEmitter } = require('node:events');
const fs = require('node:fs');
const path = require('node:path');

const boot = require('./boot');

// Enough to see how a run ended without letting a chatty child grow the heap.
const RING_LINES = 2000;
const LOG_MAX_BYTES = 8 * 1024 * 1024;

class Supervisor extends EventEmitter {
  constructor(cfg) {
    super();
    this.cfg = cfg;
    this.plan = null;
    this.children = new Map();          // id -> { spec, child, exited }
    this.ring = [];
    this.logStream = null;
    this.logBytes = 0;
    this.logFile = null;
    this.startedAt = null;
    this.pingTimer = null;
    this.missedPings = 0;
    this.lastHealth = null;
    this.stopping = false;
    this.lastStopWasForced = false;
    this.services = null;
  }

  get running() {
    for (const e of this.children.values()) if (!e.exited) return true;
    return false;
  }

  primaryEntry() {
    for (const e of this.children.values()) if (e.spec.primary) return e;
    return null;
  }

  status() {
    const prim = this.primaryEntry();
    return {
      running: this.running,
      pid: prim && !prim.exited && prim.child ? prim.child.pid : null,
      startedAt: this.startedAt,
      uptimeS: this.startedAt && this.running ? (Date.now() - this.startedAt) / 1000 : null,
      lastHealth: this.lastHealth,
      missedPings: this.missedPings,
      unresponsive: this.running && this.missedPings >= this.cfg.values.pingMissesToAlarm,
      lastStopWasForced: this.lastStopWasForced,
      processes: [...this.children.values()].map((e) => ({
        id: e.spec.id,
        primary: e.spec.primary,
        pid: e.child ? e.child.pid : null,
        running: !e.exited,
        hasControl: !!e.spec.control,
      })),
    };
  }

  tail(n = 200) { return this.ring.slice(-n); }

  // --- logging ---------------------------------------------------------

  _openLog() {
    const dir = this.cfg.logDir;
    fs.mkdirSync(dir, { recursive: true });
    const stamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19);
    const file = path.join(dir, `run-${stamp}.log`);
    this.logStream = fs.createWriteStream(file, { flags: 'a' });
    this.logBytes = 0;
    this.logFile = file;
    // A broken log must never take the machine with it. Disk full, permissions,
    // an antivirus holding the file -- all survivable; losing the machine over
    // them is not.
    this.logStream.on('error', (e) => {
      this._record(`[launcher] log write failed, continuing without a log file: ${e.message}`);
      this.logStream = null;
    });
    return file;
  }

  _record(line) {
    this.ring.push(line);
    if (this.ring.length > RING_LINES) this.ring.splice(0, this.ring.length - RING_LINES);
    if (this.logStream && this.logBytes < LOG_MAX_BYTES) {
      this.logBytes += line.length + 1;
      this.logStream.write(line + '\n');
      if (this.logBytes >= LOG_MAX_BYTES) {
        this.logStream.write('[launcher] log size cap reached; further output is in the ring buffer only\n');
      }
    }
    this.emit('line', line);
  }

  // Partial lines are held until their newline arrives, so a line split across
  // two chunks is not reported as two lines.
  _attachStream(stream, tag) {
    let pending = '';
    stream.setEncoding('latin1');
    stream.on('data', (chunk) => {
      pending += chunk;
      let nl;
      while ((nl = pending.indexOf('\n')) >= 0) {
        const line = pending.slice(0, nl).replace(/\r$/, '');
        pending = pending.slice(nl + 1);
        this._record(tag + line);
      }
      // A child that writes a huge line with no newline must not grow this
      // buffer without bound.
      if (pending.length > 64 * 1024) {
        this._record(tag + pending.slice(0, 64 * 1024));
        pending = '';
      }
    });
    stream.on('error', (e) => this._record(`[launcher] ${tag.trim()} stream error: ${e.message}`));
  }

  // --- lifecycle -------------------------------------------------------

  start(plan) {
    if (this.running) throw new Error('already running');

    this.plan = plan;
    this.children.clear();
    this.ring = [];
    this.stopping = false;
    this.lastStopWasForced = false;
    this.missedPings = 0;
    this.lastHealth = null;
    this.services = boot.makeServices(plan, (m) => this._record(m), () => this.status().processes);

    const logFile = this._openLog();
    this._record(`[launcher] log ${logFile}`);

    for (const spec of plan.processes) {
      const tag = plan.processes.length > 1 ? `[${spec.id}] ` : '';
      this._record(`[launcher] ${spec.id}: exec ${spec.exe}`);
      if (spec.args.length) this._record(`[launcher] ${spec.id}: args ${spec.args.join(' ')}`);
      this._record(`[launcher] ${spec.id}: cwd  ${spec.cwd}`);
      if (!spec.control) {
        this._record(`[launcher] ${spec.id}: declares NO control channel -- `
                   + 'it can only ever be force killed');
      }

      const child = spawn(spec.exe, spec.args, {
        cwd: spec.cwd,
        env: { ...process.env, ...spec.env },
        // stdin 'ignore', not 'inherit': a packaged Electron app has no console,
        // so 'inherit' hands the child an invalid handle. stdout and stderr are
        // both piped AND both drained -- see the file header.
        stdio: ['ignore', 'pipe', 'pipe'],
        windowsHide: true,
      });

      const entry = { spec, child, exited: false };
      this.children.set(spec.id, entry);

      this._attachStream(child.stdout, tag);
      this._attachStream(child.stderr, tag + '[err] ');
      child.on('error', (e) => this._record(`[launcher] ${spec.id}: spawn failed: ${e.message}`));
      child.on('exit', (code, signal) => this._onChildExit(entry, code, signal, logFile));
    }

    this.startedAt = Date.now();
    if (plan.processes.some((p) => p.control)) this._startPinging();
    this.emit('started', this.status());
    return { logFile, pid: this.status().pid };
  }

  _onChildExit(entry, code, signal, logFile) {
    entry.exited = true;
    const ranFor = (Date.now() - this.startedAt) / 1000;
    this._record(`[launcher] ${entry.spec.id} exited code=${code} signal=${signal} after ${ranFor.toFixed(1)} s`);

    if (!entry.spec.primary) {
      // A helper dying is reported and nothing else. Tearing the rest down
      // would be the launcher deciding that a process it knows nothing about is
      // essential; leaving it silent would hide a degraded machine. So: say it
      // loudly, mark the state, and let a person decide -- the same reason the
      // primary is not auto-restarted.
      if (!this.stopping) {
        this._record(`[launcher] ${entry.spec.id} was not the primary process -- `
                   + 'the application is still running but is now incomplete');
        this.emit('health', this.status());
      }
      return;
    }

    // The primary is gone: the application is down.
    this._stopPinging();
    const wasStopping = this.stopping;
    const tail = this.tail(200);
    if (this.logStream) { this.logStream.end(); this.logStream = null; }

    // Bring down anything still up, so a dead application cannot leave helpers
    // holding serial ports and sockets that the next start will need.
    const survivors = [...this.children.values()].filter((e) => !e.exited);
    if (survivors.length) {
      this._record(`[launcher] primary is gone; stopping ${survivors.length} remaining process(es)`);
      for (const e of survivors) this._forceKill(e);
    }

    this.emit('exit', {
      code, signal, ranFor,
      expected: wasStopping,
      forced: this.lastStopWasForced,
      tail, logFile,
    });
  }

  // Wait for the application to say it is ready, bounded by the longest timeout
  // any of its processes declared. A null is NOT a failure: this core retries
  // camera init, which is slow and normal, so the caller shows the UI anyway
  // and lets the health monitor report the truth.
  async waitUntilReady(onProgress = () => {}) {
    const plan = this.plan;
    if (!plan) return null;
    const budget = Math.max(...plan.processes.map((p) => p.readyTimeoutMs));
    const deadline = Date.now() + budget;
    while (Date.now() < deadline && this.running) {
      const r = await boot.callHook(plan, 'isReady', this.services,
                                    boot.defaults.isReady, (m) => this._record(m));
      if (r) { this.lastHealth = { at: Date.now(), ...r }; return r; }
      await new Promise((res) => setTimeout(res, 400));
    }
    if (this.running) onProgress(`not ready within ${budget} ms -- continuing anyway`);
    return null;
  }

  _startPinging() {
    this._stopPinging();
    this.pingTimer = setInterval(async () => {
      if (!this.running || this.stopping) return;
      const h = await boot.callHook(this.plan, 'health', this.services,
                                    boot.defaults.health, (m) => this._record(m));
      if (h && h.ok) {
        this.missedPings = 0;
        this.lastHealth = { at: Date.now(), ...h };
      } else {
        this.missedPings++;
        // Reported, never acted on. "It stopped answering" is not "it should be
        // restarted": this core has been seen alive with its ports still bound
        // and a dead serial link, and it is also what a long blocking operation
        // looks like from out here.
        if (this.missedPings === this.cfg.values.pingMissesToAlarm) {
          this._record(`[launcher] no healthy answer for ${this.missedPings} checks `
                     + `(${(h && h.error) || 'no detail'})`);
        }
      }
      this.emit('health', this.status());
    }, this.cfg.values.pingIntervalMs);
    this.pingTimer.unref?.();
  }

  _stopPinging() {
    if (this.pingTimer) { clearInterval(this.pingTimer); this.pingTimer = null; }
  }

  _forceKill(entry) {
    if (entry.exited || !entry.child) return;
    const pid = entry.child.pid;
    if (process.platform === 'win32') execFile('taskkill', ['/pid', String(pid), '/t', '/f'], () => {});
    else { try { process.kill(pid, 'SIGKILL'); } catch { /* already gone */ } }
  }

  // Ask the way the application asked to be asked; then insist.
  async stop() {
    if (!this.running) return { stopped: true, forced: false, note: 'not running' };
    this.stopping = true;
    this._stopPinging();

    const allExited = new Promise((resolve) => {
      const check = () => { if (!this.running) { this.off('line', check); resolve('exited'); } };
      this.on('line', check);
      check();
    });

    const acks = await boot.callHook(this.plan, 'requestShutdown', this.services,
                                     boot.defaults.requestShutdown, (m) => this._record(m));
    if (Array.isArray(acks)) {
      for (const a of acks) {
        this._record(a.acked ? `[launcher] ${a.id}: shutdown acked`
                             : `[launcher] ${a.id}: no shutdown ack (${a.error || 'unknown'})`);
      }
    }

    const timeout = new Promise((resolve) =>
      setTimeout(() => resolve('timeout'), this.cfg.values.shutdownTimeoutMs));
    const which = await Promise.race([allExited, timeout]);
    if (which === 'exited') return { stopped: true, forced: false };

    this.lastStopWasForced = true;
    const stuck = [...this.children.values()].filter((e) => !e.exited);
    this._record(`[launcher] ${stuck.map((e) => e.spec.id).join(', ')} did not exit within `
               + `${this.cfg.values.shutdownTimeoutMs} ms -- FORCE KILLING. `
               + 'State written after this point may be incomplete.');
    for (const e of stuck) this._forceKill(e);
    await Promise.race([allExited, new Promise((r) => setTimeout(r, 3000))]);
    return { stopped: true, forced: true };
  }
}

module.exports = { Supervisor };
