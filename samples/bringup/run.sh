#!/usr/bin/env bash
# Start a core that behaves like a machine, with no machine attached.
#
#   ./run.sh                 # work in ./run/, core on 4090
#   ./run.sh /tmp/mybench    # somewhere else
#   CORE_PORT=4190 ./run.sh  # beside a core that is already running
#
# What it does: makes a working directory, seeds data/ from this sample, and
# starts the core with the fake camera pointed at frames/. Nothing is copied
# back out and nothing outside the working directory is touched.
#
# Ctrl-C stops it. The working directory can be deleted; re-running rebuilds it.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
WORK="${1:-$HERE/run}"
CORE_PORT="${CORE_PORT:-4090}"
LOG_WS_PORT="${LOG_WS_PORT:-4091}"

for c in "$REPO"/InspectionCore/build/*/visSele.exe "$REPO"/InspectionCore/build/*/visSele; do
  [ -x "$c" ] && BIN="$c" && break
done
if [ -z "${BIN:-}" ]; then
  echo "no visSele binary found -- build it first (see samples/README.md)" >&2
  exit 2
fi
BINDIR="$(cd "$(dirname "$BIN")" && pwd)"
export PATH="$BINDIR:$PATH"
export LD_LIBRARY_PATH="$BINDIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DYLD_LIBRARY_PATH="$BINDIR${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
case "$BINDIR" in *mingw*|*msys*)
  for d in /mingw64/bin /c/msys64/mingw64/bin "${MSYSTEM_PREFIX:-}/bin"; do
    [ -n "$d" ] && [ -d "$d" ] && export PATH="$PATH:$d"
  done ;;
esac

mkdir -p "$WORK/data"
# Never overwrite: a working directory is the machine's own state once it has
# been run, and clobbering it on every start is how someone loses the def they
# just saved through the UI. Same rule --init-data follows.
for f in "$HERE"/data/*; do
  b="$(basename "$f")"
  if [ -e "$WORK/data/$b" ]; then echo "  keep  data/$b"
  else cp "$f" "$WORK/data/$b"; echo "  copy  data/$b"; fi
done

# The core resolves relative paths against its working directory, and the WebUI
# asks for recipes under data/ -- so it has to be started from $WORK, not here.
cd "$WORK"

# A private log ring, so this does not attach to a core already running on this
# machine: the ring is named shared memory, and sharing it mixes two machines'
# logs together and makes the other one's drainer write a crash dump when this
# process exits.
export INSP_LOG_RING_NAME="sample_bringup_$$"
export INSP_LOG_WS_PORT="$LOG_WS_PORT"

# THE FAKE CAMERA. The frames are the same synthetic part sample1.hydef was
# built from, so the live path and samples/headless must agree about where it
# is -- two different routes to one answer, which is the only version of this
# check that can catch a difference between them.
export FORCE_BMP_CAROUSEL="$HERE/frames"

echo
echo "core        : $BIN"
echo "working dir : $WORK"
echo "frames      : $FORCE_BMP_CAROUSEL"
echo "core ws     : ws://127.0.0.1:$CORE_PORT/"
echo
echo "Now serve the WebUI and open it (see samples/README.md). If the UI talks to"
echo "a different port than $CORE_PORT, set it in the browser console:"
echo "    localStorage.setItem('coreport','$CORE_PORT'); location.reload()"
echo
exec "$BIN" "port=$CORE_PORT"
