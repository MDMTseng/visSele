#!/usr/bin/env python3
"""Build an update package the new launcher will accept.

    python make_package.py <src_dir> <out.zip> [--version X.Y.Z]

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


def walk(root):
    out = []
    for dirpath, dirnames, filenames in os.walk(root):
        # Nothing good comes of shipping these, and they make the manifest
        # differ between two builds of identical content.
        dirnames[:] = [d for d in dirnames if d not in (".git", "__pycache__", ".DS_Store")]
        for name in filenames:
            if name in (".DS_Store", "manifest.json"):
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

    out = os.path.abspath(args.out_zip)
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    total = 0
    # ZIP_DEFLATED, not stored: these travel on USB sticks.
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for rel in files + ["manifest.json"]:
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
    print(f"  {len(files)} files + manifest, {total / 1048576:.1f} MB -> "
          f"{os.path.getsize(out) / 1048576:.1f} MB")
    print(f"  package sha256 {sha256(out)}")


if __name__ == "__main__":
    main()
