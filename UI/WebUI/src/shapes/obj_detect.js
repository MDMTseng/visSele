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
  let color = '#26c6da';                                  // default cyan
  if (rep) color = (rep.status === 0) ? '#00e676' : (rep.status === -1 ? '#ff5252' : '#bdbdbd');

  ctx.save();
  ctx.lineWidth = renderer.getIndicationLineSize();
  ctx.strokeStyle = color;
  ctx.beginPath();
  ctx.moveTo(a.x, a.y); ctx.lineTo(b.x, a.y); ctx.lineTo(b.x, b.y); ctx.lineTo(a.x, b.y);
  ctx.closePath();
  ctx.stroke();
  // faint fill so an empty region is still visible
  ctx.fillStyle = (rep && rep.status === -1) ? 'rgba(255,82,82,0.10)' : 'rgba(38,198,218,0.08)';
  ctx.fill();

  // live stats text (when a report is present).
  if (rep && rep.status !== -128) {
    const pr = renderer.getPointSize ? renderer.getPointSize() : 0.05;
    ctx.fillStyle = color;
    const tx = Math.min(a.x, b.x), ty = Math.min(a.y, b.y) - pr * 2;
    const f = (v) => (typeof v === 'number' ? v.toFixed(1) : '—');
    ctx.font = `${pr * 6}px sans-serif`;
    ctx.fillText(`B ${f(rep.bright_mean)}/${f(rep.bright_max)}  E ${f(rep.edge_mean)}/${f(rep.edge_max)}`, tx, ty);
  }
  ctx.restore();
}
