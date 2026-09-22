#!/usr/bin/env python3
"""Build an update package the new launcher will accept.

    python make_package.py <src_dir> <out.zip> [--version X.Y.Z]
                          [--base-package PREVIOUS.zip]
                          [--based-on VERSION --base-dir DIR]

--base-package makes a DELTA package against a package that was built earlier:
the manifest still describes the complete version, but only the files that
differ from that one are carried in the zip. It reads the previous package's
own manifest.json -- hashes, not contents -- so the base it is measured against
is the zip that was actually shipped, which is sitting in the update folder
already. Nothing else has to be kept.

(--based-on/--base-dir does the same against an unpacked version folder, for
when the package is gone but an installation of it is not.)

It is worth doing because the application does not change: of 238 MB, some
29 MB is the core, its symbols and the WebUI, and the rest is vendor runtime
that moves once a year. A delta is about a tenth of the size.

THE SECOND FIELD OF THE VERSION IS THE LIB GENERATION. 2.0.4 and 2.0.7 ship the
same vendor runtime; 2.1.0 says it changed. A delta records the GENERATION and
not the base it was built against, so the launcher can take the files it did not
carry from ANY 2.0.x the machine has -- one that skipped four updates included.
Each of those files is hashed against this manifest before it is used, so the
generation is a place to look and never something taken on trust.

Which means: BUMP THE SECOND FIELD WHENEVER A FILE OUTSIDE THE APPLICATION
CHANGES. Getting it wrong cannot ship a machine made of mismatched halves --
the hashes fail and the install stops -- but it does produce a delta that
nobody can install.

The launcher assembles the carried files and the reused ones into a directory
and then verifies every file in it against the manifest, so what gets installed
is a complete, independently checked version, exactly as a full package
produces. Nothing downstream knows the difference.

It is NOT a substitute for a full package. A machine with no version of the
generation cannot use one, so the fleet always needs a full package to land on;
point release.json at that, and use deltas for the machines already on it.

<src_dir> is an assembled application folder. Only two entries are required,
because they are the only two the launcher requires:

    info.json          the version string; --version overwrites it
    scripts/boot.js    how this version starts -- executables, arguments,
                       working directory, control channel, UI location

Everything else in the folder is whatever THIS application happens to consist
of. The launcher has no opinion about it and neither does this script.

The package carries a manifest.json listing every file with its SHA256. The
launcher refuses a package with no manifest, a file missing from it, OR a file
present on disk that the manifest does not list -- an unlisted file is a file
nobody hashed, and a manifest covering only part of a package would let
anything ride along in the uncovered part.

Written to be run by hand as well as from the Makefile: servicing a machine in
the field should not require the whole build environment, only this file and a
directory someone trusts.
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import zipfile

# Exactly what the launcher requires -- REQUIRED_ENTRIES in src/apps.js -- and
# nothing else.
#
# This list used to demand Core/visSele.exe and WebUI/index.html, which was
# wrong in both directions. A package with no scripts/boot.js sailed through
# here and was then REFUSED at install, moving the failure from the bench to the
# machine; and a headless build with no WebUI could not be packaged at all even
# though the launcher runs one happily.
#
# The launcher knows nothing about Core/ or WebUI/ -- that is boot.js's business
# now -- so neither does the tool that builds its packages. Two files: a version
# to name the directory by, and a description of how to start it.
REQUIRED_ALL = [
    "info.json",
    "scripts/boot.js",
]


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# Directories and files that are not part of the application.
#
# crashlog/ is the big one. The app folder on a dev bench IS a running
# installation -- the core is started in it, so its crash evidence accumulates
# inside it -- and 38 MB of one bench's minidumps was going out to every machine
# on the fleet, in a package of 274 MB. Somebody else's crash dumps are not
# merely wasted bytes: they are confusing, they are named after a pid that means
# nothing here, and they get read as if they described THIS machine.
#
# Excluded here, in the packager, rather than cleaned up before packing: a
# cleanup step is one that can be forgotten, and this one had been.
SKIP_DIRS = (".git", "__pycache__", ".DS_Store", "crashlog", "crash_reports")
SKIP_SUFFIXES = (".dmp", ".dump")


def lib_family(version):
    """major.lib -- the generation of vendor runtime a build was made against.

    Kept identical to libFamily() in the launcher's apps.js. Two fields, so
    2.0.7 and 2.0.4-rc1 are the same generation and 2.1.0 is not.
    """
    p = str(version).split(".")
    return ".".join(p[:2]) if len(p) >= 2 else str(version)


def read_base_package(zip_path):
    """The version and per-file hashes recorded in a package built earlier.

    A package carries the manifest it was verified against, so the previous zip
    is a complete and self-describing statement of what the machines running
    that version have on disk -- which is the only thing a delta needs. Reading
    it costs nothing: the manifest is a few hundred KB inside a 90 MB file and
    it is the only member that is read.
    """
    if not os.path.isfile(zip_path):
        sys.exit(f"--base-package: no such file {zip_path}")
    with zipfile.ZipFile(zip_path) as z:
        names = [n for n in z.namelist() if n.endswith("manifest.json")]
        if not names:
            sys.exit(f"--base-package: {zip_path} has no manifest.json -- not a package")
        m = json.loads(z.read(min(names, key=len)))
    if not isinstance(m.get("files"), dict) or not isinstance(m.get("version"), str):
        sys.exit(f"--base-package: {zip_path}'s manifest has no version/files")
    if m.get("libBase"):
        # A delta's manifest lists every file of the complete version, so this
        # would in fact produce a correct package -- but it would be a delta
        # onto a delta, and a machine can only apply it if it applied the other
        # one first. That chain is a thing to get wrong in the field, for no
        # gain over pointing both at the same full package.
        sys.exit(f"--base-package: {zip_path} is itself a delta (lib base {m['libBase']}); "
                 "measure against a FULL package, so what it says is on the machine "
                 "is what somebody actually put there")
    return m["version"], m["files"]


def _hash_if_present(path):
    """The base version's hash of a file, or None if it does not have one.

    None never equals a real hash, so a file the base does not have is always
    carried -- which is what makes adding a file to a version work.
    """
    return sha256(path) if os.path.isfile(path) else None


def walk(root):
    out = []
    for dirpath, dirnames, filenames in os.walk(root):
        # Nothing good comes of shipping these, and they make the manifest
        # differ between two builds of identical content.
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for name in filenames:
            if name in (".DS_Store", "manifest.json"):
                continue
            if name.endswith(SKIP_SUFFIXES):
                continue
            full = os.path.join(dirpath, name)
            rel = os.path.relpath(full, root).replace(os.sep, "/")
            out.append(rel)
    out.sort()
    return out


def check_boot_js(path):
    """Catch a broken boot.js here rather than on the machine.

    A syntax error or a missing describe() is otherwise not discovered until
    someone selects the version and presses start -- which is both the worst
    time and the worst place. This is the cheapest possible check that the file
    is what it claims to be; the launcher does the full validation when it
    actually loads it.

    Skipped, with a warning, if node is not available: this script is meant to
    be runnable in the field with nothing but python.
    """
    node = shutil.which("node")
    if node is None:
        print("  ! node not found -- skipping the boot.js sanity check")
        return
    # The probe reports its OWN verdict on one line, prefixed, rather than
    # letting node's uncaught-exception output be parsed out here. Guessing
    # which of node's stderr lines is the reason does not work: the last line is
    # its version banner, and the source line it echoes back often contains the
    # word "Error" itself, so both obvious heuristics pick the wrong line.
    probe = (
        "try{"
        "const m=require(process.argv[1]);"
        "if(!m||typeof m.describe!=='function')throw new Error('it does not export describe(ctx)');"
        "if(!Number.isInteger(m.apiVersion))throw new Error('it has no integer apiVersion');"
        "process.stdout.write('OK '+m.apiVersion);"
        "}catch(e){process.stderr.write('BOOTCHK '+(e&&e.message||e));process.exit(1);}"
    )
    r = subprocess.run([node, "-e", probe, os.path.abspath(path)],
                       capture_output=True, text=True)
    if r.returncode != 0 or not r.stdout.startswith("OK "):
        marked = [l for l in r.stderr.splitlines() if l.startswith("BOOTCHK ")]
        why = marked[0][len("BOOTCHK "):] if marked else (r.stderr.strip() or "unknown error")
        sys.exit("scripts/boot.js is not usable:" + os.linesep + "  " + why)
    print(f"  boot.js ok (apiVersion {r.stdout[3:].strip()})")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src_dir")
    ap.add_argument("out_zip")
    ap.add_argument("--version", default=None,
                    help="overwrite info.json's version before hashing")
    ap.add_argument("--base-package", default=None, metavar="ZIP",
                    help="carry only what differs from this previously built package")
    ap.add_argument("--based-on", default=None, metavar="VERSION",
                    help="carry only the files that differ from this installed version")
    ap.add_argument("--base-dir", default=None, metavar="DIR",
                    help="where that version's folder is (default: beside src_dir)")
    args = ap.parse_args()

    src = os.path.abspath(args.src_dir)
    if not os.path.isdir(src):
        sys.exit(f"not a directory: {src}")

    info_path = os.path.join(src, "info.json")
    if not os.path.exists(info_path):
        sys.exit(f"missing {info_path}")

    with open(info_path, encoding="utf-8") as f:
        info = json.load(f)
    if args.version:
        info["version"] = args.version
        # Rewritten BEFORE hashing, or the manifest would describe a file that
        # no longer exists by the time it is verified.
        with open(info_path, "w", encoding="utf-8") as f:
            json.dump(info, f, indent=2)

    version = info.get("version")
    if not isinstance(version, str) or not version:
        sys.exit("info.json has no version string")
    # The launcher uses this as a directory name.
    if not all(c.isalnum() or c in "._-" for c in version):
        sys.exit(f'version "{version}" is not usable as a folder name')

    missing = [r for r in REQUIRED_ALL if not os.path.exists(os.path.join(src, r))]
    if missing:
        sys.exit("source tree cannot be started by the launcher; missing: " + ", ".join(missing))

    check_boot_js(os.path.join(src, "scripts", "boot.js"))

    files = walk(src)
    print(f"version {version}: hashing {len(files)} files...")
    manifest = {
        "version": version,
        "file_count": len(files),
        "files": {rel: sha256(os.path.join(src, rel)) for rel in files},
    }
    manifest_path = os.path.join(src, "manifest.json")
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=1, sort_keys=True)

    # --- which files actually have to travel ---------------------------------
    #
    # A full package carries all of them. A delta carries the ones whose hash
    # differs from the base version, and the launcher takes the rest from its
    # own copy of that version -- which it then hashes against this manifest
    # like everything else, so a base that has been tampered with or has rotted
    # on disk fails the install rather than being trusted because it was
    # already there.
    carried = files
    base_hashes, based_on = None, args.based_on
    if args.base_package:
        if args.based_on:
            sys.exit("--base-package and --based-on name the same thing twice; use one")
        based_on, base_hashes = read_base_package(args.base_package)
    elif args.based_on:
        base = os.path.abspath(args.base_dir or os.path.join(os.path.dirname(src), args.based_on))
        if not os.path.isdir(base):
            sys.exit(f"--based-on {args.based_on}: no such folder {base}")
        base_hashes = {rel: _hash_if_present(os.path.join(base, rel)) for rel in files}

    if based_on:
        if lib_family(based_on) != lib_family(version):
            sys.exit(
                f"the base is {based_on} and this is {version}: different lib generations "
                f"({lib_family(based_on)} vs {lib_family(version)}). The second field says the "
                "vendor runtime changed, so none of it can be reused -- ship a full package.")
        if based_on == version:
            sys.exit(f"the base is {based_on}, the same version as this package; "
                     "a delta onto itself installs nothing")
        # REQUIRED_ALL always travels, whether or not it changed. info.json is
        # what the launcher finds the package by (findRoot), and boot.js is the
        # one file it will execute -- neither should be able to go missing from
        # a package because a rebuild happened to leave it byte-identical.
        keep = set(REQUIRED_ALL)
        carried = [rel for rel in files
                   if rel in keep
                   or manifest["files"][rel] != base_hashes.get(rel)]
        # The GENERATION is what travels, not the base this happened to be built
        # against: the launcher takes what it needs from any version of it, so a
        # machine four updates behind is not shut out by a name it never had.
        manifest["libBase"] = lib_family(version)
        manifest["carried"] = sorted(carried)
        # Rewritten: libBase and carried were added after it was first dumped.
        with open(manifest_path, "w", encoding="utf-8") as f:
            json.dump(manifest, f, indent=1, sort_keys=True)
        print(f"  delta against {based_on}, lib base {lib_family(version)}: "
              f"carrying {len(carried)} of {len(files)} files")

    out = os.path.abspath(args.out_zip)
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    total = 0
    # ZIP_DEFLATED, not stored: these travel on USB sticks.
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for rel in carried + ["manifest.json"]:
            full = os.path.join(src, rel)
            z.write(full, rel)
            total += os.path.getsize(full)

    # The manifest belongs INSIDE the zip, not in the source tree. Leaving it
    # behind means the assembled app folder gains a file that goes stale the
    # moment anything is rebuilt -- and a manifest that no longer matches the
    # files beside it is worse than none, because it looks like a guarantee.
    # Nothing verifies it there: the launcher only checks manifests at install
    # time, out of the package.
    os.remove(manifest_path)

    print(f"wrote {out}")
    print(f"  {len(carried)} files + manifest, {total / 1048576:.1f} MB -> "
          f"{os.path.getsize(out) / 1048576:.1f} MB")
    if based_on:
        print(f"  describes {len(files)} files; the other {len(files) - len(carried)} "
              f"come from any {lib_family(version)}.x on the machine")
    print(f"  package sha256 {sha256(out)}")


if __name__ == "__main__":
    main()
