// Install the launcher code carried by this package, keeping what it replaces.
//
// Shipped as <version>/scripts/postinstall.js by `./dev.sh pack --with-launcher`.
// The updater runs it after every file in the package has been verified against
// manifest.json and moved into place, and before the version is selected --
// the one moment where failing is still free, because the old version is intact
// and still the one that starts.
//
// WHAT IT CAN AND CANNOT REPLACE
//
// The launcher's JavaScript (main.js, preload.js, shell/, src/, tools/) lives in
// resources/app and is plain files: replacing them is a copy, and the new code
// runs at the next launcher start. The Electron binary and its DLLs are what
// this process is executing out of, and Windows will not let a running image be
// overwritten, so they are not touched and a package must never try.
//
// WHEN IT TAKES EFFECT
//
// At the next launcher start. The running launcher has main.js and src/ already
// loaded, so it keeps running the old code -- but shell/ is read by the renderer
// on each navigation, so a launcher that goes back to its own shell after this
// point will show the NEW shell against the OLD main process. That pair is only
// as compatible as the change was; restart the launcher rather than leaving it
// mixed.
//
// RE-RUNNABLE, because the operator retries. A second run with nothing to do
// notices that and exits without making another backup.
//
// EXIT CODES, which the updater distinguishes: 0 ok, 2 "not this machine, do
// not retry", anything else "failed, worth retrying".

'use strict';
const fs = require('fs');
const path = require('path');

const KEEP_BACKUPS = 5;
// Exactly what the launcher's own code is. node_modules is deliberately absent:
// it is large, it is not what changes, and replacing it is how a launcher ends
// up with a half-installed dependency tree.
const FILES = ['main.js', 'preload.js'];
const DIRS  = ['shell', 'src', 'tools'];

const appDir = process.env.INSP_APP_DIR || process.cwd();
const payload = path.join(appDir, 'launcher');

// A package with no launcher payload is the normal case. Nothing to do.
if (!fs.existsSync(payload)) {
  console.log('no launcher/ in this package -- nothing to install');
  process.exit(0);
}

// WHERE THE LAUNCHER LIVES, from the launcher itself.
//
// Derived here it would be a guess about a directory layout that is allowed to
// differ between a bench and a machine. INSP_LAUNCHER_APP_DIR is passed by the
// updater and is path.join(process.resourcesPath, 'app') -- the authoritative
// answer, because it comes from the process that is running out of it.
const target = process.env.INSP_LAUNCHER_APP_DIR;
if (!target || !fs.existsSync(target)) {
  console.error('this launcher did not say where its code lives '
              + '(INSP_LAUNCHER_APP_DIR) -- it is too old to take a launcher update');
  process.exit(2);          // a retry cannot help; a newer launcher is needed
}

const sha = (p) => require('crypto').createHash('sha256')
  .update(fs.readFileSync(p)).digest('hex');

function listFiles(root) {
  const out = [];
  const walk = (rel) => {
    const abs = path.join(root, rel);
    let st;
    try { st = fs.statSync(abs); } catch { return; }
    if (st.isDirectory()) {
      for (const n of fs.readdirSync(abs)) walk(path.join(rel, n));
    } else if (st.isFile()) {
      out.push(rel.split(path.sep).join('/'));
    }
  };
  for (const f of FILES) walk(f);
  for (const d of DIRS) walk(d);
  return out.sort();
}

// Already installed? Then this is a retry, and a retry must not make a second
// backup of code it is about to leave alone.
const want = listFiles(payload);
if (!want.length) {
  console.error('launcher/ in this package is empty');
  process.exit(2);
}
const same = want.every((rel) => {
  const a = path.join(target, rel.split('/').join(path.sep));
  try { return fs.existsSync(a) && sha(a) === sha(path.join(payload, rel.split('/').join(path.sep))); }
  catch { return false; }
});
if (same) {
  console.log(`launcher code already matches this package (${want.length} files) -- nothing to do`);
  process.exit(0);
}

// --- back up what is there ---------------------------------------------------
//
// Beside the launcher, not inside resources/: Electron resolves resources/app
// and resources/app.asar, and a sibling of those is one mistake away from being
// picked up as the application. One directory per install, named by the moment,
// so "put back the one from before lunch" is a directory name and not a puzzle.
const stamp = new Date().toISOString().replace(/[:.]/g, '-').replace('T', '_').slice(0, 19);
const backupRoot = path.resolve(target, '..', '..', 'launcher_backups');
const backupDir = path.join(backupRoot, stamp);

function copyInto(srcRoot, dstRoot, rels) {
  for (const rel of rels) {
    const src = path.join(srcRoot, rel.split('/').join(path.sep));
    const dst = path.join(dstRoot, rel.split('/').join(path.sep));
    fs.mkdirSync(path.dirname(dst), { recursive: true });
    fs.copyFileSync(src, dst);
  }
}

try {
  const have = listFiles(target);
  fs.mkdirSync(backupDir, { recursive: true });
  copyInto(target, backupDir, have);
  // What it was, written next to it -- a backup nobody can identify is not one.
  fs.writeFileSync(path.join(backupDir, 'BACKUP.json'), JSON.stringify({
    at: new Date().toISOString(),
    replaced_by_version: process.env.INSP_APP_VERSION || '(unknown)',
    files: have.length,
    restore: 'copy the contents of this folder over resources/app, '
           + 'or run ./dev.sh launcher_restore ' + stamp,
  }, null, 2));
  console.log(`backed up ${have.length} file(s) to launcher_backups/${stamp}`);
} catch (e) {
  // No backup, no install. The whole point of the backup is that this step is
  // reversible; doing it without one would be the opposite.
  console.error('could not back up the current launcher, so nothing was replaced: ' + e.message);
  process.exit(1);
}

// --- install -----------------------------------------------------------------
//
// File by file over the top, not a directory swap: the directory is open by the
// process doing the copying, and Windows will not rename it. A file that is
// already loaded is read into memory, so overwriting it is safe -- and is why
// this takes effect at the next start rather than now.
try {
  copyInto(payload, target, want);
  console.log(`installed ${want.length} launcher file(s) -- effective at the next launcher start`);
} catch (e) {
  console.error('installing the launcher code failed: ' + e.message
              + ' -- restore from launcher_backups/' + stamp);
  process.exit(1);
}

// --- prune -------------------------------------------------------------------
try {
  const all = fs.readdirSync(backupRoot, { withFileTypes: true })
    .filter((d) => d.isDirectory()).map((d) => d.name).sort();
  for (const name of all.slice(0, Math.max(0, all.length - KEEP_BACKUPS))) {
    fs.rmSync(path.join(backupRoot, name), { recursive: true, force: true });
    console.log(`removed old backup ${name}`);
  }
} catch (e) {
  // Housekeeping. It failing is not a reason to call the install failed.
  console.log('could not prune old backups: ' + e.message);
}

process.exit(0);
