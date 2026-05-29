#!/usr/bin/env bash
# Search-point test harness: build visSele, run the new (caliper) path + the legacy
# gold-standard path on the 10221 golden sample, dump remap images, and compare.
#
# Usage:
#   ./run_sp_test.sh             # build + run both + compare
#   ./run_sp_test.sh nobuild     # skip the build step
set -euo pipefail

ROOT=/Users/mdm/workspace/visSele/InspectionCore
BUILD=$ROOT/build/mac-arm64
TEST=/Users/mdm/workspace/HY_sync/DEV/test
IMG="$TEST/10221 BOS-LT12BH4211 SORTING_bk.png"
LEG="$TEST/10221 BOS-LT12BH4211 SORTING_bk.hydef"
CAL=/tmp/10221_sp_caliper.hydef          # legacy def patched with locating:caliper

# 1) build (unless 'nobuild')
if [ "${1:-}" != "nobuild" ]; then
  cmake --build "$BUILD" --target visSele -j8
fi

# 2) regenerate the caliper-patched def from the gold-standard legacy def
python3 - "$LEG" "$CAL" <<'PY'
import json, sys
src, out = sys.argv[1], sys.argv[2]
d = json.load(open(src)); n = 0
def walk(o):
    global n
    if isinstance(o, dict):
        if o.get('type') == 'search_point' and o.get('angleDeg') is not None:
            o['locating'] = 'caliper'; n += 1
        for v in o.values(): walk(v)
    elif isinstance(o, list):
        [walk(v) for v in o]
walk(d); json.dump(d, open(out, 'w'), indent=2)
print('patched %d search_point defs -> %s' % (n, out))
PY

# 3) run both (must run from Core0_1/ so the calib map path resolves)
cd "$ROOT/Core0_1"
export DYLD_LIBRARY_PATH="$BUILD"
rm -f /tmp/spcv*pt*.png /tmp/spcv_imgpts.csv

echo "=== NEW (caliper) ==="
SPCV_DUMP=1 SP_PT_DUMP=1 "$BUILD/visSele" --insp "$IMG" "$CAL" /tmp/10221_new.json >/tmp/sp_new.log 2>&1
echo "=== LEGACY (gold standard) ==="
SP_LEGACY_DUMP=1 "$BUILD/visSele" --insp "$IMG" "$LEG" /tmp/10221_leg.json >/tmp/sp_leg.log 2>&1

# 4) compare new vs legacy (== ground truth). A search point is a DIRECTIONAL caliper:
#    the measured quantity is the position ALONG the search direction; the along-edge
#    (lateral) position is free and can slide. So the meaningful error is proj_srch
#    (delta projected onto sVnor), NOT euclidean. proj_lat = the don't-care slide.
python3 - <<'PY'
import json, re, math
# per-point search direction (sVnor) from the legacy [SPLEG] dump
sv = {}
for l in open('/tmp/sp_leg.log'):
    m = re.search(r'id=(\d+) pt=\([-0-9.]+,[-0-9.]+\) sVnor=\(([-0-9.]+),([-0-9.]+)\)', l)
    if m: sv[int(m.group(1))] = (float(m.group(2)), float(m.group(3)))
def sps(fn):  # only true search-point report nodes carry 'status'
    d = json.load(open(fn)); mmpp = d.get('mmpp', 0.008857849); seen = {}
    def walk(o):
        if isinstance(o, dict):
            if {'id','x','y'} <= set(o) and 'status' in o and 'cx' not in o and 'r' not in o and 'circumcenter' not in o:
                try: seen[o['id']] = (o['x']/mmpp, o['y']/mmpp)
                except: pass
            for v in o.values(): walk(v)
        elif isinstance(o, list): [walk(v) for v in o]
    walk(d); return seen
new = sps('/tmp/10221_new.json'); leg = sps('/tmp/10221_leg.json')
print("%-5s %8s %11s %10s" % ("id", "euclid", "proj_srch", "proj_lat"))
for i in sorted(set(list(new)+list(leg)), key=str):
    if i not in new or i not in leg: continue
    dx, dy = new[i][0]-leg[i][0], new[i][1]-leg[i][1]
    eu = math.hypot(dx, dy)
    if i in sv:
        sx, sy = sv[i]; ps = abs(dx*sx+dy*sy); pl = abs(dx*(-sy)+dy*sx)
        flag = "  <-- big" if ps > 4 else ""
        print("%-5s %8.1f %11.1f %10.1f%s" % (i, eu, ps, pl, flag))
    else:
        print("%-5s %8.1f %11s %10s" % (i, eu, "-", "-"))
PY

echo
echo "remap dumps: /tmp/spcv_pt*.png   logs: /tmp/sp_new.log /tmp/sp_leg.log"
echo "[SPCV] lines:  grep SPCV /tmp/sp_new.log"
