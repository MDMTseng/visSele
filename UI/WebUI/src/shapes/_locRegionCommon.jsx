// Shared logic for the shape-based localizer's feature-extraction regions:
//   loc_include  — polygon(s) WHERE line2Dup features are generated
//   loc_exclude  — polygon(s) carved OUT (avoid-generation areas)
// Both are authored as ordinary shapes (object-frame mm, exactly like every other
// shape: shape.points[i] is in the same frame as a line's pt1). At save,
// defFileGeneration converts them to the def's localization_include /
// localization_exclude arrays (a direct copy of the points — no transform) and
// removes them from `features`. At load, SetDefInfo rebuilds them from those arrays.
import React from 'react';
import {
  Row, Section, NumberField, TextField,
} from './_propertySheet/primitives.jsx';

// applyDefaults: a region is just an ordered point list. Keep a name for the list.
export function applyDefaultsRegion(shape) {
  if (!Array.isArray(shape.points)) shape.points = [];
  return shape;
}

// Region shapes never reference other shapes.
export function availableRefShapes() { return []; }

// fit-camera: centroid of the polygon points.
export function fitCameraCenterRegion(shape) {
  const pts = shape && shape.points;
  if (!Array.isArray(pts) || pts.length === 0) return null;
  let sx = 0, sy = 0;
  for (const p of pts) { sx += p.x; sy += p.y; }
  return { x: sx / pts.length, y: sy / pts.length };
}

// Build a per-type draw function. `stroke` is the outline color.
export function makeDraw(stroke) {
  return function draw(ctx, shape, renderer /*, opts */) {
    const pts = Array.isArray(shape.points) ? shape.points : [];
    if (pts.length === 0 && !shape._cursor) return;

    ctx.save();
    ctx.lineWidth = renderer.getIndicationLineSize();
    ctx.strokeStyle = stroke;

    // Polygon outline (closed once we have >=3 vertices).
    if (pts.length >= 1) {
      ctx.beginPath();
      ctx.moveTo(pts[0].x, pts[0].y);
      for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i].x, pts[i].y);
      // In-progress preview: rubber-band the last edge to the cursor.
      if (shape._cursor) ctx.lineTo(shape._cursor.x, shape._cursor.y);
      if (pts.length >= 3 && !shape._cursor) ctx.closePath();
      ctx.stroke();
    }

    // Vertices.
    for (const p of pts) renderer.drawpoint(ctx, p);
    ctx.restore();
  };
}

// Generic property sheet for a region. Shows the name, point count, a clear
// button, and — for small polygons — a per-vertex x/y editor (large baked
// polygons are edited by redrawing on the canvas, so the list is suppressed
// past a threshold to keep the panel responsive).
const PER_POINT_EDIT_MAX = 40;

export function makePropertySheet(kindLabel) {
  return function LocRegionPropertySheet({ shape, onUpdate }) {
    const pts = Array.isArray(shape.points) ? shape.points : [];
    const update = (patch) => onUpdate({ ...shape, ...patch });
    const setPoint = (i, patch) => {
      const next = pts.map((p, idx) => (idx === i ? { ...p, ...patch } : p));
      update({ points: next });
    };
    const delPoint = (i) => update({ points: pts.filter((_, idx) => idx !== i) });
    const clearAll = () => update({ points: [] });

    return <div>
      <Row label="type"><span style={{ fontSize: 12 }}>{kindLabel}</span></Row>
      <TextField label="name" value={shape.name}
        onCommit={(name) => update({ name })} />
      <Row label="points"><span style={{ fontSize: 12 }}>{pts.length}</span></Row>
      <Row label=""><button onClick={clearAll} style={{ fontSize: 11 }}>clear all points</button></Row>
      {pts.length > 0 && pts.length <= PER_POINT_EDIT_MAX &&
        <Section label="vertices (mm)">
          {pts.map((p, i) => <div key={i} style={{ display: 'flex', gap: 4, alignItems: 'center' }}>
            <div style={{ flex: 1 }}>
              <NumberField label={`[${i}] x`} value={p.x} onCommit={(x) => setPoint(i, { x })} />
              <NumberField label={`[${i}] y`} value={p.y} onCommit={(y) => setPoint(i, { y })} />
            </div>
            <button onClick={() => delPoint(i)} style={{ fontSize: 11 }}>del</button>
          </div>)}
        </Section>}
      {pts.length > PER_POINT_EDIT_MAX &&
        <Row label=""><span style={{ fontSize: 11, color: '#999' }}>
          {pts.length} pts — redraw on canvas to edit
        </span></Row>}
    </div>;
  };
}
