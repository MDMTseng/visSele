#!/bin/bash
# Walk the pairing to its edge, one deterministic fault at a time.
#
# Overload is the wrong instrument for this. Harsh conditions are supposed to
# end in a halt and they do -- CAM_CLOCK_LOST with nothing mis-sorted -- but a
# halt tells you nothing about WHERE the edge is, and the one mis-sort ever seen
# under overload took thirteen runs to appear once and could not be attributed
# afterwards. So inject the fault directly, one report at a time.
#
# Each row restarts the core with a different injected fault and runs the slip
# probe. What to expect, and why:
#
#   ts +2000    inside the 5000us window. Should match correctly: no halt, no
#               misplacement. This is the "recoverable" band.
#   ts +8000    outside the window but far short of the 33000us spacing. The
#               true object is still the NEAREST, so the machine should refuse
#               (a miss) rather than pick anyone else. One miss is tolerated;
#               two consecutive halt.
#   ts +33000   one full object spacing. The neighbour is now genuinely the
#               nearest AND inside the window -- the evidence really does point
#               at it. A mis-sort here is not a bug, it is the boundary, and
#               this run exists to show exactly where it sits.
#   drop        the answer never arrives. That part must skip and the machine
#               must keep going.
#   dup         the same answer twice. The second has nothing left to land on.
#
# Usage: ./edge_sweep.sh [seconds]
set -u
SEC=${1:-60}
ROOT=/Users/mdm/workspace/visSele
CORE=$ROOT/InspectionCore/Core0_1
TOOLS=$ROOT/Peripheral/uInspESP32/tools
DEF=$CORE/data/test1.hydef
DRV=$ROOT/UI/WebUI/tools/webctl/sync_driver.mjs
LOGDIR=${SCRATCH:-/tmp}

run_case() {
  local name="$1"; shift
  echo ""
  echo "=============================================================="
  echo "  $name"
  echo "=============================================================="
  pkill -TERM -f sync_driver.mjs >/dev/null 2>&1
  sleep 1
  pkill -TERM -f "build/mac-arm64/visSele" >/dev/null 2>&1
  sleep 5
  ( cd "$CORE" && env "$@" \
      INSP_PERIF_LOG=1 INSP_PERIF_CONSOLE=4099 \
      INSP_PERIF_VERDICT_PATTERN=20260806 \
      nohup ../build/mac-arm64/visSele > "$LOGDIR/edge_$name.log" 2>&1 & )
  sleep 12
  ( cd "$(dirname "$DRV")" && nohup node "$DRV" "$DEF" 2400 >/dev/null 2>&1 & )
  sleep 8
  ( cd "$TOOLS" && python3 slip_probe.py --seconds "$SEC" 2>&1 | tail -8 )
}

run_case "ts_plus_2000"  INSP_PERIF_FAULT_EVERY=5  INSP_PERIF_FAULT_TS_US=2000
run_case "ts_plus_8000"  INSP_PERIF_FAULT_EVERY=5  INSP_PERIF_FAULT_TS_US=8000
run_case "ts_plus_33000" INSP_PERIF_FAULT_EVERY=5  INSP_PERIF_FAULT_TS_US=33000
run_case "drop"          INSP_PERIF_FAULT_EVERY=5  INSP_PERIF_FAULT_DROP=1
run_case "dup"           INSP_PERIF_FAULT_EVERY=5  INSP_PERIF_FAULT_DUP=1

echo ""
echo "leaving the machine stopped"
python3 - <<'EOF'
import socket, time
try:
    s = socket.create_connection(('127.0.0.1', 4099), timeout=5); s.settimeout(0.4)
    for c in (b'{"type":"set_setup","plate_freq":0}\n', b'{"type":"exit_insp_mode"}\n'):
        s.sendall(c); time.sleep(0.8)
    s.close()
except Exception as e:
    print("could not confirm shutdown: %s" % e)
EOF
pkill -TERM -f sync_driver.mjs >/dev/null 2>&1
