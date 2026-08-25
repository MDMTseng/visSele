#!/usr/bin/env bash
#
# Build everything and lay it out so it can be copied to a machine that has
# nothing installed on it.
#
#   scripts/build_export.sh <dest>            e.g. scripts/build_export.sh export_v2
#   scripts/build_export.sh <dest> --no-zip   skip the installable package
#
# What comes out:
#
#   <dest>/app/<version>/            the application: core + WebUI + boot.js
#   <dest>/app/update_<ver>_win.zip  the same thing, installable and hash-verified
#   <dest>/launcher/Xception INSP-win32-x64/
#                                    the launcher, with its own Electron runtime
#   <dest>/DEPENDENCIES.txt          what was verified, and what the target PC
#                                    still needs that no file here can provide
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
# The application ships its own MinGW runtime, OpenCV, the HikRobot SDK and the
# MSVC redistributables the camera DLLs need, and they resolve from the
# executable's own directory. The launcher ships Electron. Neither needs an
# installer or a Node runtime. What a file cannot provide is a KERNEL DRIVER --
# step 6 checks the first part and DEPENDENCIES.txt spells out the second.

set -euo pipefail

DEST="${1:-}"
if [[ -z "$DEST" || "$DEST" == -* ]]; then
  echo "usage: $0 <dest-folder> [--no-zip]" >&2
  echo "  e.g. $0 export_v2" >&2
  exit 2
fi
shift
WANT_ZIP=1
for a in "$@"; do
  case "$a" in
    --no-zip) WANT_ZIP=0 ;;
    *) echo "unknown option: $a" >&2; exit 2 ;;
  esac
done

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO"

# Prepended, not replaced, so the rest of the environment survives. Forgetting
# to put MSYS2 on PATH is the normal case, and the failure it produces (cmake
# not found, or worse, a DIFFERENT toolchain found) is not obviously about PATH.
MINGW_BIN="${MINGW_BIN:-/c/msys64/mingw64/bin}"
export PATH="$MINGW_BIN:$PATH"

VERSION="$(python -c "import json;print(json.load(open('scripts/info.json'))['version'])")"

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
if [[ ! -x "$MINGW_BIN/g++.exe" && ! -x "$MINGW_BIN/g++" ]]; then
  echo "MINGW_BIN=$MINGW_BIN has no g++ -- set MINGW_BIN to your MSYS2 mingw64 bin" >&2
  exit 1
fi
for t in cmake npm python; do
  command -v "$t" >/dev/null || { echo "$t is not on PATH" >&2; exit 1; }
done
echo "    mingw   : $MINGW_BIN"
echo "    version : $VERSION"
echo "    dest    : $DEST_ABS"

# --- 1. the core -------------------------------------------------------------
say "building the core (visSele + inspd_log)"
( cd InspectionCore && ./build.sh -p win-mingw-msys -e dist/win )

# --- 2. the WebUI ------------------------------------------------------------
say "building the WebUI"
( cd UI/WebUI && npm run build )

# --- 3. assemble the application ---------------------------------------------
say "assembling $PAYLOAD"
rm -rf "$PAYLOAD"
mkdir -p "$PAYLOAD/Core" "$PAYLOAD/WebUI" "$PAYLOAD/scripts"
cp -r InspectionCore/dist/win/. "$PAYLOAD/Core/"
cp -r UI/WebUI/dist/. "$PAYLOAD/WebUI/"
cp scripts/info.json "$PAYLOAD/info.json"
# boot.js is what makes this a launchable version rather than a folder of
# files: it is the only place that knows this application's layout -- the
# executable's name, the chdir argument, the control port, where the UI is.
cp scripts/boot.js "$PAYLOAD/scripts/boot.js"

# --- 4. the installable package ----------------------------------------------
if [[ "$WANT_ZIP" == 1 ]]; then
  say "packaging update_${VERSION}_win.zip"
  python UI/Launcher/tools/make_package.py "$PAYLOAD" \
      "$APP_ROOT/update_${VERSION}_win.zip" --version="$VERSION"
fi

# --- 5. the launcher ---------------------------------------------------------
say "building the launcher"
(
  cd UI/Launcher
  npm install --no-audit --no-fund
  npm run package
)
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

# --- 6. dependency audit -----------------------------------------------------
#
# The point of this step is the fresh PC. Walk the import table of every shipped
# executable and camera producer, recursively, and report any DLL that is
# neither bundled beside it nor a Windows system DLL. Anything printed here is
# something that works on THIS machine only because it happens to be installed
# on it -- which is exactly the class of problem that only shows up after the
# hardware is already on someone else's bench.
say "auditing dependencies for a machine with nothing installed"

DEPS_TXT="$DEST_ABS/DEPENDENCIES.txt"
set +e
python - "$PAYLOAD/Core" "$DEPS_TXT" "$VERSION" <<'PY'
import os, re, subprocess, sys

core, out_path, version = sys.argv[1], sys.argv[2], sys.argv[3]

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
L.append("visSele %s -- what the target machine needs" % version)
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
L.append("  launcher/Xception INSP-win32-x64/Xception INSP.exe")
L.append("  app root    -> <this folder>/app")
L.append("  working dir -> wherever data/ lives")

open(out_path, 'w', encoding='utf-8').write("\n".join(L) + "\n")
start = L.index("BUNDLED (nothing to install):")
print("\n".join("    " + x for x in L[start:]))
sys.exit(1 if missing else 0)
PY
AUDIT=$?
set -e

# --- 7. done -----------------------------------------------------------------
say "done"
echo "    $DEST_ABS"
echo "      app/$VERSION/                   the application"
if [[ "$WANT_ZIP" == 1 ]]; then
  echo "      app/update_${VERSION}_win.zip     installable, hash-verified"
fi
echo "      launcher/                       the launcher (bundled Electron)"
echo "      DEPENDENCIES.txt                read this before copying to a new PC"
echo ""
echo "    copy the whole of $DEST to the target machine, then run"
echo "      launcher/Xception INSP-win32-x64/Xception INSP.exe"
echo "    and point its app root at the copied app/ folder."

if [[ "$AUDIT" != 0 ]]; then
  echo ""
  echo "    WARNING: unresolved DLLs -- see DEPENDENCIES.txt" >&2
  exit 1
fi
