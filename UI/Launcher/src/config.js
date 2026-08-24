// launcher.json -- the launcher's own settings.
//
// It always lives at <userData>/launcher.json, never next to the executable.
// The old launcher wrote app_setup_info.json into app.getAppPath(), which is
// inside the packaged app directory: under Program Files that path is not
// writable, so the "choose a folder" buttons fail with EPERM on exactly the
// installs that were done properly. userData is per-user and always writable.
//
// Everything the file can hold has a working default, so a missing or corrupt
// file is not an error condition -- it is a first run.
//
// WHAT THIS FILE DOES NOT CONTAIN: anything about how an application starts.
// No executable name, no arguments, no ports, no directory layout. All of that
// comes from the application's own scripts/boot.js -- see src/boot.js.
'use strict';

const fs = require('node:fs');
const path = require('node:path');

const DEFAULTS = {
  // Where installed application versions live, and the current.json that says
  // which one runs. Operator-settable: it may belong on a data drive, or be
  // shared between accounts. Defaults to <userData>/apps.
  appRoot: null,

  // The core's WORKING DIRECTORY -- the directory it is started in.
  //
  // Every path inside the core is relative: "data/machine_setting.json",
  // "data/featureDetect", "data/SAMPLE", "data/last_fi_payload" and a dozen
  // more. So this is the PARENT of data/, not data/ itself.
  //
  // It is deliberately OUTSIDE appRoot, and the launcher NEVER creates, copies,
  // moves or seeds anything in it. This directory is the machine's identity --
  // its calibration, its recipe, its snapshots -- and it belongs to the
  // machine, not to any version of the software. An app directory is versioned,
  // is replaced wholesale by an install, and is deleted when old versions are
  // pruned; anything of the machine's kept in there would be swapped on every
  // version change and thrown away on every cleanup.
  //
  // No default that points anywhere real: the operator says where it is, and
  // until they do, the launcher refuses to start rather than guessing.
  workingDir: null,

  // How often to ask the application's renderer to collect garbage, in ms.
  // 0 disables it.
  //
  // NOT A WORKAROUND FOR A LEAK -- there is no leak. Measured 2026-08-23 over
  // four controlled conditions: renderer memory grows in proportion to the
  // number of image frames RECEIVED (10.16 MB/min at 5.2 fps, 2.53 at 1.0 fps)
  // and not to their size (87 KB frames grew no faster than 32 KB ones) nor to
  // how many times they are decoded. The JS heap stays between 26 and 33 MB
  // throughout, every array in the store keeps its length, and one forced
  // collection hands back 260 MB at once.
  //
  // So the memory is garbage that nothing retains and nothing sweeps: it lives
  // outside the JS heap, where V8 cannot see it and therefore never feels the
  // pressure that would trigger a collection. Left alone it reached 1.6 GB in
  // 105 minutes and did NOT self-correct as free memory fell to 673 MB -- which
  // on the 4 GB target is a machine that stops before the shift does.
  //
  // The launcher is the right place for this. It already owns resource
  // supervision, it can do it through webContents.debugger without the
  // application knowing, and a periodic collection is exactly what the
  // measurement says is missing.
  rendererGcIntervalMs: 120000,

  // Payload versions kept on disk. Older ones are removed only after a
  // successful start, so a failed update always has something to roll back to.
  keepVersions: 3,

  // --- supervision --------------------------------------------------------
  // These are launcher behaviour, not application layout. Anything the
  // APPLICATION knows better -- its control port, how long it needs to become
  // ready -- comes from boot.js and is not duplicated here.

  // How long a graceful shutdown is given before the force-kill. The core's own
  // teardown measured 0.8 s on this machine; 8 s is room for a bad day, not an
  // expectation.
  shutdownTimeoutMs: 8000,

  // Health ping cadence, and how many consecutive misses before the UI says
  // the core is unresponsive. Deliberately does NOT kill anything -- see the
  // note in supervisor.js.
  pingIntervalMs: 5000,
  pingMissesToAlarm: 3,
};

class Config {
  constructor(userDataDir) {
    this.file = path.join(userDataDir, 'launcher.json');
    this.userDataDir = userDataDir;
    this.values = { ...DEFAULTS };
    this.loadError = null;
    this.load();
  }

  load() {
    let raw;
    try {
      raw = fs.readFileSync(this.file, 'utf8');
    } catch (e) {
      if (e.code !== 'ENOENT') this.loadError = `${this.file}: ${e.message}`;
      return;                              // first run, or unreadable -> defaults
    }
    try {
      const parsed = JSON.parse(raw);
      // Only keys we know about. An unknown key is almost always a typo of a
      // known one, and silently carrying it forward makes the typo permanent.
      for (const k of Object.keys(parsed)) {
        if (k in DEFAULTS) this.values[k] = parsed[k];
        else this.loadError = `${this.file}: unknown key "${k}" ignored`;
      }
    } catch (e) {
      // A corrupt file must not stop the machine from starting. Say so, keep
      // the defaults, and do not overwrite it -- the operator may want to see
      // what is in there.
      this.loadError = `${this.file}: not valid JSON (${e.message}) -- using defaults`;
    }
  }

  save() {
    const tmp = this.file + '.tmp';
    fs.writeFileSync(tmp, JSON.stringify(this.values, null, 2));
    fs.renameSync(tmp, this.file);         // never leave a half-written config
  }

  get appRoot() {
    return this.values.appRoot || path.join(this.userDataDir, 'apps');
  }

  // null until the operator sets it. Callers must handle that; there is no
  // sensible guess, and a wrong guess runs the machine on someone else's
  // calibration.
  get workingDir() {
    return this.values.workingDir || null;
  }

  get logDir() {
    return path.join(this.userDataDir, 'logs');
  }

  set(key, value) {
    if (!(key in DEFAULTS)) throw new Error(`unknown setting: ${key}`);
    this.values[key] = value;
    this.save();
  }
}

module.exports = { Config, DEFAULTS };
