import { overlayKit, OVERLAY } from 'JSSRCROOT/canvas/overlayKit';
// Per-shape module: OBJ_DETECT — an axis-aligned (object-frame) rectangle region whose
// brightness mean/max and Sobel-edge mean/max the core measures at the located pose and
// self-judges against optional bounds. pt1/pt2 are opposite corners in object-frame mm
// (same frame as every other shape). ignore_rotation keeps it axis-aligned; ignore_
// translation pins it to the teach/absolute image position.
export { ObjDetectPropertySheet as PropertySheet } from './_propertySheet/ObjDetectPropertySheet.jsx';

export const type = 'obj_detect';

export function applyDefaults(shape) {
  if (shape.ignore_rotation === undefined) shape.ignore_rotation = false;
  if (shape.ignore_translation === undefined) shape.ignore_translation = false;
  if (shape.downsample === undefined) shape.downsample = 1;
  // dark_thresh and on_fail are deliberately NOT defaulted here. Absent means
  // "no dark measurement" and "NA on trip" in the core already, and writing them
  // in would rewrite the def -- and its sha1 -- for every existing region that
  // never asked for either.
  return shape;
}

export function availableRefShapes() { return []; }

export function fitCameraCenter(shape) {
  if (!shape.pt1 || !shape.pt2) return null;
  return { x: (shape.pt1.x + shape.pt2.x) / 2, y: (shape.pt1.y + shape.pt2.y) / 2 };
}

export function draw(ctx, shape, renderer) {
  const a = shape.pt1, b = shape.pt2;
  if (!a || !b) return;
  // status color from the latest inspection report (objDetect_by_id), if any.
  const rep = renderer.objDetect_by_id && renderer.objDetect_by_id[shape.id];
  const K = overlayKit(ctx, renderer);
  let color = K.C.search;                                 // no verdict yet
  if (rep) color = (rep.status === 0) ? K.C.ok : (rep.status === -1 ? K.C.ng : K.C.neutral);

  ctx.save();
  ctx.setLineDash([]);
  ctx.lineWidth = renderer.getIndicationLineSize();
  ctx.strokeStyle = color;
  ctx.beginPath();
  ctx.moveTo(a.x, a.y); ctx.lineTo(b.x, a.y); ctx.lineTo(b.x, b.y); ctx.lineTo(a.x, b.y);
  ctx.closePath();
  ctx.stroke();
  // faint fill so an empty region is still visible
  ctx.fillStyle = K.withAlpha(color, (rep && rep.status === -1) ? OVERLAY.alpha.region : OVERLAY.alpha.search);
  ctx.fill();
  // Verdict flag in the top-left corner: it reads at a glance and does not ask
  // anyone to tell two similar outline hues apart.
  if (rep && rep.status !== -128) {
    const x0 = Math.min(a.x, b.x), y0 = Math.min(a.y, b.y), q = 5 * K.ps;
    ctx.fillStyle = color;
    ctx.beginPath(); ctx.moveTo(x0, y0); ctx.lineTo(x0 + q, y0); ctx.lineTo(x0, y0 + q * 0.8); ctx.closePath(); ctx.fill();
  }

  // live stats text (when a report is present).
  // Stats go through draw_Text in chips: raw fillText rotated and mirrored with
  // the view and had no ground under it, and sitting ABOVE the box it collided
  // with whatever was up there. Inside the box, top-left, is safe.
  if (rep && rep.status !== -128 && K.showDetail(Math.abs(b.x - a.x))) {
    const f = (v) => (typeof v === 'number' ? v.toFixed(1) : '—');
    const x0 = Math.min(a.x, b.x), y0 = Math.min(a.y, b.y);
    const rows = [];
    // The dark line is what a clean-space region is actually set up on, so it
    // goes first and only appears when dark_thresh is set (the core omits it).
    if (typeof rep.dark_area_mm2 === 'number')
      rows.push(`暗 ${rep.dark_area_mm2.toFixed(3)}mm² (${(rep.dark_ratio * 100).toFixed(2)}%)`);
    rows.push(`B ${f(rep.bright_mean)}/${f(rep.bright_max)}  E ${f(rep.edge_mean)}/${f(rep.edge_max)}`);
    if (shape.name) rows.unshift(shape.name);
    const fs = OVERLAY.font.small, lh = 1.5 * K.fpx * fs;
    rows.forEach((t, i) => {
      const w = 0.62 * K.fpx * fs * t.length;
      K.chip(t, x0 + w / 2 + 3 * K.ps, y0 + 5 * K.ps + i * lh, color, fs);
    });
  }
  ctx.restore();
}
