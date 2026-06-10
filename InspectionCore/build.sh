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
NO_CONFIGURE=0
BUNDLE_ONLY=0
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
  --no-configure          skip the cmake configure / vcpkg step; just build the
                          already-configured dir + bundle (fast iteration)
  --bundle-only           skip configure AND build; only (re)bundle the existing
                          build output into -e <dir>. Requires -e.
  --allow-rebuild         proceed even if the toolchain drifted (vcpkg/gcc moved);
                          otherwise the configure aborts to avoid a surprise
                          ~30-min dependency rebuild
  -j, --jobs N            parallel build jobs (default: $JOBS)
  -h, --help              show this help

Env: FORCE_RUNTIME=1 re-copies the heavy HikRobot MVS runtime even if the
     export dir already has it (default: skip when already bundled).

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
    --no-configure) NO_CONFIGURE=1; shift;;
    --bundle-only) BUNDLE_ONLY=1; NO_CONFIGURE=1; shift;;
    --allow-rebuild) export ALLOW_REBUILD=1; shift;;
    -j|--jobs)     JOBS="$2"; shift 2;;
    -h|--help)     usage; exit 0;;
    *) echo "unknown arg: $1" >&2; usage >&2; exit 2;;
  esac
done

# ---- map platform shortcut -> CMake preset ------------------------------
case "$PLATFORM" in
  mac-arm64|mac-arm64-opencv|linux-x64|win-mingw|win-mingw-ninja|win-mingw-msys)  PRESET="$PLATFORM" ;;
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

# ---- ccache auto-wire ---------------------------------------------------
# If ccache is on PATH, route the compiler launcher through it so even a
# --clean build doesn't pay full recompile cost (object cache is keyed on
# source content, so it survives wipes of the build dir). Disable with
# NO_CCACHE=1 in the environment. Saves ~10-20x on full rebuilds once warm.
CCACHE_OPTS=()
if [[ -z "${NO_CCACHE:-}" ]] && command -v ccache >/dev/null 2>&1; then
  echo "==> ccache: $(ccache --version | head -1)  dir=$(ccache -k cache_dir 2>/dev/null || echo '~/.cache/ccache')"
  CCACHE_OPTS=(
    -DCMAKE_C_COMPILER_LAUNCHER=ccache
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
  )
fi

# ---- bundle-only sanity -------------------------------------------------
if (( BUNDLE_ONLY )); then
  [[ -n "$EXPORT_DIR" ]] || { echo "ERROR: --bundle-only requires -e <dir>" >&2; exit 2; }
  [[ -d "$BUILD_DIR" ]]  || { echo "ERROR: --bundle-only but no build dir at $BUILD_DIR (build it first)" >&2; exit 2; }
fi

# ---- configure ----------------------------------------------------------
# CMake presets fix CMAKE_BUILD_TYPE=Release; override on the command line.
if (( NO_CONFIGURE )); then
  echo "==> skip configure (--no-configure)"
  [[ -f "$BUILD_DIR/CMakeCache.txt" ]] || { echo "ERROR: $BUILD_DIR is not configured yet; run once without --no-configure" >&2; exit 2; }
else
  # Guard against a surprise ~30-min dependency rebuild: if the toolchain that
  # vcpkg hashes (its checkout commit / gcc) drifted from the pinned values,
  # abort here instead of letting cmake silently rebuild everything.
  if (( needs_vcpkg )) && [[ -f "$SCRIPT_DIR/tools/toolchain_guard.sh" ]]; then
    bash "$SCRIPT_DIR/tools/toolchain_guard.sh" || exit $?
  fi
  echo "==> cmake --preset $PRESET -DCMAKE_BUILD_TYPE=$CONFIG ${CCACHE_OPTS[*]}"
  cmake -S "$SCRIPT_DIR" --preset "$PRESET" -DCMAKE_BUILD_TYPE="$CONFIG" "${CCACHE_OPTS[@]}"
fi

# ---- build --------------------------------------------------------------
if (( BUNDLE_ONLY )); then
  echo "==> skip build (--bundle-only)"
else
  echo "==> cmake --build $BUILD_DIR -j $JOBS"
  cmake --build "$BUILD_DIR" -j "$JOBS"
fi

# ---- bundle (Windows targets only) --------------------------------------
if [[ -n "$EXPORT_DIR" ]]; then
  mkdir -p "$EXPORT_DIR"
  case "$PRESET" in
    win-mingw|win-mingw-ninja|win-mingw-cross|win-mingw-msys)
      echo "==> Bundling Windows binaries + DLLs -> $EXPORT_DIR"
      # 1) all .exe from the build dir
      find "$BUILD_DIR" -maxdepth 2 -name "*.exe" -exec cp -v {} "$EXPORT_DIR/" \;
      if [[ "$PRESET" == "win-mingw-msys" ]]; then
        # Dynamic (pacman) OpenCV: copy the full transitive mingw DLL closure of
        # the exes (OpenCV + jpeg/png/webp/zlib/lzma/tbb + libgcc/stdc++/winpthread)
        # into an opencv_dll/ subdir, plus the runtime DLLs the loader needs flat
        # next to the exe. objdump walks the import table recursively; anything
        # found under the mingw bin dir is part of the closure (camera SDK DLLs
        # live elsewhere -> handled by steps 4/4b/5 below).
        _objdump="$(command -v objdump || true)"; _mb="$(dirname "$(command -v gcc)")"
        if [[ -n "$_objdump" ]]; then
          mkdir -p "$EXPORT_DIR/opencv_dll"
          declare -A _seen; _q=("$EXPORT_DIR"/*.exe); _n=0
          while ((${#_q[@]})); do
            _f="${_q[0]}"; _q=("${_q[@]:1}")
            while read -r _d; do
              _l="${_d,,}"; [[ -n "${_seen[$_l]:-}" ]] && continue
              if [[ -f "$_mb/$_d" ]]; then
                _seen[$_l]=1; cp -f "$_mb/$_d" "$EXPORT_DIR/opencv_dll/"; _q+=("$_mb/$_d"); _n=$((_n+1))
              fi
            done < <("$_objdump" -p "$_f" 2>/dev/null | grep -i 'DLL Name:' | sed 's/.*DLL Name:[[:space:]]*//')
          done
          # libgcc/libstdc++/libwinpthread must sit NEXT TO the exe (loaded before
          # any DLL-search-path tweak); copy those out of the closure dir too.
          for _rt in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
            [[ -f "$EXPORT_DIR/opencv_dll/$_rt" ]] && cp -f "$EXPORT_DIR/opencv_dll/$_rt" "$EXPORT_DIR/"
          done
          echo "==> bundled $_n mingw DLLs into opencv_dll/ (OpenCV + transitive closure)"
        else echo "WARN: objdump not found; cannot resolve OpenCV DLL closure" >&2; fi
        # Launcher: OpenCV DLLs live in opencv_dll/, but Windows resolves an exe's
        # import-table DLLs at process start (before main), so that dir must be on
        # PATH BEFORE launch. This .bat does it; point your Electron/launcher at it
        # (or replicate "PATH=<bundle>\opencv_dll;%PATH%" in the spawn env).
        printf '@echo off\r\nset "PATH=%%~dp0opencv_dll;%%PATH%%"\r\n"%%~dp0visSele.exe" %%*\r\n' > "$EXPORT_DIR/run_visSele.bat"
        echo "==> wrote run_visSele.bat (prepends opencv_dll to PATH)"
      else
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
      # Native Windows/MSYS2 build (win-mingw): the runtime DLLs sit next to gcc
      # in the toolchain bin dir. Derive it from gcc's location, and add the
      # common MSYS2 path as a fallback.
      if command -v gcc >/dev/null 2>&1; then
        mingw_search+=("$(dirname "$(command -v gcc)")")
      fi
      mingw_search+=("/c/msys64/mingw64/bin" "/mingw64/bin")
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
      fi  # end DLL provisioning (msys dynamic closure vs vcpkg static + runtime)
      # 4) HikRobot SDK DLL (lives in the repo).
      hik_dll="$SCRIPT_DIR/CoreHub/Core/MvCameraControl.dll"
      [[ -f "$hik_dll" ]] && cp -v "$hik_dll" "$EXPORT_DIR/"
      # 4b) Full HikRobot MVS runtime (DLLs + .cti producers + ThirdParty/) so the
      #     bundle runs on machines without MVS installed. Not in the repo -- pulled
      #     from the local MVS install. Override the path with $HIK_MVS_RUNTIME, or
      #     set it empty to skip (rely on MVS being installed on the target).
      hik_rt="${HIK_MVS_RUNTIME-/c/Program Files (x86)/Common Files/MVS/Runtime/Win64_x64}"
      hik_sentinel="$EXPORT_DIR/.mvs_runtime_bundled"
      if [[ -f "$hik_sentinel" && -z "${FORCE_RUNTIME:-}" ]]; then
        echo "==> HikRobot MVS runtime already bundled (skip; FORCE_RUNTIME=1 to re-copy)"
      elif [[ -n "$hik_rt" && -d "$hik_rt" ]]; then
        echo "==> Bundling HikRobot MVS runtime from: $hik_rt"
        cp -r "$hik_rt"/. "$EXPORT_DIR/"
        touch "$hik_sentinel"
      elif [[ -n "$hik_rt" ]]; then
        echo "WARN: HikRobot MVS runtime not found at '$hik_rt' (set HIK_MVS_RUNTIME). The bundle will need MVS installed on the target machine." >&2
      fi
      # 5) MindVision SDK runtime DLL (FEATURE_MINDVISION). visSele.exe imports
      #    it by the name MVCAMSDK_X64.DLL; the repo ships it as MVCAMSDK.dll.
      mv_dll="$SCRIPT_DIR/contrib/MindVision_GIGE/win_x64/lib/MVCAMSDK.dll"
      [[ -f "$mv_dll" ]] && cp -v "$mv_dll" "$EXPORT_DIR/MVCAMSDK_X64.DLL"
      ;;
    *)
      echo "==> Bundling native binaries -> $EXPORT_DIR"
      find "$BUILD_DIR" -maxdepth 1 -type f -perm -u+x -not -name "*.cmake" -exec cp -v {} "$EXPORT_DIR/" \;
      ;;
  esac
fi

echo "==> Done."
