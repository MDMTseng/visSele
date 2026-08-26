// Install an application version from a LOCAL zip -- a file on disk or on a
// USB stick.
//
// There is no download step and no network. That was the decision: a
// production machine should not need internet to be serviced, and removing the
// network removes the whole class of failures around it. If a remote channel is
// ever wanted, it fetches a file and then calls install() -- the verification
// and swap below do not change.
//
// The order matters more than any single check:
//
//   extract to .staging -> verify hashes -> verify structure -> rename into
//   place -> (separately, later) write current.json
//
// Everything before the rename is reversible by deleting one directory, and the
// running version has not been touched. The rename is same-volume so it is
// near-atomic, and the pointer write is a single small file. There is no point
// at which a half-installed version can be selected.
//
// Installing does NOT switch. That is a second, explicit act -- see
// AppStore.setCurrent. An update that took effect the moment it finished
// copying would leave no moment at which an operator could decide not to.
//
// NOTHING HERE TOUCHES THE MACHINE'S WORKING DIRECTORY. Not to read it, not to
// seed it, not to back it up. The calibration and recipe in there belong to the
// machine and outlive every version; an installer that writes to them is an
// installer that can destroy them.
'use strict';

const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
const { execFile } = require('node:child_process');

const { AppStore, STAGING, REPLACED, REQUIRED_ENTRIES, INFO } = require('./apps');

// Zip extraction with no npm dependency. The old launcher pulled in `unzipper`
// and its tree for this one operation; the operating system already ships
// something that does it.
//
// The extractor is named by ABSOLUTE PATH on Windows, never as bare `tar`.
// Windows 10 1803+ ships bsdtar at System32/tar.exe, which reads zip -- but
// PATH may well find a different tar first. On this machine MSYS2's GNU tar
// won, and GNU tar cannot read zip at all: "this does not look like a tar
// archive", on a perfectly good package. Electron inherits the user's PATH, so
// that is not a developer-only accident.
function winTar() {
  const root = process.env.SystemRoot || path.join('C:', path.sep, 'Windows');
  const p = path.join(root, 'System32', 'tar.exe');
  return fs.existsSync(p) ? p : null;
}

function run(cmd, args, opts) {
  return new Promise((resolve, reject) => {
    execFile(cmd, args, { windowsHide: true, ...opts }, (err, stdout, stderr) => {
      if (err) reject(new Error((stderr || err.message).trim()));
      else resolve(stdout);
    });
  });
}

async function extractZip(zipPath, destDir) {
  const abs = path.resolve(zipPath);
  const bsdtar = process.platform === 'win32' ? winTar() : 'tar';
  if (bsdtar) {
    try {
      // Named by BASENAME with cwd set to its folder. bsdtar reads an -f
      // argument containing a colon as a remote `host:path` spec, so a Windows
      // absolute path fails with "Cannot connect to C: resolve failed". -C does
      // not go through that parser, so the destination stays absolute.
      await run(bsdtar, ['-xf', path.basename(abs), '-C', destDir], { cwd: path.dirname(abs) });
      return;
    } catch (e) {
      if (process.platform !== 'win32') throw new Error(`extract failed: ${e.message}`);
      // fall through to PowerShell
    }
  }
  if (process.platform === 'win32') {
    // Older Windows, or a bsdtar that refused the archive. Slower, but always
    // present.
    const q = (s) => s.replace(/'/g, "''");
    await run('powershell', ['-NoProfile', '-NonInteractive', '-Command',
      `Expand-Archive -LiteralPath '${q(abs)}' -DestinationPath '${q(destDir)}' -Force`])
      .catch((e) => { throw new Error(`extract failed: ${e.message}`); });
    return;
  }
  throw new Error('extract failed: no usable zip extractor found');
}

function sha256File(file) {
  return new Promise((resolve, reject) => {
    const h = crypto.createHash('sha256');
    const s = fs.createReadStream(file);
    s.on('data', (d) => h.update(d));
    s.on('error', reject);
    s.on('end', () => resolve(h.digest('hex')));
  });
}

function walk(dir, base = dir, out = []) {
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, e.name);
    if (e.isDirectory()) walk(full, base, out);
    else out.push(path.relative(base, full).split(path.sep).join('/'));
  }
  return out;
}

// A zip may or may not wrap everything in a single top-level folder depending
// on how it was made. Accept both rather than making the packaging script the
// only thing that can produce a valid package.
function findRoot(stagingDir) {
  if (fs.existsSync(path.join(stagingDir, INFO))) return stagingDir;
  const entries = fs.readdirSync(stagingDir, { withFileTypes: true }).filter((d) => d.isDirectory());
  if (entries.length === 1) {
    const inner = path.join(stagingDir, entries[0].name);
    if (fs.existsSync(path.join(inner, INFO))) return inner;
  }
  return null;
}

class Updater {
  constructor(cfg, apps) {
    this.cfg = cfg;
    this.apps = apps || new AppStore(cfg);
  }

  // onLog is called with human-readable progress; it is what the shell shows.
  async install(zipPath, onLog = () => {}) {
    const log = (m) => { onLog(m); return m; };

    if (!fs.existsSync(zipPath)) throw new Error(`no such file: ${zipPath}`);
    const zipSize = fs.statSync(zipPath).size;
    log(`package: ${zipPath} (${(zipSize / 1048576).toFixed(1)} MB)`);
    log(`package sha256: ${await sha256File(zipPath)}`);

    const staging = path.join(this.apps.dir, STAGING);
    fs.rmSync(staging, { recursive: true, force: true });
    fs.mkdirSync(staging, { recursive: true });

    try {
      log('extracting...');
      await extractZip(zipPath, staging);

      const root = findRoot(staging);
      if (!root) throw new Error(`no ${INFO} at the top of the package -- is this an application update?`);

      // --- version ---
      let info;
      try { info = JSON.parse(fs.readFileSync(path.join(root, INFO), 'utf8')); }
      catch (e) { throw new Error(`${INFO} is not valid JSON: ${e.message}`); }
      if (typeof info.version !== 'string' || !info.version.length) {
        throw new Error(`${INFO} has no version string`);
      }
      // Used as a directory name, so it must not be able to escape the app
      // directory or collide with the staging name.
      // A leading dot is refused as well: list() treats dot-directories as the
      // launcher's own machinery, so a version called ".1" would install and
      // then be invisible.
      if (!/^[A-Za-z0-9._-]+$/.test(info.version) || info.version.startsWith('.') || info.version === STAGING
          || info.version === '.' || info.version === '..') {
        throw new Error(`${INFO} version "${info.version}" is not usable as a folder name`);
      }
      log(`version: ${info.version}`);

      // --- integrity ---
      //
      // Per-file SHA256 against a manifest carried inside the package. This
      // catches a truncated copy, a bad USB stick, a partially written file --
      // the things that actually happen when packages travel on removable
      // media. It does NOT prove authorship; that needs a signature, and
      // verifySignature() below is where that goes when it is wanted.
      //
      // It also earns the right to run scripts/boot.js later: the launcher DOES
      // execute code out of an installed version, and this is what makes that
      // verify-then-execute rather than the old execute-and-hope.
      const manifestPath = path.join(root, 'manifest.json');
      if (!fs.existsSync(manifestPath)) {
        throw new Error('manifest.json is missing -- refusing to install an unverifiable package');
      }
      const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
      if (!manifest.files || typeof manifest.files !== 'object') {
        throw new Error('manifest.json has no files map');
      }
      if (manifest.version !== info.version) {
        throw new Error(`manifest says ${manifest.version} but ${INFO} says ${info.version}`);
      }

      const onDisk = walk(root).filter((f) => f !== 'manifest.json');
      const listed = Object.keys(manifest.files);
      const notListed = onDisk.filter((f) => !(f in manifest.files));
      const missing = listed.filter((f) => !fs.existsSync(path.join(root, f)));
      if (missing.length) throw new Error(`package is missing ${missing.length} listed file(s): ${missing.slice(0, 5).join(', ')}`);
      // An unlisted file is a file nobody hashed. Refusing it is the whole point
      // of verifying: a manifest that only covers part of the package would let
      // anything ride along in the uncovered part -- including, now, a
      // scripts/boot.js that the launcher is going to execute.
      if (notListed.length) throw new Error(`package has ${notListed.length} file(s) not in the manifest: ${notListed.slice(0, 5).join(', ')}`);

      log(`verifying ${listed.length} files...`);
      let checked = 0;
      for (const rel of listed) {
        const actual = await sha256File(path.join(root, rel));
        if (actual !== manifest.files[rel]) throw new Error(`checksum mismatch on ${rel}`);
        if (++checked % 200 === 0) log(`  ${checked}/${listed.length}`);
      }
      log(`all ${listed.length} files verified`);

      await this.verifySignature(root, manifest, log);

      // --- structure ---
      //
      // Two entries, and only two: info.json so the version has a name, and
      // scripts/boot.js so the launcher can ask how to start it. Everything
      // else about the layout is boot.js's business.
      const missingReq = REQUIRED_ENTRIES.filter((rel) => !fs.existsSync(path.join(root, rel)));
      if (missingReq.length) throw new Error(`package cannot be started: missing ${missingReq.join(', ')}`);

      // --- install ---
      const dest = this.apps.versionDir(info.version);

      // Replacing an existing version used to be rmSync(dest) then
      // rename(root, dest). Those are two steps with a gap between them, and
      // in that gap the old version no longer exists. If the rename then fails
      // -- an antivirus holding a handle, a permission change, a full disk --
      // the finally below wipes staging and the machine is left with NEITHER
      // version. An update that can delete a working install is worse than an
      // update that fails.
      //
      // So: move the old one aside, put the new one in, and only then delete
      // what was moved. Every step is a rename until the last, and the last is
      // a delete of something nothing points at.
      let displaced = null;
      if (fs.existsSync(dest)) {
        log(`replacing existing ${info.version}`);
        if (this.apps.currentVersion() === info.version) {
          // Overwriting the selected version would pull files out from under a
          // running core.
          throw new Error(`${info.version} is the version currently selected -- select another version first, or bump the version`);
        }
        displaced = path.join(this.apps.dir, REPLACED, `${info.version}-${Date.now()}`);
        fs.mkdirSync(path.dirname(displaced), { recursive: true });
        fs.renameSync(dest, displaced);
      }
      fs.mkdirSync(path.dirname(dest), { recursive: true });
      try {
        fs.renameSync(root, dest);
      } catch (e) {
        if (displaced) {
          // Put it back. If even this fails the old version is still intact at
          // `displaced`, so say where it is rather than hide it.
          try {
            fs.renameSync(displaced, dest);
            log(`install failed; restored the previous ${info.version}`);
          } catch {
            log(`install failed AND could not restore -- the previous ${info.version} is at ${displaced}`);
          }
        }
        throw e;
      }
      log(`installed to ${dest}`);
      if (displaced) fs.rmSync(displaced, { recursive: true, force: true });

      return { version: info.version, dir: dest };
    } finally {
      // Whatever happened, the staging area goes. On success `root` was renamed
      // out of it and this removes the empty shell; on failure it removes the
      // whole rejected package.
      fs.rmSync(staging, { recursive: true, force: true });
    }
  }

  // Hook, deliberately left as a no-op.
  //
  // Per-file hashes prove the package arrived intact; they prove nothing about
  // who made it, because anyone who can replace the files can replace the
  // manifest too. Closing that needs a detached signature over manifest.json
  // verified against a public key compiled into the launcher. That is a real
  // decision (key custody, rotation, what happens to packages already in the
  // field) and is not worth guessing at -- so the seam is here, empty and
  // named, rather than absent.
  //
  // It matters more now than it did: the launcher runs scripts/boot.js out of
  // an installed version, so "the package is intact" and "the package is ours"
  // have stopped being the same question.
  async verifySignature(rootDir, manifest, log) {
    const sig = path.join(rootDir, 'manifest.sig');
    if (fs.existsSync(sig)) {
      log('manifest.sig present but signature checking is not enabled in this build');
    }
  }

  select(version) { return this.apps.setCurrent(version); }
}

module.exports = { Updater, sha256File, walk };
