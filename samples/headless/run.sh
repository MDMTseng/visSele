#!/usr/bin/env bash
# Does this build compute the right numbers?
#
#   ./run.sh                        # find the binary, inspect, compare
#   ./run.sh /path/to/visSele       # use that binary
#   ./run.sh --bless                # rewrite expect.json from this run
#
# One synthetic frame, one def, one answer. No camera, no controller board, no
# browser, no database -- and no data/ directory either: this runs in an empty
# directory, which is the point. It is the smallest thing that can tell a fresh
# port "the engine is working" apart from "it builds".
#
# Exit 0 = the numbers match. Anything else = they do not, and the output says
# which one moved by how much.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"

BLESS=0
BIN=""
for a in "$@"; do
  case "$a" in
    --bless) BLESS=1 ;;
    *) BIN="$a" ;;
  esac
done

# The build directory is named after the platform (see InspectionCore/CMakeLists
# PLAT); take whichever one exists rather than making the caller know.
if [ -z "$BIN" ]; then
  # .exe FIRST. On Windows the extensionless name resolves through a shim that
  # does not pick up the camera SDK DLLs sitting beside the binary, so the same
  # build "works" or exits 127 depending on which spelling it was launched with.
  for c in "$REPO"/InspectionCore/build/*/visSele.exe "$REPO"/InspectionCore/build/*/visSele; do
    [ -x "$c" ] && BIN="$c" && break
  done
fi
if [ -z "$BIN" ] || [ ! -x "$BIN" ]; then
  echo "no visSele binary found. Build it first:" >&2
  echo "  cmake -S InspectionCore -B InspectionCore/build/linux_x64 -DFEATURE_ARAVIS=OFF" >&2
  echo "  cmake --build InspectionCore/build/linux_x64 --target visSele -j" >&2
  exit 2
fi

# Camera SDK shared libraries ship NEXT TO the binary. On Windows the MindVision
# DLL is required to load the process even for --insp, which touches no camera at
# all; on Linux the default build has FEATURE_ARAVIS=ON and finds Aravis through
# the system, so this is usually a no-op. Either way, look beside the binary
# before deciding the build is broken.
BINDIR="$(cd "$(dirname "$BIN")" && pwd)"
export PATH="$BINDIR:$PATH"
export LD_LIBRARY_PATH="$BINDIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DYLD_LIBRARY_PATH="$BINDIR${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"

# MSYS/MinGW builds also need the toolchain's own runtime DLLs, and without them
# the loader fails naming the FIRST library it could not resolve -- which is the
# camera SDK sitting right there beside the binary, so the message sends you
# looking for a file that is not missing. Harmless everywhere else: the loop
# only adds directories that exist.
case "$BINDIR" in
  *mingw*|*msys*)
    for d in /mingw64/bin /c/msys64/mingw64/bin "${MSYSTEM_PREFIX:-}/bin"; do
      [ -n "$d" ] && [ -d "$d" ] && export PATH="$PATH:$d"
    done ;;
esac

# A python that RUNS, not one that merely exists on PATH. On Windows,
# `python3` usually resolves to the Microsoft Store stub in WindowsApps: it is
# executable, `command -v` finds it, and it prints nothing and exits 49. A check
# for existence therefore picks the one candidate that cannot work, and the
# failure is silent -- no output, no traceback, a number nobody recognises.
PY=""
for c in ${PYTHON:-} python3 python py; do
  [ -n "$c" ] || continue
  if "$c" -c 'import sys; sys.exit(0)' >/dev/null 2>&1; then PY="$c"; break; fi
done
if [ -z "$PY" ]; then
  echo "no working python found (tried: \$PYTHON, python3, python, py)." >&2
  echo "Set PYTHON=/path/to/python and re-run." >&2
  exit 2
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
OUT="$WORK/report.json"

echo "binary : $BIN"
echo "image  : $HERE/sample1.png"
echo "def    : $HERE/sample1.hydef"

# A private log ring. The core's logs live in named shared memory, so a run that
# uses the default name attaches to a core that is already running on this
# machine -- mixing its output into someone's live session and, on exit, making
# the drainer there write a spurious "producer died" crash dump.
export INSP_LOG_RING_NAME="sample_headless_$$"
export INSP_LOG_WS_PORT="${INSP_LOG_WS_PORT:-4291}"

# cd into the scratch directory ON PURPOSE: this must pass with no data/ next to
# it. If a future change makes the offline path read machine state from disk,
# this is where it will fail, and that is worth knowing.
( cd "$WORK" && "$BIN" --insp "$HERE/sample1.png" "$HERE/sample1.hydef" "$OUT" ) \
  >"$WORK/log.txt" 2>&1 || {
    echo "--- the core exited $? ---"; tail -40 "$WORK/log.txt"; exit 3; }

[ -s "$OUT" ] || { echo "no report was written"; tail -40 "$WORK/log.txt"; exit 3; }

if [ "$BLESS" = "1" ]; then
  "$PY" "$HERE/check.py" "$HERE/expect.json" "$OUT" --bless
else
  "$PY" "$HERE/check.py" "$HERE/expect.json" "$OUT"
fi
