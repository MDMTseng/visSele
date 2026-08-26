// The installed applications, and the single pointer that says which one runs.
//
//   <appRoot>/
//     current.json          {"version":"1.1.103"}      <- the only switch
//     1.1.103/
//       info.json           the version string
//       scripts/boot.js     how this version starts    <- see src/boot.js
//       manifest.json       SHA256 of every file
//       ...                 whatever else this version consists of
//     1.1.102/
//     .staging/                                        <- transient, see updater
//
// Same shape as the ESP32's app0/app1 + otadata, and for the same reason: the
// dangerous moment in any update is the instant the running thing changes, so
// make that instant a single small write that either happened or did not.
//
// WHAT IS HARD-CODED HERE, and nothing else:
//
//   info.json        so a directory can be named by its version
//   scripts/boot.js  so the launcher can ask how to start it
//
// There is no expectation of Core/, of WebUI/, of an executable name or of any
// layout at all. An application that puts its binary somewhere else, or has two
// of them, or ships no UI, is described by its own boot.js and this file never
// needs to hear about it.
'use strict';

const fs = require('node:fs');
const path = require('node:path');

const STAGING = '.staging';
const INFO = 'info.json';
const BOOT = path.join('scripts', 'boot.js');

// The complete list. Kept as data so the updater and the shell can quote it
// back to whoever built a package that is missing one.
const REQUIRED_ENTRIES = [INFO, BOOT];

class AppStore {
  constructor(cfg) { this.cfg = cfg; }

  get dir() { return this.cfg.appRoot; }
  get currentFile() { return path.join(this.dir, 'current.json'); }

  versionDir(version) { return path.join(this.dir, version); }

  list() {
    let names;
    try {
      names = fs.readdirSync(this.dir, { withFileTypes: true })
        .filter((d) => d.isDirectory() && d.name !== STAGING)
        .map((d) => d.name);
    } catch {
      return [];
    }
    const cur = this.currentVersion();
    const out = names.map((name) => {
      const info = this.readInfo(name);
      const v = this.validate(name);
      return {
        version: name,
        current: name === cur,
        declaredVersion: info ? info.version : null,
        valid: v.ok,
        missing: v.missing,
        installedAt: this.installedAt(name),
      };
    });
    // Numerically per dot-separated field, so 1.1.103 sorts after 1.1.99. A
    // plain string sort gets that backwards, and a version list that lies about
    // which is newest is worse than no list.
    out.sort((a, b) => cmpVersion(b.version, a.version));
    return out;
  }

  installedAt(version) {
    try { return fs.statSync(this.versionDir(version)).mtimeMs; } catch { return null; }
  }

  readInfo(version) {
    try {
      return JSON.parse(fs.readFileSync(path.join(this.versionDir(version), INFO), 'utf8'));
    } catch { return null; }
  }

  // Structural only. Whether the application can actually RUN is boot.js's
  // question, and it is asked at start time -- a version whose data directory
  // is missing is perfectly well installed, and saying otherwise here would
  // hide the real reason behind a vague "incomplete".
  validate(version) {
    const base = this.versionDir(version);
    const missing = REQUIRED_ENTRIES.filter((rel) => !fs.existsSync(path.join(base, rel)));
    if (missing.length) return { ok: false, missing };
    const info = this.readInfo(version);
    if (!info || typeof info.version !== 'string' || !info.version.length) {
      return { ok: false, missing: [`${INFO}:version`] };
    }
    return { ok: true, missing: [] };
  }

  currentVersion() {
    try {
      const j = JSON.parse(fs.readFileSync(this.currentFile, 'utf8'));
      return typeof j.version === 'string' ? j.version : null;
    } catch { return null; }
  }

  // The switch. One small write, via a temp file and a rename, so a power cut
  // mid-update leaves either the old pointer or the new one and never a
  // truncated file that names no version at all.
  setCurrent(version) {
    const v = this.validate(version);
    if (!v.ok) throw new Error(`app ${version} is incomplete: missing ${v.missing.join(', ')}`);
    fs.mkdirSync(this.dir, { recursive: true });
    const tmp = this.currentFile + '.tmp';
    fs.writeFileSync(tmp, JSON.stringify({ version, selectedAt: new Date().toISOString() }, null, 2));
    fs.renameSync(tmp, this.currentFile);
    return version;
  }

  // Resolve what should run. null is a normal first-run state, not an error.
  resolve() {
    const explicit = this.currentVersion();
    if (explicit && this.validate(explicit).ok) return this.describe(explicit);

    // The pointer names something missing or broken. Rather than refuse to
    // start, fall back to the newest version that does validate and say so --
    // the operator gets a working machine plus a warning, instead of a dead
    // launcher and a JSON file to hand-edit.
    const candidate = this.list().find((e) => e.valid);
    if (!candidate) return null;
    return { ...this.describe(candidate.version), fellBackFrom: explicit };
  }

  describe(version) {
    const info = this.readInfo(version);
    return {
      version,
      declaredVersion: info ? info.version : null,
      dir: this.versionDir(version),
    };
  }

  // Called only after a successful start, so a bad update always still has
  // something to fall back to. Never removes the current version.
  //
  // This only ever touches directories under appRoot. The machine's working
  // directory is elsewhere by construction (see config.js) and nothing in this
  // file can reach it.
  // `running` is the version the launcher ACTUALLY started, which is not always
  // the one current.json names -- resolve() deliberately falls back to the
  // newest valid version when the pointer is missing or broken, and then starts
  // it. Re-deriving "current" here would put that running version in the doomed
  // list like any other and delete its directory out from under the live
  // process, so the caller passes what it started.
  prune(keep, running) {
    const cur = running || this.currentVersion();

    // Delete ONLY directories this store recognises as versions.
    //
    // list() returns every subdirectory, valid or not -- that is right for the
    // shell, which should show a broken install rather than hide it, and wrong
    // here. prune's whole input used to be that list, so anything sharing the
    // folder was a deletion candidate: point appRoot at a machine's working
    // directory by mistake and the next SUCCESSFUL start silently removes
    // data/, the calibration and the recipes, with rmSync force and no
    // confirmation. The comment on this class says the working directory is
    // elsewhere "by construction"; that was an assumption, not a check.
    //
    // Skipping the unrecognised ones makes a misconfigured appRoot fail by
    // doing NOTHING rather than by destroying something. It is also why the
    // skipped names are returned: silence here is what made the old behaviour
    // invisible.
    const keepN = Number.isFinite(Number(keep)) ? Math.max(1, Math.floor(Number(keep))) : 3;
    const all = this.list().filter((e) => e.version !== cur);
    const versions = all.filter((e) => e.valid);
    const foreign = all.filter((e) => !e.valid).map((e) => e.version);

    const doomed = versions.slice(Math.max(0, keepN - 1));
    const removed = [];
    for (const e of doomed) {
      try {
        fs.rmSync(this.versionDir(e.version), { recursive: true, force: true });
        removed.push(e.version);
      } catch { /* a locked file is not worth failing a start over */ }
    }
    return { removed, foreign, kept: versions.length - removed.length };
  }
}

// Numeric per field, with non-numeric fields compared as strings so a tag like
// "1.2.0-rc1" still orders sensibly rather than throwing.
function cmpVersion(a, b) {
  const pa = String(a).split(/[.\-+]/);
  const pb = String(b).split(/[.\-+]/);
  for (let i = 0; i < Math.max(pa.length, pb.length); i++) {
    const x = pa[i], y = pb[i];
    if (x === undefined) return -1;
    if (y === undefined) return 1;
    const nx = Number(x), ny = Number(y);
    if (Number.isFinite(nx) && Number.isFinite(ny)) {
      if (nx !== ny) return nx - ny;
    } else if (x !== y) {
      return x < y ? -1 : 1;
    }
  }
  return 0;
}

module.exports = { AppStore, cmpVersion, STAGING, REQUIRED_ENTRIES, INFO, BOOT };
