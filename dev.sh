#!/usr/bin/env bash
#
# dev.sh -- build, install and run the app during development.
#
# Everything here is something that was being typed by hand several times an
# hour: build the WebUI, drop it into the installed app, restart the launcher.
# Each step is its own command so a change to one layer does not rebuild the
# others -- the WebUI is 35 seconds, the core is 3 to 4 minutes, and pressing
# them together is how an afternoon disappears.
#
# Run `./dev.sh` with no arguments for the help.
#
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

# ---------------------------------------------------------------- settings --
# The installed app this repo builds into. Read from current.json so it follows
# whatever the launcher is actually running.
VERSION="$(sed -n 's/.*"version"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' export_v2/app/current.json 2>/dev/null | head -1)"
VERSION="${VERSION:-2.0.0-rc2}"
APP_DIR="export_v2/app/$VERSION"
WEBUI_SRC="UI/WebUI"
LAUNCHER="export_v2/launcher/Xception INSP-win32-x64/Xception INSP.exe"
MINGW_BIN="/c/msys64/mingw64/bin"
ESP32_DIR="Peripheral/uInspESP32"
ESP32_ENV="esp32dev"
# PlatformIO installs under the WINDOWS user profile, and $HOME in MSYS2 is
# /home/<user> by DEFAULT -- so this pointed at a directory that does not exist
# on any MSYS2 that was not reconfigured, and flashing failed with "platformio
# not found" while it sat right there. Look in both.
PIO=""
for _pio_dir in "$HOME" \
                "$(cygpath -u "${USERPROFILE:-}" 2>/dev/null)" \
                "/c/Users/${USERNAME:-$(basename "$HOME")}"; do
  [[ -n "$_pio_dir" && -x "$_pio_dir/.platformio/penv/Scripts/pio.exe" ]] || continue
  PIO="$_pio_dir/.platformio/penv/Scripts/pio.exe"; break
done
[[ -n "$PIO" ]] || PIO="$HOME/.platformio/penv/Scripts/pio.exe"   # for the error message

# The Vite dev server. Port and strictPort come from UI/WebUI/vite.config.mjs;
# if you move it there, move it here.
DEV_PORT=8081
DEV_URL="http://localhost:$DEV_PORT"
DEV_LOG="$WEBUI_SRC/.vite-dev.log"
DEV_PID="$WEBUI_SRC/.vite-dev.pid"

# Core builds are memory-hungry: this is an 8 GB machine and -j4 has been OOM
# killed twice with the launcher and an editor open. -j2 was killed too, on
# 2026-09-10, with 1.5 GB free.
#
# One job is also FASTER here, which is not the paradox it looks like: 96% of
# this target's own source is a single 14.7k-line translation unit
# (Core0_1/wiringPanel.cpp), so the critical path is one compiler process and
# the second job only competes with it for memory. Measured the same afternoon:
# -j2 101s, -j1 72s. Raise it with -j once that file is split.
CORE_JOBS=1

# ------------------------------------------------------------------ output --
if [[ -t 1 ]]; then
  B=$'\033[1m'; DIM=$'\033[2m'; G=$'\033[32m'; Y=$'\033[33m'; R=$'\033[31m'; N=$'\033[0m'
else
  B=''; DIM=''; G=''; Y=''; R=''; N=''
fi
say()  { printf '%s==>%s %s\n' "$B" "$N" "$*"; }
ok()   { printf '%s ok %s %s\n' "$G" "$N" "$*"; }
warn() { printf '%s  ! %s %s\n' "$Y" "$N" "$*"; }
die()  { printf '%s ERR%s %s\n' "$R" "$N" "$*" >&2; exit 1; }

# Every step says how long it took, next to how long it usually takes, because
# "is this hung or is this normal" is otherwise unanswerable.
timed() {
  local label="$1" expect="$2"; shift 2
  say "$label ${DIM}(usually ${expect})${N}"
  local t0=$SECONDS
  "$@" || return $?
  local dt=$(( SECONDS - t0 ))
  ok "$label -- ${dt}s"
}

# ------------------------------------------------------------------- steps --

step_web() {
  # A fresh clone has no node_modules, and `npm run build` there fails with
  # vite-not-found rather than saying what is missing. Install once, then build.
  if [[ ! -d "$WEBUI_SRC/node_modules" ]]; then
    say "first build here -- installing WebUI dependencies (a few minutes)"
    ( cd "$WEBUI_SRC" && npm install ) || die "npm install failed"
  fi
  ( cd "$WEBUI_SRC" && npm run build ) || die "WebUI build failed"
}

step_install() {
  [[ -f "$WEBUI_SRC/dist/index.html" ]] || die "no $WEBUI_SRC/dist -- run './dev.sh web' first"
  [[ -d "$APP_DIR" ]] || die "no $APP_DIR -- is export_v2/app/current.json right?"
  rm -rf "$APP_DIR/WebUI"
  cp -r "$WEBUI_SRC/dist" "$APP_DIR/WebUI"
  ok "installed into $APP_DIR/WebUI"
}

# Marker: the app's Core/ holds a FAST-PATH build (unstripped, not bundled).
# step_overlay refuses to ship one, because an update zip built from it would
# carry a 33 MB debug exe and stale DLLs.
CORE_DEV_MARK="$APP_DIR/Core/.dev_fast_build"

# Put the freshly built binaries where the launcher actually runs them.
#
# THIS STEP DID NOT EXIST. `dev.sh core` built into InspectionCore/dist/win and
# stopped there, while the launcher runs export_v2/app/<version>/Core -- so a
# core build followed by `dev.sh up` started the OLD core, silently, and the
# only hint was two different timestamps in `dev.sh status`.
install_core() {
  local src="$1"
  [[ -d "$APP_DIR/Core" ]] || die "no $APP_DIR/Core -- is export_v2/app/current.json right?"
  local n=0
  for exe in visSele.exe inspd_log.exe; do
    [[ -f "$src/$exe" ]] || continue
    cp -f "$src/$exe" "$APP_DIR/Core/$exe" || die "could not replace $APP_DIR/Core/$exe (is it running?)"
    n=$((n + 1))
    # The symbols go with the binary they describe, when the build made any.
    #
    # A full build strips the exe and writes <exe>.debug beside it, and this
    # copied only the exe -- so the shipped app had a stripped core and NO
    # symbols, and a crash on a machine resolved to bare addresses. The fast
    # path deletes any .debug for the opposite reason (it installs an
    # unstripped exe, and a .debug from an older build describes a different
    # binary), so the two paths together kept the app permanently without them
    # and it was being papered over by copying the files across by hand.
    [[ -f "$src/$exe.debug" ]] && cp -f "$src/$exe.debug" "$APP_DIR/Core/$exe.debug"
  done
  [[ "$n" -gt 0 ]] || die "nothing to install from $src"
  ok "installed $n core binaries into $APP_DIR/Core"
}

step_core() {
  local full=0
  [[ "${1:-}" == "full" ]] && full=1
  command -v cmake >/dev/null 2>&1 || export PATH="$MINGW_BIN:$PATH"
  # A running exe in the build dir makes the link fail with a bare "Error 3".
  stop_launcher_quiet

  if [[ "$full" == 1 ]]; then
    # The deployable bundle: strip, archive the symbols, copy the 36 mingw DLLs
    # and the camera runtimes, then crash the result on purpose to prove it
    # symbolicates itself. Measured at 22s on top of the build, every time.
    ( cd InspectionCore && ./build.sh -p win-mingw-msys --no-configure -j "$CORE_JOBS" -e dist/win ) \
      || die "core build failed (out of memory? try -j 1, and close the editor)"
    install_core "InspectionCore/dist/win"
    rm -f "$CORE_DEV_MARK"
    return
  fi

  # FAST PATH -- the default, because it is what iterating actually needs.
  #
  # Compile and link, then copy the two exes into the app. No -e, so none of
  # the packaging runs: nothing is stripped, no symbol archive is written, the
  # 36 DLLs that did not change are not copied again, and the exe is not
  # crashed to test the crash handler. That was 22 of the 72 seconds.
  #
  # The installed exe is the UNSTRIPPED one (~33 MB against 2.8 MB). That is
  # the right trade here: it costs a local file copy and it makes a crash
  # symbolicate from the exe itself, with no .debug to keep in step.
  ( cd InspectionCore && ./build.sh -p win-mingw-msys --no-configure -j "$CORE_JOBS" ) \
    || die "core build failed (out of memory? try -j 1, and close the editor)"
  install_core "InspectionCore/build/win-mingw-msys"
  # dist/win too. It is not the launcher's copy, but it IS where the bench
  # tooling and BUILD.md point, and leaving it behind a fast build recreates
  # exactly the trap this command was changed to close: I started a bench core
  # from dist/win after building, and spent twenty minutes debugging a handler
  # that was not in the binary I was running.
  if [[ -d InspectionCore/dist/win ]]; then
    for _exe in visSele.exe inspd_log.exe; do
      [[ -f "InspectionCore/build/win-mingw-msys/$_exe" ]] || continue
      cp -f "InspectionCore/build/win-mingw-msys/$_exe" "InspectionCore/dist/win/$_exe" 2>/dev/null || true
    done
  fi
  # A .debug left from an earlier full build describes a DIFFERENT binary, and
  # the symbolicator prefers it over the exe. Stale symbols are worse than no
  # symbols: they resolve, plausibly, to the wrong lines.
  rm -f "$APP_DIR/Core/visSele.exe.debug" "$APP_DIR/Core/inspd_log.exe.debug"
  : > "$CORE_DEV_MARK"
  warn "fast build: unstripped, not bundled. './dev.sh core full' before shipping."
}

# THE LAUNCHER IS CODE TOO, and nothing here was copying it.
#
# up_dev_server starts the PACKAGED launcher and points it at Vite, so a WebUI
# edit appears in the window with no rebuild -- but the launcher's own shell
# (its first page, its IPC, main.js) is baked into that .exe's resources/app.
# Editing UI/Launcher and running up_dev_server therefore tested the OLD
# launcher against the NEW WebUI, silently, which cost an afternoon of "the fix
# is not there" on a fix that was.
#
# Same shape as install: replace the code, leave node_modules alone.
step_launcher() {
  local dst="$(dirname "$LAUNCHER")/resources/app"
  [[ -d "$dst" ]] || die "no $dst -- is export_v2/launcher unpacked?"
  for f in main.js preload.js; do
    [[ -f "UI/Launcher/$f" ]] && cp "UI/Launcher/$f" "$dst/$f"
  done
  for d in shell src tools; do
    [[ -d "UI/Launcher/$d" ]] || continue
    rm -rf "${dst:?}/$d"
    cp -r "UI/Launcher/$d" "$dst/$d"
  done
  ok "launcher code copied into $dst"
}

step_overlay() {
  # Never build an update out of a fast build: it would ship a 33 MB unstripped
  # exe next to whatever DLLs happened to be in Core/ already.
  [[ -f "$CORE_DEV_MARK" ]] && die "Core/ holds a fast build -- run './dev.sh core full' first"
  local sha="${1:-$(git rev-parse --short=8 HEAD)}"
  local out="export_v2/app/overlay_${VERSION}_${sha}.zip"
  python - "$APP_DIR" "$out" <<'PY' || die "overlay zip failed"
import os, sys, zipfile
app, out = sys.argv[1], os.path.abspath(sys.argv[2])
os.chdir(app)
z = zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED)
for f in ["Core/visSele.exe", "Core/inspd_log.exe"]:
    if os.path.exists(f): z.write(f, f)
for r, _, fs in os.walk("WebUI"):
    for f in fs:
        p = os.path.join(r, f).replace(os.sep, "/")
        z.write(p, p)
z.close()
n = len(zipfile.ZipFile(out).namelist())
print("   %s  (%d entries, %.1f MB)" % (out, n, os.path.getsize(out) / 1e6))
PY
  ok "overlay zip"
}

# A REAL UPDATE PACKAGE, which is not what step_overlay makes.
#
# overlay writes a bare zip of Core/ + WebUI/ that somebody unzips over a
# version directory by hand. Useful on a bench, and nothing verifies it.
#
# This builds the thing the launcher's updater accepts: info.json for the
# version, scripts/boot.js for how to start it, and a manifest.json carrying a
# SHA256 for EVERY file. The launcher refuses a package with no manifest, with a
# listed file missing, or with a file on disk the manifest does not list -- an
# unlisted file is a file nobody hashed. It installs to a staging directory,
# verifies, and only then renames into place, so a bad package cannot damage the
# version that is running.
#
# Into X2Updates, because that is the folder a machine picks updates up from.
step_pack() {
  [[ -f "$CORE_DEV_MARK" ]] && die "Core/ holds a fast build -- run './dev.sh core full' first"
  local with_launcher=0 ver="" delta_from=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --with-launcher) with_launcher=1 ;;
      --delta-from) delta_from="${2:-}"; shift ;;
      *) ver="$1" ;;
    esac
    shift
  done
  ver="${ver:-$VERSION}"
  local dst="InspectionCore/Core0_1/data/sync/DEV/X2Updates"
  [[ -d "$dst" ]] || die "no $dst -- that is where machines look for updates"
  [[ -f "$APP_DIR/info.json" ]] || die "no $APP_DIR/info.json -- not an assembled app"
  [[ -f "$APP_DIR/scripts/boot.js" ]] || die "no $APP_DIR/scripts/boot.js -- the launcher needs it"
  local out="$dst/update_${ver}_win.zip"

  # DELTA: carry only what differs from a package that was already shipped.
  #
  # The base is named as the previous ZIP rather than as a version, because the
  # zip carries the manifest it was verified against -- so the thing being
  # measured against is what the machines actually have, and nothing extra has
  # to be kept on this bench to make the next delta possible. The app folder is
  # rebuilt in place, so it cannot be its own base.
  #
  # What the package then ASKS for is the lib generation (the version's second
  # field), not this base, so a machine several updates behind can still take
  # it. See make_package.py.
  local delta_args=()
  if [[ -n "$delta_from" ]]; then
    [[ -f "$delta_from" ]] || die "--delta-from: no such package $delta_from"
    delta_args=(--base-package "$delta_from")
    out="$dst/update_${ver}_win_delta.zip"
  fi

  # The launcher rides INSIDE the version, under launcher/, and a postinstall
  # script installs it. Put here rather than bolted onto the zip afterwards
  # because every file in a package must be in manifest.json -- the launcher
  # refuses one that is not -- and the manifest is built from this directory.
  #
  # Left behind when the flag is not given, so a package built without it does
  # not silently carry the last one: both are removed first, every time.
  rm -rf "$APP_DIR/launcher" "$APP_DIR/scripts/postinstall.js"
  if [[ "$with_launcher" == "1" ]]; then
    mkdir -p "$APP_DIR/launcher"
    for f in main.js preload.js; do
      [[ -f "UI/Launcher/$f" ]] && cp "UI/Launcher/$f" "$APP_DIR/launcher/$f"
    done
    for d in shell src tools; do
      [[ -d "UI/Launcher/$d" ]] && cp -r "UI/Launcher/$d" "$APP_DIR/launcher/$d"
    done
    cp UI/Launcher/tools/postinstall_launcher.js "$APP_DIR/scripts/postinstall.js" \
      || die "no UI/Launcher/tools/postinstall_launcher.js"
    ok "launcher code added to the package ($(find "$APP_DIR/launcher" -type f | wc -l) files)"
  fi

  if [[ -n "$delta_from" ]]; then
    say "packaging $APP_DIR as $ver, as a delta onto $(basename "$delta_from")"
  else
    say "packaging $APP_DIR as $ver (about 160 files, ~90 MB, under a minute)"
  fi
  python UI/Launcher/tools/make_package.py "$APP_DIR" "$out" --version "$ver" \
    "${delta_args[@]+"${delta_args[@]}"}" \
    || die "packaging failed"
  ok "update package: $out"
  if [[ -n "$delta_from" ]]; then
    say "it installs on any machine that has a ${ver%.*}.x version already;"
    say "  keep a full package in $dst for everything else"
  fi
  if [[ "$with_launcher" == "1" ]]; then
    say "the launcher's JavaScript is in it; postinstall backs up the old one to"
    say "  <launcher>/launcher_backups/<timestamp>/  and it takes effect at the next start"
  else
    warn "the LAUNCHER is not in this package -- add --with-launcher, or see './dev.sh -h'"
  fi
}

# PUT BACK A LAUNCHER, from what the postinstall kept.
#
# The reason the postinstall makes a backup at all: a launcher that will not
# start cannot be fixed by the launcher, so the way back has to be a file copy
# somebody can run from here -- or by hand, which is all this does.
step_launcher_restore() {
  # ABSOLUTE, both of them. The copy below runs inside the backup directory, so
  # a relative destination resolves against THAT -- which is what the first
  # version did: it wrote every file into the backup it was reading from,
  # touched nothing in resources/app, and said "restored". A restore that
  # silently does nothing is worse than one that fails.
  local lroot dst root
  lroot="$(cd "$(dirname "$LAUNCHER")" && pwd)" || die "cannot resolve $LAUNCHER"
  root="$lroot/launcher_backups"
  dst="$lroot/resources/app"
  [[ -d "$root" ]] || die "no $root -- nothing has replaced the launcher yet"

  local want="${1:-}"
  if [[ -z "$want" ]]; then
    echo "   backups in $root:"
    local n=0
    for d in "$root"/*/; do
      [[ -d "$d" ]] || continue
      n=$((n+1))
      local nf ver
      # BACKUP.json is the label, not launcher code, so it is not counted.
      nf="$(find "$d" -type f ! -name BACKUP.json | wc -l | tr -d " ")"
      ver="$(sed -n "s/.*\"replaced_by_version\": *\"\([^\"]*\)\".*/\1/p" "$d/BACKUP.json" 2>/dev/null)"
      printf "     %s   %s file(s)%s\n" "$(basename "$d")" "$nf" \
             "${ver:+   replaced by $ver}"
    done
    [[ "$n" == 0 ]] && die "none"
    echo "   restore one with: ./dev.sh launcher_restore <timestamp>"
    return 0
  fi

  local src="$root/$want"
  [[ -d "$src" ]] || die "no backup named $want in $root"
  [[ -d "$dst" ]] || die "no $dst"
  stop_launcher_quiet
  local n=0
  while IFS= read -r -d "" f; do
    f="${f#./}"
    mkdir -p "$dst/$(dirname "$f")"
    cp "$src/$f" "$dst/$f" || die "could not restore $f"
    n=$((n+1))
  done < <(cd "$src" && find . -type f ! -name BACKUP.json -print0)
  [[ "$n" == 0 ]] && die "$want contains no files"
  ok "restored $n file(s) from $want into $dst -- ./dev.sh up to start it"
}

step_flash_uinspesp32() {
  local port="${1:-}"
  [[ -x "$PIO" ]] || die "platformio not found at $PIO"
  if [[ -z "$port" ]]; then
    # One CP210x is the normal case; more than one and the choice is the
    # operator's, because flashing the wrong board is not undoable from here.
    port="$(powershell.exe -NoProfile -Command \
      "(Get-CimInstance Win32_PnPEntity | Where-Object { \$_.Name -match 'COM\d+' } | ForEach-Object { if (\$_.Name -match '\((COM\d+)\)') { \$Matches[1] } }) -join ' '" \
      2>/dev/null | tr -d '\r')"
    local n; n=$(wc -w <<<"$port")
    [[ "$n" -eq 0 ]] && die "no serial port found -- is the board plugged in?"
    [[ "$n" -gt 1 ]] && die "several serial ports ($port) -- name one: ./dev.sh flash_uinspesp32 COM3"
  fi
  say "flashing $ESP32_DIR -> $port"
  # The core holds the port open, so it has to go first.
  stop_launcher_quiet
  ( cd "$ESP32_DIR" && "$PIO" run -e "$ESP32_ENV" -t upload --upload-port "$port" ) \
    || die "flash failed"
}

# --------------------------------------------------------- the dev server --
#
# The launcher already knows how to do this: INSP_UI_DEV_URL points the Electron
# window at Vite instead of at the bundle on disk (scripts/boot.js). React
# Refresh then re-mounts the touched component WITHOUT reloading the page, so
# the Redux store and the WS connection to the core survive the edit -- the
# session stays where it was instead of needing the whole bring-up again.

dev_server_pid() {
  [[ -f "$DEV_PID" ]] || return 1
  local pid; pid="$(cat "$DEV_PID" 2>/dev/null)"
  [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null && { echo "$pid"; return 0; }
  rm -f "$DEV_PID"; return 1
}

start_dev_server() {
  if dev_server_pid >/dev/null; then ok "dev server already up ($DEV_URL)"; return 0; fi
  say "starting Vite on $DEV_URL ${DIM}(log: $DEV_LOG)${N}"
  ( cd "$WEBUI_SRC" && nohup npm run dev >"$ROOT/$DEV_LOG" 2>&1 & echo $! > "$ROOT/$DEV_PID" )
  # strictPort is set, so "ready" and "port in use" are the only two outcomes
  # and both are decidable from the log rather than from a fixed sleep.
  local i
  for i in $(seq 1 60); do
    grep -qi "ready in\|Local:" "$DEV_LOG" 2>/dev/null && { ok "dev server up"; return 0; }
    grep -qi "is in use\|EADDRINUSE" "$DEV_LOG" 2>/dev/null && die "port $DEV_PORT is taken -- './dev.sh down_dev_server', or find what is on it"
    dev_server_pid >/dev/null || { tail -20 "$DEV_LOG"; die "vite exited -- see $DEV_LOG"; }
    sleep 1
  done
  warn "vite did not say it was ready in 60s; carrying on -- see $DEV_LOG"
}

step_down_dev_server() {
  local pid
  if pid="$(dev_server_pid)"; then
    # The npm wrapper is the parent; kill the tree or vite keeps the port.
    powershell.exe -NoProfile -Command "taskkill /PID $pid /T /F" >/dev/null 2>&1
    kill -9 "$pid" 2>/dev/null
    rm -f "$DEV_PID"
    ok "dev server stopped ($pid)"
  else
    ok "dev server not running"
  fi
}

step_up_dev_server() {
  start_dev_server
  # The launcher code goes with it. Without this, "edit and restart" quietly
  # meant "the new WebUI against the launcher that was packaged weeks ago".
  step_launcher
  [[ -f "$LAUNCHER" ]] || die "launcher not found at $LAUNCHER"
  stop_launcher_quiet
  say "starting the launcher against $DEV_URL"
  # Passed in the environment, not baked into the app: a production machine
  # must not be able to end up pointing at a dev server because a variable was
  # left behind somewhere. The launcher only accepts loopback URLs.
  INSP_UI_DEV_URL="$DEV_URL" nohup "$LAUNCHER" >/dev/null 2>&1 &
  disown 2>/dev/null || true
  sleep 6
  step_status
  echo "   ${DIM}edit UI/WebUI/src/** and the window updates in place (no rebuild, no restart)${N}"
}

# ------------------------------------------------------------- the launcher --

launcher_pids() {
  powershell.exe -NoProfile -Command \
    "(Get-Process | Where-Object { \$_.ProcessName -match 'Xception|visSele' } | ForEach-Object { \$_.Id }) -join ' '" \
    2>/dev/null | tr -d '\r'
}

stop_launcher_quiet() {
  local pids; pids="$(launcher_pids)"
  [[ -z "$pids" ]] && return 0
  powershell.exe -NoProfile -Command \
    "Get-Process | Where-Object { \$_.ProcessName -match 'Xception|visSele' } | Stop-Process -Force -ErrorAction SilentlyContinue" \
    >/dev/null 2>&1
  sleep 2
}

step_down() {
  step_down_dev_server
  local pids; pids="$(launcher_pids)"
  if [[ -z "$pids" ]]; then ok "not running"; return 0; fi
  stop_launcher_quiet
  ok "stopped ($pids)"
}

step_up() {
  [[ -f "$LAUNCHER" ]] || die "launcher not found at $LAUNCHER"
  # The launcher code goes with it, exactly as up_dev_server already did.
  # Leaving it out here meant the two ways of starting the app disagreed about
  # which launcher they ran: up_dev_server picked up an edit to UI/Launcher and
  # plain `up` silently ran whatever was packaged, so a launcher change tested
  # fine and then vanished the next time somebody started it the other way.
  step_launcher
  stop_launcher_quiet
  powershell.exe -NoProfile -Command "Start-Process -FilePath '$(cygpath -w "$LAUNCHER" 2>/dev/null || echo "$LAUNCHER")'" \
    >/dev/null 2>&1 || die "could not start the launcher"
  sleep 6
  step_status
}

step_status() {
  echo "   version   $VERSION   ${DIM}($APP_DIR)${N}"
  echo "   branch    $(git rev-parse --abbrev-ref HEAD)  $(git rev-parse --short=8 HEAD)"
  local installed built
  installed=$(date -r "$APP_DIR/WebUI/index.html" '+%m-%d %H:%M' 2>/dev/null || echo 'none')
  built=$(date -r "$WEBUI_SRC/dist/index.html" '+%m-%d %H:%M' 2>/dev/null || echo 'none')
  echo "   WebUI     built $built   installed $installed"
  if dev_server_pid >/dev/null; then
    echo "   dev srv   ${G}$DEV_URL${N} (pid $(dev_server_pid))  ${DIM}$DEV_LOG${N}"
  else
    echo "   dev srv   ${DIM}not running${N}"
  fi
  local pids; pids="$(launcher_pids)"
  if [[ -n "$pids" ]]; then
    powershell.exe -NoProfile -Command \
      "Get-Process | Where-Object { \$_.ProcessName -match 'Xception|visSele' } | ForEach-Object { '   running   ' + \$_.ProcessName + ' (' + \$_.Id + ')' }" \
      2>/dev/null | tr -d '\r'
  else
    echo "   running   ${DIM}nothing${N}"
  fi
}

# -------------------------------------------------------------------- help --

usage() {
  cat <<EOF
${B}dev.sh${N} -- build, install and run visSele

  ${B}Everyday loop${N}   change some WebUI code, then:

      ./dev.sh ship            build the WebUI, install it, restart the app
      ./dev.sh ship --keep     ... but leave the running app alone

  ${B}One layer at a time${N}

      ./dev.sh web             build the WebUI only            ${DIM}~35s${N}
      ./dev.sh install         copy the last build into the app  ${DIM}instant${N}
      ./dev.sh core            build InspectionCore + install it ${DIM}~25s${N}
      ./dev.sh core full       ... and strip, bundle and self-test ${DIM}~50s${N}
      ./dev.sh flash_uinspesp32 [COM3]
                               flash the uInsp ESP32             ${DIM}~35s${N}
      ./dev.sh overlay [sha]   bare Core+WebUI zip, unzip by hand ${DIM}~5s${N}
      ./dev.sh pack [version] [--with-launcher] [--delta-from OLD.zip]
                               a VERIFIED update package into X2Updates ${DIM}~1min${N}
                               (info.json + boot.js + per-file SHA256; this is
                                what the launcher's updater will accept)
                               --delta-from carries only what changed ${DIM}~4 MB${N}
      ./dev.sh launcher_restore [timestamp]
                               list, or put back, a launcher the postinstall
                               replaced
      ./dev.sh launcher        copy UI/Launcher into the packaged app instant
                               (up and up_dev_server both do this for you now;
                                this is for copying it without a restart)

  ${B}The app${N}

      ./dev.sh up              (re)start the launcher on the INSTALLED build
                               ${R}a WebUI edit is NOT in it until you build and
                               install it${N} -- ./dev.sh web && ./dev.sh install,
                               or just ./dev.sh ship
      ./dev.sh up_dev_server   (re)start it against Vite -- a WebUI edit then
                               appears in the running window with no rebuild,
                               no restart, and the session left where it was
      ./dev.sh down_dev_server stop Vite (leave the app running)
      ./dev.sh down            stop both
      ./dev.sh status          what is built, what is installed, what runs

  ${B}Everything${N}

      ./dev.sh all             core + web + install + overlay + up  ${DIM}~2min${N}

  ${B}Options${N}

      -j N        parallel jobs for the core build (default $CORE_JOBS on this box;
                  4 has been OOM-killed here with the launcher open)
      --keep      do not touch the running launcher
      -h          this text

  ${B}Notes${N}

    * ${B}core${N} and ${B}flash_uinspesp32${N} stop the launcher first, and they have to: a running
      visSele.exe locks the build output, and it holds the serial port open.
    * ${B}core${N} installs what it built into the app -- so ${B}core${N} then ${B}up${N} runs the
      core you just compiled. It used to build into dist/win and stop there,
      which meant ${B}up${N} quietly kept running the previous one.
    * ${B}core${N} skips packaging (22s of stripping, symbol archiving, copying 36
      unchanged DLLs and crash-testing the result) and installs the unstripped
      exe. ${B}core full${N} does all of it and is what ${B}all${N} and any shipped
      overlay use; ${B}overlay${N} refuses to run on top of a fast build.
    * The installed app is read from export_v2/app/current.json -- currently
      ${B}$VERSION${N}. Switch versions there, not here.
    * ${B}up_dev_server${N} is the one to use while working on the UI. It only
      changes where the window loads its files from -- the core, the machine
      and the data are the same ones ${B}up${N} uses, so what you see is real.
      ${B}up${N} puts it back on the installed bundle.
    * ${R}SWITCHING FROM up_dev_server BACK TO up LOSES EVERY UNBUILT UI EDIT${N},
      and it does it silently: ${B}up${N} loads export_v2/app/<ver>/WebUI, which is
      whatever ${B}install${N} last put there. An afternoon of work on the dev
      server can be sitting only in src/ and in Vite's memory, and the window
      comes back looking like the morning. ${B}status${N} is the check -- it prints
      "WebUI built <t1> installed <t2>" and a t2 older than t1 is exactly this.
      Build and install before you switch, or use ${B}ship${N}, which is both.
    * ${B}pack${N} vs ${B}overlay${N}: overlay is a bare zip for a bench, unzipped over a
      version directory by hand and verified by nobody. ${B}pack${N} is the package
      the launcher itself installs -- staged, hashed file by file, and renamed
      into place only after every hash matches, so a truncated copy on a USB
      stick cannot damage the version that is running.
    * ${B}pack --delta-from X2Updates/update_<old>_win.zip${N} builds a package that
      carries only the files that differ from that one -- about 4 MB instead of
      90, because the core, its symbols and the WebUI are some 29 MB of a 238 MB
      application and the rest is vendor runtime that changes once a year.
      ${B}The second field of the version is the lib generation.${N} 2.0.4 and 2.0.7
      ship the same runtime; 2.1.0 says it changed. A delta records the
      GENERATION, not the package it was measured against, so the launcher takes
      what it did not carry from ANY 2.0.x on the machine -- one that skipped
      four updates included. So bump the second field whenever something outside
      the application changes, and the packager refuses a base from a different
      generation.
      Every reused file is hashed against the manifest before it is used, and
      then the whole assembled directory is verified again, file by file. The
      version number says where to look; it is never believed. A file that
      rotted on disk or differs between two builds of a generation is not a
      match, and the install stops with its name.
      The base is named as the previous ZIP because that zip carries the
      manifest it was verified against; nothing has to be kept on this bench.
      A machine with no version of the generation cannot use a delta, so keep a
      full package in X2Updates and point release.json at THAT.
    * ${B}pack --with-launcher${N} ships the launcher's JavaScript too. A package
      installs into export_v2/app/<version>/, and the launcher lives beside
      that tree and is what DOES the installing -- so the code rides in the
      version under launcher/ and scripts/postinstall.js copies it into
      resources/app after every file has been verified. It takes effect at the
      NEXT launcher start.
      Before replacing anything the postinstall copies what is there to
      <launcher>/launcher_backups/<timestamp>/ and refuses to install if that
      copy fails -- the backup is what makes this reversible, so there is no
      version of it without one. Five are kept. Put one back with
      ${B}./dev.sh launcher_restore${N}, which with no argument lists them.
    * ${R}THE ELECTRON BINARY AND ITS DLLs ARE NEVER IN A PACKAGE${N}. Windows will
      not let a running image be overwritten, and the launcher is the process
      doing the installing, so an Electron version change needs an out-of-band
      install. Only the JavaScript can travel this way.
    * ${B}./dev.sh launcher${N} is the bench path: it copies the JavaScript straight
      into the packaged app here, no package and no backup.
    * Nothing in this script commits, pushes or uploads anything.
EOF
}

# -------------------------------------------------------------------- main --

KEEP=0
ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -j)      CORE_JOBS="$2"; shift 2;;
    --keep)  KEEP=1; shift;;
    -h|--help|help) usage; exit 0;;
    *)       ARGS+=("$1"); shift;;
  esac
done
set -- "${ARGS[@]:-}"

CMD="${1:-}"
[[ -z "$CMD" ]] && { usage; exit 0; }
shift || true

T0=$SECONDS
case "$CMD" in
  web)      timed "WebUI build" "35s" step_web ;;
  install)  step_install ;;
  # `core` is the fast path; `core full` is the deployable one.
  core)     if [[ "${1:-}" == "full" ]]; then timed "core build (full bundle)" "50s" step_core full
            else                              timed "core build" "25s" step_core; fi ;;
  overlay)  step_overlay "${1:-}" ;;
  launcher) step_launcher ;;
  # No shift: main already consumed the command, so "$@" is the arguments.
  pack)     step_pack "$@" ;;
  launcher_restore) step_launcher_restore "${1:-}" ;;
  # Named for the board, not for the chip: this repo has a dozen ESP32
  # firmwares under Peripheral/ and "esp32" did not say which one.
  flash_uinspesp32) timed "uInspESP32 flash" "35s" step_flash_uinspesp32 "${1:-}" ;;
  esp32)    die "renamed: ./dev.sh flash_uinspesp32 [COM3]" ;;
  up)       step_up ;;
  up_dev_server|updev)   step_up_dev_server ;;
  down_dev_server|downdev) step_down_dev_server ;;
  down)     step_down ;;
  status)   step_status ;;
  ship)
    timed "WebUI build" "35s" step_web
    step_install
    [[ "$KEEP" == 1 ]] && { warn "launcher left alone (--keep); reload it with Ctrl+R"; } || step_up
    ;;
  all)
    timed "core build (full bundle)" "50s" step_core full
    timed "WebUI build" "35s"   step_web
    step_install
    step_overlay
    [[ "$KEEP" == 1 ]] || step_up
    ;;
  *) die "unknown command '$CMD' -- run ./dev.sh for the list" ;;
esac
printf '%stotal %ss%s\n' "$DIM" "$(( SECONDS - T0 ))" "$N"
