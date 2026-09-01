#!/usr/bin/env bash
#
# Build everything and lay it out so it can be copied to a machine that has
# nothing installed on it.
#
#   scripts/build_export.sh <dest>                 e.g. scripts/build_export.sh export_v2
#   scripts/build_export.sh <dest> --no-zip        skip the installable package
#   scripts/build_export.sh <dest> --app-only      core + WebUI only, no launcher
#
# The three pieces it builds are Core0_1 (the C++ inspection core), the WebUI
# (the Vite bundle it serves) and the Launcher (the Electron shell that installs
# versions and supervises the core). Nothing else is a deliverable.
#
# The launcher changes far less often than the application and is the slow half
# (npm install + an Electron pack). --app-only is the edit-build-run loop: it
# leaves an already-exported launcher/ in place, so the same folder stays
# runnable. Combine with --no-zip to skip the SHA256 pass over ~250 MB as well.
#
# What comes out:
#
#   <dest>/app/<version>/            the application: core + WebUI + boot.js
#   <dest>/app/update_<ver>_<tgt>.zip
#                                    the same thing, installable and hash-verified
#   <dest>/launcher/Xception INSP-<platform>-<arch>/
#                                    the launcher, with its own Electron runtime
#   <dest>/DEPENDENCIES.txt          what was verified, and what the target
#                                    machine still needs that no file here can
#                                    provide
#
# HOST = TARGET
#
# There is no cross-compilation here. The script builds for the machine it runs
# on, and everything platform-shaped -- which CMake preset, whether the core is
# visSele or visSele.exe, which Electron platform/arch to pack, and how to audit
# the result -- is decided once in the "target" block below. Building a Windows
# package therefore means running this on the Windows build box (MSYS2), exactly
# as before; running it on the Linux machine produces a Linux package.
#
# WHY THE LAUNCHER IS NOT INSIDE app/
#
# app/ is the launcher's "app root": every directory in it is treated as an
# installed VERSION. On start the launcher prunes that directory down to
# keepVersions (UI/Launcher/src/apps.js, prune()), and a directory that is not
# a version is simply one it does not recognise -- so a launcher/ folder in
# there is eligible for deletion BY THE LAUNCHER ITSELF, silently, on some
# later start when keepVersions happens to be small enough. Keeping it one
# level up costs nothing and removes the possibility entirely.
#
# ON A FRESH MACHINE
#
# Windows: the application ships its own MinGW runtime, OpenCV, the HikRobot SDK
# and the MSVC redistributables the camera DLLs need, and they resolve from the
# executable's own directory. Linux: it does NOT -- OpenCV and Aravis come from
# the distribution, because a .so closure copied out of one distro's /usr/lib is
# a worse bet than naming the packages. Either way the launcher ships Electron,
# so neither needs a Node runtime. What a file cannot provide is a KERNEL DRIVER
# -- step 6 checks the first part and DEPENDENCIES.txt spells out the second.

set -euo pipefail

DEST="${1:-}"
if [[ -z "$DEST" || "$DEST" == -* ]]; then
  echo "usage: $0 <dest-folder> [--no-zip] [--app-only]" >&2
  echo "  e.g. $0 export_v2" >&2
  exit 2
fi
shift
WANT_ZIP=1
WANT_LAUNCHER=1
for a in "$@"; do
  case "$a" in
    --no-zip) WANT_ZIP=0 ;;
    --app-only|--no-launcher) WANT_LAUNCHER=0 ;;
    *) echo "unknown option: $a" >&2; exit 2 ;;
  esac
done

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO"

# --- target ------------------------------------------------------------------
#
# Everything that differs between a Windows package and a Linux one is decided
# here, once. Anything below this block that has to ask "which platform?" again
# is a bug in this block.
HOST_OS="$(uname -s)"
HOST_MACHINE="$(uname -m)"
case "$HOST_MACHINE" in
  x86_64|amd64)   ARCH=x64 ;;
  aarch64|arm64)  ARCH=arm64 ;;
  *) echo "unsupported machine: $HOST_MACHINE" >&2; exit 1 ;;
esac

case "$HOST_OS" in
  MINGW*|MSYS*|CYGWIN*)
    TARGET=win
    CORE_PRESET=win-mingw-msys
    CORE_DIST=dist/win            # relative to InspectionCore/
    CORE_EXE=visSele.exe
    PKG_TAG=win                   # names the zip; the field has update_<ver>_win.zip
    ELECTRON_PLATFORM=win32
    # The launcher has always been packed x64 on Windows and the field machines
    # are x64; an arm64 Windows build box would otherwise silently produce a
    # package none of them can run.
    ELECTRON_ARCH=x64
    ;;
  Linux)
    TARGET=linux
    CORE_PRESET=linux
    CORE_DIST="dist/linux-$ARCH"
    CORE_EXE=visSele
    PKG_TAG="linux-$ARCH"
    ELECTRON_PLATFORM=linux
    ELECTRON_ARCH="$ARCH"
    ;;
  Darwin)
    TARGET=mac
    [[ "$ARCH" == arm64 ]] || { echo "only mac-arm64 has a preset; this host is $ARCH" >&2; exit 1; }
    CORE_PRESET=mac-arm64
    CORE_DIST="dist/mac-$ARCH"
    CORE_EXE=visSele
    PKG_TAG="mac-$ARCH"
    ELECTRON_PLATFORM=darwin
    ELECTRON_ARCH="$ARCH"
    ;;
  *) echo "unsupported host: $HOST_OS" >&2; exit 1 ;;
esac

# Prepended, not replaced, so the rest of the environment survives. Forgetting
# to put MSYS2 on PATH is the normal case, and the failure it produces (cmake
# not found, or worse, a DIFFERENT toolchain found) is not obviously about PATH.
# Windows only: on Linux and macOS the compiler is simply on PATH.
if [[ "$TARGET" == win ]]; then
  MINGW_BIN="${MINGW_BIN:-/c/msys64/mingw64/bin}"
  export PATH="$MINGW_BIN:$PATH"
fi

# python3 on a Debian box, python on MSYS2. Resolved once; every later use goes
# through $PY so the script cannot half-work because one of the two exists.
PY="$(command -v python3 || command -v python || true)"
[[ -n "$PY" ]] || { echo "no python3 / python on PATH (needed to make the manifest)" >&2; exit 1; }

VERSION="$("$PY" -c "import json;print(json.load(open('scripts/info.json'))['version'])")"

# Absolute, because build.sh and npm both run from other directories.
mkdir -p "$DEST"
DEST_ABS="$(cd "$DEST" && pwd)"
APP_ROOT="$DEST_ABS/app"
PAYLOAD="$APP_ROOT/$VERSION"
LAUNCHER_OUT="$DEST_ABS/launcher"

say() { printf '\n==> %s\n' "$*"; }

# --- 0. toolchain ------------------------------------------------------------
# Fail here, with a sentence, rather than 200 lines into a cmake run.
say "checking the toolchain"
if [[ "$TARGET" == win ]]; then
  if [[ ! -x "$MINGW_BIN/g++.exe" && ! -x "$MINGW_BIN/g++" ]]; then
    echo "MINGW_BIN=$MINGW_BIN has no g++ -- set MINGW_BIN to your MSYS2 mingw64 bin" >&2
    exit 1
  fi
  echo "    mingw   : $MINGW_BIN"
else
  command -v g++ >/dev/null || { echo "g++ is not on PATH" >&2; exit 1; }
  # OpenCV is a hard dependency of MatchingEngine and Aravis is the camera
  # backend the non-Windows presets turn on. find_package/pkg_check_modules
  # would say so too, but only after a configure run long enough to look like
  # progress -- and the fix is an apt line, not a code change.
  if [[ "$TARGET" == linux ]]; then
    for m in opencv4 aravis-0.8; do
      pkg-config --exists "$m" 2>/dev/null || {
        echo "pkg-config cannot find $m -- the core will not configure." >&2
        echo "  sudo apt install cmake pkg-config libopencv-dev libaravis-dev libusb-1.0-0-dev" >&2
        exit 1; }
    done
    echo "    opencv  : $(pkg-config --modversion opencv4)"
    echo "    aravis  : $(pkg-config --modversion aravis-0.8)"
  fi
fi
for t in cmake npm; do
  command -v "$t" >/dev/null || { echo "$t is not on PATH" >&2; exit 1; }
done
echo "    target  : $TARGET/$ARCH   (cmake preset: $CORE_PRESET)"
echo "    version : $VERSION"
echo "    dest    : $DEST_ABS"

# --- 1. the core (Core0_1) ---------------------------------------------------
say "building the core (visSele + inspd_log)"
( cd InspectionCore && ./build.sh -p "$CORE_PRESET" -e "$CORE_DIST" )

# --- 2. the WebUI ------------------------------------------------------------
say "building the WebUI"
# --legacy-peer-deps: react-drag-sortable@1.0.6 declares a peer of react ^0.14
# || ^15 and this app is on react 16, so npm 7+ refuses the tree outright. The
# package works; the range is stale. Without this the build box cannot install
# at all, which is a worse answer than an old peer range.
#
# --include=dev: vite IS the build, and it is a devDependency. A build box with
# NODE_ENV=production in its environment -- a normal thing for a machine that
# also RUNS things -- makes npm omit dev dependencies silently, and the failure
# lands three lines later as "sh: 1: vite: not found", which reads like a broken
# install rather than a policy npm applied on its own.
( cd UI/WebUI && npm install --include=dev --legacy-peer-deps --no-audit --no-fund \
    && npm run build )

# --- 3. assemble the application ---------------------------------------------
say "assembling $PAYLOAD"
rm -rf "$PAYLOAD"
mkdir -p "$PAYLOAD/Core" "$PAYLOAD/WebUI" "$PAYLOAD/scripts"
cp -r "InspectionCore/$CORE_DIST/." "$PAYLOAD/Core/"
cp -r UI/WebUI/dist/. "$PAYLOAD/WebUI/"
cp scripts/info.json "$PAYLOAD/info.json"
# boot.js is what makes this a launchable version rather than a folder of
# files: it is the only place that knows this application's layout -- the
# executable's name, the chdir argument, the control port, where the UI is.
cp scripts/boot.js "$PAYLOAD/scripts/boot.js"

# boot.js names the executable by platform and the launcher will refuse to start
# a version whose core is not there. Catch that here, where the fix is a build
# argument, rather than on the machine, where it is a re-ship.
[[ -f "$PAYLOAD/Core/$CORE_EXE" ]] || {
  echo "assembled payload has no Core/$CORE_EXE -- the core did not build" >&2
  exit 1; }
[[ -f "$PAYLOAD/WebUI/index.html" ]] || {
  echo "assembled payload has no WebUI/index.html -- the vite build produced nothing" >&2
  exit 1; }

# --- 4. the installable package ----------------------------------------------
if [[ "$WANT_ZIP" == 1 ]]; then
  say "packaging update_${VERSION}_${PKG_TAG}.zip"
  "$PY" UI/Launcher/tools/make_package.py "$PAYLOAD" \
      "$APP_ROOT/update_${VERSION}_${PKG_TAG}.zip" --version="$VERSION"
fi

# --- 5. the launcher ---------------------------------------------------------
if [[ "$WANT_LAUNCHER" == 1 ]]; then
  say "building the launcher ($ELECTRON_PLATFORM-$ELECTRON_ARCH)"
  (
    cd UI/Launcher
    # --include=dev for the same reason as the WebUI: electron and
    # electron-packager are BOTH dev dependencies, so with NODE_ENV=production
    # set this install completes happily and installs nothing at all.
    npm install --include=dev --no-audit --no-fund
    # platform/arch passed here rather than pinned in package.json: the same
    # command has to produce a win32-x64 pack on the Windows box and a
    # linux-arm64 one on the machine, and a hard-coded --arch=x64 in the script
    # quietly produced an unrunnable pack on the second.
    npm run package -- --platform="$ELECTRON_PLATFORM" --arch="$ELECTRON_ARCH"
  )
  # Replaced wholesale rather than copied over: a stale file from a previous
  # Electron version left behind in there is the kind of thing that only fails
  # on the target machine.
  rm -rf "$LAUNCHER_OUT"
  mkdir -p "$LAUNCHER_OUT"
  cp -r UI/Launcher/release-builds/. "$LAUNCHER_OUT/"

  # electron-packager --prune=true prunes node_modules IN PLACE, in the source
  # tree, not in a copy. Every packaging run therefore strips this repo's dev
  # dependencies -- including playwright, which the soak and the launcher test
  # suites need. Put them back, so the next `node tools/soak.mjs` in this
  # checkout works instead of failing with a missing module.
  say "restoring the launcher's dev dependencies (packaging pruned them in place)"
  ( cd UI/Launcher && npm install --include=dev --no-audit --no-fund )
else
  # Deliberately NOT deleted: the point of --app-only is that the folder stays
  # runnable, and on a build machine that has never packaged one there is
  # nothing to keep -- so say which of the two just happened.
  if [[ -d "$LAUNCHER_OUT" ]]; then
    say "skipping the launcher (--app-only); keeping the one already in $LAUNCHER_OUT"
  else
    say "skipping the launcher (--app-only); there is none in $LAUNCHER_OUT yet"
  fi
fi

# The packed app directory is named by electron-packager from productName; read
# it back rather than reconstructing the name, so the RUN IT line at the end
# points at something that exists.
LAUNCHER_APP_DIR=""
if [[ -d "$LAUNCHER_OUT" ]]; then
  LAUNCHER_APP_DIR="$(find "$LAUNCHER_OUT" -maxdepth 1 -mindepth 1 -type d | head -1)"
fi

# --- 6. dependency audit -----------------------------------------------------
#
# The point of this step is the fresh machine. Walk what each shipped executable
# actually loads and report anything that is neither bundled beside it nor
# provided by the operating system. Anything printed here is something that
# works on THIS machine only because it happens to be installed on it -- which
# is exactly the class of problem that only shows up after the hardware is
# already on someone else's bench.
say "auditing dependencies for a machine with nothing installed"

DEPS_TXT="$DEST_ABS/DEPENDENCIES.txt"
set +e
if [[ "$TARGET" == win ]]; then
"$PY" - "$PAYLOAD/Core" "$DEPS_TXT" "$VERSION" "$LAUNCHER_APP_DIR" <<'PY'
import os, re, subprocess, sys

core, out_path, version = sys.argv[1], sys.argv[2], sys.argv[3]
launcher_dir = sys.argv[4] if len(sys.argv) > 4 else ''

# Shipped with every supported Windows; not ours to bundle. Anything NOT on
# this list and not in the payload is a real finding.
SYSTEM = {
    'kernel32.dll','user32.dll','gdi32.dll','advapi32.dll','shell32.dll',
    'ole32.dll','oleaut32.dll','ws2_32.dll','wsock32.dll','winmm.dll',
    'msvcrt.dll','ntdll.dll','rpcrt4.dll','setupapi.dll','iphlpapi.dll',
    'crypt32.dll','secur32.dll','shlwapi.dll','comctl32.dll','comdlg32.dll',
    'version.dll','psapi.dll','dbghelp.dll','imm32.dll','d3d9.dll','d3d11.dll',
    'dxgi.dll','opengl32.dll','glu32.dll','winspool.drv','mpr.dll','netapi32.dll',
    'userenv.dll','wintrust.dll','bcrypt.dll','ncrypt.dll','powrprof.dll',
    'cfgmgr32.dll','devobj.dll','avicap32.dll','msimg32.dll','uxtheme.dll',
    'dwmapi.dll','winhttp.dll','urlmon.dll','wininet.dll','oleacc.dll',
    'propsys.dll','usp10.dll','gdiplus.dll','winusb.dll','ksuser.dll',
}

# Present on a normal Windows desktop, but NOT on the N / KN editions (sold
# without media components) or on Server Core, unless the Media Feature Pack is
# installed. Bundling them is not allowed and not possible, so they are neither
# a packaging bug nor something to wave through in silence: they get their own
# section, and they are the reason a machine can fail to start the camera stack
# on a fresh PC while every file we ship is present and correct.
CONDITIONAL = {
    'avifil32.dll': 'Video for Windows -- Media Feature Pack',
    'msvfw32.dll':  'Video for Windows -- Media Feature Pack',
    'mfplat.dll':   'Media Foundation -- Media Feature Pack',
    'mf.dll':       'Media Foundation -- Media Feature Pack',
}

def is_system(n):
    n = n.lower()
    return (n in SYSTEM or n.startswith('api-ms-win-')
            or n.startswith('ext-ms-win-'))

have = {f.lower(): f for f in os.listdir(core)}
roots = [f for f in os.listdir(core) if f.lower().endswith(('.exe', '.cti'))]

def imports(path):
    try:
        o = subprocess.run(['objdump', '-p', path],
                           capture_output=True, text=True, timeout=60)
    except Exception:
        return []
    return re.findall(r'DLL Name:\s*(\S+)', o.stdout)

seen, missing, conditional = set(), {}, {}
queue = list(roots)
while queue:
    name = queue.pop()
    key = name.lower()
    if key in seen:
        continue
    seen.add(key)
    p = os.path.join(core, have.get(key, name))
    if not os.path.isfile(p):
        continue
    for dep in imports(p):
        dk = dep.lower()
        if dk in have:
            if dk not in seen:
                queue.append(dep)
        elif dk in CONDITIONAL:
            conditional.setdefault(dep, set()).add(name)
        elif not is_system(dep):
            missing.setdefault(dep, set()).add(name)

L = []
L.append("visSele %s -- what the target machine needs (Windows)" % version)
L.append("")
L.append("Generated by scripts/build_export.sh. The BUNDLED and UNRESOLVED")
L.append("sections were checked against the files actually in app/%s/Core/," % version)
L.append("by walking the import table of every .exe and .cti in it.")
L.append("")
L.append("BUNDLED (nothing to install):")
L.append("  MinGW runtime   libstdc++-6, libgcc_s_seh-1, libwinpthread-1, libgomp-1")
L.append("  OpenCV          libopencv_*-413.dll")
L.append("  HikRobot SDK    MvCameraControl + the MvProducer*.cti producers")
L.append("  MSVC redist     msvcr120/msvcp120 for the VC120 camera DLLs, plus")
L.append("                  msvcr100 and the VC90 set for the older ones")
L.append("  Electron        inside launcher/ -- no Node.js install needed")
L.append("")
L.append("%d files scanned, %d reachable modules." % (len(roots), len(seen)))
L.append("")
if missing:
    L.append("UNRESOLVED -- these WILL bite on a fresh machine:")
    for d in sorted(missing):
        L.append("  %-34s needed by %s" % (d, ', '.join(sorted(missing[d]))))
else:
    L.append("UNRESOLVED: none. Every non-system DLL reachable from the shipped")
    L.append("executables and camera producers is present in Core/.")
L.append("")
if conditional:
    L.append("CONDITIONAL -- present on a normal Windows desktop, MISSING on the")
    L.append("N / KN editions and Server Core without the Media Feature Pack:")
    for d in sorted(conditional):
        L.append("  %-34s %s" % (d, CONDITIONAL[d.lower()]))
        L.append("  %-34s needed by %s" % ("", ', '.join(sorted(conditional[d]))))
    L.append("  Install the Media Feature Pack if the camera stack fails to load")
    L.append("  on a machine where every file listed above is present.")
    L.append("")
L.append("NOT SHIPPABLE AS FILES -- do these on the target machine:")
L.append("  1. HikRobot USB (U3V) driver. The .cti producers here are user-space;")
L.append("     the kernel driver comes with the MVS installer. Without it the")
L.append("     camera enumerates as nothing and every part reads NA -- which")
L.append("     looks exactly like a software fault and is not one.")
L.append("  2. The board's USB-serial driver (CH340 / CP210x, whichever the")
L.append("     uInspESP32 board uses). Without it there is no COM port to open.")
L.append("  3. A machine data directory. The launcher's working directory must")
L.append("     contain data/ (machine_setting.json, lens_calib.json, the")
L.append("     recipes). Copy it from a working machine -- the core refuses to")
L.append("     start without it, on purpose.")
L.append("")
L.append("RUN IT:")
L.append("  %s" % (os.path.join(launcher_dir, 'Xception INSP.exe')
                   if launcher_dir else 'launcher/Xception INSP-win32-x64/Xception INSP.exe'))
L.append("  app root    -> <this folder>/app")
L.append("  working dir -> wherever data/ lives")

open(out_path, 'w', encoding='utf-8').write("\n".join(L) + "\n")
start = L.index("BUNDLED (nothing to install):")
print("\n".join("    " + x for x in L[start:]))
sys.exit(1 if missing else 0)
PY
else
"$PY" - "$PAYLOAD/Core" "$DEPS_TXT" "$VERSION" "$TARGET" "$CORE_EXE" "$LAUNCHER_APP_DIR" <<'PY'
import os, re, subprocess, sys

core, out_path, version = sys.argv[1], sys.argv[2], sys.argv[3]
target, core_exe = sys.argv[4], sys.argv[5]
launcher_dir = sys.argv[6] if len(sys.argv) > 6 else ''

# The ELF/Mach-O build does NOT bundle its shared libraries, and that is a
# decision rather than an omission: OpenCV and Aravis come from the
# distribution's packages, which are patched by the distribution and match its
# glibc. Copying one machine's /usr/lib closure into the payload produces a
# bundle that works until the target's glibc is older, and then fails with a
# symbol-version error that says nothing about where it came from.
#
# So the audit's job here is the inverse of the Windows one: not "did we forget
# to copy something", but "name exactly what the target has to install".

def is_exec(p):
    return os.path.isfile(p) and os.access(p, os.X_OK)

roots = sorted(f for f in os.listdir(core) if is_exec(os.path.join(core, f)))
bundled = {f.lower() for f in os.listdir(core)}

# soname -> resolved path, and the "not found" set, which is the only thing
# here that is an outright error.
resolved, notfound, by_root = {}, {}, {}

def deps_linux(path):
    try:
        o = subprocess.run(['ldd', path], capture_output=True, text=True, timeout=60)
    except Exception:
        return []
    out = []
    for line in o.stdout.splitlines():
        line = line.strip()
        m = re.match(r'^(\S+)\s+=>\s+(.*?)(?:\s+\(0x[0-9a-f]+\))?$', line)
        if m:
            out.append((m.group(1), m.group(2).strip()))
        else:
            m = re.match(r'^(\S+)\s+\(0x[0-9a-f]+\)$', line)   # vdso / ld.so
            if m:
                out.append((m.group(1), 'system'))
    return out

def deps_mac(path):
    try:
        o = subprocess.run(['otool', '-L', path], capture_output=True, text=True, timeout=60)
    except Exception:
        return []
    out = []
    for line in o.stdout.splitlines()[1:]:
        line = line.strip()
        if not line or line.endswith(':'):
            continue
        p = line.split(' (')[0].strip()
        out.append((os.path.basename(p), p))
    return out

probe = deps_linux if target == 'linux' else deps_mac

for r in roots:
    for soname, path in probe(os.path.join(core, r)):
        by_root.setdefault(soname, set()).add(r)
        if path == 'not found':
            notfound.setdefault(soname, set()).add(r)
        else:
            resolved[soname] = path

# ldd above walks the ENTIRE transitive closure, which is what you want for
# finding a missing library and exactly what you do not want for an install
# line: OpenCV alone drags in GDAL, HDF5, LAPACK and around 180 packages, and a
# list that long is one nobody reads. The DT_NEEDED entries are the libraries
# our own binaries actually link against -- roughly a dozen -- and apt resolves
# the rest of the closure by itself.
direct = set()
if target == 'linux':
    for r in roots:
        try:
            o = subprocess.run(['objdump', '-p', os.path.join(core, r)],
                               capture_output=True, text=True, timeout=60)
            direct.update(re.findall(r'NEEDED\s+(\S+)', o.stdout))
        except Exception:
            pass
# Fall back to the full closure only if objdump gave us nothing at all, so a
# build box without binutils still gets an answer rather than an empty section.
want = direct if direct else set(resolved)

# Map each library back to the package that owns it, so the answer is an
# install command rather than a list of file names nobody can act on.
packages, unowned = {}, []
if target == 'linux' and subprocess.run(['sh', '-c', 'command -v dpkg-query'],
                                        capture_output=True).returncode == 0:
    for soname in sorted(want):
        path = resolved.get(soname)
        if soname.lower() in bundled or not path or path in ('system', ''):
            continue
        real = os.path.realpath(path)
        o = subprocess.run(['dpkg-query', '-S', real], capture_output=True, text=True)
        if o.returncode == 0 and ':' in o.stdout:
            pkg = o.stdout.split(':')[0].strip()
            packages.setdefault(pkg, []).append(soname)
        else:
            unowned.append(soname)

L = []
L.append("visSele %s -- what the target machine needs (%s)" % (version, target))
L.append("")
L.append("Generated by scripts/build_export.sh, by resolving the shared-library")
L.append("dependencies of every executable actually in app/%s/Core/." % version)
L.append("")
L.append("%d executables scanned; %d libraries linked directly, %d in the full"
         % (len(roots), len(want), len(by_root)))
L.append("transitive closure.")
L.append("  " + ", ".join(roots))
L.append("")
L.append("BUNDLED (nothing to install):")
L.append("  the core        Core/%s and its siblings, built on this machine" % core_exe)
L.append("  the WebUI       WebUI/ -- a static bundle, served from disk")
L.append("  Electron        inside launcher/ -- no Node.js install needed")
L.append("")
if notfound:
    L.append("UNRESOLVED -- the dynamic loader could not find these HERE, so they")
    L.append("will not be found on the target either:")
    for d in sorted(notfound):
        L.append("  %-38s needed by %s" % (d, ', '.join(sorted(notfound[d]))))
    L.append("")
if packages:
    L.append("REQUIRED PACKAGES -- install these on the target machine:")
    L.append("")
    L.append("  sudo apt install " + " ".join(sorted(packages)))
    L.append("")
    L.append("  These are what the binaries link against directly; apt pulls the")
    L.append("  rest of the closure (GDAL, HDF5, LAPACK and the rest of OpenCV's")
    L.append("  own dependencies) in behind them.")
    L.append("")
    L.append("  (what each one provides, from the actual link:)")
    for pkg in sorted(packages):
        libs = sorted(set(packages[pkg]))
        head = libs[:4]
        L.append("  %-34s %s%s" % (pkg, ", ".join(head),
                                   " (+%d more)" % (len(libs) - len(head)) if len(libs) > len(head) else ""))
    L.append("")
if unowned:
    L.append("NOT OWNED BY ANY PACKAGE -- these resolved to files no package")
    L.append("claims, so they came from a local build and will be absent on a")
    L.append("fresh machine:")
    for d in sorted(set(unowned)):
        L.append("  %s" % d)
    L.append("")
L.append("NOT SHIPPABLE AS FILES -- do these on the target machine:")
if target == 'linux':
    L.append("  1. Camera access. This build talks to the camera through Aravis")
    L.append("     (GigE Vision); the HikRobot and MindVision SDKs are Windows-")
    L.append("     only and are OFF in this preset. A GigE camera needs its")
    L.append("     interface on the same subnet and jumbo frames enabled, and a")
    L.append("     USB3 Vision one needs a udev rule granting the user access.")
    L.append("     Without either, the camera enumerates as nothing and every")
    L.append("     part reads NA -- which looks exactly like a software fault")
    L.append("     and is not one.")
    L.append("  2. Serial access to the uInspESP32 board. The user running the")
    L.append("     launcher must be in the dialout group, or /dev/ttyUSB* cannot")
    L.append("     be opened and there is no peripheral link at all.")
else:
    L.append("  1. Camera access via Aravis, as configured for this bench.")
    L.append("  2. Serial access to the uInspESP32 board.")
L.append("  3. A machine data directory. The launcher's working directory must")
L.append("     contain data/ (machine_setting.json, lens_calib.json, the")
L.append("     recipes). Copy it from a working machine -- the core refuses to")
L.append("     start without it, on purpose.")
L.append("")
L.append("RUN IT:")
L.append("  %s" % (os.path.join(launcher_dir, 'Xception INSP')
                   if launcher_dir else 'launcher/<packed app>/Xception INSP'))
L.append("  app root    -> <this folder>/app")
L.append("  working dir -> wherever data/ lives")

open(out_path, 'w', encoding='utf-8').write("\n".join(L) + "\n")
start = L.index("BUNDLED (nothing to install):")
print("\n".join("    " + x for x in L[start:]))
sys.exit(1 if notfound else 0)
PY
fi
AUDIT=$?
set -e

# --- 7. done -----------------------------------------------------------------
say "done"
echo "    $DEST_ABS"
echo "      app/$VERSION/                   the application"
if [[ "$WANT_ZIP" == 1 ]]; then
  echo "      app/update_${VERSION}_${PKG_TAG}.zip   installable, hash-verified"
fi
if [[ "$WANT_LAUNCHER" == 1 ]]; then
  echo "      launcher/                       the launcher (bundled Electron)"
elif [[ -d "$LAUNCHER_OUT" ]]; then
  echo "      launcher/                       unchanged (--app-only)"
else
  echo "      launcher/                       NOT BUILT (--app-only) -- run without it to ship"
fi
echo "      DEPENDENCIES.txt                read this before copying to a new machine"
echo ""
if [[ -n "$LAUNCHER_APP_DIR" ]]; then
  if [[ "$TARGET" == win ]]; then
    LAUNCHER_EXE="$LAUNCHER_APP_DIR/Xception INSP.exe"
  else
    LAUNCHER_EXE="$LAUNCHER_APP_DIR/Xception INSP"
  fi
  echo "    copy the whole of $DEST to the target machine, then run"
  echo "      $LAUNCHER_EXE"
  echo "    and point its app root at the copied app/ folder."
else
  # Saying "copy this and run the launcher" when no launcher was built is how
  # someone ends up shipping half an application.
  echo "    app/ only. To ship this folder, run again without --app-only so a"
  echo "    launcher is built alongside it -- or drop app/$VERSION into the app"
  echo "    root of a machine that already has one."
fi

if [[ "$AUDIT" != 0 ]]; then
  echo ""
  echo "    WARNING: unresolved libraries -- see DEPENDENCIES.txt" >&2
  exit 1
fi
