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
PIO="$HOME/.platformio/penv/Scripts/pio.exe"

# Core builds are memory-hungry: this is an 8 GB machine and -j4 has been OOM
# killed twice with the launcher and an editor open. Override with -j.
CORE_JOBS=2

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
  ( cd "$WEBUI_SRC" && npm run build ) || die "WebUI build failed"
}

step_install() {
  [[ -f "$WEBUI_SRC/dist/index.html" ]] || die "no $WEBUI_SRC/dist -- run './dev.sh web' first"
  [[ -d "$APP_DIR" ]] || die "no $APP_DIR -- is export_v2/app/current.json right?"
  rm -rf "$APP_DIR/WebUI"
  cp -r "$WEBUI_SRC/dist" "$APP_DIR/WebUI"
  ok "installed into $APP_DIR/WebUI"
}

step_core() {
  command -v cmake >/dev/null 2>&1 || export PATH="$MINGW_BIN:$PATH"
  # A running exe in the build dir makes the link fail with a bare "Error 3".
  stop_launcher_quiet
  ( cd InspectionCore && ./build.sh -p win-mingw-msys --no-configure -j "$CORE_JOBS" -e dist/win ) \
    || die "core build failed (out of memory? try -j 1, and close the editor)"
}

step_overlay() {
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

step_esp32() {
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
    [[ "$n" -gt 1 ]] && die "several serial ports ($port) -- name one: ./dev.sh esp32 COM3"
  fi
  say "flashing $ESP32_DIR -> $port"
  # The core holds the port open, so it has to go first.
  stop_launcher_quiet
  ( cd "$ESP32_DIR" && "$PIO" run -e "$ESP32_ENV" -t upload --upload-port "$port" ) \
    || die "flash failed"
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
  local pids; pids="$(launcher_pids)"
  if [[ -z "$pids" ]]; then ok "not running"; return 0; fi
  stop_launcher_quiet
  ok "stopped ($pids)"
}

step_up() {
  [[ -f "$LAUNCHER" ]] || die "launcher not found at $LAUNCHER"
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
      ./dev.sh core            build InspectionCore              ${DIM}~3-4min${N}
      ./dev.sh esp32 [COM3]    flash the uInsp ESP32             ${DIM}~35s${N}
      ./dev.sh overlay [sha]   zip Core + WebUI for the update   ${DIM}~5s${N}

  ${B}The app${N}

      ./dev.sh up              (re)start the launcher
      ./dev.sh down            stop it
      ./dev.sh status          what is built, what is installed, what runs

  ${B}Everything${N}

      ./dev.sh all             core + web + install + overlay + up  ${DIM}~5min${N}

  ${B}Options${N}

      -j N        parallel jobs for the core build (default $CORE_JOBS on this box;
                  4 has been OOM-killed here with the launcher open)
      --keep      do not touch the running launcher
      -h          this text

  ${B}Notes${N}

    * ${B}core${N} and ${B}esp32${N} stop the launcher first, and they have to: a running
      visSele.exe locks the build output, and it holds the serial port open.
    * The installed app is read from export_v2/app/current.json -- currently
      ${B}$VERSION${N}. Switch versions there, not here.
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
  core)     timed "core build" "3-4min" step_core ;;
  overlay)  step_overlay "${1:-}" ;;
  esp32)    timed "ESP32 flash" "35s" step_esp32 "${1:-}" ;;
  up)       step_up ;;
  down)     step_down ;;
  status)   step_status ;;
  ship)
    timed "WebUI build" "35s" step_web
    step_install
    [[ "$KEEP" == 1 ]] && { warn "launcher left alone (--keep); reload it with Ctrl+R"; } || step_up
    ;;
  all)
    timed "core build" "3-4min" step_core
    timed "WebUI build" "35s"   step_web
    step_install
    step_overlay
    [[ "$KEEP" == 1 ]] || step_up
    ;;
  *) die "unknown command '$CMD' -- run ./dev.sh for the list" ;;
esac
printf '%stotal %ss%s\n' "$DIM" "$(( SECONDS - T0 ))" "$N"
