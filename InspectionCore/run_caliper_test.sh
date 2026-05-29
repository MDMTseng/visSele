#!/usr/bin/env bash
# Caliper line/arc test harness: build visSele, run the new (caliper) path + the
# legacy gold-standard path on the 10221 golden sample, dump combined per-caliper
# strip images (CALIP_DUMP), and compare line/circle results vs legacy (== GT).
#
# Usage:
#   ./run_caliper_test.sh           # build + run both + compare
#   ./run_caliper_test.sh nobuild   # skip the build step
set -euo pipefail

ROOT=/Users/mdm/workspace/visSele/InspectionCore
BUILD=$ROOT/build/mac-arm64
TEST=/Users/mdm/workspace/HY_sync/DEV/test
IMG="$TEST/10221 BOS-LT12BH4211 SORTING_bk.png"
LEG="$TEST/10221 BOS-LT12BH4211 SORTING_bk.hydef"
CAL=/tmp/10221_caliper.hydef             # legacy def patched with locating:caliper on line+circle

# 1) build (unless 'nobuild')
if [ "${1:-}" != "nobuild" ]; then
  cmake --build "$BUILD" --target visSele -j8
fi

# 2) regenerate the caliper-patched def: every line + circle primitive -> locating:caliper.
#    Optionally inject an "edge" block from env so you can sweep the selector toolbox:
#      CAL_METHOD = strongest|first|last|middle|nth   (default: leave unset -> strongest)
#      CAL_POL    = any|rising|falling
#      CAL_NTH    = <int>     CAL_MINSTR = <float>
#    e.g.  CAL_METHOD=first CAL_POL=falling CAL_MINSTR=15 ./run_caliper_test.sh nobuild
CAL_METHOD="${CAL_METHOD:-}" CAL_POL="${CAL_POL:-}" CAL_NTH="${CAL_NTH:-}" CAL_MINSTR="${CAL_MINSTR:-}" \
python3 - "$LEG" "$CAL" <<'PY'
import json, sys, os
src, out = sys.argv[1], sys.argv[2]
edge = {}
if os.environ.get('CAL_METHOD'): edge['method'] = os.environ['CAL_METHOD']
if os.environ.get('CAL_POL'):    edge['polarity'] = os.environ['CAL_POL']
if os.environ.get('CAL_NTH'):    edge['nth'] = int(os.environ['CAL_NTH'])
if os.environ.get('CAL_MINSTR'): edge['min_strength'] = float(os.environ['CAL_MINSTR'])
d = json.load(open(src)); n = 0
def walk(o):
    global n
    if isinstance(o, dict):
        if o.get('type') in ('line', 'circle'):
            o['locating'] = 'caliper'
            if edge: o['edge'] = dict(edge)
            n += 1
        for v in o.values(): walk(v)
    elif isinstance(o, list):
        [walk(v) for v in o]
walk(d); json.dump(d, open(out, 'w'), indent=2)
print('patched %d line/circle defs (edge=%s) -> %s' % (n, edge or 'default', out))
PY

# 3) run both (must run from Core0_1/ so the calib map path resolves)
cd "$ROOT/Core0_1"
export DYLD_LIBRARY_PATH="$BUILD"
rm -f /tmp/calip_line_*.png /tmp/calip_arc_*.png

echo "=== NEW (caliper) ==="
CALIP_DUMP=1 "$BUILD/visSele" --insp "$IMG" "$CAL" /tmp/10221_cal.json >/tmp/cal_new.log 2>&1
echo "=== LEGACY (gold standard) ==="
"$BUILD/visSele" --insp "$IMG" "$LEG" /tmp/10221_cal_leg.json >/tmp/cal_leg.log 2>&1

# 4) compare new vs legacy (== ground truth).
#    Lines: angle error (deg) + perpendicular offset (px, the measured quantity).
#    Circles: center distance (px) + radius error (px).
python3 - <<'PY'
import json, math
def collect(fn):
    d = json.load(open(fn)); mmpp = d.get('mmpp', 0.008857849)
    lines, circ = {}, {}
    def walk(o):
        if isinstance(o, dict):
            if o.get('status') is not None and 'vx' in o and 'cx' in o:
                lines[o['id']] = (o['cx']/mmpp, o['cy']/mmpp, o['vx'], o['vy'], o.get('s'))
            elif o.get('status') is not None and 'r' in o and 'x' in o and 'y' in o and 'vx' not in o:
                circ[o['id']] = (o['x']/mmpp, o['y']/mmpp, o['r']/mmpp, o.get('s'))
            for v in o.values(): walk(v)
        elif isinstance(o, list): [walk(v) for v in o]
    walk(d); return lines, circ, mmpp
nl, nc, mmpp = collect('/tmp/10221_cal.json')
ll, lc, _   = collect('/tmp/10221_cal_leg.json')

print("== LINES ==  (angle deg | perp offset px)")
print("%-8s %10s %12s %10s" % ("id","ang_err","perp_off_px","new_rms"))
for i in sorted(set(nl)&set(ll), key=str):
    cx,cy,vx,vy,s = nl[i]; lcx,lcy,lvx,lvy,_ = ll[i]
    an = math.degrees(math.atan2(vy,vx)); al = math.degrees(math.atan2(lvy,lvx))
    da = abs(((an-al+90)%180)-90)                       # line angle diff, 0..90
    nlen = math.hypot(lvx,lvy) or 1.0; nx,ny = -lvy/nlen, lvx/nlen   # legacy normal
    perp = abs((cx-lcx)*nx + (cy-lcy)*ny)               # offset of new anchor from legacy line
    flag = "  <-- big" if (da>0.5 or perp>4) else ""
    print("%-8s %10.3f %12.2f %10s%s" % (i, da, perp, ("%.2f"%s if s is not None else "-"), flag))

print("\n== CIRCLES ==  (center dist px | radius err px)")
print("%-8s %12s %12s %10s" % ("id","ctr_dist_px","rad_err_px","new_rms"))
for i in sorted(set(nc)&set(lc), key=str):
    x,y,r,s = nc[i]; lx,ly,lr,_ = lc[i]
    cd = math.hypot(x-lx, y-ly); dr = abs(r-lr)
    flag = "  <-- big" if (cd>4 or dr>4) else ""
    print("%-8s %12.2f %12.2f %10s%s" % (i, cd, dr, ("%.2f"%s if s is not None else "-"), flag))
PY

echo
echo "strip dumps: /tmp/calip_line_*.png   logs: /tmp/cal_new.log /tmp/cal_leg.log"
echo "[CALIP] lines:  grep CALIP /tmp/cal_new.log"
