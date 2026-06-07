#!/usr/bin/env bash
# visSele inspection-core build driver.
#
# Wraps CMake presets + (for Windows targets) DLL bundling into one entry
# point so you can rebuild + package a deployable directory in one shot.
#
# Examples:
#   ./build.sh                                        # mac-arm64 Release, no bundle
#   ./build.sh -p win-cross -c Release -e dist/win    # cross-compile Win64 + bundle
#   ./build.sh -p mac-arm64 -c Debug --clean          # wipe + Debug rebuild
#   ./build.sh -p win-cross -e ../release_win64       # bundle to absolute path
#
# Env vars consulted:
#   VCPKG_ROOT     -- override vcpkg location (default: ~/vcpkg)
#   MINGW_PREFIX   -- override mingw-w64 prefix (default: /opt/homebrew/opt/mingw-w64)
#   JOBS           -- parallel build jobs (default: nproc/sysctl)

set -euo pipefail

# ---- defaults -----------------------------------------------------------
PLATFORM="mac-arm64"
CONFIG="Release"
EXPORT_DIR=""
CLEAN=0
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
MINGW_PREFIX="${MINGW_PREFIX:-/opt/homebrew/opt/mingw-w64}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
  cat <<EOF
Usage: $0 [options]

  -p, --platform <id>     mac-arm64 | mac-arm64-opencv | linux | win-cross | win-mingw
                          (default: mac-arm64)
  -c, --config <type>     Debug | Release | RelWithDebInfo  (default: Release)
  -e, --export <dir>      bundle the built binaries (and runtime DLLs for
                          Windows targets) into <dir>. Created if missing.
  --clean                 wipe the build directory before configuring
  -j, --jobs N            parallel build jobs (default: $JOBS)
  -h, --help              show this help

Platform notes:
  win-cross  cross-compile from macOS using mingw-w64 + vcpkg
             (preset: win-mingw-cross). Needs \$VCPKG_ROOT bootstrapped.
  win-mingw  native MSYS2/MinGW64 build on Windows (run this script there).
EOF
}

# ---- arg parse ----------------------------------------------------------
while [[ $# -gt 0 ]]; do
  case "$1" in
    -p|--platform) PLATFORM="$2"; shift 2;;
    -c|--config)   CONFIG="$2"; shift 2;;
    -e|--export)   EXPORT_DIR="$2"; shift 2;;
    --clean)       CLEAN=1; shift;;
    -j|--jobs)     JOBS="$2"; shift 2;;
    -h|--help)     usage; exit 0;;
    *) echo "unknown arg: $1" >&2; usage >&2; exit 2;;
  esac
done

# ---- map platform shortcut -> CMake preset ------------------------------
case "$PLATFORM" in
  mac-arm64|mac-arm64-opencv|linux-x64|win-mingw)  PRESET="$PLATFORM" ;;
  linux)                                           PRESET="linux-x64" ;;
  win-cross)                                       PRESET="win-mingw-cross" ;;
  *) echo "unknown platform: $PLATFORM" >&2; exit 2 ;;
esac

BUILD_DIR="$SCRIPT_DIR/build/$PRESET"

echo "==> Platform : $PLATFORM   (preset: $PRESET)"
echo "==> Config   : $CONFIG"
echo "==> Jobs     : $JOBS"
echo "==> BuildDir : $BUILD_DIR"
[[ -n "$EXPORT_DIR" ]] && echo "==> Export   : $EXPORT_DIR"

# ---- vcpkg sanity for vcpkg-using presets -------------------------------
needs_vcpkg=0
case "$PRESET" in mac-arm64-opencv|win-mingw|win-mingw-cross) needs_vcpkg=1 ;; esac
if (( needs_vcpkg )); then
  if [[ ! -x "$VCPKG_ROOT/vcpkg" ]]; then
    echo "ERROR: vcpkg not found at \$VCPKG_ROOT=$VCPKG_ROOT" >&2
    echo "       git clone https://github.com/microsoft/vcpkg \$VCPKG_ROOT && \$VCPKG_ROOT/bootstrap-vcpkg.sh -disableMetrics" >&2
    exit 1
  fi
  export VCPKG_ROOT
fi

# ---- clean --------------------------------------------------------------
if (( CLEAN )) && [[ -d "$BUILD_DIR" ]]; then
  echo "==> Cleaning $BUILD_DIR"
  rm -rf "$BUILD_DIR"
fi

# ---- configure ----------------------------------------------------------
# CMake presets fix CMAKE_BUILD_TYPE=Release; override on the command line.
echo "==> cmake --preset $PRESET -DCMAKE_BUILD_TYPE=$CONFIG"
cmake -S "$SCRIPT_DIR" --preset "$PRESET" -DCMAKE_BUILD_TYPE="$CONFIG"

# ---- build --------------------------------------------------------------
echo "==> cmake --build $BUILD_DIR -j $JOBS"
cmake --build "$BUILD_DIR" -j "$JOBS"

# ---- bundle (Windows targets only) --------------------------------------
if [[ -n "$EXPORT_DIR" ]]; then
  mkdir -p "$EXPORT_DIR"
  case "$PRESET" in
    win-mingw|win-mingw-cross)
      echo "==> Bundling Windows binaries + DLLs -> $EXPORT_DIR"
      # 1) all .exe from the build dir
      find "$BUILD_DIR" -maxdepth 2 -name "*.exe" -exec cp -v {} "$EXPORT_DIR/" \;
      # 2) vcpkg runtime DLLs (opencv, ffmpeg, png/jpeg/tiff/webp, zlib, ...).
      vcpkg_bin="$BUILD_DIR/vcpkg_installed/x64-mingw-dynamic/bin"
      if [[ -d "$vcpkg_bin" ]]; then
        find "$vcpkg_bin" -maxdepth 1 -name "*.dll" -exec cp -v {} "$EXPORT_DIR/" \;
      fi
      # 3) mingw runtime DLLs (libgcc / libstdc++ / libwinpthread). Across
      #    distros these land in either .../x86_64-w64-mingw32/bin or
      #    .../x86_64-w64-mingw32/lib (the homebrew formula at the time of
      #    writing splits libwinpthread into bin/ and libgcc/libstdc++ into
      #    lib/). Search both, plus the toolchain-Cellar lib path for the
      #    Cellar-keyed version. First hit per DLL wins.
      mingw_search=(
        "$MINGW_PREFIX/toolchain-x86_64/x86_64-w64-mingw32/bin"
        "$MINGW_PREFIX/toolchain-x86_64/x86_64-w64-mingw32/lib"
      )
      # Add brew Cellar paths (resolves the canonical install if MINGW_PREFIX
      # points at the opt symlink).
      if command -v brew >/dev/null 2>&1; then
        cellar_root="$(brew --cellar mingw-w64 2>/dev/null)"
        if [[ -n "$cellar_root" && -d "$cellar_root" ]]; then
          for v in "$cellar_root"/*; do
            mingw_search+=("$v/toolchain-x86_64/x86_64-w64-mingw32/bin")
            mingw_search+=("$v/toolchain-x86_64/x86_64-w64-mingw32/lib")
          done
        fi
      fi
      for dll in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
        found=""
        for d in "${mingw_search[@]}"; do
          if [[ -f "$d/$dll" ]]; then found="$d/$dll"; break; fi
        done
        if [[ -n "$found" ]]; then
          cp -v "$found" "$EXPORT_DIR/"
        else
          echo "WARN: $dll not found in any of: ${mingw_search[*]}" >&2
        fi
      done
      # 4) HikRobot SDK DLL (lives in the repo).
      hik_dll="$SCRIPT_DIR/CoreHub/Core/MvCameraControl.dll"
      [[ -f "$hik_dll" ]] && cp -v "$hik_dll" "$EXPORT_DIR/"
      ;;
    *)
      echo "==> Bundling native binaries -> $EXPORT_DIR"
      find "$BUILD_DIR" -maxdepth 1 -type f -perm -u+x -not -name "*.cmake" -exec cp -v {} "$EXPORT_DIR/" \;
      ;;
  esac
fi

echo "==> Done."
