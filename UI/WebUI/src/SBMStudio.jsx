// Self-contained SBM localization "studio": a hook-driven canvas (no global redux
// state machine) + an SBM-only toolbar, hosted in a full-screen modal. All drawing and
// interaction live in `sbmDrawHook`; the committed data still lives in the def
// (loc_include/loc_exclude shapes in shapeList + def_image_reg), so the existing save
// (defFileGeneration) round-trips unchanged. See InspectionCore/docs/sbm_setup_studio_plan.md.
import React, { useRef, useState, useCallback, useEffect } from 'react';
import { useSelector, useDispatch } from 'react-redux';
import ReactResizeDetector from 'react-resize-detector';
import { Button, Divider, Checkbox, InputNumber, Select } from 'antd';

import EC_CANVAS_Ctrl from './EverCheckCanvasComponent';
import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import { defFileGeneration, stampRefImagePath } from 'UTIL/MISC_Util';

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
  const { reg, mmpp, shapeList, work, featPts, roiPts, tool } = ctx_state;
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

  // The four polygon regions, filled. Kept as one table rather than a chain of
  // type tests: two of these were added later and the colours, the toolbar and
  // the delete buttons all have to agree about them, which is the shape of bug
  // this file has produced before.
  for (const s of shapeList) {
    const R = REGION_KINDS[s.type];
    if (!R) continue;
    const pts = s.points || []; if (pts.length < 2) continue;
    poly(ctx, pts, true);
    ctx.fillStyle = R.fill; ctx.fill();
    ctx.lineWidth = lw; ctx.strokeStyle = R.stroke; ctx.stroke();
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

  const REG = REGION_BY_TOOL[tool];
  if (REG) {
    work.cursor = { x: g.mouseOnCanvas.x, y: g.mouseOnCanvas.y };
    if (g.mouseEdge && st.status === 0) {                 // mouse-up edge
      const movedPx = Math.hypot(st.x - st.px, st.y - st.py);
      if (movedPx < 6) {                                  // a click, not a pan-drag
        const p = { x: g.mouseOnCanvas.x, y: g.mouseOnCanvas.y };
        if (work.poly.length >= 3) {
          const f = work.poly[0];
          const closeW = 12 / scale;                       // ~12px in world units
          if (Math.hypot(p.x - f.x, p.y - f.y) < closeW) {
            onPoly(REG.shapeType, work.poly.slice());
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

// The polygon regions this studio authors. TWO DIFFERENT JOBS, and the reason
// they are separate entries rather than one pair with a flag:
//
//   loc_*    the FEATURE GENERATION mask -- where line2Dup extracts features in
//            order to FIND the part. Consumed at train time.
//   fence_*  the MEASUREMENT FENCE -- where a caliper may pick up an edge once
//            the part HAS been found. Consumed at inspect time.
//
// Using the first for the second bounds measurement by wherever somebody drew
// the matching region, which is why the core keeps them in separate members too.
export const REGION_KINDS = {
  loc_include:   { tool: 'include',   shapeType: 'loc_include',
                   name: '@__LOC_INCLUDE__',   stroke: '#00c853', fill: 'rgba(0,200,80,0.16)',
                   label: '＋ include 生成區' },
  loc_exclude:   { tool: 'exclude',   shapeType: 'loc_exclude',
                   name: '@__LOC_EXCLUDE__',   stroke: '#ff5252', fill: 'rgba(255,70,70,0.20)',
                   label: '－ exclude 避免區' },
  fence_include: { tool: 'fence_in',  shapeType: 'fence_include',
                   name: '@__FENCE_INCLUDE__', stroke: '#2979ff', fill: 'rgba(41,121,255,0.16)',
                   label: '＋ 量測範圍 fence' },
  fence_exclude: { tool: 'fence_out', shapeType: 'fence_exclude',
                   name: '@__FENCE_EXCLUDE__', stroke: '#ff9100', fill: 'rgba(255,145,0,0.20)',
                   label: '－ 量測禁區 fence' },
};
const REGION_BY_TOOL = Object.fromEntries(
  Object.values(REGION_KINDS).map((R) => [R.tool, R]));

// ── The studio view. ───────────────────────────────────────────────────────────
export function SBMSetupView({ sendBPG, onSave, onClose }) {
  const dispatch = useDispatch();
  const edit_info = useSelector((s) => s.UIData.edit_info);
  const [tool, setTool] = useState('pan');
  const [featPts, setFeatPts] = useState(undefined);   // {features:[],roi:[]} from the SF round-trip
  const featRef = useRef(undefined);                   // mirror — drawScene reads this so the
  featRef.current = featPts;                            // overlay never goes stale across redraws
  const [genBusy, setGenBusy] = useState(false);
  const work = useRef({ poly: [], cursor: null, line: null });

  const reg = edit_info.def_image_reg || {};
  const obj = edit_info._obj;
  const mmpp = obj && obj.getEditorMmpp ? obj.getEditorMmpp() : 1;
  const shapeList = (obj && obj.shapeList) || [];
  const roiPts = edit_info.roi_refine_points || [];

  const onRoi = useCallback((pts) => {
    dispatch(DefConfAct.EditInfo_Patch({ roi_refine_points: pts }));
  }, [dispatch]);

  const onPoly = useCallback((type, pts) => {
    dispatch(DefConfAct.Shape_Set({
      shape: { type, points: pts.map((p) => ({ x: p.x, y: p.y })),
               name: (REGION_KINDS[type] || {}).name || type },
      id: undefined,
    }));
  }, [dispatch]);

  const onReg = useCallback((r) => {
    dispatch(DefConfAct.EditInfo_Patch({ def_image_reg: { ...(edit_info.def_image_reg || {}), ...r } }));
    setTool('pan');
  }, [dispatch, edit_info]);

  // "生成特徵點": push the current (in-progress) def to the core's SF command and
  // overlay the returned line2Dup features + ROI points. Trains from the on-disk
  // <def>.png, so a brand-new unsaved def must be saved first.
  const genFeatures = useCallback(() => {
    if (!sendBPG) return;
    let deffile;
    try { deffile = defFileGeneration(edit_info); stampRefImagePath(deffile, edit_info); }
    catch (e) { return; }
    setGenBusy(true);
    new Promise((resolve, reject) => sendBPG('SF', 0, { definfo: deffile }, undefined, { resolve, reject }))
      .then((pkts) => {
        const sf = (pkts || []).find((p) => p.type === 'SF');
        setFeatPts(sf && sf.data ? sf.data : { features: [], roi: [] });
        // 把核心訓練出來的特徵存進 def, 之後載入不必重抽 (見 MISC_Util 的
        // __shape_cache 說明)。點在畫面上只是視覺化, 這一行才是真的留下來的。
        if (sf && sf.data && sf.data.shape_cache)
          dispatch(DefConfAct.EditInfo_Patch({ __shape_cache: sf.data.shape_cache }));
      })
      .catch(() => {})
      .finally(() => setGenBusy(false));
  }, [sendBPG, edit_info]);

  const captureDrag = (tool === 'locline' || tool === 'roi');
  // "自動產生 ROI 點": ask the core for its auto-selected ROI points and put them into
  // the editable list. Strips roi_refine_points from the sent def so the core
  // auto-selects (otherwise it would echo back the current explicit list).
  const autoFillRoi = useCallback(() => {
    if (!sendBPG) return;
    let deffile;
    try { deffile = defFileGeneration(edit_info); stampRefImagePath(deffile, edit_info); }
    catch (e) { return; }
    if (deffile.featureSet && deffile.featureSet[0]) delete deffile.featureSet[0].roi_refine_points;
    setGenBusy(true);
    new Promise((resolve, reject) => sendBPG('SF', 0, { definfo: deffile }, undefined, { resolve, reject }))
      .then((pkts) => {
        const sf = (pkts || []).find((p) => p.type === 'SF');
        const roi = (sf && sf.data && sf.data.roi) || [];
        dispatch(DefConfAct.EditInfo_Patch({ roi_refine_points: roi.map((p) => ({ x: p.x, y: p.y })) }));
      })
      .catch(() => {})
      .finally(() => setGenBusy(false));
  }, [sendBPG, edit_info, dispatch]);

  const dhook = useCallback((isCtrl, g, canvas) => {
    canvas.captureDrag = captureDrag;
    const ctx_state = { reg, mmpp, shapeList, work: work.current,
      featPts: featRef.current, roiPts, tool, onPoly, onReg, onRoi };
    if (isCtrl) ctrlScene(g, canvas, ctx_state);
    else drawScene(g, canvas, ctx_state);
  }, [tool, reg, mmpp, shapeList, featPts, roiPts, onPoly, onReg, onRoi]);

  // Auto-generate the feature-point overlay once when the studio opens (if the def is
  // saved on disk). The user can refresh with the button after editing regions.
  useEffect(() => { genFeatures(); /* eslint-disable-next-line */ }, []);

  // Tool toggle: clicking the active tool returns to 'pan' (drag = pan, wheel = zoom),
  // so no separate pan button is needed.
  const TBtn = ({ id, children, ...p }) => (
    <Button size="small" block style={{ marginBottom: 4 }} type={tool === id ? 'primary' : 'default'}
      onClick={() => { work.current = { poly: [], cursor: null, line: null }; setTool(tool === id ? 'pan' : id); }} {...p}>
      {children}</Button>
  );

  const delLast = (type) => {
    const last = [...shapeList].reverse().find((s) => s.type === type);
    if (last) dispatch(DefConfAct.Shape_Set({ shape: null, id: last.id }));
  };
  // One row per region kind: the toggle (with a live count) and its delete.
  // Driven off REGION_KINDS so a kind cannot exist in the drawing code and be
  // missing from the toolbar.
  const RegionTools = ({ kinds }) => <>
    {kinds.map((k) => {
      const R = REGION_KINDS[k];
      const n = shapeList.filter((s) => s.type === k).length;
      return <div key={k} style={{ display: 'flex', gap: 4, alignItems: 'stretch', marginBottom: 2 }}>
        <div style={{ flex: 1 }}><TBtn id={R.tool}>{R.label}（{n}）</TBtn></div>
        <Button size="small" disabled={n === 0} onClick={() => delLast(k)}>刪last</Button>
      </div>;
    })}
  </>;

  return <div style={{ display: 'flex', height: '84vh', gap: 8 }}>
    <div style={{ width: 240, overflowY: 'auto', fontSize: 12, paddingRight: 6 }}>
      <Divider orientation="left" style={{ margin: '4px 0' }}>定位 registration</Divider>
      <TBtn id="locline" title="拖一條線:按下=原點,放開方向=0°軸。設定 def_image_reg。">✛ 設定定位線（拖曳）</TBtn>
      <div style={{ fontSize: 11, color: '#999' }}>
        origin(mm): {reg.cx !== undefined ? `${(reg.cx).toFixed(2)}, ${(reg.cy).toFixed(2)}` : '—'} ／ angle: {reg.angle !== undefined ? `${(reg.angle * 180 / Math.PI).toFixed(1)}°` : '—'}
      </div>

      <Divider orientation="left" style={{ margin: '8px 0 4px' }}>特徵範圍 regions（定位用）</Divider>
      <RegionTools kinds={['loc_include', 'loc_exclude']} />
      <div style={{ fontSize: 11, color: '#999', marginTop: 4 }}>
        線上定位時用來生成特徵的範圍。點頂點圍住零件,點回第一個頂點收尾。拖曳=平移視角,滾輪=縮放。
      </div>

      <Divider orientation="left" style={{ margin: '8px 0 4px' }}>量測範圍 fence（檢驗用）</Divider>
      <RegionTools kinds={['fence_include', 'fence_exclude']} />
      <div style={{ fontSize: 11, color: '#999', marginTop: 4 }}>
        定位完成後,caliper 只能在 fence 內抓邊。不畫 = 全圖都能量(和以前一樣)。
        只畫禁區也可以:fence 從全圖開始,禁區在上面挖洞。
      </div>

      <Divider orientation="left" style={{ margin: '8px 0 4px' }}>ROI 取樣點（{roiPts.length}）</Divider>
      <Button size="small" block loading={genBusy} onClick={autoFillRoi}
        title="向 core 取得自動選的 ROI 點,填入下方清單(橘框)後即可編輯。">
        ⚙ 自動產生 ROI 點</Button>
      <TBtn id="roi" title="點影像新增 ROI 點(橘框);點既有點可刪除。">◻ 編輯 ROI 點（點擊增/刪）</TBtn>
      <Button size="small" onClick={() => onRoi([])}>清除全部</Button>
      <div style={{ fontSize: 11, color: '#999', marginTop: 2 }}>
        core 只用這些 ROI 點。全部清除 = 不做 ROI 微調(僅粗定位)。
      </div>

      <Divider orientation="left" style={{ margin: '8px 0 4px' }}>可視化</Divider>
      <Button size="small" block loading={genBusy} onClick={genFeatures}
        title="把目前設定送到 core,回傳並疊上實際生成的 line2Dup 特徵點(藍)。需先存過檔(<def>.png 在磁碟上)。">
        🔵 生成特徵點(藍)</Button>
      {featPts && <div style={{ fontSize: 11, color: '#999', marginTop: 2 }}>
        line2Dup features: {(featPts.features || []).length}
        <Button size="small" type="link" onClick={() => setFeatPts(undefined)}>清除</Button>
      </div>}

      <Divider orientation="left" style={{ margin: '8px 0 4px' }}>matching 參數</Divider>
      <div style={{ display: 'flex', gap: 4, alignItems: 'center', marginBottom: 3 }}>
        <span style={{ width: 92 }}>coarse scale</span>
        <InputNumber size="small" min={0.1} max={1} step={0.1} style={{ width: 70 }}
          value={edit_info.shape_match_scale ?? 0.3}
          onChange={(v) => dispatch(DefConfAct.Shape_Match_Scale_Update(v))} />
      </div>
      <div style={{ display: 'flex', gap: 4, alignItems: 'center', marginBottom: 3 }}>
        <span style={{ width: 92 }}>angle ±°</span>
        <InputNumber size="small" min={0} max={180} step={5} style={{ width: 70 }}
          value={edit_info.matching_angle_margin_deg ?? 180}
          onChange={(v) => dispatch(DefConfAct.Matching_Angle_Margin_Deg_Update(v))} />
      </div>
      <div style={{ display: 'flex', gap: 4, alignItems: 'center' }}>
        <span style={{ width: 92 }}>face</span>
        <Select size="small" style={{ width: 110 }} value={edit_info.matching_face ?? 1}
          onChange={(v) => dispatch(DefConfAct.Matching_Face_Update(v))}
          options={[{ value: 1, label: '正面 front' }, { value: -1, label: '反面 back' }, { value: 0, label: '兩面 both' }]} />
      </div>

      <Divider orientation="left" style={{ margin: '8px 0 4px' }}>特徵生成（邊緣強度）</Divider>
      <div style={{ display: 'flex', gap: 4, alignItems: 'center', marginBottom: 3 }}>
        <span style={{ width: 92 }}>weak 弱邊</span>
        <InputNumber size="small" min={1} max={255} step={5} style={{ width: 70 }}
          value={edit_info.shape_weak_thres ?? 50}
          onChange={(v) => dispatch(DefConfAct.EditInfo_Patch({ shape_weak_thres: v }))} />
      </div>
      <div style={{ display: 'flex', gap: 4, alignItems: 'center' }}>
        <span style={{ width: 92 }}>strong 強邊</span>
        <InputNumber size="small" min={1} max={255} step={5} style={{ width: 70 }}
          value={edit_info.shape_strong_thres ?? 80}
          onChange={(v) => dispatch(DefConfAct.EditInfo_Patch({ shape_strong_thres: v }))} />
      </div>
      <div style={{ fontSize: 11, color: '#999', marginTop: 2 }}>
        梯度門檻:小=更多特徵(含雜訊),大=只留強邊。改後按「生成特徵點」看效果。
      </div>

      <div style={{ borderTop: '1px solid #444', marginTop: 10, paddingTop: 8 }}>
        <Button type="primary" size="small" block onClick={() => onClose && onClose()}>完成(套用到編輯暫存)</Button>
        <div style={{ fontSize: 11, color: '#999', marginTop: 4 }}>設定即時套用到編輯暫存;回主編輯器按「存檔」才寫入磁碟。</div>
      </div>
    </div>

    <div style={{ flex: 1, minWidth: 0, height: '100%', border: '1px solid #333' }}>
      <HookCanvasComponent dhook={dhook} image={edit_info.img} captureDrag={captureDrag} />
    </div>
  </div>;
}
