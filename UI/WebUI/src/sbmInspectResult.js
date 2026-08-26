// What an SBM test run actually tells you, extracted from an II report.
//
// Kept out of SBMStudio.jsx and free of React on purpose: this is the part with
// an ANSWER, so it is the part a contract can hold to. The panel around it only
// renders what these two functions decide.

// Image-mm -> object-frame mm, through the pose the CORE FOUND.
//
// Not through the authored def_image_reg, which is the trap: the studio's canvas
// rectifies by the authored reg, so overlaying a report with that transform
// would draw the measurement in the frame you MEANT rather than the one the
// machine used -- and the two differing is precisely what a test run is for.
//
// This is the inverse of the studio canvas transform in drawImage():
//   ctx.scale(1, flip ? -1 : 1); ctx.rotate(angle); ctx.translate(-cx, -cy)
// which composes as world = FlipY( R(angle) * (p - c) ).
export function objFromImage(pose) {
  const cx = pose && Number.isFinite(pose.cx) ? pose.cx : 0;
  const cy = pose && Number.isFinite(pose.cy) ? pose.cy : 0;
  const a = pose && Number.isFinite(pose.rotate) ? pose.rotate : 0;
  const sy = (pose && pose.isFlipped) ? -1 : 1;
  const ca = Math.cos(a), sa = Math.sin(a);
  return (p) => {
    const dx = p.x - cx, dy = p.y - cy;
    return { x: dx * ca - dy * sa, y: sy * (dx * sa + dy * ca) };
  };
}

// The shortest signed difference between two angles, in radians. A pose delta
// of +179.9deg and -179.9deg is 0.2deg apart, not 359.8.
export function angleDelta(a, b) {
  let d = (a || 0) - (b || 0);
  while (d > Math.PI) d -= 2 * Math.PI;
  while (d < -Math.PI) d += 2 * Math.PI;
  return d;
}

const STATUS = { SUCCESS: 0, FAILURE: -1, UNSET: -100, NA: -128 };

// Every measured primitive in the report, flattened, with the ONE question a
// test run answers per row: did it measure, and if not, why not.
//
// Lines and arcs report a centre (cx,cy); search points and aux points report a
// point (x,y). Both are image-mm. A row with no usable position still appears --
// it is the rows that failed that the operator came here to see, and dropping
// them would make a broken def look like a short list.
const GROUPS = [
  ['detectedLines', 'line', (o) => ({ x: o.cx, y: o.cy })],
  ['detectedCircles', 'arc', (o) => ({ x: o.x, y: o.y })],
  ['searchPoints', 'search_point', (o) => ({ x: o.x, y: o.y })],
  ['auxPoints', 'aux_point', (o) => ({ x: o.x, y: o.y })],
];

export function inspectSummary(rp, authoredReg) {
  const top = rp && rp.reports && rp.reports[0];
  const one = top && top.reports && top.reports[0];
  if (!one) {
    // WHY it did not locate, when the core is willing to say. `locate` is only
    // present when there is a comment to make, and its three fields answer
    // three different problems:
    //   best/thres  -- it saw the part and scored it too low: threshold or
    //                  lighting. The gap is the number to tune against.
    //   candidates 0 with no score -- nothing was even a candidate: training,
    //                  framing or scale, which a threshold will never fix.
    //   region_dropped -- the working region threw the object away before the
    //                  locator ever ran, which looks identical to both above.
    const L = (top && top.locate) || null;
    const dropped = (top && top.region_dropped) || 0;
    let why;
    if (L && Number.isFinite(L.best) && Number.isFinite(L.thres)) {
      why = `最佳比對 ${L.best.toFixed(4)}，門檻 ${L.thres.toFixed(2)}`
          + `（差 ${(L.thres - L.best).toFixed(4)}，試了 ${L.candidates} 個候選）`;
    } else if (L) {
      why = L.reason === 'shape matcher returned no candidate'
        ? '完全沒有候選 — 不是門檻的問題,要看訓練/取景/縮放'
        : L.reason;
    } else if (dropped > 0) {
      why = `檢驗區把 ${dropped} 個物件擋掉了 — 定位器沒跑到`;
    } else {
      why = (top && top.error) || (rp && rp.error) || '核心沒有給原因';
    }
    return { located: false, rows: [], counts: { ok: 0, na: 0, ng: 0 },
             locate: L, regionDropped: dropped, why };
  }

  const pose = { cx: one.cx, cy: one.cy, rotate: one.rotate,
                 isFlipped: !!one.isFlipped, similarity: one.similarity };
  const toObj = objFromImage(pose);

  const rows = [];
  for (const [key, type, pos] of GROUPS) {
    for (const o of (one[key] || [])) {
      const p = pos(o);
      const has = Number.isFinite(p.x) && Number.isFinite(p.y);
      rows.push({
        id: o.id, name: o.name, type,
        status: o.status,
        ok: o.status === STATUS.SUCCESS,
        // The core omits na_reason when it has none. Say so rather than
        // rendering "undefined" at an operator.
        reason: o.na_reason || (o.status === STATUS.SUCCESS ? '' : '(核心沒有給原因)'),
        at: has ? toObj(p) : null,
        hits: ((o.extra && o.extra.cal_hits) || []).length,
      });
    }
  }

  const counts = {
    ok: rows.filter((r) => r.ok).length,
    na: rows.filter((r) => r.status === STATUS.NA || r.status === STATUS.UNSET).length,
    ng: rows.filter((r) => r.status === STATUS.FAILURE).length,
  };

  // The pose the locator found vs the pose the operator drew. This is the
  // number the panel exists for: a def can report every primitive OK and still
  // be sitting on the part 0.3mm off, and nothing else on this screen says so.
  let poseDelta = null;
  if (authoredReg && Number.isFinite(authoredReg.cx) && Number.isFinite(pose.cx)) {
    poseDelta = {
      dx: pose.cx - authoredReg.cx,
      dy: pose.cy - authoredReg.cy,
      dDeg: angleDelta(pose.rotate, authoredReg.angle) * 180 / Math.PI,
      flipDiffers: !!pose.isFlipped !== !!authoredReg.isFlipped,
    };
    poseDelta.dist = Math.hypot(poseDelta.dx, poseDelta.dy);
  }

  return { located: true, pose, poseDelta, rows, counts, toObj, why: '' };
}
