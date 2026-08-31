// Self-contained SBM localization "studio": a hook-driven canvas (no global redux
// state machine) + an SBM-only toolbar, hosted in a full-screen modal. All drawing and
// interaction live in `sbmDrawHook`; the committed data still lives in the def
// (loc_include/loc_exclude shapes in shapeList + def_image_reg), so the existing save
// (defFileGeneration) round-trips unchanged. See InspectionCore/docs/sbm_setup_studio_plan.md.
import React, { useRef, useState, useCallback, useEffect } from 'react';
import { useSelector, useDispatch } from 'react-redux';
import ReactResizeDetector from 'react-resize-detector';
import { Button, Divider, Checkbox, InputNumber, Select, Modal } from 'antd';

import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import { defFileGeneration, stampRefImagePath } from 'UTIL/MISC_Util';
import { inspectSummary } from './sbmInspectResult';
import { useDefImages } from 'UTIL/useDefImages';
import { SWEEP_AXES, sweepValues, perturbFor, sweepRow, sweepVerdict } from './sbmSweep';
import { acceptanceFloor, headroom } from 'UTIL/matchThreshold';

// ── React wrapper: mount a DrawHook_CanvasComponent on a <canvas>, feed it the
// image + the draw hook, resize via ReactResizeDetector. ──────────────────────
export function HookCanvasComponent({ dhook, image, captureDrag, style, mmpp }) {
  const canvasRef = useRef(null);
  const _ = useRef({ canvComp: undefined });

  useEffect(() => {
    if (!canvasRef.current) return undefined;
    if (!_.current.canvComp)
      _.current.canvComp = new EC_CANVAS_Ctrl.DrawHook_CanvasComponent(canvasRef.current);
    const c = _.current.canvComp;
    return () => { if (c && c.resourceClean) c.resourceClean(); _.current.canvComp = undefined; };
  }, [canvasRef]);

  // dhook / captureDrag changes → rebind + redraw.
  useEffect(() => {
    const c = _.current.canvComp; if (!c) return;
    c.drawHook = dhook;
    c.captureDrag = !!captureDrag;
    c.ctrlLogic(); c.draw();
  }, [dhook, captureDrag]);

  // image changes → load + redraw.
  //
  // The `!image` bail is why a brand-new object opened the SBM studio on the
  // PREVIOUS def's picture. edit_info.img is null until something is captured,
  // this effect returned, and the canvas kept the pixels it was last given --
  // and SetImg cannot undo that either, because it guards `img_info == null`
  // too. Nothing in the chain can clear an image once one has been drawn.
  //
  // Remounting is the clear: a null image bumps the key below, React drops the
  // canvas element and DrawHook_CanvasComponent with it, and what comes back
  // has never had an image. Only the has-image/no-image transition changes the
  // key, so swapping one picture for another still reuses the canvas and keeps
  // the view where it was.
  useEffect(() => {
    const c = _.current.canvComp; if (!c || !image) return;
    // TELL THE CANVAS THE SCALE BEFORE HANDING IT THE PICTURE.
    //
    // SetImg fits the image to the view exactly once, and the fit divides by
    // rUtil's mmpp -- which nobody had ever set on this canvas, so it was 1.
    // The scene is drawn at the def's real mmpp (~0.014 for this camera), so
    // the fit came out ~70x too small: a new object opened in the studio as a
    // forty-pixel stamp near the corner, and the operator had to zoom in by
    // hand before the registration line could be drawn at all.
    //
    // Invisible on an OLD def, which is why it survived: those carry a
    // signature, the def canvas has already been panned by the time anyone
    // opens the studio, and a wrong first fit just looks like a view someone
    // left zoomed out. It is the new-object path -- the one place with nothing
    // to compare against -- where it actually costs something.
    if (Number.isFinite(mmpp) && mmpp > 0) c.rUtil.renderParam.mmpp = mmpp;
    c.SetImg(image); c.draw();
  }, [image, mmpp]);

  const onResize = (w, h) => { const c = _.current.canvComp; if (c) { c.resize(w, h); c.draw(); } };

  return <div style={{ width: '100%', height: '100%', position: 'relative', ...style }}>
    <canvas ref={canvasRef} style={{ width: '100%', height: '100%', display: 'block' }} />
    <ReactResizeDetector handleWidth handleHeight onResize={onResize} />
  </div>;
}

// ── Draw the reference image into the canvas world frame. ──────────────────────
// Normal (object) frame: rectify by def_image_reg so object-frame-mm overlays land on
// the part. Raw frame (locline tool): only scale by mmpp, so world == image-mm and the
// drawn localization line yields def_image_reg directly.
function drawImage(g, canvas, reg, mmpp, rawFrame) {
  const sec = canvas.secCanvas;
  if (!sec || sec.width === 0) return;
  const ctx = g.ctx;
  ctx.save();
  if (!rawFrame) {
    ctx.scale(1, reg.isFlipped ? -1 : 1);
    ctx.rotate(reg.angle || 0);
    ctx.translate(-(reg.cx || 0), -(reg.cy || 0));
  }
  ctx.scale(mmpp, mmpp);
  ctx.imageSmoothingEnabled = false;
  ctx.drawImage(sec, 0, 0);
  ctx.restore();
}

function poly(ctx, pts, close) {
  ctx.beginPath();
  ctx.moveTo(pts[0].x, pts[0].y);
  for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i].x, pts[i].y);
  if (close) ctx.closePath();
}

// ── The SBM scene draw. Everything is in object-frame mm (== world), except in
// locline mode where we show the raw image to author def_image_reg. ────────────
function drawScene(g, canvas, ctx_state) {
  const { reg, mmpp, shapeList, work, featPts, roiPts, tool, insp } = ctx_state;
  const ctx = g.ctx;
  const scale = canvas.camera.GetCameraScale() || 100;
  const lw = 1.6 / scale, pr = 2.4 / scale;
  const rawFrame = (tool === 'locline');

  drawImage(g, canvas, reg, mmpp, rawFrame);

  if (rawFrame) {
    // Only the in-progress localization line (image-mm).
    if (work.line) {
      const { o, t } = work.line;
      ctx.lineWidth = lw; ctx.strokeStyle = '#ffab00';
      ctx.beginPath(); ctx.moveTo(o.x, o.y); ctx.lineTo(t.x, t.y); ctx.stroke();
      ctx.fillStyle = '#ff6d00'; ctx.beginPath(); ctx.arc(o.x, o.y, pr * 1.6, 0, 7); ctx.fill();
    }
    return;
  }

  // include / exclude regions (filled).
  for (const s of shapeList) {
    if (s.type !== 'loc_include' && s.type !== 'loc_exclude') continue;
    const pts = s.points || []; if (pts.length < 2) continue;
    const inc = (s.type === 'loc_include');
    poly(ctx, pts, true);
    ctx.fillStyle = inc ? 'rgba(0,200,80,0.16)' : 'rgba(255,70,70,0.20)';
    ctx.fill();
    ctx.lineWidth = lw; ctx.strokeStyle = inc ? '#00c853' : '#ff5252'; ctx.stroke();
  }

  // generated line2Dup feature points (blue) from the round-trip — what the localizer
  // keys on. (The auto ROI selection is NOT drawn here; "自動產生 ROI 點" turns it into
  // editable orange points instead.)
  if (featPts) {
    ctx.fillStyle = '#29b6f6';
    for (const p of (featPts.features || [])) { ctx.beginPath(); ctx.arc(p.x, p.y, pr * 0.8, 0, 7); ctx.fill(); }
  }

  // ROI refine points (orange squares) — the ONLY ROI representation; saved as
  // roi_refine_points. Empty = no ROI refine.
  if (roiPts && roiPts.length) {
    ctx.strokeStyle = '#ff9100'; ctx.lineWidth = lw * 1.4;
    for (const p of roiPts) ctx.strokeRect(p.x - pr * 1.8, p.y - pr * 1.8, pr * 3.6, pr * 3.6);
  }

  // TEST RESULT: where the core actually measured, in the frame the core used.
  //
  // Drawn LAST so it sits over the regions and the feature points -- when this
  // is on screen it is the thing being looked at. Each row gets a ring at the
  // measured position and, for a row whose def shape is on the canvas, a stem
  // back to where the def put it: the stem IS the measurement, and a long one
  // is visible without reading a number.
  if (insp && insp.located) {
    // Every located object gets a marker at the pose the core put it, drawn in
    // the CANVAS's frame -- same transform the picture got. These land on the
    // parts. The def's own shapes stay where the def says they are, so when the
    // two are far apart the picture is telling you the part is not where the
    // recipe expects, which is the thing worth seeing.
    for (const P of (insp.poses || [])) {
      if (!P.at || !Number.isFinite(P.at.x)) continue;
      ctx.strokeStyle = '#ffd54f'; ctx.lineWidth = lw * 1.2;
      ctx.beginPath(); ctx.arc(P.at.x, P.at.y, pr * 4, 0, 7); ctx.stroke();
      // A stub along the found 0-degree axis, so a rotated or flipped match
      // reads as a direction and not only as a number in the panel.
      //
      // Direction from the two transformed points, never from an angle
      // recomposed here. Length in SCREEN terms like every other marker on this
      // canvas -- the first version used 3 mm, which at this def's ~0.009 mm/px
      // is about 340 px and ran off the edge of the frame.
      if (P.axis && Number.isFinite(P.axis.x)) {
        const dx = P.axis.x - P.at.x, dy = P.axis.y - P.at.y;
        const n = Math.hypot(dx, dy);
        if (n > 1e-9) {
          // 3 mm in WORLD units, back at the operator's request: a long line
          // reads as a bearing across the frame, which is what it is for. It
          // was briefly made screen-proportional because it ran off the edge --
          // but that was the direction being wrong, not the length.
          const L = 3;
          ctx.beginPath(); ctx.moveTo(P.at.x, P.at.y);
          ctx.lineTo(P.at.x + (dx / n) * L, P.at.y + (dy / n) * L);
          ctx.stroke();
        }
      }
    }

    for (const r of insp.rows) {
      if (!r.at) continue;
      const col = r.ok ? '#00e676' : '#ff1744';
      ctx.strokeStyle = col; ctx.lineWidth = lw * 1.6;
      ctx.beginPath(); ctx.arc(r.at.x, r.at.y, pr * 2.2, 0, 7); ctx.stroke();
      if (!r.ok) {                                   // an X, so a failure reads without colour
        const e = pr * 1.5;
        ctx.beginPath();
        ctx.moveTo(r.at.x - e, r.at.y - e); ctx.lineTo(r.at.x + e, r.at.y + e);
        ctx.moveTo(r.at.x + e, r.at.y - e); ctx.lineTo(r.at.x - e, r.at.y + e);
        ctx.stroke();
      }
    }
  }

  // localization origin (object (0,0)) + 0° axis.
  ctx.lineWidth = lw; ctx.strokeStyle = '#ffab00';
  ctx.beginPath(); ctx.moveTo(0, 0); ctx.lineTo(2, 0); ctx.stroke();   // +x axis, 2mm
  ctx.fillStyle = '#ff6d00'; ctx.beginPath(); ctx.arc(0, 0, pr * 1.6, 0, 7); ctx.fill();

  // in-progress polygon.
  if (work.poly && work.poly.length) {
    const p = work.poly;
    ctx.beginPath(); ctx.moveTo(p[0].x, p[0].y);
    for (let i = 1; i < p.length; i++) ctx.lineTo(p[i].x, p[i].y);
    if (work.cursor) ctx.lineTo(work.cursor.x, work.cursor.y);
    ctx.lineWidth = lw; ctx.strokeStyle = '#ffd54f'; ctx.stroke();
    ctx.fillStyle = '#ffd54f';
    for (const v of p) { ctx.beginPath(); ctx.arc(v.x, v.y, pr, 0, 7); ctx.fill(); }
  }
}

// ── The SBM control (mouse) logic. ─────────────────────────────────────────────
function ctrlScene(g, canvas, ctx_state) {
  const { tool, work, roiPts, onPoly, onReg, onRoi } = ctx_state;
  const st = g.mouseStatus;
  const scale = canvas.camera.GetCameraScale() || 100;

  // Touch devices fire touchend -> onmouseup AND a synthetic mouseup, so one physical
  // release produces TWO mouse-up edges. That double-toggled a ROI point off right after
  // adding it (and double-added polygon vertices). Swallow a 2nd mouse-up within 200ms.
  if (g.mouseEdge && st.status === 0) {
    const now = (typeof performance !== 'undefined' && performance.now) ? performance.now() : 0;
    if (now && now - (work.lastUpMs || 0) < 200) return;
    work.lastUpMs = now;
  }

  if (tool === 'roi') {
    // roi mode captures the drag (no camera pan), so any press→release places a point
    // at the release position; clicking on an existing point removes it.
    if (g.mouseEdge && st.status === 0) {                  // mouse-up
      const p = { x: g.mouseOnCanvas.x, y: g.mouseOnCanvas.y };
      const pts = (roiPts || []).map((q) => ({ x: q.x, y: q.y }));
      const closeW = 14 / scale;
      const idx = pts.findIndex((q) => Math.hypot(q.x - p.x, q.y - p.y) < closeW);
      if (idx >= 0) pts.splice(idx, 1);                     // click on a point removes it
      else pts.push(p);                                     // else add a new override point
      onRoi(pts);
    }
    return;
  }

  if (tool === 'include' || tool === 'exclude') {
    work.cursor = { x: g.mouseOnCanvas.x, y: g.mouseOnCanvas.y };
    if (g.mouseEdge && st.status === 0) {                 // mouse-up edge
      const movedPx = Math.hypot(st.x - st.px, st.y - st.py);
      if (movedPx < 6) {                                  // a click, not a pan-drag
        const p = { x: g.mouseOnCanvas.x, y: g.mouseOnCanvas.y };
        if (work.poly.length >= 3) {
          const f = work.poly[0];
          const closeW = 12 / scale;                       // ~12px in world units
          if (Math.hypot(p.x - f.x, p.y - f.y) < closeW) {
            onPoly(tool === 'include' ? 'loc_include' : 'loc_exclude', work.poly.slice());
            work.poly = []; work.cursor = null; return;
          }
        }
        work.poly.push(p);
      }
    }
  } else if (tool === 'locline') {
    if (st.status === 1) {
      work.line = { o: { x: g.pmouseOnCanvas.x, y: g.pmouseOnCanvas.y },
                    t: { x: g.mouseOnCanvas.x, y: g.mouseOnCanvas.y } };
    } else if (g.mouseEdge && st.status === 0 && work.line) {
      const { o, t } = work.line;
      // A CLICK is not a registration. atan2(0,0) is 0, so releasing without
      // dragging used to write angle: 0 and move the origin to wherever the
      // pointer happened to be -- silently replacing a registration somebody
      // measured. And because angle_offset_deg is in the shape cache's
      // fingerprint, that also invalidates the trained features: the def then
      // falls back to sig360 and still locates, so nothing looks wrong.
      //
      // 12 px in world units, the same threshold the polygon tool uses to
      // decide a click from a drag.
      const minLen = 12 / (canvas.camera.GetCameraScale() || 100);
      if (Math.hypot(t.x - o.x, t.y - o.y) < minLen) { work.line = null; return; }
      // NEGATED, because def_image_reg.angle is in ROTATE space and this drag
      // measures an IMAGE angle. DefConfUI writes the field as `angle:
      // reg.rotate` straight off an inspection report, and the canvas rectifies
      // by rotating the image by +angle -- which only lands the part on the
      // world x-axis if angle is MINUS the image angle. Storing the raw atan2
      // here rectified by 2x the drawn angle, invisibly, because every def on
      // this bench has a registration angle of ~0. See imageAngleOf().
      onReg({ cx: o.x, cy: o.y, angle: -Math.atan2(t.y - o.y, t.x - o.x), isFlipped: false });
      work.line = null;
    }
  }
}

// ── The studio view. ───────────────────────────────────────────────────────────
// The verdict of a test run. Three things, in the order they are worth reading:
// did it find the part, is it where you said it was, and which primitives
// failed and why.
function InspectPanel({ insp, onClear }) {
  const row = { display: 'flex', justifyContent: 'space-between', fontSize: 11 };
  if (!insp.located) {
    // A failure gets MORE room than a success, not less. It is the case the
    // operator is stuck on, and "定位失敗" alone tells them nothing they did not
    // already know from the blank canvas.
    const L = insp.locate;
    const gap = L && Number.isFinite(L.best) && Number.isFinite(L.thres)
      ? L.thres - L.best : null;
    return <div style={{ fontSize: 11, marginTop: 4, border: '1px solid #a61d24',
                         borderRadius: 4, padding: 6, background: '#2a1215' }}>
      <div style={{ color: '#ff7875', fontWeight: 600, marginBottom: 3 }}>定位失敗</div>
      <div style={{ color: '#d89a9a', lineHeight: 1.5 }}>{insp.why}</div>
      {gap !== null && <div style={{ color: '#d89a9a', marginTop: 4 }}>
        差距很小的話,先看照明和 matching 參數;差很多通常是特徵範圍或 coarse scale。
      </div>}
      {L && L.candidates === 0 && !Number.isFinite(L.best) &&
        <div style={{ color: '#d89a9a', marginTop: 4 }}>
          先按「🔵 生成特徵點」看有沒有抽到特徵——沒有特徵就不會有候選。
        </div>}
      <Button size="small" type="link" onClick={onClear}>清除</Button>
    </div>;
  }
  const d = insp.poseDelta;
  // 0.05mm / 0.2deg: not a spec, a legibility threshold -- below it the number
  // is the locator's own noise and colouring it red would train people to
  // ignore the colour. Anything the operator actually cares about is coarser.
  const poseOff = d && (d.dist > 0.05 || Math.abs(d.dDeg) > 0.2 || d.flipDiffers);
  const bad = insp.rows.filter((r) => !r.ok);
  return <div style={{ marginTop: 4 }}>
    {insp.poses.length > 1 &&
      <div style={{ ...row, color: '#ffd54f' }}>
        <span>找到 {insp.poses.length} 個物件</span>
        <span style={{ fontSize: 10 }}>下面的數字是第 1 個</span>
      </div>}
    <div style={row}>
      <span>相似度 similarity</span>
      <b style={{ color: insp.pose.similarity >= 0.9 ? '#00c853' : '#ff9100' }}>
        {(insp.pose.similarity ?? 0).toFixed(4)}</b>
    </div>
    {d && <div style={row}>
      <span title="核心找到的位姿 vs 你畫的定位線。這個差就是定位誤差。">定位偏差</span>
      <b style={{ color: poseOff ? '#ff9100' : '#00c853' }}>
        {d.dist.toFixed(3)}mm / {d.dDeg.toFixed(2)}°{d.flipDiffers ? ' ⚠翻面不同' : ''}</b>
    </div>}
    <div style={row}>
      <span>量測</span>
      <b><span style={{ color: '#00c853' }}>{insp.counts.ok} OK</span>
        {insp.counts.na > 0 && <span style={{ color: '#ff1744' }}>　{insp.counts.na} NA</span>}
        {insp.counts.ng > 0 && <span style={{ color: '#ff1744' }}>　{insp.counts.ng} NG</span>}
      </b>
    </div>
    {bad.length > 0 && <div style={{ marginTop: 3, maxHeight: 150, overflowY: 'auto' }}>
      {bad.map((r) => <div key={r.type + r.id} style={{ fontSize: 11, color: '#ff5252',
                            borderTop: '1px solid #333', padding: '2px 0' }}>
        <b>#{r.id}</b> {r.name || r.type}
        <div style={{ color: '#c77' }}>{r.reason}</div>
      </div>)}
    </div>}
    <Button size="small" type="link" onClick={onClear}>清除疊圖</Button>
  </div>;
}

// The sweep, as a strip you can read down. One row per step, the baseline
// marked, and the two numbers that matter per row: the match score and -- where
// the axis has a ground truth -- how far the reported pose is from the pose we
// imposed.
function SweepPanel({ sweep, floor }) {
  if (!sweep) return null;
  const A = SWEEP_AXES[sweep.axis] || {};
  const u = A.unit || '';
  const fmt = (v) => (Math.abs(v) >= 10 ? v.toFixed(0) : v.toFixed(2));
  // THE BAR IS HEADROOM ABOVE THE ACCEPTANCE FLOOR, not the score.
  //
  // It first scaled to the min/max of the run, on the reasoning that scores all
  // sit near 1.0 and a 0..1 bar makes them look identical. That reasoning was
  // wrong in the direction that matters: on a real sweep spanning 0.986 to
  // 0.998 -- twelve thousandths, comfortably above a 0.50 gate -- it drew a bar
  // swinging from nearly empty to full. A reader would tune against that, and
  // there is nothing there to tune. Auto-scaling turns any run into a dramatic
  // curve, including one that is flat.
  //
  // Against the floor, a healthy sweep is a column of nearly-full bars, which
  // is the true statement, and a step actually approaching the gate visibly
  // shortens. The spread stays legible as the printed number next to it.
  const bar = (sim) => headroom(sim, floor);
  return <div style={{ marginTop: 4 }}>
    <div style={{ fontSize: 11, color: '#999' }}>
      {sweep.done}/{sweep.total}{sweep.aborted ? '(已中止)' : ''}
      <span style={{ marginLeft: 6 }} title="長條 = 分數距離接受門檻還有多少餘裕(滿格 = 1.0)">
        門檻 {floor.toFixed(2)}</span>
    </div>
    {sweep.verdict && <div style={{ fontSize: 11, color: '#69c0ff', margin: '3px 0',
                                    lineHeight: 1.5 }}>{sweep.verdict}</div>}
    <div style={{ maxHeight: 220, overflowY: 'auto', marginTop: 3 }}>
      {sweep.rows.map((r, i) => {
        const isBase = Math.abs(r.value - A.neutral) <= 1e-9;
        return <div key={i} style={{ display: 'flex', alignItems: 'center', gap: 4,
                     fontSize: 11, fontVariantNumeric: 'tabular-nums',
                     borderTop: '1px solid #333', padding: '1px 0',
                     background: isBase ? '#111c2a' : undefined }}>
          <span style={{ width: 46, textAlign: 'right', color: isBase ? '#69c0ff' : '#ccc' }}>
            {fmt(r.value)}{u}</span>
          <span style={{ width: 34, flex: '0 0 auto', height: 8, background: '#222',
                         borderRadius: 2, overflow: 'hidden' }}>
            {r.located && <span style={{ display: 'block', height: '100%',
              width: `${Math.max(4, bar(r.sim) * 100)}%`,
              background: '#00c853' }} />}
          </span>
          <span style={{ width: 42, color: r.located ? '#ccc' : '#ff5252' }}>
            {r.located ? r.sim.toFixed(3) : '失敗'}</span>
          {Number.isFinite(r.residual)
            ? <span style={{ flex: '1 1 auto', textAlign: 'right',
                             color: r.signSuspect ? '#ffab00'
                                  : Math.abs(r.residual) > 0.5 ? '#ff9100' : '#888' }}
                title={`施加 ${fmt(r.expected)}${u},量到 ${fmt(r.moved)}${u}`}>
                {r.residual >= 0 ? '+' : ''}{r.residual.toFixed(3)}{u}
                {r.signSuspect ? ' ⚠符號' : ''}
              </span>
            : <span style={{ flex: '1 1 auto', textAlign: 'right', color: '#666' }}>
                {r.located ? `${r.ok}/${r.ok + r.na}` : ''}
              </span>}
        </div>;
      })}
    </div>
    {sweep.rows.some((r) => r.signSuspect) &&
      <div style={{ fontSize: 11, color: '#ffab00', marginTop: 3, lineHeight: 1.5 }}>
        ⚠ 殘差約等於施加值的兩倍 — 這是角度符號反了,不是定位差了兩倍。
      </div>}
  </div>;
}

// The same test across every sample beside the def. One row per image, and the
// summary line is the sentence somebody would otherwise have to assemble by
// hand: how many located, and the worst pose offset among them.
function BatchPanel({ batch, onClear }) {
  if (!batch) return null;
  const done = batch.rows.filter((r) => r.sum);
  const found = done.filter((r) => r.sum.located);
  const offs = found.map((r) => (r.sum.poseDelta ? r.sum.poseDelta.dist : NaN))
                    .filter(Number.isFinite);
  return <div style={{ marginTop: 4 }}>
    <div style={{ fontSize: 11, color: '#999' }}>
      {batch.done}/{batch.total}{batch.aborted ? '（已中止）' : ''}
    </div>
    {batch.done === batch.total && <div style={{ fontSize: 11, color: '#69c0ff',
        margin: '3px 0', lineHeight: 1.5 }}>
      {found.length}/{done.length} 張定位成功
      {offs.length ? `，位姿偏差最大 ${Math.max(...offs).toFixed(3)}mm` : ''}
      {found.length < done.length ? '　⚠ 有影像定位不到' : ''}
    </div>}
    <div style={{ maxHeight: 190, overflowY: 'auto', marginTop: 3 }}>
      {batch.rows.map((r, i) => {
        const s = r.sum, ok = s && s.located;
        return <div key={i} style={{ borderTop: '1px solid #333', padding: '2px 0',
                     fontSize: 11, fontVariantNumeric: 'tabular-nums' }}>
          <div style={{ display: 'flex', gap: 4 }}>
            <span style={{ flex: '1 1 auto', minWidth: 0, overflow: 'hidden',
                           textOverflow: 'ellipsis', whiteSpace: 'nowrap',
                           color: ok ? '#ccc' : '#ff5252' }} title={r.name}>{r.name}</span>
            {ok && <span style={{ color: '#888' }}>{s.pose.similarity.toFixed(3)}</span>}
            {ok && s.poseDelta &&
              <span style={{ color: s.poseDelta.dist > 0.1 ? '#ff9100' : '#888' }}>
                {s.poseDelta.dist.toFixed(3)}mm</span>}
            {ok && <span style={{ color: s.counts.na + s.counts.ng ? '#ff1744' : '#00c853' }}>
              {s.counts.ok}/{s.counts.ok + s.counts.na + s.counts.ng}</span>}
          </div>
          {!ok && s && <div style={{ color: '#c77' }}>{s.why}</div>}
        </div>;
      })}
    </div>
    <Button size="small" type="link" onClick={onClear}>清除</Button>
  </div>;
}

// SBMSetupView -- the first version of this studio -- was removed on 2026-08-30.
//
// It shipped beside v2 while v2 was being built, deliberately: the operator had
// something that worked while the replacement grew. v2 has since taken every
// job it had, and two studios reading the same def is two places for the same
// bug. What stays here is HookCanvasComponent, which v2 imports -- the canvas
// was never the part that was replaced.
//
// The whole file is in the history if the old view is ever wanted back.
