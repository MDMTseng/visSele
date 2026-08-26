// What an SBM test run actually tells you, extracted from an II report.
//
// Kept out of SBMStudio.jsx and free of React on purpose: this is the part with
// an ANSWER, so it is the part a contract can hold to. The panel around it only
// renders what these two functions decide.

// Image-mm -> the STUDIO CANVAS's world, which is the frame the picture is
// drawn in.
//
// This is the inverse of the canvas transform in drawImage():
//   ctx.scale(1, flip ? -1 : 1); ctx.rotate(angle); ctx.translate(-cx, -cy)
// composing as world = FlipY( R(angle) * (p - c) ).
//
// FEED IT THE SAME TRANSFORM THE IMAGE GOT, which is the AUTHORED
// def_image_reg. It first took the pose the core FOUND, on the reasoning that
// a report should be read in the frame the machine used -- and that put every
// measurement in a frame the picture is not in. The rings landed exactly on
// the def's own shapes and floated over blank background, because that is
// where the def's shapes are; the part was elsewhere. An overlay that is
// correct relative to the recipe and wrong relative to the photograph is
// wrong, and it fails hardest in the case it exists for: when the part is NOT
// where the def expects.
//
// The found pose still has a job -- it is what the pose DELTA is measured
// from -- but it is not how anything gets drawn.
//
// Accepts either key for the angle: a report says `rotate`, def_image_reg says
// `angle`. Two names for one quantity is how the wrong one gets read as 0.
export function objFromImage(pose) {
  const cx = pose && Number.isFinite(pose.cx) ? pose.cx : 0;
  const cy = pose && Number.isFinite(pose.cy) ? pose.cy : 0;
  const a = pose && Number.isFinite(pose.rotate) ? pose.rotate
          : (pose && Number.isFinite(pose.angle) ? pose.angle : 0);
  const sy = (pose && pose.isFlipped) ? -1 : 1;
  const ca = Math.cos(a), sa = Math.sin(a);
  return (p) => {
    const dx = p.x - cx, dy = p.y - cy;
    return { x: dx * ca - dy * sa, y: sy * (dx * sa + dy * ca) };
  };
}

// The object's 0-degree axis as an IMAGE-frame angle, which is NOT `rotate`.
//
// MEASURED, not read off the source. `visSele --insp img def out '{"rot_deg":5}'`
// reports rotate = +5.0000 deg, and test_perturb.cpp pins rot_deg +5 as moving
// image content to -5 deg in raw image atan2 (its own check negates y before
// the atan2, which is where the sign hides). So:
//
//     image-frame angle = -rotate
//
// The core says the same thing in code -- SingleMatching does `angle = -angle`
// with the comment "the angle we get from matching is current object rotates
// 'angle' to match target ... we want to rotate feature set to match current
// object, so opposite direction" -- and then builds
// TemplateDomain_TO_PixDomain from that negated angle. Reading it there was not
// enough: every def on this bench has a registration angle of ~0, so the sign
// was invisible until an orientation stub pointed the wrong way on a machine.
//
// def_image_reg.angle is in ROTATE space, not image space: DefConfUI stores it
// as `angle: reg.rotate`, straight off an inspection report. That is why the
// canvas rotating by +reg.angle rectifies correctly -- it is rotating by minus
// the image angle -- and why a pose delta may compare rotate to reg.angle
// directly. Mixing the two spaces is the one mistake this whole note exists to
// prevent.
export function imageAngleOf(rotate) { return -(rotate || 0); }

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
      // The locator RAN and succeeded; the working region rejected what it
      // found. Saying "the locator did not run" here was backwards, and it is
      // the difference between "this def is broken" and "the part is not at the
      // station" -- which are looked for in completely different places.
      why = `定位器找到了 ${dropped} 個,但都被檢驗區擋掉 — 零件不在工位範圍內`;
    } else {
      why = (top && top.error) || (rp && rp.error) || '核心沒有給原因';
    }
    return { located: false, rows: [], counts: { ok: 0, na: 0, ng: 0 },
             locate: L, regionDropped: dropped, why };
  }

  // EVERY object the core found, not just the first.
  //
  // The bench frame that showed this up has three parts in it. Reporting only
  // reports[0] means two of them are inspected, judged, and invisible here --
  // and if the def locates the wrong one, the panel confidently describes a
  // part the operator is not looking at.
  const objs = top.reports;

  // ONE transform for every object, and it is the canvas's own: the picture is
  // drawn through the authored reg, so this is what puts a reported point on
  // the pixel it came from. It is also the only choice that works with more
  // than one object -- rectifying the image to a found pose can only ever
  // straighten one of them, and would then misplace the rest.
  const toCanvas = objFromImage(authoredReg || { cx: 0, cy: 0, angle: 0 });

  const rows = [];
  objs.forEach((obj, oi) => {
    for (const [key, type, pos] of GROUPS) {
      for (const o of (obj[key] || [])) {
        const p = pos(o);
        const has = Number.isFinite(p.x) && Number.isFinite(p.y);
        rows.push({
          obj: oi, id: o.id, name: o.name, type,
          status: o.status,
          ok: o.status === STATUS.SUCCESS,
          // The core omits na_reason when it has none. Say so rather than
          // rendering "undefined" at an operator.
          reason: o.na_reason || (o.status === STATUS.SUCCESS ? '' : '(核心沒有給原因)'),
          at: has ? toCanvas(p) : null,
          hits: ((o.extra && o.extra.cal_hits) || []).length,
        });
      }
    }
  });

  const counts = {
    ok: rows.filter((r) => r.ok).length,
    na: rows.filter((r) => r.status === STATUS.NA || r.status === STATUS.UNSET).length,
    ng: rows.filter((r) => r.status === STATUS.FAILURE).length,
  };

  // Per object: where the locator put it, and how far that is from where the
  // operator drew the registration. A def can report every primitive OK and
  // still be sitting on the part 0.3mm off, and nothing else on this screen
  // says so.
  const poses = objs.map((obj) => {
    const pose = { cx: obj.cx, cy: obj.cy, rotate: obj.rotate,
                   isFlipped: !!obj.isFlipped, similarity: obj.similarity };
    pose.at = toCanvas({ x: pose.cx, y: pose.cy });     // where to draw its marker
    // The object's 0-degree axis, as a SECOND POINT through the same transform
    // rather than as an angle through a formula.
    //
    // Composing the canvas rotation with the found rotation by hand needs the
    // sign of ctx.rotate, the order the flip lands in, and whether the reg
    // angle adds or subtracts -- three chances to be wrong, and the result
    // looks plausible whichever way it comes out, so it is not self-checking.
    // Mapping a point one mm along the axis cannot get any of that wrong,
    // because it is the same call that puts the measurements on the part.
    const ia = imageAngleOf(pose.rotate);
    pose.axis = toCanvas({ x: pose.cx + Math.cos(ia), y: pose.cy + Math.sin(ia) });
    if (authoredReg && Number.isFinite(authoredReg.cx) && Number.isFinite(pose.cx)) {
      const d = {
        dx: pose.cx - authoredReg.cx,
        dy: pose.cy - authoredReg.cy,
        dDeg: angleDelta(pose.rotate, authoredReg.angle) * 180 / Math.PI,
        flipDiffers: !!pose.isFlipped !== !!authoredReg.isFlipped,
      };
      d.dist = Math.hypot(d.dx, d.dy);
      pose.delta = d;
    }
    return pose;
  });

  // The first object stays the headline, because the single-value readouts and
  // the sweep are written against one number. Which one it is is now stated
  // rather than assumed.
  const pose = poses[0];
  const poseDelta = pose.delta || null;

  return { located: true, pose, poses, poseDelta, rows, counts, toCanvas, why: '' };
}
