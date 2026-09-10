#!/usr/bin/env bash
#
# Set up a Windows machine to build and run visSele.
#
#   Run it in the MSYS2 MINGW64 shell:   ./setupTool/devSetup.sh
#
# It is idempotent -- safe to re-run, and re-running is how you pick up a new
# dependency after a pull. It installs what pacman owns, and for the two things
# that live outside MSYS2 (Node.js and PlatformIO) it CHECKS and tells you what
# to do rather than reaching outside the package manager behind your back.
#
# WHY THIS EXISTS
#   setupTool/msysSetup.sh was the previous answer and had gone stale: no
#   OpenCV (the build's one hard dependency), no ccache, no Node, and a comment
#   pointing at a hand-placed C:\OpenCV-MinGW from the CoreHub era. The working
#   instructions were prose in InspectionCore/Core0_1/BUILD.md. A setup that is
#   only correct in a document is a setup that is wrong on the next machine.
#
# WHAT IT DOES NOT DO
#   - It does not install MSYS2 itself (you are running inside it).
#   - It does not run `pacman -Syu` unless you pass --upgrade. A full system
#     upgrade can require closing the shell mid-way and is not something a
#     setup script should spring on you while you wait for it.
#   - It does not touch vcpkg. That is the opt-in static-OpenCV path
#     (build.sh -p win-mingw) and costs a ~30-60 min dependency build; the
#     default win-mingw-msys path uses the pacman OpenCV and needs none of it.
#
set -uo pipefail

B=$'\033[1m'; G=$'\033[32m'; Y=$'\033[33m'; R=$'\033[31m'; DIM=$'\033[2m'; N=$'\033[0m'
[[ -t 1 ]] || { B=''; G=''; Y=''; R=''; DIM=''; N=''; }
say()  { printf '%s==>%s %s\n' "$B" "$N" "$*"; }
ok()   { printf '  %sok%s   %s\n' "$G" "$N" "$*"; }
warn() { printf '  %s!%s    %s\n' "$Y" "$N" "$*"; }
bad()  { printf '  %sX%s    %s\n' "$R" "$N" "$*"; }

UPGRADE=0
for a in "$@"; do
  case "$a" in
    --upgrade) UPGRADE=1 ;;   # see the warning printed before it runs
    -h|--help)
      sed -n '2,30p' "$0" | sed 's/^#//;s/^ //'
      echo "Options:  --upgrade   also run 'pacman -Syu' first"
      exit 0 ;;
    *) bad "unknown option '$a' -- try --help"; exit 2 ;;
  esac
done

# ---------------------------------------------------------------- the shell --
# MINGW64, not the plain MSYS shell. They have different compilers and
# different package prefixes, and building in the wrong one produces confusing
# link errors rather than an honest refusal.
if [[ "${MSYSTEM:-}" != "MINGW64" ]]; then
  bad "this needs the MSYS2 ${B}MINGW64${N} shell (MSYSTEM is '${MSYSTEM:-unset}')"
  echo "     Start menu -> 'MSYS2 MINGW64', then run this again."
  exit 1
fi
ok "MSYS2 MINGW64 shell"

# A stale lock from a pacman that was killed blocks everything below with a
# message that does not say it is stale. Say it here, with the fix, rather than
# letting the first install fail cryptically.
LCK=/var/lib/pacman/db.lck
if [[ -e "$LCK" ]]; then
  warn "pacman database is locked ($LCK)"
  echo "     If no package manager is running, this lock is stale:  rm -f $LCK"
  exit 1
fi

# ------------------------------------------------------------------ upgrade --
if (( UPGRADE )); then
  warn "--upgrade updates EVERY installed package, including the compiler and OpenCV."
  warn "A major OpenCV bump breaks this build; downgrade from the pacman cache if it happens."
  say "updating the package database and system (this may ask you to reopen the shell)"
  pacman -Syu --noconfirm || { bad "pacman -Syu failed"; exit 1; }
else
  # -Sy WITHOUT -u, on purpose, and only far enough to install something that
  # is missing. It is also why the install below is version-pinned to what is
  # already here: see the note there.
  say "refreshing the package database"
  pacman -Sy --noconfirm >/dev/null 2>&1 || warn "pacman -Sy failed -- continuing with what is cached"
fi

# ------------------------------------------------------------------ packages --
# Every entry earns its place; the comment says which part of the build needs it.
PKGS=(
  mingw-w64-x86_64-toolchain     # gcc, g++, binutils -- addr2line and objcopy
                                 # are used by the crash symbolication path
  mingw-w64-x86_64-cmake         # the build is CMake presets
  mingw-w64-x86_64-make          # mingw32-make, the generator used here
  mingw-w64-x86_64-opencv        # THE hard dependency: find_package(OpenCV REQUIRED)
  mingw-w64-x86_64-pkgconf       # find_package(PkgConfig)
  mingw-w64-x86_64-ccache        # a full rebuild is 285s without it, 23s with
  mingw-w64-x86_64-python        # dev.sh's helper scripts and the overlay zip
  mingw-w64-x86_64-gdb           # not needed to build; needed the day it breaks
  git
  base-devel
)

# INSTALL WHAT IS MISSING. DO NOT TOUCH WHAT IS THERE.
#
# `pacman -S --needed` does not mean "only if absent" -- it skips a reinstall of
# the SAME version and upgrades anything the repo has moved past. Running this
# script on 2026-09-10 therefore silently took OpenCV from 4.13.0 to 5.0.0 on a
# machine that was deliberately pinned to 4.13, and the next core build failed
# in TestPerturb.h on a symbol OpenCV 5 had moved. It also pulled new mingw
# headers and crt underneath a working toolchain.
#
# It got past a pin to do it. /etc/pacman.conf here already carried
#   IgnorePkg = mingw-w64-x86_64-opencv
# and that is honoured by `pacman -Syu` -- but an EXPLICITLY NAMED `pacman -S
# <pkg>` only asks "it is in IgnorePkg, install anyway?", and --noconfirm
# answers yes. So the one guard the machine had was defeated by the one flag a
# script has to pass.
#
# That is the worst thing a setup script can do: the person running it is
# trying to get to a working build, and it broke one that already worked. So
# the default installs only what is genuinely absent -- which never names an
# installed package and therefore never reaches that prompt -- and upgrading is
# something you ask for with --upgrade, having decided to.
say "installing build packages"
_missing=()
for _pkg in "${PKGS[@]}"; do
  # A GROUP (mingw-w64-x86_64-toolchain, base-devel) is not a package, so -Qq
  # never finds one and the first version of this check re-resolved them every
  # run -- which is the upgrade path it was written to close. -Qg answers for
  # groups; a group counts as present once any of it is installed, which for
  # these two is what "the toolchain is here" means.
  pacman -Qq "$_pkg" >/dev/null 2>&1 && continue
  pacman -Qg "$_pkg" >/dev/null 2>&1 && continue
  _missing+=("$_pkg")
done
if (( ${#_missing[@]} == 0 )); then
  ok "all build packages already installed (nothing upgraded)"
elif pacman -S --needed --noconfirm "${_missing[@]}"; then
  ok "installed: ${_missing[*]}"
else
  bad "pacman install failed -- fix the error above and re-run"
  exit 1
fi

# ------------------------------------------------------------------- ccache --
# These two settings are what make ccache worth having on THIS codebase, and
# build.sh exports them per-build so a fresh clone behaves the same. Setting
# them in the user config too means an ad-hoc `make` outside build.sh also
# benefits instead of quietly filling the cache with entries it cannot reuse.
if command -v ccache >/dev/null 2>&1; then
  # ccache on Windows locates its cache through USERPROFILE and refuses to do
  # anything without it. A login MINGW64 shell has it; a stripped environment
  # (a CI runner, a shell spawned by a tool) may not -- and the failure is one
  # line on stderr, which is how the first version of this script cheerfully
  # reported success after configuring nothing.
  # A real MINGW64 shell exports USERPROFILE; a stripped one may not, and
  # ccache refuses to run without it. Derive it rather than give up -- the
  # Windows profile is exactly where MSYS2 puts the user's home anyway.
  if [[ -z "${USERPROFILE:-}" ]]; then
    for _p in "/c/Users/${USERNAME:-${USER:-}}" "$HOME"; do
      [[ -n "$_p" && -d "$_p" ]] || continue
      export USERPROFILE="$(cygpath -w "$_p" 2>/dev/null || echo "$_p")"
      break
    done
  fi
  _cc_err=""
  _cc_set() {
    local out
    if ! out="$(ccache --set-config "$1" 2>&1)"; then _cc_err="$out"; return 1; fi
  }
  # wiringPanel.cpp reports its build time from __DATE__/__TIME__, and ccache
  # will not cache such a file unless told the staleness is acceptable -- which
  # would skip the 14.7k-line unit that dominates every build.
  # On a MISS, depend_mode takes dependencies from the compiler's own -MD output
  # instead of a second preprocessor pass; without it ccache makes a one-line
  # edit SLOWER (24s -> 27s) while only helping full rebuilds.
  if _cc_set sloppiness=time_macros && _cc_set depend_mode=true && _cc_set max_size=20G; then
    ok "ccache $(ccache --version | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+') configured" \
       "(time_macros, depend_mode, 20G)"
  else
    warn "ccache is installed but could not be configured: ${_cc_err:-unknown error}"
    echo "     Builds still work; build.sh exports the same settings per-build."
    if [[ -z "${USERPROFILE:-}" ]]; then
      echo "     USERPROFILE is unset in this shell -- ccache needs it. Run this from"
      echo "     a normal MSYS2 MINGW64 shell rather than a stripped environment."
    fi
  fi
fi

# ------------------------------------------------------------------ opencv --
# The one dependency whose VERSION matters, so it is checked rather than
# assumed. OpenCV 5 moved symbols this tree uses (getRotationMatrix2D out of
# where TestPerturb.h looks for it, calib3d split into calib + geometry), and
# an accidental major upgrade shows up as a compile error in an unrelated file.
if _ocv="$(pacman -Q mingw-w64-x86_64-opencv 2>/dev/null | awk '{print $2}')"; then
  case "$_ocv" in
    4.*) ok "opencv $_ocv" ;;
    *)   warn "opencv $_ocv -- this tree builds against OpenCV 4.x"
         echo "     Downgrade from the pacman cache, e.g.:"
         echo "       pacman -U /var/cache/pacman/pkg/mingw-w64-x86_64-opencv-4.13.0-7-any.pkg.tar.zst"
         echo "     and consider pinning it:  IgnorePkg = mingw-w64-x86_64-opencv  in /etc/pacman.conf" ;;
  esac
fi

# --------------------------------------------------------- outside of MSYS2 --
say "checking what pacman does not own"

if command -v node >/dev/null 2>&1 && command -v npm >/dev/null 2>&1; then
  ok "node $(node -v), npm $(npm -v)  ${DIM}$(command -v node)${N}"
else
  # Deliberately not installed from pacman: the WebUI is built with the same
  # Node the app ships against, and the MSYS2 nodejs package is a different
  # build with its own quirks under Electron.
  warn "node/npm not on PATH -- the WebUI cannot be built"
  echo "     Install Node.js 20+ for Windows (nodejs.org or 'winget install OpenJS.NodeJS.LTS'),"
  echo "     then make sure its directory is on PATH in this shell."
fi

# $HOME in an MSYS2 shell may be /home/<user> OR the Windows profile, depending
# on how MSYS2 was configured -- and PlatformIO always installs under the
# WINDOWS profile. Looking only at $HOME reported "not found" for an install
# that was sitting right there. dev.sh makes the same assumption; this finds it
# either way and prints the path it used, so a mismatch is visible.
PIO=""
for _d in "$HOME" \
          "$(cygpath -u "${USERPROFILE:-}" 2>/dev/null)" \
          "/c/Users/${USERNAME:-$(basename "$HOME")}"; do
  [[ -n "$_d" && -x "$_d/.platformio/penv/Scripts/pio.exe" ]] || continue
  PIO="$_d/.platformio/penv/Scripts/pio.exe"; break
done
if [[ -n "$PIO" ]]; then
  ok "platformio  ${DIM}$PIO${N}"
else
  # Only needed to flash the uInspESP32 board; nothing else in the repo uses it.
  warn "platformio not found -- './dev.sh flash_uinspesp32' will not work"
  echo "     Install the PlatformIO IDE extension in VS Code, or 'pip install platformio'."
fi

# ---------------------------------------------------------------- the repo ---
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -d "$ROOT/UI/WebUI" ]]; then
  if [[ -d "$ROOT/UI/WebUI/node_modules" ]]; then
    ok "WebUI node_modules present  ${DIM}(./dev.sh web to build)${N}"
  else
    warn "WebUI dependencies not installed -- ./dev.sh web installs them on its first run"
  fi
fi
if [[ ! -d "$ROOT/InspectionCore/Core0_1/data" ]]; then
  warn "no InspectionCore/Core0_1/data -- seed it once with: visSele --init-data"
fi

# ------------------------------------------------------------------- report --
say "toolchain"
for t in gcc g++ cmake mingw32-make ccache python git node npm; do
  p="$(command -v "$t" 2>/dev/null)"
  if [[ -n "$p" ]]; then printf '  %-14s %s\n' "$t" "$p"
  else                   printf '  %-14s %s(missing)%s\n' "$t" "$Y" "$N"; fi
done

echo
say "next"
echo "     ./dev.sh core       build the core and install it into the app  ${DIM}~25s${N}"
echo "     ./dev.sh web        build the WebUI                             ${DIM}~35s${N}"
echo "     ./dev.sh up         start the launcher"
echo "     ./dev.sh            the full list"
