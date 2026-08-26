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
// Where install() parks the directory it is replacing until the new one is
// safely in place. Dot-prefixed so list() never mistakes it for a version.
const REPLACED = '.replaced';
// The version that has PROVED itself, and the only thing that makes a rollback
// target reliable. See markGood/lastGood below.
const LAST_GOOD = 'last_good.json';
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
        // Anything dot-prefixed is the launcher's own machinery -- .staging
        // while a package is being verified, .replaced while an install swaps
        // a directory. A version name may not start with a dot (enforced in
        // updater.install), so this can never hide a real version.
        .filter((d) => d.isDirectory() && !d.name.startsWith('.'))
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
    // Remember what this displaced. It is the fallback rollback target for a
    // machine that has never accumulated a last_good -- one restarted more
    // often than the good-timer runs, which is exactly the machine most likely
    // to need rolling back.
    const previous = this.currentVersion();
    fs.writeFileSync(tmp, JSON.stringify({
      version,
      previous: previous && previous !== version ? previous : null,
      selectedAt: new Date().toISOString(),
    }, null, 2));
    fs.renameSync(tmp, this.currentFile);
    return version;
  }

  // Put back anything an interrupted install left parked.
  //
  // install() renames the old directory into .replaced/ before renaming the new
  // one in, so that a failed rename cannot lose both. If the process dies
  // BETWEEN those two renames -- a power cut, a kill -- the version is intact
  // but not where it belongs, and the machine comes up believing it is not
  // installed. Nothing else would ever put it back.
  //
  // Only ever restores into an empty slot: if the destination now exists, the
  // install completed and the parked copy is the superseded one, so it goes.
  recoverInterrupted(log = () => {}) {
    const parked = path.join(this.dir, REPLACED);
    let entries;
    try { entries = fs.readdirSync(parked, { withFileTypes: true }).filter((d) => d.isDirectory()); }
    catch { return { restored: [], discarded: [] }; }

    const restored = [];
    const discarded = [];
    for (const d of entries) {
      // "<version>-<timestamp>" -- the timestamp is what makes it unique, and
      // the version is everything before the last dash.
      const cut = d.name.lastIndexOf('-');
      const version = cut > 0 ? d.name.slice(0, cut) : d.name;
      const src = path.join(parked, d.name);
      const dest = this.versionDir(version);
      try {
        if (fs.existsSync(dest)) {
          fs.rmSync(src, { recursive: true, force: true });
          discarded.push(version);
        } else {
          fs.renameSync(src, dest);
          restored.push(version);
          log(`an interrupted install had parked ${version}; put it back`);
        }
      } catch (e) {
        log(`could not tidy the parked copy of ${version}: ${e.message}`);
      }
    }
    try { fs.rmdirSync(parked); } catch { /* not empty, or not there */ }
    return { restored, discarded };
  }

  previousVersion() {
    try {
      const j = JSON.parse(fs.readFileSync(this.currentFile, 'utf8'));
      return typeof j.previous === 'string' ? j.previous : null;
    } catch { return null; }
  }

  // --- last known good -------------------------------------------------------
  //
  // "It started" is not evidence that a version works: a build that dies after
  // ten minutes starts perfectly. So a version becomes good only after it has
  // RUN, for a stretch long enough to be a real production period -- see
  // goodAfterMs in config.js.
  //
  // Written when the timer fires, never at shutdown. A machine that crashes
  // must still accumulate the record, and one that is killed must not be able
  // to claim a stretch it did not finish.
  get lastGoodFile() { return path.join(this.dir, LAST_GOOD); }

  lastGood() {
    try {
      const j = JSON.parse(fs.readFileSync(this.lastGoodFile, 'utf8'));
      return typeof j.version === 'string' ? j : null;
    } catch { return null; }
  }

  markGood(version, ranForS) {
    const tmp = this.lastGoodFile + '.tmp';
    fs.mkdirSync(this.dir, { recursive: true });
    fs.writeFileSync(tmp, JSON.stringify({
      version, ranForS: Math.round(ranForS), at: new Date().toISOString(),
    }, null, 2));
    fs.renameSync(tmp, this.lastGoodFile);
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
    // Never delete a rollback target.
    //
    // prune runs after EVERY successful start, and it used to protect only the
    // version that was running. That is fine when a person installs a version
    // every few months; it is wrong the moment updates arrive on their own,
    // because three updates in a row silently delete the version you would want
    // to go back to. keepVersions is a disk-space policy and was being asked to
    // double as a safety policy, which it cannot do -- the version worth
    // keeping is not "recent", it is "proved".
    //
    // So: the last known good, and whatever the current pointer displaced, are
    // kept regardless of how far down the list they have fallen.
    const good = this.lastGood();
    const protectedVersions = new Set([cur, good && good.version, this.previousVersion()].filter(Boolean));

    const keepN = Number.isFinite(Number(keep)) ? Math.max(1, Math.floor(Number(keep))) : 3;
    const all = this.list().filter((e) => e.version !== cur);
    const versions = all.filter((e) => e.valid);
    const foreign = all.filter((e) => !e.valid).map((e) => e.version);

    const doomed = versions.slice(Math.max(0, keepN - 1))
      .filter((e) => !protectedVersions.has(e.version));
    const kept_protected = versions.slice(Math.max(0, keepN - 1))
      .filter((e) => protectedVersions.has(e.version)).map((e) => e.version);
    const removed = [];
    for (const e of doomed) {
      try {
        fs.rmSync(this.versionDir(e.version), { recursive: true, force: true });
        removed.push(e.version);
      } catch { /* a locked file is not worth failing a start over */ }
    }
    return { removed, foreign, protected: kept_protected, kept: versions.length - removed.length };
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

module.exports = { AppStore, cmpVersion, STAGING, REPLACED, LAST_GOOD, REQUIRED_ENTRIES, INFO, BOOT };
