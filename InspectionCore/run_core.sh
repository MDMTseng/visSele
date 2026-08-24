#!/usr/bin/env bash
# Launch the inspection core on the Windows/MSYS2 bench.
#
#   ./run_core.sh                 real camera (HikRobot build)
#   ./run_core.sh --bench         no camera: synth cam_ts + dev console on 4099
#   ./run_core.sh --build NAME    force a build under InspectionCore/build/
#   ./run_core.sh --check         run the preflight checks and exit
#
# IN THE FOREGROUND, ON PURPOSE. Ctrl-C is the only working shutdown: the core
# handles SIGINT/SIGTERM and tears down through mainLoop, but a core started
# with `nohup ... &` has no console and no window, so Windows refuses anything
# but `taskkill /F` ("this process can only be forcibly terminated"). Launch it
# detached and you have thrown the graceful path away. Measured 2026-08-20.
#
# The preflight checks below are not ceremony. Every one of them is a failure
# that already cost bench time, and all of them fail QUIETLY -- the core starts,
# the UI looks normal, and the numbers are wrong.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"     # InspectionCore/
CORE_DIR="$HERE/Core0_1"
BUILD_DIR="$HERE/build"
MVS_RT="/c/Program Files (x86)/Common Files/MVS/Runtime/Win64_x64"
MINGW="/c/msys64/mingw64/bin"
# Needed before the build check below, which uses objdump to ask a binary
# whether it really links the camera SDK rather than trusting its name.
export PATH="$MINGW:$PATH"

BUILD=""; BENCH=0; CHECK_ONLY=0
while [ $# -gt 0 ]; do
  case "$1" in
    --bench)  BENCH=1; shift ;;
    --build)  BUILD="${2:-}"; shift 2 ;;
    --check)  CHECK_ONLY=1; shift ;;
    -h|--help) sed -n '2,20p' "${BASH_SOURCE[0]}" | sed 's/^# \?//'; exit 0 ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
done

say()  { printf '  %s\n' "$*"; }
ok()   { printf '  ok    %s\n' "$*"; }
warn() { printf '  WARN  %s\n' "$*"; }
die()  { printf '  STOP  %s\n' "$*" >&2; exit 1; }

# --- which build ------------------------------------------------------------
# There is ONE build now (2026-08-20): the six that had accumulated were deleted
# and win-mingw-msys rebuilt from scratch. It is HikRobot-capable, so it serves
# both modes -- --bench differs by environment (synth cam_ts, dev console), not
# by binary.
#
# The name is still not trusted. A build with FEATURE_HIKROBOT=OFF does not fail
# with a real camera attached: it enumerates the BMP carousel and hands you a
# FAKE camera, with nothing on screen looking wrong. So the binary is asked what
# it links, and anything under build/ is accepted as long as it answers yes --
# that way a differently-named experiment still works without editing this list.
hik_capable() {   # a build that actually links the HikRobot SDK
  [ -f "$BUILD_DIR/$1/visSele.exe" ] || return 1
  objdump -p "$BUILD_DIR/$1/visSele.exe" 2>/dev/null | grep -qi "MVCameraControl.dll"
}
if [ -z "$BUILD" ]; then
  for b in win-mingw-msys; do hik_capable "$b" && { BUILD="$b"; break; }; done
  if [ -z "$BUILD" ]; then      # any other build that links the SDK
    for p in "$BUILD_DIR"/*/; do
      b="$(basename "$p")"; hik_capable "$b" && { BUILD="$b"; break; }
    done
  fi
  if [ -z "$BUILD" ] && [ "$BENCH" = 1 ]; then   # bench needs no camera at all
    for p in "$BUILD_DIR"/*/; do
      [ -f "$p/visSele.exe" ] && { BUILD="$(basename "$p")"; break; }
    done
  fi
  [ -n "$BUILD" ] || die "no build under $BUILD_DIR links MVCameraControl.dll -- configure with FEATURE_HIKROBOT=ON and build"
fi
EXE="$BUILD_DIR/$BUILD/visSele.exe"
[ -f "$EXE" ] || die "no such build: $EXE"

echo "core launcher"
say "build   $BUILD"
say "mode    $([ "$BENCH" = 1 ] && echo 'bench (synth cam_ts, no camera)' || echo 'real camera')"

# --- PATH -------------------------------------------------------------------
# Three directories, not one. The exe resolves DLLs through PATH, not through
# its own directory:
#   mingw64/bin      OpenCV 4.13 + libgomp
#   $BUILD           the build's own DLLs, incl. MVCAMSDK_X64.DLL -- a
#                    MindVision link-time dependency the exe will not start
#                    without, even though this bench has no MindVision camera.
#                    It used to be borrowed off a sibling build dir (nohik-cv4),
#                    so deleting an unrelated build broke every other one. It is
#                    now copied in from dist/win at build time; the check below
#                    still verifies it, because a fresh build dir will not have
#                    it until somebody does.
#   MVS runtime      MvCameraControl.dll (HikRobot)
export PATH="$MINGW:$BUILD_DIR/$BUILD:$MVS_RT:$PATH"

# --- preflight --------------------------------------------------------------
echo "preflight"
fail=0

# An orphan core holds 4090/4099 and COM3. The new one fails to bind and exits
# QUIETLY, so every command you then send reaches the OLD process -- and the
# run looks completely normal. This has happened.
if tasklist 2>/dev/null | grep -qi visSele; then
  warn "a core is ALREADY RUNNING -- it holds 4090/4099 and COM3."
  say  "      this one would exit quietly and your commands would reach the old one."
  say  "      stop it first:  taskkill //IM visSele.exe //F"
  fail=1
else
  ok "no other core running"
fi

# A leftover Playwright browser does the PD CONNECT for you. Headless tooling
# then appears to work while actually riding on the browser's channel; kill the
# browser and the same tooling stops working. Cost most of 2026-08-19 to find.
if tasklist 2>/dev/null | grep -qi "chrome-headless"; then
  warn "a headless Chromium is attached -- it will PD CONNECT behind your back"
  say  "      taskkill //IM chrome-headless-shell.exe //F"
else
  ok "no orphan headless browser"
fi

for dll in libopencv_core-413.dll MVCAMSDK_X64.DLL; do
  found=""
  IFS=':' read -ra P <<< "$PATH"
  for d in "${P[@]}"; do [ -f "$d/$dll" ] && { found="$d"; break; }; done
  if [ -n "$found" ]; then ok "$dll"
  else
    warn "$dll NOT on PATH -- the exe will refuse to start"
    [ "$dll" = "MVCAMSDK_X64.DLL" ] && \
      say "      cp \"$HERE/dist/win/MVCAMSDK_X64.DLL\" \"$BUILD_DIR/$BUILD/\""
    fail=1
  fi
done
if [ "$BENCH" = 0 ]; then
  if [ -f "$MVS_RT/MvCameraControl.dll" ]; then ok "MvCameraControl.dll (HikRobot)"
  else warn "MvCameraControl.dll not found at $MVS_RT -- is the MVS runtime installed?"; fail=1; fi
  hik_capable "$BUILD" && ok "$BUILD links the HikRobot SDK" \
    || warn "$BUILD does NOT link HikRobot -- you will get the BMP carousel, silently"
fi

[ -d "$CORE_DIR/data" ] && ok "data/ present (cwd will be Core0_1)" \
  || { warn "no $CORE_DIR/data -- defs and camera settings resolve relative to cwd"; fail=1; }

if [ "$CHECK_ONLY" = 1 ]; then exit $fail; fi
[ "$fail" = 0 ] || { echo; die "preflight found problems above -- fix them or pass --check to review"; }

# --- go ---------------------------------------------------------------------
cd "$CORE_DIR" || die "cannot cd $CORE_DIR"
if [ "$BENCH" = 1 ]; then
  # Without the synth there is no camera for the clock calibration to learn
  # from: the board sits in 102 and lands in 112 with error 14
  # (CAM_CLOCK_CAL_FAILED). That is what "no camera" means to the state
  # machine, not a fault.
  export INSP_PERIF_CONSOLE=4099
  export INSP_CAM_TS_SYNTH=1
  export INSP_CAM_TS_OFFSET_US=800
  export INSP_CAM_TS_MULT=1.0
  say "console 4099, synth cam_ts = t_us*1.0 + 800us"
fi
echo
echo "starting -- Ctrl-C for a clean shutdown (do NOT background this)"
echo "----------------------------------------------------------------"
exec "$EXE"
