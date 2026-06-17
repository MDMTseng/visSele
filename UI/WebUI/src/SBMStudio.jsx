// Self-contained SBM localization "studio": a hook-driven canvas (no global redux
// state machine) + an SBM-only toolbar, hosted in a full-screen modal. All drawing and
// interaction live in `sbmDrawHook`; the committed data still lives in the def
// (loc_include/loc_exclude shapes in shapeList + def_image_reg), so the existing save
// (defFileGeneration) round-trips unchanged. See InspectionCore/docs/sbm_setup_studio_plan.md.
import React, { useRef, useState, useCallback, useEffect } from 'react';
import { useSelector, useDispatch } from 'react-redux';
import ReactResizeDetector from 'react-resize-detector';
import { Button, Divider, Checkbox } from 'antd';

import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';

// ── React wrapper: mount a DrawHook_CanvasComponent on a <canvas>, feed it the
// image + the draw hook, resize via ReactResizeDetector. ──────────────────────
export function HookCanvasComponent({ dhook, image, captureDrag, style }) {
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
  useEffect(() => {
    const c = _.current.canvComp; if (!c || !image) return;
    c.SetImg(image); c.draw();
  }, [image]);

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
  const { reg, mmpp, shapeList, work, featPts, tool } = ctx_state;
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

  // generated feature points (line2Dup = blue, ROI = magenta) from the core round-trip.
  if (featPts) {
    ctx.fillStyle = '#29b6f6';
    for (const p of (featPts.features || [])) { ctx.beginPath(); ctx.arc(p.x, p.y, pr * 0.8, 0, 7); ctx.fill(); }
    ctx.fillStyle = '#ff4081';
    for (const p of (featPts.roi || [])) { ctx.beginPath(); ctx.arc(p.x, p.y, pr * 1.8, 0, 7); ctx.fill(); }
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
  const { tool, work, onPoly, onReg } = ctx_state;
  const st = g.mouseStatus;
  const scale = canvas.camera.GetCameraScale() || 100;

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
      onReg({ cx: o.x, cy: o.y, angle: Math.atan2(t.y - o.y, t.x - o.x), isFlipped: false });
      work.line = null;
    }
  }
}

// ── The studio view. ───────────────────────────────────────────────────────────
export function SBMSetupView() {
  const dispatch = useDispatch();
  const edit_info = useSelector((s) => s.UIData.edit_info);
  const [tool, setTool] = useState('pan');
  const [featPts] = useState(undefined);        // filled by the SF round-trip (P-C)
  const work = useRef({ poly: [], cursor: null, line: null });

  const reg = edit_info.def_image_reg || {};
  const obj = edit_info._obj;
  const mmpp = obj && obj.getEditorMmpp ? obj.getEditorMmpp() : 1;
  const shapeList = (obj && obj.shapeList) || [];

  const onPoly = useCallback((type, pts) => {
    dispatch(DefConfAct.Shape_Set({
      shape: { type, points: pts.map((p) => ({ x: p.x, y: p.y })),
               name: type === 'loc_include' ? '@__LOC_INCLUDE__' : '@__LOC_EXCLUDE__' },
      id: undefined,
    }));
  }, [dispatch]);

  const onReg = useCallback((r) => {
    dispatch(DefConfAct.EditInfo_Patch({ def_image_reg: { ...(edit_info.def_image_reg || {}), ...r } }));
    setTool('pan');
  }, [dispatch, edit_info]);

  const dhook = useCallback((isCtrl, g, canvas) => {
    canvas.captureDrag = (tool === 'locline');
    const ctx_state = { reg, mmpp, shapeList, work: work.current, featPts, tool, onPoly, onReg };
    if (isCtrl) ctrlScene(g, canvas, ctx_state);
    else drawScene(g, canvas, ctx_state);
  }, [tool, reg, mmpp, shapeList, featPts, onPoly, onReg]);

  const TBtn = ({ id, children, ...p }) => (
    <Button size="small" block style={{ marginBottom: 4 }} type={tool === id ? 'primary' : 'default'}
      onClick={() => { work.current = { poly: [], cursor: null, line: null }; setTool(id); }} {...p}>
      {children}</Button>
  );

  const delLast = (type) => {
    const last = [...shapeList].reverse().find((s) => s.type === type);
    if (last) dispatch(DefConfAct.Shape_Set({ shape: null, id: last.id }));
  };
  const nIncl = shapeList.filter((s) => s.type === 'loc_include').length;
  const nExcl = shapeList.filter((s) => s.type === 'loc_exclude').length;

  return <div style={{ display: 'flex', height: '84vh', gap: 8 }}>
    <div style={{ width: 240, overflowY: 'auto', fontSize: 12, paddingRight: 6 }}>
      <Divider orientation="left" style={{ margin: '4px 0' }}>定位 registration</Divider>
      <TBtn id="locline" title="拖一條線:按下=原點,放開方向=0°軸。設定 def_image_reg。">✛ 設定定位線（拖曳）</TBtn>
      <div style={{ fontSize: 11, color: '#999' }}>
        origin(mm): {reg.cx !== undefined ? `${(reg.cx).toFixed(2)}, ${(reg.cy).toFixed(2)}` : '—'} ／ angle: {reg.angle !== undefined ? `${(reg.angle * 180 / Math.PI).toFixed(1)}°` : '—'}
      </div>

      <Divider orientation="left" style={{ margin: '8px 0 4px' }}>特徵範圍 regions</Divider>
      <TBtn id="include">＋ include 生成區（{nIncl}）</TBtn>
      <TBtn id="exclude">－ exclude 避免區（{nExcl}）</TBtn>
      <div style={{ display: 'flex', gap: 4 }}>
        <Button size="small" onClick={() => delLast('loc_include')}>刪last include</Button>
        <Button size="small" onClick={() => delLast('loc_exclude')}>刪last exclude</Button>
      </div>
      <div style={{ fontSize: 11, color: '#999', marginTop: 4 }}>
        點頂點圍住零件,點回第一個頂點收尾。拖曳=平移視角,滾輪=縮放。
      </div>

      <Divider orientation="left" style={{ margin: '8px 0 4px' }}>檢視</Divider>
      <TBtn id="pan">🖐 平移/縮放</TBtn>
      <div style={{ fontSize: 11, color: '#999' }}>「生成特徵點」可視化(core 往返)與設定整併:下一步 P-C。</div>
    </div>

    <div style={{ flex: 1, minWidth: 0, height: '100%', border: '1px solid #333' }}>
      <HookCanvasComponent dhook={dhook} image={edit_info.img} captureDrag={tool === 'locline'} />
    </div>
  </div>;
}
