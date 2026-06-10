# visSele inspection core — build instructions

Build and run paths supported:

| Path | Section |
|---|---|
| macOS arm64 native | §1 |
| Linux x64 native | §2 |
| Windows x64 (MSYS2 / MinGW64) native | §3 |
| Windows x64 cross-compiled from macOS | §4 |
| Running the Win64 binary on macOS via **Wine** (no VM) | §5 |
| `build.sh` option reference | §6 |
| Troubleshooting | §7 |
| Windows crash dumps (minidump) | §8 |

All builds drive through one script:

```bash
./InspectionCore/build.sh -p <preset> -c <Debug|Release> -e <export-dir>
./InspectionCore/build.sh -h    # show all options
```

The script wraps `cmake --preset` + `cmake --build` + (for Windows targets) DLL bundling into one command. Every section below also covers how to **run** the result + how to bring up the WebUI side-by-side.

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

The core daemon and the WebUI run as separate processes. Open two terminals:

```bash
# Terminal 1 — inspection daemon (must be run from Core0_1/ so relative
# data/ paths resolve)
cd InspectionCore/Core0_1
../build/mac-arm64/visSele
# binds 0.0.0.0:4090 (BPG WebSocket)
# expect: "Try to open websocket... port:4090" / "opened 0.0.0.0:4090"

# Terminal 2 — WebUI dev server
cd UI/WebUI
npm install      # one-time
npm run dev      # serves http://localhost:5173 (Vite default)
```

Then open `http://localhost:5173` in a browser. It auto-connects to `ws://localhost:4090`. With no real camera attached, use the **fake camera (BMP_carousel)** drawer to load PNG/BMP frames from `InspectionCore/Core0_1/data/`.

To stop: `Ctrl-C` in each terminal.

---

## 2 · Linux x64 (Ubuntu / Debian native)

### Prerequisites (clean Ubuntu 22.04+ or Debian 12+)

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config git \
  libopencv-dev libaravis-dev \
  libglib2.0-dev libgtk-3-dev libusb-1.0-0-dev
```

`libaravis-dev` provides the GigE Vision camera SDK (default-on for Linux). If you don't need GigE camera support, pass `-DFEATURE_ARAVIS=OFF` to CMake and you can skip `libaravis-dev` / `libgtk-3-dev`.

### Build

```bash
git clone <repo-url> visSele
cd visSele
./InspectionCore/build.sh -p linux

# Or with a bundle directory (copies built binaries into dist/)
./InspectionCore/build.sh -p linux -c Release -e dist
```

Artifacts: `InspectionCore/build/linux-x64/visSele`, `inspd_log`, `calib_chessboard`.

### Run

```bash
# Terminal 1 — core daemon
cd InspectionCore/Core0_1
../build/linux-x64/visSele       # binds 0.0.0.0:4090

# Terminal 2 — WebUI
cd UI/WebUI && npm install && npm run dev
# open http://localhost:5173
```

### Notes
- HikRobot / MindVision SDKs aren't enabled by default on Linux. HikRobot does ship a Linux SDK (separate download); to use it, drop the `.so` next to the headers and toggle `FEATURE_HIKROBOT=ON`.
- `smem_channel` links `-lrt` on Linux for POSIX shared-memory primitives (handled automatically by CMakeLists).

---

## 3 · Windows x64 (MSYS2 / MinGW64 native)

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

# Recommended default: prebuilt pacman OpenCV, no vcpkg, configures in ~4.5s.
# Needs: pacman -S --needed mingw-w64-x86_64-opencv  (one-time).
./InspectionCore/build.sh -p win-mingw-msys -c Release -e dist

# Opt-in static/reproducible path: first run triggers vcpkg to build opencv4 +
# deps for the x64-mingw-static triplet. Expect ~30-60 min one-time.
./InspectionCore/build.sh -p win-mingw -c Release -e dist
```

The `-e dist` step copies `visSele.exe` and friends into `dist/`. With the
default `win-mingw-msys` (pacman) path, OpenCV is **dynamic**, so the bundle adds
an `opencv_dll/` subdir plus a `run_visSele.bat` launcher (see §3 for the PATH
detail). With the `-Vcpkg` / `win-mingw` static triplet, OpenCV is linked into
the exe, so the folder is largely self-contained (you still need
`MvCameraControl.dll` next to the exe for HikRobot).

### Run

```bash
# Terminal 1 (MSYS2 MINGW64) — core daemon
cd /path/to/visSele/dist
./visSele.exe                    # binds 0.0.0.0:4090

# Terminal 2 — WebUI (any shell with Node.js)
cd /path/to/visSele/UI/WebUI
npm install && npm run dev       # open http://localhost:5173
```

The dist/ folder needs the runtime `data/` directory next to `visSele.exe`. Copy it once:

```bash
cp -r InspectionCore/Core0_1/data dist/
```

To use a real HikRobot camera, install MVS from HikRobot's site so the GenICam DLLs (`GenApi_*.dll`, `GCBase_*.dll`, `MvRender.dll`) are on `PATH`, then toggle the camera type in your camera setting JSON.

### Windows — from PowerShell (no MSYS2 shell needed)

Two helper scripts in `InspectionCore/Core0_1/` let you build/iterate straight
from PowerShell, setting the toolchain env for you (MinGW on `PATH`, plus —
only when you opt into vcpkg — `VCPKG_ROOT`,
`VCPKG_DEFAULT_HOST_TRIPLET=x64-mingw-static`, and the in-repo overlay triplet).

**The default OpenCV is now the prebuilt MSYS2/pacman package**
(`mingw-w64-x86_64-opencv`, dynamic DLLs), driven by the **`win-mingw-msys`**
CMake preset. No vcpkg, no 30-min dependency builds — it configures in ~4.5 s.
Install it once in the MSYS2 MINGW64 shell:

```bash
pacman -S --needed mingw-w64-x86_64-opencv
```

The **old vcpkg static path** (`win-mingw` preset, OpenCV linked statically) is
now **opt-in** via a `-Vcpkg` switch on the scripts — use it for reproducible /
release bundles. It still needs vcpkg bootstrapped (steps above) and pays the
one-time ~30-min dependency build.

| Script | Purpose |
|---|---|
| `build.ps1` | the single **deploy** script: full build **+ DLL bundle**. Wraps `build.sh`. Switches: `-Debug`, `-Release`, `-Clean`, `-NoBundle`, `-Out <dir>`, `-Fast` (incremental / skip configure), `-ForceRuntime`, `-Vcpkg`. A bare call (or one with no config selected) prints help. |
| `dev.ps1` | **fast inner loop**: incremental compile + run in place (skips configure/bundle). Switches: `-NoRun`, `-Probe`, `-Port N`, `-Config Debug\|Release`, `-Vcpkg`. |

(`export.ps1` has been **removed**; its `-Out`, `-Fast`, and `-ForceRuntime`
features were merged into `build.ps1`.)

```powershell
cd InspectionCore\Core0_1
.\build.ps1 -Release      # Release build, bundled+runnable -> dist\win  (pacman OpenCV)
.\build.ps1 -Debug        # Debug build,   bundled         -> dist\win_debug
.\build.ps1 -Release -Out dist\test    # bundle to a custom folder
.\build.ps1 -Release -Fast             # incremental: skip configure (fast re-bundle)
.\build.ps1 -Release -Vcpkg            # static vcpkg build (reproducible release)
.\dev.ps1 -Run            # edit-compile-run loop (fast); add -Probe for JPEGq logging
```

The bundle (`build.ps1` / `build.sh -e`) is self-contained: it copies the MinGW
runtime DLLs, the MindVision `MVCAMSDK_X64.DLL`, and the full HikRobot MVS
runtime, so `dist\win` runs on a machine without the camera SDKs installed.
Run `visSele.exe` from `dist\win`, **not** from `build\win-mingw-msys\` (that
has no camera DLLs).

#### Dynamic-OpenCV deploy detail (default `win-mingw-msys` path)

Because the pacman OpenCV is **dynamic**, the bundle can't be a flat folder of
the exe alone. `build.sh` walks the exes' import tables (`objdump -p`) and copies
the full transitive OpenCV DLL closure (OpenCV + jpeg/png/webp/zlib/lzma/tbb)
into an **`opencv_dll/`** subdir, then writes a **`run_visSele.bat`** launcher.

Windows resolves an exe's import-table DLLs at **process start** (before
`main()`), so `opencv_dll/` must be on `PATH` *before* launch — a plain
double-click of `visSele.exe` won't find them. The generated `.bat` handles this:

```bat
set "PATH=%~dp0opencv_dll;%PATH%"
"%~dp0visSele.exe" %*
```

An Electron / custom launcher must do the same (prepend `<bundle>\opencv_dll`
to the spawn env's `PATH`). The 3 C++ runtime DLLs
(`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`) are copied
**flat next to the exe** as well, since the loader needs them before any
PATH tweak takes effect.

The `-Vcpkg` (static) path doesn't need `opencv_dll/` — OpenCV is baked into the
exe — and uses the in-repo overlay triplet at
`InspectionCore/triplets/x64-mingw-static.cmake`, which sets
`VCPKG_BUILD_TYPE release` so vcpkg builds only the release variant of each
dependency (skipping the unused debug halves, ~2× faster).

---

## 4 · Cross-compile Windows binary from macOS

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

## 5 · Running the Windows binary on macOS via Wine

Skip the VM. Wine runs the Win64 `visSele.exe` directly on macOS (Rosetta 2 underneath on Apple Silicon). The inspection daemon — WebSocket, OpenCV, BMP_carousel fake camera, lens / field calibration, full BPG protocol — works end-to-end. The only thing that *won't* work is real-hardware HikRobot (Windows kernel driver requirement).

### Prerequisites

```bash
brew install --cask wine-stable
```

First-ever `wine` invocation creates `~/.wine` and asks about Mono/Gecko — say **no** (visSele doesn't use .NET).

### One-time: build a HikRobot-off variant

The HikRobot DLL imports `GenApi_*.dll`, `GCBase_*.dll`, and `MvRender.dll` — those ship only with the HikRobot **MVS** installer, not the SDK headers/lib in this repo. Without them, Wine's loader bails before `main()` runs. So for Wine, build with `FEATURE_HIKROBOT=OFF`:

```bash
VCPKG_ROOT=$HOME/vcpkg cmake -S InspectionCore --preset win-mingw-cross \
  -DFEATURE_HIKROBOT=OFF \
  -B InspectionCore/build/win-mingw-cross-noHik
cmake --build InspectionCore/build/win-mingw-cross-noHik -j
```

(For native HikRobot testing, use a real Windows machine. Wine cannot host the camera kernel driver.)

### Bundle

Reuse `build.sh -e` for everything *except* the .exe — then drop the HikRobot-off .exe over it:

```bash
./InspectionCore/build.sh -p win-cross -c Release -e /tmp/vissele_win_dist
cp InspectionCore/build/win-mingw-cross-noHik/visSele.exe /tmp/vissele_win_dist/
rm /tmp/vissele_win_dist/MvCameraControl.dll    # no longer linked
cp -r InspectionCore/Core0_1/data /tmp/vissele_win_dist/   # runtime config
```

### Run

```bash
cd /tmp/vissele_win_dist
WINEDEBUG=fixme-all wine ./visSele.exe
```

`WINEDEBUG=fixme-all` silences the "fixme" stub warnings. Useful tunings:
- `WINEDEBUG=-all` for the quietest run (also hides Vulkan probe).
- `WINEDEBUG=err+all` to surface every `err:` line during diagnosis.

You should see, within a few seconds:

```
[I] WIN32 WSAStartup ret:0
[I] Try to open websocket... port:4090
opened 0.0.0.0:4090  listenSocket:64
[I] connectCamera driver_name:BMP_carousel id:BMP_carousel_0
[I] load_lens_calib: data/lens_calib.json ok=1 ...
[I] load_field_calib: ... uniformity=85.4%
[I] CameraSetup framerate:2.000000
[E] PHYLayer is not able to establish    ← /dev/cu.usbserial peripheral missing, expected
[I] SetEventCallBack is set...
```

Once that shows, point a browser tab at the WebUI dev server and it'll connect to `ws://localhost:4090` exactly like a native build. Use the **BMP carousel** drawer to feed PNG/BMP images from `data/` for inspection.

### What works under Wine

| Subsystem | Status |
|---|---|
| WinSock (BPG WebSocket on 4090) | ✅ |
| pthread / std::thread / mutex | ✅ |
| File I/O, JSON, cJSON | ✅ |
| OpenCV (imread, imwrite, calib3d, undistort) | ✅ |
| BMP_carousel fake camera + brightness/rotate/noise augmentation | ✅ |
| Lens + field calibration load/apply | ✅ |
| WebUI ↔ core BPG round-trip | ✅ |
| HikRobot real camera | ❌ kernel driver |
| Serial peripheral `/dev/cu.usbserial-*` | ❌ no Mac equivalent (gracefully logged) |

### Known Wine quirks

- **Vulkan probe spam** on startup is harmless; visSele doesn't use Vulkan. Pipe through `grep -vE 'VK_|mvk-info|hid:handle|GPU '` if it bothers you.
- **`err:hid:handle_DeviceMatchingCallback`** lines come from Wine enumerating Mac USB HID devices — ignore.
- **Filesystem path translation**: Wine maps your whole Mac under `Z:\`, so `Z:\tmp\vissele_win_dist\data\BMP_carousel_test\*.png` is the path the fake camera sees when you set the folder from the WebUI drawer.

---

## 6 · build.sh option reference

```text
-p, --platform <id>     mac-arm64 | mac-arm64-opencv | linux | win-cross |
                        win-mingw-msys | win-mingw   (default: mac-arm64)
-c, --config <type>     Debug | Release | RelWithDebInfo   (default: Release)
-e, --export <dir>      bundle built binaries + runtime DLLs into <dir>
--clean                 wipe the build directory before configuring
--no-configure          skip cmake configure/vcpkg; just build + bundle (fast)
--bundle-only           skip configure AND build; only (re)bundle into -e <dir>
-j, --jobs N            parallel build jobs (default: all cores)
```

On Windows, **`win-mingw-msys` is the default platform** for the PowerShell
scripts (prebuilt pacman OpenCV, dynamic — see §3). `win-mingw` is the opt-in
static vcpkg path (the scripts' `-Vcpkg` switch).

Environment overrides:

- `VCPKG_ROOT` — vcpkg location (default `~/vcpkg`).
- `MINGW_PREFIX` — mingw-w64 install prefix (default `/opt/homebrew/opt/mingw-w64`).
- `JOBS` — parallel jobs (same as `--jobs`).
- `NO_CCACHE=1` — disable the auto ccache wiring (default: on if `ccache` is on PATH).

### Speeding up rebuilds

- **Drop `--clean` for normal iteration.** It wipes `vcpkg_installed/` (the OpenCV+ffmpeg+png+jpeg+tiff+... build), `CMakeCache.txt`, and every .o — turning a 10 s edit-rebuild into a 14+ min full reinstall. Reserve it for "reset to known state."
- **`brew install ccache`** (one-time). `build.sh` auto-detects ccache on `PATH` and routes the compiler through it, so even a `--clean` reuses cached .o files (keyed on source content, survives `rm -rf build/`). Disable with `NO_CCACHE=1`.
- **vcpkg binary cache** (`~/.cache/vcpkg/archives/` on macOS / `$XDG_CACHE_HOME` on Linux / `%LOCALAPPDATA%\vcpkg\archives\` on Windows) is on by default. Once a triplet is built once, subsequent clean builds reinstall from the cache in seconds instead of recompiling.

---

## 7 · Troubleshooting

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

---

## 8 · Windows crash dumps (minidump)

On Windows, `visSele.exe` installs a crash handler that writes a **minidump**
(`.dmp` — the Windows equivalent of a core file) when it crashes. It covers both:

- **SEH** faults (access violation, illegal instruction, …) via an unhandled-
  exception filter, and
- **uncaught C++ exceptions**, via `std::set_terminate`.

The dump is named `insp_crash_<pid>_<timestamp>.dmp` and its full path is printed
to **stderr** (`[CRASH] minidump written: …`), so you see it even if the log
drainer is already gone. By default it's a compact minidump; set
**`INSP_CRASH_FULLDUMP=1`** for a full-memory dump (much larger).

### Symbolizing a dump

For useful `file:line` in a MinGW build, build with debug info — i.e.
`-c RelWithDebInfo` (or `-Debug`):

```bash
# on the MinGW build, resolve an address to file:line
addr2line -e visSele.exe -f -C <address>
# or load the exe + dump in gdb
gdb visSele.exe
```

Or open the `.dmp` directly in **WinDbg** / Visual Studio (it carries the crash
context and loaded-module list).
