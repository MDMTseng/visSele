# visSele inspection core — build instructions

Three supported build paths:

| Target | Where you build | Preset / shortcut |
|---|---|---|
| **macOS (arm64) native** | macOS arm64 | `mac-arm64` |
| **Windows x64 (MSYS2 / MinGW64) native** | Windows | `win-mingw` |
| **Windows x64 cross-compiled from macOS** | macOS arm64 | `win-cross` |

All three drive through the same script:

```bash
./InspectionCore/build.sh -p <preset> -c <Debug|Release> -e <export-dir>
./InspectionCore/build.sh -h    # show all options
```

The script wraps `cmake --preset` + `cmake --build` + (for Windows targets) DLL bundling into one command.

---

## 1 · macOS arm64 (native dev)

### Prerequisites (clean MacBook)

```bash
# Xcode CLT — provides clang, make, git
xcode-select --install

# Homebrew (skip if installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Build dependencies
brew install cmake pkg-config opencv libusb glib gtk+3 aravis
```

### Build

```bash
git clone <repo-url> visSele
cd visSele

# Quick Release build (uses Homebrew OpenCV)
./InspectionCore/build.sh

# Or: use vcpkg-provided OpenCV (matches Windows pipeline)
./InspectionCore/build.sh -p mac-arm64-opencv

# Debug + clean rebuild
./InspectionCore/build.sh -c Debug --clean
```

Build artifacts land in `InspectionCore/build/mac-arm64/`:

- `visSele` — main inspection daemon
- `inspd_log` — log drainer
- `calib_chessboard` — offline lens calibration CLI

### Run

```bash
cd InspectionCore/Core0_1
../build/mac-arm64/visSele
# Then point the WebUI at ws://localhost:4090 (see UI/WebUI README)
```

---

## 2 · Windows x64 (MSYS2 / MinGW64 native)

### Prerequisites (clean Windows machine)

1. Install **[MSYS2](https://www.msys2.org/)**. Reboot if asked.
2. Open the **MSYS2 MINGW64** shell (NOT the plain MSYS2 shell). Then:
   ```bash
   pacman -Syu                                # update; close + reopen shell when asked
   pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake \
                      mingw-w64-x86_64-make git
   ```
3. Bootstrap **vcpkg** (one-time):
   ```bash
   git clone https://github.com/microsoft/vcpkg /c/vcpkg
   /c/vcpkg/bootstrap-vcpkg.bat
   export VCPKG_ROOT=/c/vcpkg
   echo 'export VCPKG_ROOT=/c/vcpkg' >> ~/.bashrc
   ```
4. (Optional) Install camera SDKs you actually plan to use. The HikRobot DLL ships in the repo at `InspectionCore/CoreHub/Core/MvCameraControl.dll`.

### Build

```bash
git clone <repo-url> visSele
cd visSele

# First time: this triggers vcpkg to build opencv4 + deps for
# the x64-mingw-static triplet. Expect ~30-60 min one-time.
./InspectionCore/build.sh -p win-mingw -c Release -e dist
```

The `-e dist` step copies `visSele.exe` and friends into `dist/`. With the `x64-mingw-static` triplet (used by `win-mingw`), runtime DLLs are statically linked, so the folder is largely self-contained (you still need `MvCameraControl.dll` next to the exe for HikRobot).

### Run

```bash
cd dist
./visSele.exe
```

---

## 3 · Cross-compile Windows binary from macOS

Use this when you want to produce a Windows `.exe` without leaving your Mac dev environment.

### Prerequisites

```bash
# Mingw-w64 toolchain (Mac → Windows cross)
brew install mingw-w64 cmake pkg-config

# Bootstrap vcpkg
git clone https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh -disableMetrics
export VCPKG_ROOT=$HOME/vcpkg
echo 'export VCPKG_ROOT=$HOME/vcpkg' >> ~/.zshrc
```

### Build + bundle

```bash
git clone <repo-url> visSele
cd visSele

# First time: vcpkg builds opencv4 for x64-mingw-dynamic. ~14 min.
# Subsequent builds reuse the cache and complete in ~1-2 min.
./InspectionCore/build.sh -p win-cross -c Release -e ../vissele-win64
```

When done, `../vissele-win64/` contains:

- `visSele.exe`, `inspd_log.exe`, `calib_chessboard.exe`, `logctrl_test_ring.exe`
- 28 OpenCV / ffmpeg / png / jpeg / tiff / webp / abseil / protobuf DLLs
- `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`
- `MvCameraControl.dll` (HikRobot)

That folder is everything needed on the target Windows machine — zip it, copy it over, run `visSele.exe`.

### What's different vs native MSYS2

| | Native MSYS2 (`win-mingw`) | Cross from Mac (`win-cross`) |
|---|---|---|
| vcpkg triplet | `x64-mingw-static` | `x64-mingw-dynamic` |
| Runtime DLLs | mostly baked into .exe | shipped alongside |
| Camera SDKs available | All (Aravis on Windows variant, MindVision, HikRobot) | HikRobot only by default |
| First-build cost | ~30–60 min | ~14 min |

Camera-SDK selection is controlled by `FEATURE_ARAVIS` / `FEATURE_MINDVISION` / `FEATURE_HIKROBOT` cache vars in the presets.

---

## build.sh option reference

```text
-p, --platform <id>     mac-arm64 | mac-arm64-opencv | win-cross | win-mingw
                        (default: mac-arm64)
-c, --config <type>     Debug | Release | RelWithDebInfo   (default: Release)
-e, --export <dir>      bundle built binaries + runtime DLLs into <dir>
--clean                 wipe the build directory before configuring
-j, --jobs N            parallel build jobs (default: all cores)
```

Environment overrides:

- `VCPKG_ROOT` — vcpkg location (default `~/vcpkg`).
- `MINGW_PREFIX` — mingw-w64 install prefix (default `/opt/homebrew/opt/mingw-w64`).
- `JOBS` — parallel jobs (same as `--jobs`).

---

## Troubleshooting

### vcpkg fails to find Python on macOS
Install via Homebrew: `brew install python` and re-run.

### `mingw-w64` not found
`brew install mingw-w64` (or set `MINGW_PREFIX=/path/to/mingw`).

### `vcpkg` build of OpenCV fails with port-specific error
vcpkg's `opencv4` port occasionally needs a patch on a niche triplet. Run:
```bash
~/vcpkg/vcpkg install opencv4:x64-mingw-dynamic --debug
```
and read the per-port log; copy a patch from `~/vcpkg/buildtrees/opencv4/` to an overlay-port if needed.

### `find_package(OpenCV)` fails after vcpkg succeeded
Likely a chain-loaded toolchain restricting `CMAKE_FIND_ROOT_PATH_MODE_PACKAGE` to `ONLY`. Our `toolchain-mingw64.cmake` uses `BOTH` for that reason — match it if you write a new toolchain.

### Cross build can't find HikRobot symbols
The Windows `.lib` is a GNU `ar` archive (mingw-compatible) — `file InspectionCore/contrib/hikrobot_camera_sdk/libraries/win64/MvCameraControl.lib` should report `current ar archive`. If it ever changes to a COFF format, convert with `gendef` + `dlltool` or relink the vendor DLL.

### Native MSYS2 + native dirent conflicts
`compat_dirent.h` delegates to the system header on `__MINGW32__` / `__MINGW64__`. If you target plain MSVC (not currently supported), the tronkko shim block is what's needed.

### "PowerShell not found" on the Mac cross-build
Disable vcpkg's applocal step. Already done in the preset via `VCPKG_APPLOCAL_DEPS=OFF` / `X_VCPKG_APPLOCAL_DEPS_INSTALL=OFF`.

### Clean rebuild
```bash
./InspectionCore/build.sh -p <preset> --clean
```
or manually: `rm -rf InspectionCore/build/<preset>` then re-run.

### CMake says "is not a directory"
The script uses absolute paths internally; if you call cmake manually, always pass the absolute path to `--build`.
