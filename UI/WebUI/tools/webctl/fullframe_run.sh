#!/bin/zsh
# A/B harness for the IM send path at FULL SENSOR (where the JPEG encode is
# expensive). Fresh log ring, FI session, full-sensor ROI, N/s for T seconds,
# then print the dview split line.
set -e
cd "$(dirname "$0")"
RATE=${1:-10}; SECS=${2:-45}; TAG=${3:-run}
pkill -f fi_hold 2>/dev/null || true
pkill -f "build/mac-arm64/visSele" 2>/dev/null || true
sleep 3; pkill -f inspd_log 2>/dev/null || true; sleep 1
cd /Users/mdm/workspace/visSele/InspectionCore/Core0_1
INSP_LOG_RING_NAME=ff_$TAG$$ INSP_CAM_TRIGMODE_ONCE=1 INSP_PERIF_CONSOLE=4099 \
  DYLD_LIBRARY_PATH=../build/mac-arm64 ../build/mac-arm64/visSele > /tmp/core_$TAG.log 2>&1 &
sleep 24
cd /Users/mdm/workspace/visSele/UI/WebUI/tools/webctl
node fi_hold.mjs /Users/mdm/workspace/visSele/InspectionCore/Core0_1/data/test1.hydef > /tmp/fi_$TAG.log 2>&1 &
sleep 8
node fi_watch.mjs 70 > /tmp/sub2_$TAG.log 2>&1 &
sleep 2
node roi_full.mjs
sleep 2
node pulse_load.mjs $RATE $SECS
sleep 4
node logdump.mjs > /dev/null
sleep 3
echo "--- $TAG (full sensor, ${RATE}/s x ${SECS}s)"
grep -a "dview split" /Users/mdm/workspace/visSele/InspectionCore/Core0_1/latest_dump.dump | tail -1
grep -a "JPEG size" /Users/mdm/workspace/visSele/InspectionCore/Core0_1/latest_dump.dump | tail -1
