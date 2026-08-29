// SBM Studio v2 -- the Surface Go layout, running beside the original.
//
// A COPY, deliberately. The old studio stays reachable and untouched so a
// machine on the line can fall back to it the moment this one misbehaves;
// that is worth more right now than not duplicating ~800 lines. The two are
// expected to converge and one of them to be deleted -- until then, a fix
// that matters must be applied to BOTH files, and this comment is the only
// thing that will remind anyone of that.
//
// What is NOT duplicated: HookCanvasComponent and every pure helper
// (sbmSweep, sbmInspectResult, matchThreshold, MISC_Util) are imported from
// where they already live, so the parts with an answer have one copy.
// Self-contained SBM localization "studio": a hook-driven canvas (no global redux
// state machine) + an SBM-only toolbar, hosted in a full-screen modal. All drawing and
// interaction live in `sbmDrawHook`; the committed data still lives in the def
// (localization polygons in @__SBM_INFO__ via edit_info.__loc_include /
// __loc_exclude, plus def_image_reg -- NOT in shapeList), so the existing save
// (defFileGeneration) round-trips unchanged. See InspectionCore/docs/sbm_setup_studio_plan.md.
import React, { useRef, useState, useCallback, useEffect } from 'react';
import { useSelector, useDispatch } from 'react-redux';
import { Button, InputNumber, Select, Modal } from 'antd';

import * as DefConfAct from 'REDUX_STORE_SRC/actions/DefConfAct';
import { defFileGeneration, stampRefImagePath } from 'UTIL/MISC_Util';
import { inspectSummary } from './sbmInspectResult';
import { useDefImages } from 'UTIL/useDefImages';
import { SWEEP_AXES, sweepValues, perturbFor, sweepRow, sweepVerdict } from './sbmSweep';
import { acceptanceFloor, headroom } from 'UTIL/matchThreshold';
import { HookCanvasComponent } from './SBMStudio';

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

const SBM2_CSS = ".sbm2-root{flex-direction:row}.sbm2-canvas{flex:1 1 auto;min-width:0;min-height:0}.sbm2-rail{flex:0 0 330px;min-height:0}@media (max-aspect-ratio: 1/1){.sbm2-root{flex-direction:column}.sbm2-canvas{flex:0 0 44%}.sbm2-rail{flex:1 1 auto}}";

export function SBMSetupView2({ sendBPG, onSave, onClose }) {
  const dispatch = useDispatch();
  const edit_info = useSelector((s) => s.UIData.edit_info);
  const [tool, setTool] = useState('pan');
  // Which block is expanded. undefined = follow the current step; -99 = the
  // user collapsed the current one and wants nothing open.
  const [openStep, setOpenStep] = useState(undefined);
  const [featPts, setFeatPts] = useState(undefined);   // {features:[],roi:[]} from the SF round-trip
  const featRef = useRef(undefined);                   // mirror — drawScene reads this so the
  featRef.current = featPts;                            // overlay never goes stale across redraws
  const [genBusy, setGenBusy] = useState(false);
  const [insp, setInsp] = useState(undefined);         // inspectSummary() of the last test run
  const [inspBusy, setInspBusy] = useState(false);
  const [sweep, setSweep] = useState(undefined);       // {axis, from, to, steps, rows, verdict}
  const [sweepAxis, setSweepAxis] = useState('rot');
  const [batch, setBatch] = useState(undefined);       // {rows, done, total}
  const [sweepRange, setSweepRange] = useState({});    // axis -> {from,to,steps}
  const abortRef = useRef(false);
  // Which floor a match has to clear, per the locator this def actually uses.
  const floorInfo = acceptanceFloor(edit_info);
  const inspRef = useRef(undefined);                   // mirror, same reason as featRef
  inspRef.current = insp;
  const work = useRef({ poly: [], cursor: null, line: null });

  const reg = edit_info.def_image_reg || {};
  const obj = edit_info._obj;
  const mmpp = obj && obj.getEditorMmpp ? obj.getEditorMmpp() : 1;
  // NO component-level shapeList. Nothing in this studio reads it any more:
  // the canvas is fed `locShapes`, built from edit_info.__loc_* below, and the
  // counters read those arrays directly. Keeping a binding named shapeList
  // around is how the counters came to be written against it in the first place.
  // Fed to the draw hook in the shape the hook already understands, so the
  // renderer did not have to change when the storage did.
  const locShapes = [
    ...((edit_info.__loc_include || []).map((poly) => ({ type: 'loc_include', points: poly }))),
    ...((edit_info.__loc_exclude || []).map((poly) => ({ type: 'loc_exclude', points: poly }))),
  ];
  const roiPts = edit_info.roi_refine_points || [];

  // The studio used to be stuck on whichever image was loaded when it opened.
  // The switcher DefConfUI puts on screen is behind this modal, and a def is
  // tested against several samples, not one -- so the studio gets its own,
  // driven by the same hook so both move the CORE's cached image and not just
  // their own bitmap.
  //
  // Switching THROWS AWAY the last test result. It was measured on a different
  // image, and leaving it on screen (or on the canvas) next to the new one is
  // the worst outcome available here: a verdict that looks current and is not.
  const { imageList, currentImagePath, switchImage } = useDefImages({
    afterLoad: () => { setInsp(undefined); setFeatPts(undefined); },
  });

  // A retake replaces the part. The feature overlay and the last test result
  // were both measured on the PREVIOUS one, and this component is not remounted
  // by the capture -- so they stayed on screen, drawn over the new image, as a
  // handful of blue points belonging to a part that is no longer there.
  const retakeRef = useRef(edit_info.__img_fresh_capture);
  useEffect(() => {
    if (edit_info.__img_fresh_capture && !retakeRef.current) {
      setFeatPts(undefined); setInsp(undefined);
    }
    retakeRef.current = edit_info.__img_fresh_capture;
  }, [edit_info.__img_fresh_capture]);

  const onRoi = useCallback((pts) => {
    dispatch(DefConfAct.EditInfo_Patch({ roi_refine_points: pts }));
  }, [dispatch]);

  // Regions are the localizer's, not the measurement list's.
  //
  // They used to be written as shapes into shapeList so this canvas could draw
  // them like anything else. shapeList is what becomes featureSet[0].features,
  // and an unrecognised type there fails the WHOLE def in the core -- so every
  // def carrying a region broke full inspection. They live in edit_info now.
  const onPoly = useCallback((type, pts) => {
    const key = (type === 'loc_include') ? '__loc_include' : '__loc_exclude';
    const poly = pts.map((p) => ({ x: p.x, y: p.y }));
    // One polygon per kind, replacing whatever was there -- the same thing the
    // old single-shape write did.
    dispatch(DefConfAct.EditInfo_Patch({ [key]: [poly] }));
  }, [dispatch]);

  const onReg = useCallback((r) => {
    dispatch(DefConfAct.EditInfo_Patch({ def_image_reg: { ...(edit_info.def_image_reg || {}), ...r } }));
    setTool('pan');
  }, [dispatch, edit_info]);

  // "生成特徵點": push the current (in-progress) def to the core's SF command and
  // overlay the returned line2Dup features + ROI points. Trains from the on-disk
  // <def>.png, so a brand-new or freshly re-taken def must be saved first --
  // see the failure branch below for why that is a rule and not a bug.
  const genFeatures = useCallback((regenerate) => {
    if (!sendBPG) return;
    let deffile;
    try { deffile = defFileGeneration(edit_info); stampRefImagePath(deffile, edit_info); }
    catch (e) { return; }
    setGenBusy(true);
    // regenerate: true means 生成特徵點 -- extract fresh and ignore whatever
    // cache the def carries. The auto-call when the studio opens omits it, so
    // opening the panel SHOWS the features the def actually uses rather than
    // quietly replacing them with a new extraction.
    new Promise((resolve, reject) => sendBPG('SF', 0,
      { definfo: deffile, ...(regenerate ? { regenerate: true } : {}) },
      undefined, { resolve, reject }))
      .then((pkts) => {
        const sf = (pkts || []).find((p) => p.type === 'SF');
        setFeatPts(sf && sf.data ? sf.data : { features: [], roi: [] });
        // A GENERATION THAT PRODUCED NOTHING MUST NOT LOOK LIKE ONE THAT WORKED.
        //
        // The cache is only patched below when the reply carries one, so a
        // failure already leaves the previous features alone -- which is right.
        // What was missing is saying so: the panel went to "0" and the operator
        // had no way to tell "this def has no features" from "the core could
        // not read the reference image".
        const nFeat = ((sf && sf.data && sf.data.features) || []).length;
        const gotCache = !!(sf && sf.data && sf.data.shape_cache);
        if (!gotCache || nFeat === 0) {
          // The commonest cause is not a bad setting, it is that there is no
          // template file yet.
          //
          // The core trains from a file on disk -- _ref_image_path, then the
          // def's reference_image, then <def-base>.png -- and has no path that
          // uses the image currently on screen. A def that has just been
          // re-taken has no sidecar written yet, and stampRefImagePath
          // deliberately refuses to point at the PREVIOUS def's picture. So
          // "generate" cannot succeed until the def has been saved once, and
          // saying "no features were extracted" sends someone to tune
          // thresholds against a problem that is not about thresholds.
          // A retake writes a scratch sidecar and sets __tmp_ref_image_path, so a
          // fresh capture DOES have a template -- claiming otherwise sends someone
          // off to save a file that is not the problem. Only the case with no
          // sidecar is genuinely template-less.
          const noTemplate = !!(edit_info && edit_info.__img_fresh_capture
                                && !edit_info.__tmp_ref_image_path);
          Modal.error({
            title: noTemplate ? '還沒有樣板影像,無法生成特徵' : '生成特徵失敗',
            content: noTemplate
              ? '這張影像是剛重新擷取的,還沒有寫進磁碟。SBM 的樣板必須是檔案 —— '
                + '存檔時才會把它寫成 <配方名>.png。先存一次檔,再回來生成特徵。'
                + '(不會沿用前一個配方的圖:那會訓練出另一個零件的特徵,而且會匹配成功。)'
              : nFeat === 0
                ? '核心沒有抽到任何特徵。通常是參考影像讀不到,或特徵範圍把零件整個排除了。'
                  + '先前的特徵沒有被覆蓋。'
                : '核心抽到了特徵但沒有回傳可儲存的結果。先前的特徵沒有被覆蓋。',
          });
        }
        // 把核心訓練出來的特徵存進 def, 之後載入不必重抽 (見 MISC_Util 的
        // __shape_cache 說明)。點在畫面上只是視覺化, 這一行才是真的留下來的。
        if (sf && sf.data && sf.data.shape_cache)
          // Clearing __shape_stale is the point: these features were trained
          // against the settings as they stand NOW, so the def is consistent
          // again and the save guard has nothing to complain about.
          dispatch(DefConfAct.EditInfo_Patch({ __shape_cache: sf.data.shape_cache,
                                               __shape_stale: undefined,
                                               __shape_lastGood: undefined }));
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
    // NO `regenerate` HERE, and not just because the identifier does not exist in
    // this closure -- it was copied from genFeatures along with its comment, and
    // reading an undeclared name in an ES module throws ReferenceError while the
    // ARGUMENT is being built, before sendBPG is ever called. The Promise
    // constructor turned that into a rejection, `.catch(() => {})` swallowed it,
    // and `.finally` cleared the spinner: the button did nothing, silently, and
    // looked like a core that had returned no points.
    //
    // Omitting it is also the right behaviour. This asks the core which points
    // IT would choose for the features the def already has; re-extracting first
    // would answer for a different feature set than the one being set up.
    new Promise((resolve, reject) => sendBPG('SF', 0,
      { definfo: deffile },
      undefined, { resolve, reject }))
      .then((pkts) => {
        const sf = (pkts || []).find((p) => p.type === 'SF');
        const roi = (sf && sf.data && sf.data.roi) || [];
        dispatch(DefConfAct.EditInfo_Patch({ roi_refine_points: roi.map((p) => ({ x: p.x, y: p.y })) }));
      })
      .catch(() => {})
      .finally(() => setGenBusy(false));
  }, [sendBPG, edit_info, dispatch]);

  // "測試檢驗": run a REAL inspection with the current, unsaved settings and show
  // what the machine did with them.
  //
  // Until this existed the studio could only be set up, never tried: you drew
  // regions, pressed save, left, and found out somewhere else. II is the same
  // round trip DefConfUI's CHECK uses -- the in-progress def against the core's
  // cached image -- so this is the machine's real answer and not a preview.
  //
  // It deliberately does NOT touch the def, redux inspection state, or the
  // shapes. A test you have to undo is a test nobody runs twice.
  // ONE inspection, optionally against a deliberately degraded image.
  //
  // Both the single test and the robustness sweep go through here, so there is
  // one description of what a test run IS: the current unsaved def, the core's
  // cached image, the def's own mmpp, and nothing written back anywhere.
  const inspectOnce = useCallback((perturb) => {
    if (!sendBPG) return Promise.reject(new Error('no link'));
    let deffile;
    try { deffile = defFileGeneration(edit_info); stampRefImagePath(deffile, edit_info); }
    catch (e) { return Promise.reject(e); }
    const img_property = {
      // calibInfo disabled: the def's own mmpp is the scale, exactly as
      // DefConfUI's orientation inspect does it. Letting a live calibration in
      // would test a different machine than the one the def describes.
      calibInfo: { type: 'disable', mmpp: deffile.featureSet[0].mmpp },
    };
    if (perturb) img_property.perturb = perturb;
    return new Promise((resolve, reject) => sendBPG('II', 0, {
      definfo: deffile, imgsrc: '__CACHE_IMG__', img_property,
    }, undefined, { resolve, reject }))
      .then((pkts) => {
        const rp = (pkts || []).find((p) => p.type === 'RP');
        return inspectSummary(rp && rp.data, edit_info.def_image_reg);
      });
  }, [sendBPG, edit_info]);

  // "測試檢驗": run a REAL inspection with the current, unsaved settings and show
  // what the machine did with them.
  //
  // Until this existed the studio could only be set up, never tried: you drew
  // regions, pressed save, left, and found out somewhere else. II is the same
  // round trip DefConfUI's CHECK uses, so this is the machine's real answer and
  // not a preview. It touches neither the def nor redux -- a test you have to
  // undo is a test nobody runs twice.
  const runInspect = useCallback(() => {
    setInspBusy(true);
    inspectOnce(null)
      .then(setInsp)
      .catch((e) => setInsp({ located: false, rows: [], counts: { ok: 0, na: 0, ng: 0 },
                              why: 'core 沒有回應:' + (e && e.message ? e.message : e) }))
      .finally(() => setInspBusy(false));
  }, [inspectOnce]);

  // ROBUSTNESS SWEEP: degrade the scene along one axis and watch where the
  // locator gives up.
  //
  // Sequential, not parallel. The core holds ONE cached image and one matching
  // engine behind a lock, so firing the whole sweep at once would serialise in
  // the core anyway while making the progress meaningless and the abort
  // impossible. One at a time also means the panel can show the curve building.
  const runSweep = useCallback(async () => {
    const A = SWEEP_AXES[sweepAxis];
    const r = sweepRange[sweepAxis] || {};
    const from = Number.isFinite(r.from) ? r.from : A.from;
    const to = Number.isFinite(r.to) ? r.to : A.to;
    const steps = Number.isFinite(r.steps) ? r.steps : A.steps;
    const values = sweepValues(sweepAxis, from, to, steps);
    // One seed for the WHOLE sweep. A per-step seed would re-roll the noise
    // between steps of a gain sweep, so the curve would mix two variables and
    // read as noise sensitivity that is not there.
    const seed = 1 + Math.floor(Math.abs(from * 1000 + to * 37 + steps));
    abortRef.current = false;
    setSweep({ axis: sweepAxis, from, to, steps, rows: [], done: 0, total: values.length });
    let base;
    const rows = [];
    for (let i = 0; i < values.length; i++) {
      if (abortRef.current) break;
      const v = values[i];
      let sum;
      try { sum = await inspectOnce(perturbFor(sweepAxis, v, seed)); }
      catch (e) { sum = { located: false, rows: [], counts: { ok: 0, na: 0, ng: 0 },
                          why: 'core 沒有回應' }; }
      if (i === 0) {
        base = sum;
        // The baseline result is also the single-test result -- it is the same
        // run. Showing it means the overlay is populated while the sweep works.
        setInsp(sum);
      }
      rows.push(sweepRow(sweepAxis, v, sum, base));
      setSweep((sw) => (sw && sw.axis === sweepAxis
        ? { ...sw, rows: [...rows], done: i + 1 } : sw));
    }
    setSweep((sw) => (sw ? { ...sw, verdict: sweepVerdict(sweepAxis, rows),
                             aborted: abortRef.current } : sw));
  }, [sweepAxis, sweepRange, inspectOnce]);

  // "跑全部影像": the same test, once per sample sitting next to the def.
  //
  // A sweep degrades ONE image and asks how much it survives. This asks the
  // other question -- does the def hold up across the samples somebody actually
  // collected -- and that is the one that gets asked out loud ("I tried five
  // and three were off"). Until now the answer had to be assembled by switching
  // images by hand and remembering.
  //
  // Sequential, and it AWAITS the switch: the core holds one cached image, so
  // firing these together would inspect whichever image happened to be loaded.
  const runBatch = useCallback(async () => {
    if (!imageList.length) return;
    const started = currentImagePath;
    abortRef.current = false;
    setBatch({ rows: [], done: 0, total: imageList.length });
    const rows = [];
    for (let i = 0; i < imageList.length; i++) {
      if (abortRef.current) break;
      const im = imageList[i];
      let sum;
      try {
        await switchImage(im.path);
        sum = await inspectOnce(null);
      } catch (e) {
        sum = { located: false, rows: [], counts: { ok: 0, na: 0, ng: 0 },
                why: 'core 沒有回應' };
      }
      rows.push({ name: im.name, path: im.path, sum });
      setBatch((b) => (b ? { ...b, rows: [...rows], done: i + 1 } : b));
    }
    // Put the operator back on the image they were looking at. A test that
    // silently leaves you on the last sample is one you have to undo.
    if (started && started !== imageList[imageList.length - 1].path) {
      try { await switchImage(started); } catch (e) { /* best effort */ }
    }
    setBatch((b) => (b ? { ...b, aborted: abortRef.current } : b));
  }, [imageList, currentImagePath, switchImage, inspectOnce]);

  const dhook = useCallback((isCtrl, g, canvas) => {
    canvas.captureDrag = captureDrag;
    const ctx_state = { reg, mmpp, shapeList: locShapes, work: work.current,
      featPts: featRef.current, insp: inspRef.current,
      roiPts, tool, onPoly, onReg, onRoi };
    if (isCtrl) ctrlScene(g, canvas, ctx_state);
    else drawScene(g, canvas, ctx_state);
  }, [tool, reg, mmpp, locShapes, featPts, insp, roiPts, onPoly, onReg, onRoi]);

  // Show the features the def ALREADY uses, when it has any.
  //
  // NOT for a def with no registration yet -- which is every def that just came
  // out of TAKE, because Def_Retake clears def_image_reg. Two things go wrong
  // there and they compound:
  //
  //   * SF is an authoring action, so with no cache it EXTRACTS. Those features
  //     are computed against an object frame that has not been chosen yet, and
  //     drawing the registration line is exactly what marks them stale -- so
  //     they are guaranteed garbage within the next thirty seconds.
  //   * with def_image_reg absent, drawImage translates by -(0,0), which puts
  //     the object-frame origin at the IMAGE CORNER. Every one of those points
  //     lands in the top-left corner of a picture they have nothing to do with,
  //     and it reads as debris left over from the previous recipe.
  //
  // So a new object opens with a clean canvas and step 1 to do, which is what
  // the progress bar says anyway.
  useEffect(() => {
    if (reg && Number.isFinite(reg.cx)) genFeatures();
    /* eslint-disable-next-line */
  }, []);

  // Tool toggle: clicking the active tool returns to 'pan' (drag = pan, wheel = zoom),
  // so no separate pan button is needed.
  const TBtn = ({ id, children, ...p }) => (
    <Button size="small" block style={{ marginBottom: 4 }} type={tool === id ? 'primary' : 'default'}
      onClick={() => { work.current = { poly: [], cursor: null, line: null }; setTool(tool === id ? 'pan' : id); }} {...p}>
      {children}</Button>
  );

  // COUNT AND DELETE WHERE THE REGIONS ACTUALLY LIVE.
  //
  // They used to be shapes in shapeList. Since the localization polygons moved
  // into @__SBM_INFO__ / edit_info.__loc_include (they are not measurement
  // features and the closed feature vocabulary rejected them), shapeList never
  // contains a loc_include again -- so counting it returns 0 forever.
  //
  // In v1 that only showed as a "0 / 0" nobody reads. Here the progress bar is
  // derived from it, so drawing a region left step 2 permanently unfinished:
  // the region was on the canvas, drawn from these very arrays by locShapes
  // twenty lines above, and the count beside it said none.
  //
  // The delete buttons had the same root: Shape_Set against an id that is not
  // in the list is a no-op, so they did nothing at all, silently.
  const inclPolys = edit_info.__loc_include || [];
  const exclPolys = edit_info.__loc_exclude || [];
  const nIncl = inclPolys.length;
  const nExcl = exclPolys.length;
  const delLast = (type) => {
    const key = (type === 'loc_include') ? '__loc_include' : '__loc_exclude';
    const cur = edit_info[key] || [];
    if (cur.length) dispatch(DefConfAct.EditInfo_Patch({ [key]: cur.slice(0, -1) }));
  };

  // ── PROGRESS ───────────────────────────────────────────────────────────────
  // Three steps, and only three. Every one is REQUIRED for the def to locate
  // with its own locator; everything else in this panel is a tool.
  //
  // Making the tests a fourth step was the first draft and it was wrong: a step
  // that gets skipped every day teaches people to ignore the progress bar, and
  // the one thing here that must never be ignored -- 特徵已失效 -- would be
  // ignored with it.
  //
  // The state is DERIVED, never a "visited" flag: a tick means the thing is
  // actually there. A progress bar that advances by being clicked is one that
  // lies, and this panel exists because something already lied once.
  const stepDone = [
    !!(reg && Number.isFinite(reg.cx)),
    nIncl > 0,
    !!edit_info.__shape_cache && !edit_info.__shape_stale,
  ];
  const STEP_T = ['定位', '特徵範圍', '生成特徵'];
  const curStep = stepDone.findIndex((d) => !d);          // -1 = all done
  const shownStep = openStep !== undefined ? openStep : (curStep < 0 ? 2 : curStep);

  // The active tool's instruction, drawn ON the canvas.
  //
  // These were title= tooltips. The target machine is a Surface Go and may be
  // driven by touch, where hover does not exist and a tooltip is simply
  // invisible -- including "changing this invalidates the features", which is
  // exactly how a def silently stops using SBM. Text that has to be read cannot
  // live in a tooltip.
  const TOOL_HINT = {
    pan: ['平移縮放', '拖曳＝移動視角,滾輪＝縮放。不會修改任何設定。'],
    locline: ['定位線', '拖一條線:按下的點＝原點,放開的方向＝0° 軸。改了這個,特徵必須重新生成。'],
    include: ['include 生成區', '點頂點圍住零件,點回第一個頂點收尾。只有框內會抽特徵。'],
    exclude: ['exclude 避免區', '框住不要抽特徵的地方,例如會晃動的鄰件或反光。'],
    roi: ['ROI 取樣點', '點畫面新增,點既有的點刪除。改它不影響特徵,不需要重新生成。'],
  };

  const P = { ink: '#e8eaed', dim: '#8b929c', line: '#333', accent: '#1668dc',
              ok: '#49aa19', bad: '#d4380d' };

  // 40px on everything touchable. antd size="small" is 24px, which is not
  // reliably hittable with a finger on a 10-inch panel.
  const H = 40;
  const Hint = ({ children }) => (
    <div style={{ fontSize: 12, color: P.dim, lineHeight: 1.6, margin: '2px 0 10px' }}>{children}</div>
  );
  const Row = ({ label, children, unit }) => (
    <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginBottom: 7 }}>
      <span style={{ flex: '1 1 auto', fontSize: 13, color: P.ink }}>{label}</span>
      {children}
      <span style={{ flex: '0 0 44px', fontSize: 11, color: P.dim }}>{unit}</span>
    </div>
  );
  const pick = (id) => { work.current = { poly: [], cursor: null, line: null };
                         setTool(tool === id ? 'pan' : id); };

  // A collapsible block. `idx` < 100 is a numbered step; >= 100 is an optional
  // tool, which never counts toward progress.
  const Block = ({ n, title, summary, idx, opt, children }) => {
    const open = opt ? openStep === idx : shownStep === idx;
    const done = !opt && stepDone[idx];
    const now = !opt && idx === curStep;
    return <div style={{ borderTop: opt ? '1px dashed ' + P.line : '1px solid #262626' }}>
      <div role="button" tabIndex={0}
        data-testid={'sbm2-block-' + (opt ? 'opt' : 'step') + '-' + idx}
        data-done={done ? '1' : '0'} data-now={now ? '1' : '0'}
        data-open={open ? '1' : '0'}
        onClick={() => setOpenStep(open ? -99 : idx)}
        onKeyDown={(e) => { if (e.key === 'Enter' || e.key === ' ') setOpenStep(open ? -99 : idx); }}
        style={{ display: 'flex', alignItems: 'center', gap: 10, minHeight: 46,
                 cursor: 'pointer', padding: '2px 0' }}>
        <span style={{ flex: '0 0 22px', height: 22, borderRadius: 11, fontSize: 11,
          fontWeight: 700, display: 'flex', alignItems: 'center', justifyContent: 'center',
          background: done ? P.ok : (now ? P.accent : 'transparent'),
          border: (done || now) ? 'none' : '1px dashed ' + P.line,
          color: (done || now) ? '#fff' : P.dim }}>{n}</span>
        <span style={{ flex: '1 1 auto', fontSize: 14, fontWeight: 600,
                       color: now ? '#5b9dff' : P.ink }}>{title}</span>
        <span style={{ fontSize: 12, color: P.dim }}>{summary}</span>
        <span style={{ flex: '0 0 18px', textAlign: 'center', color: P.dim,
          transform: open ? 'rotate(90deg)' : 'none', transition: 'transform .15s' }}>›</span>
      </div>
      {open ? <div style={{ padding: '0 0 8px 32px' }}>{children}</div> : null}
    </div>;
  };

  const toolHint = TOOL_HINT[tool] || TOOL_HINT.pan;
  const nFeat = ((featPts && featPts.features) || []).length;

  // The derived state, published. "step 3 of 3" and "the features match the
  // settings" are the assertions worth making here; that a button was clicked
  // is not. See TEAM_HANDOFF §13.
  return <div className="sbm2-root" data-testid="sbm2"
    data-step={curStep < 0 ? 'done' : String(curStep + 1)}
    data-done={stepDone.map((d) => (d ? '1' : '0')).join('')}
    data-stale={edit_info.__shape_stale ? '1' : '0'}
    data-features={String(nFeat)}
    data-roi={String(roiPts.length)}
    data-regions={nIncl + '/' + nExcl}
    data-tool={tool}
    style={{ display: 'flex', height: '100%', minHeight: 0, gap: 8, color: P.ink }}>
    {/* Landscape: canvas | rail. Portrait: canvas above, rail below.
        The v1 studio is height:84vh with a fixed 240px column, which overflows a
        portrait 10-inch screen; this is flex in both directions and the canvas
        never scrolls, so the whole image stays visible whatever the rail does. */}
    <style>{SBM2_CSS}</style>

    <div className="sbm2-canvas" style={{ position: 'relative', border: '1px solid ' + P.line,
                                          borderRadius: 6, overflow: 'hidden' }}>
      <HookCanvasComponent key={edit_info.img ? 'img' : 'noimg'}
        dhook={dhook} image={edit_info.img} captureDrag={captureDrag} />

      {edit_info.img ? null : (
        <div style={{ position: 'absolute', inset: 0, display: 'flex', alignItems: 'center',
                      justifyContent: 'center', pointerEvents: 'none', color: P.dim,
                      textAlign: 'center', lineHeight: 1.9 }}>
          <div>尚未擷取影像<br />
            <span style={{ fontSize: 12 }}>先在檢驗準備裡拍一張,再回來設定 SBM 定位。</span>
          </div>
        </div>
      )}

      {/* Tools live where they act, at 44px, and the pressed one IS the mode. */}
      <div style={{ position: 'absolute', top: 10, left: 10, display: 'flex',
                    flexDirection: 'column', gap: 6, zIndex: 2 }}>
        {[['pan', '✥'], ['locline', '✛'], ['include', '＋'], ['exclude', '－'], ['roi', '◻']]
          .map(([id, ic]) => (
          <div key={id} role="button" tabIndex={0} aria-pressed={tool === id}
            data-testid={'sbm2-tool-' + id}
            onClick={() => pick(id)}
            style={{ width: 44, height: 44, borderRadius: 9, display: 'flex',
              alignItems: 'center', justifyContent: 'center', fontSize: 19, cursor: 'pointer',
              userSelect: 'none', touchAction: 'manipulation',
              background: tool === id ? P.accent : 'rgba(255,255,255,.08)',
              border: '1px solid ' + (tool === id ? 'transparent' : 'rgba(255,255,255,.14)'),
              color: '#eef1f5' }}>{ic}</div>
        ))}
      </div>

      {/* The numbers that answer "is this recipe healthy", parked in the space a
          contain-fit leaves. Costs no rail height and is never scrolled away. */}
      <div style={{ position: 'absolute', top: 10, right: 10, display: 'flex', gap: 6,
                    zIndex: 2, flexWrap: 'wrap', justifyContent: 'flex-end', maxWidth: '64%' }}>
        {insp && insp.located
          ? <Chip2 label="分數" value={insp.pose.similarity.toFixed(3)}
              tone={insp.pose.similarity >= floorInfo.floor ? '#7ee2a8' : '#ff9a90'} />
          : null}
        <Chip2 label="特徵" value={nFeat} />
        <Chip2 label="ROI" value={roiPts.length} />
        <Chip2 label="區域" value={nIncl + '／' + nExcl} />
      </div>

      <div style={{ position: 'absolute', left: 10, right: 10, bottom: 10, zIndex: 2,
        background: 'rgba(0,0,0,.75)', border: '1px solid rgba(255,255,255,.12)',
        borderRadius: 8, padding: '8px 11px', fontSize: 12.5, lineHeight: 1.55,
        color: '#e6eaf0', pointerEvents: 'none' }}>
        <b style={{ color: '#fff' }}>{toolHint[0]}</b>　{toolHint[1]}
      </div>
    </div>

    <div className="sbm2-rail" style={{ display: 'flex', flexDirection: 'column',
                                        minWidth: 0, fontSize: 13 }}>

      {/* Not a tool and not a step. A def in this state leaves here unable to use
          its own locator, invisibly -- so it keeps the top and both exits. */}
      {edit_info.__shape_stale &&
        <div style={{ flex: '0 0 auto', border: '1px solid ' + P.bad, background: '#2a1215',
                      borderRadius: 8, padding: 11, marginBottom: 9 }}>
          <div style={{ color: '#ff7875', fontWeight: 700, fontSize: 14, marginBottom: 4 }}>
            ⚠ 特徵已失效</div>
          <div style={{ color: '#e0b4b4', fontSize: 12.5, lineHeight: 1.6 }}>
            你改了定位,目前的 SBM 特徵跟它對不上了。<b style={{ color: '#ff7875' }}>
            這樣離開的話,這個 def 不會用 SBM 定位</b>——它會退回 sig360,而且畫面上看不出來。
          </div>
          <div style={{ display: 'flex', gap: 6, marginTop: 8 }}>
            <Button danger type="primary" style={{ flex: 1, height: H }}
              loading={genBusy} onClick={() => genFeatures(true)}>重新生成特徵</Button>
            <Button style={{ flex: 1, height: H }} disabled={!edit_info.__shape_lastGood}
              onClick={() => {
                const lg = edit_info.__shape_lastGood;
                dispatch(DefConfAct.EditInfo_Patch({
                  def_image_reg: lg.def_image_reg, roi_refine_points: lg.roi_refine_points,
                  __shape_cache: lg.cache, __shape_stale: undefined, __shape_lastGood: undefined,
                }));
              }}>還原上一版</Button>
          </div>
        </div>}

      {/* PINNED. The block below scrolls; this must not, or the reader loses
          their place exactly when the panel is long enough to need it. */}
      <div style={{ flex: '0 0 auto', border: '1px solid ' + P.line, borderRadius: 8,
                    background: '#1f232a', padding: '9px 10px 10px', marginBottom: 9 }}>
        <div style={{ display: 'flex', alignItems: 'baseline', gap: 7, marginBottom: 8 }}>
          <span style={{ fontSize: 11, letterSpacing: '.08em', color: P.dim, fontWeight: 600 }}>
            步驟 {curStep < 0 ? 3 : curStep + 1}／3</span>
          <span style={{ fontSize: 14, fontWeight: 600 }}>
            {curStep < 0 ? '都完成了' : STEP_T[curStep]}</span>
        </div>
        <div style={{ display: 'flex', gap: 4 }}>
          {STEP_T.map((t, i) => (
            <div key={t} role="button" tabIndex={0} data-testid={'sbm2-seg-' + i}
              data-state={stepDone[i] ? 'done' : (i === curStep ? 'now' : 'todo')}
              onClick={() => setOpenStep(i)}
              onKeyDown={(e) => { if (e.key === 'Enter') setOpenStep(i); }}
              style={{ flex: 1, height: 38, borderRadius: 6, display: 'flex',
                alignItems: 'center', justifyContent: 'center', fontSize: 12, fontWeight: 600,
                cursor: 'pointer', touchAction: 'manipulation',
                background: stepDone[i] ? P.ok : (i === curStep ? P.accent : 'transparent'),
                border: (stepDone[i] || i === curStep) ? 'none' : '1px solid ' + P.line,
                color: (stepDone[i] || i === curStep) ? '#fff' : P.dim }}>
              {stepDone[i] ? '✓' : i + 1}</div>
          ))}
        </div>
      </div>

      {/* ONLY this scrolls. */}
      <div style={{ flex: '1 1 auto', minHeight: 0, overflowY: 'auto', paddingRight: 5 }}>

        {imageList.length > 1 &&
          <div style={{ display: 'flex', alignItems: 'center', gap: 6, marginBottom: 8 }}>
            <span style={{ flex: '0 0 auto', color: P.dim, fontSize: 12 }}>影像</span>
            <Select style={{ flex: '1 1 auto', minWidth: 0 }} value={currentImagePath}
              onChange={switchImage}
              options={imageList.map((im) => ({ value: im.path, label: im.name }))} />
          </div>}

        <Block n="1" idx={0} title="定位"
          summary={Number.isFinite(reg.cx)
            ? reg.cx.toFixed(2) + ', ' + reg.cy.toFixed(2) + ' · '
              + ((reg.angle || 0) * 180 / Math.PI).toFixed(1) + '°'
            : '未設定'}>
          <Button block style={{ height: H, marginBottom: 6 }}
            type={tool === 'locline' ? 'primary' : 'default'}
            onClick={() => pick('locline')}>✛ 拖曳設定定位線</Button>
          <Hint>按下＝原點,放開的方向＝0° 軸。
            <b style={{ color: P.ink }}>改了要重新生成特徵。</b></Hint>
        </Block>

        <Block n="2" idx={1} title="特徵範圍" summary={nIncl + ' ／ ' + nExcl}>
          <div style={{ display: 'flex', gap: 6, marginBottom: 6 }}>
            <Button style={{ flex: 1, height: H }} type={tool === 'include' ? 'primary' : 'default'}
              onClick={() => pick('include')}>＋ include</Button>
            <Button style={{ flex: 1, height: H }} type={tool === 'exclude' ? 'primary' : 'default'}
              onClick={() => pick('exclude')}>－ exclude</Button>
          </div>
          <div style={{ display: 'flex', gap: 6 }}>
            <Button style={{ flex: 1, height: H }} onClick={() => delLast('loc_include')}>刪 include</Button>
            <Button style={{ flex: 1, height: H }} onClick={() => delLast('loc_exclude')}>刪 exclude</Button>
          </div>
          <Hint>點頂點圍住零件,點回第一個頂點收尾。</Hint>
        </Block>

        <Block n="3" idx={2} title="生成特徵"
          summary={edit_info.__shape_cache ? nFeat + ' 點' : '未生成'}>
          {/* Refused, not hidden, and it says which step is missing.
              Features are extracted relative to the object frame, so extracting
              before there is one produces points that are stale the moment the
              registration line is drawn -- and until then they render at the
              image corner, where they look like leftovers from another recipe.
              This is reachable by opening step 3 directly; the pinned next
              button already sends an unregistered def to step 1. */}
          <Button data-testid="sbm2-generate" block type="primary"
            style={{ height: H, marginBottom: 6 }}
            data-enabled={stepDone[0] ? '1' : '0'}
            disabled={!stepDone[0]}
            loading={genBusy} onClick={() => genFeatures(true)}>🔵 生成特徵點</Button>
          {stepDone[0]
            ? <Hint>把目前的定位、範圍、邊緣門檻送給 core,抽出這個配方要用的特徵點(藍)。
                需要 &lt;配方名&gt;.png 已經在磁碟上。</Hint>
            : <Hint><b style={{ color: '#e0902f' }}>要先做第 1 步「定位」。</b>
                特徵是相對於定位原點抽出來的 —— 沒有原點就抽,畫出來會落在影像角落,
                而且你一畫定位線它們就失效了。</Hint>}
        </Block>

        <div style={{ fontSize: 10.5, letterSpacing: '.1em', color: P.dim, fontWeight: 600,
                      margin: '16px 0 2px', paddingTop: 10, borderTop: '2px solid ' + P.line }}>
          工具 · 選用,不影響進度
        </div>

        <Block n="◈" idx={100} opt title="測試" summary="選用">
          <Hint>這些都不是必要步驟,也不會擋住離開。它們只是幫你判斷這個配方能不能用。</Hint>
          <Button block ghost style={{ height: H, marginBottom: 6 }}
            loading={inspBusy} onClick={runInspect}>▶ 跑一次檢驗</Button>
          {insp && <InspectPanel insp={insp} onClear={() => setInsp(undefined)} />}
          {imageList.length > 1 && <>
            <div style={{ display: 'flex', gap: 6, marginTop: 6 }}>
              <Button style={{ flex: 1, height: H }} onClick={runBatch}
                disabled={!!(batch && batch.done < batch.total && !batch.aborted)}>全資料夾</Button>
              <Button danger style={{ height: H }} onClick={() => { abortRef.current = true; }}
                disabled={!(batch && batch.done < batch.total && !batch.aborted)}>中止</Button>
            </div>
            <BatchPanel batch={batch} onClear={() => setBatch(undefined)} />
          </>}
        </Block>

        <Block n="◈" idx={101} opt title="強健性掃描" summary="選用">
          <Select style={{ width: '100%', marginBottom: 6 }} value={sweepAxis} onChange={setSweepAxis}
            options={Object.entries(SWEEP_AXES).map(([k, a]) => ({ value: k, label: a.label }))} />
          <div style={{ display: 'flex', gap: 4, marginBottom: 6 }}>
            {['from', 'to', 'steps'].map((f) => {
              const A = SWEEP_AXES[sweepAxis];
              const r = sweepRange[sweepAxis] || {};
              return <InputNumber key={f} style={{ flex: 1, minWidth: 0 }}
                value={r[f] !== undefined ? r[f] : A[f]}
                step={f === 'steps' ? 1 : (A.unit === '×' ? 0.05 : 1)}
                onChange={(v) => setSweepRange((s0) => ({ ...s0,
                  [sweepAxis]: { ...(s0[sweepAxis] || {}), [f]: v } }))} />;
            })}
          </div>
          <div style={{ display: 'flex', gap: 6 }}>
            <Button style={{ flex: 1, height: H }} onClick={runSweep}
              disabled={!!(sweep && sweep.done < sweep.total && !sweep.aborted)}>▶▶ 掃描</Button>
            <Button danger style={{ height: H }} onClick={() => { abortRef.current = true; }}
              disabled={!(sweep && sweep.done < sweep.total && !sweep.aborted)}>中止</Button>
          </div>
          <Hint>{SWEEP_AXES[sweepAxis].hint}</Hint>
          <SweepPanel sweep={sweep} floor={floorInfo.floor} />
        </Block>

        {/* ROI is NOT a step any more.
            The core rebuilds these points from the def on every load (see
            trainShapeMatcher's cache-hit branch), so the extracted features never
            depended on them. Treating a ROI edit as "the features are now stale"
            was over-strict, and it made an optional refinement feel destructive.
            NOTE: the reducer still marks the cache stale on roi_refine_points --
            until that is changed, this label is the intent and not yet the
            behaviour. */}
        <Block n="＋" idx={102} opt title="ROI 取樣點" summary={'選用 · ' + roiPts.length}>
          <div style={{ display: 'flex', gap: 6, marginBottom: 6 }}>
            <Button style={{ flex: 1, height: H }} loading={genBusy} onClick={autoFillRoi}>⚙ 自動產生</Button>
            <Button style={{ height: H }} onClick={() => onRoi([])}>清除</Button>
          </div>
          <Button block style={{ height: H }} type={tool === 'roi' ? 'primary' : 'default'}
            onClick={() => pick('roi')}>◻ 編輯 ROI 點</Button>
          <Hint>core 只用這些點做定位微調。全部清除＝只做粗定位。</Hint>
        </Block>

        <Block n="⚙" idx={103} opt title="參數" summary="選用">
          <Row label="min score 門檻" unit="0–100">
            <InputNumber min={1} max={99} step={1} style={{ width: 92 }}
              value={edit_info.shape_min_score ?? 50}
              onChange={(v) => dispatch(DefConfAct.EditInfo_Patch({ shape_min_score: v }))} />
          </Row>
          <Hint>分數低於這個值就當作沒找到。
            <b style={{ color: P.ink }}>改它不用重新生成特徵。</b></Hint>
          <Row label="coarse scale" unit="0–1">
            <InputNumber min={0.1} max={1} step={0.1} style={{ width: 92 }}
              value={edit_info.shape_match_scale ?? 0.3}
              onChange={(v) => dispatch(DefConfAct.EditInfo_Patch({ shape_match_scale: v }))} />
          </Row>
          <Row label="angle ±" unit="度">
            <InputNumber min={0} max={180} step={5} style={{ width: 92 }}
              value={edit_info.matching_angle_margin_deg ?? 180}
              onChange={(v) => dispatch(DefConfAct.Matching_Angle_Margin_Update(v))} />
          </Row>
          <Row label="NMS 角度" unit="度">
            <InputNumber min={1} max={360} step={5} style={{ width: 92 }}
              value={edit_info.shape_nms_angle ?? 360}
              onChange={(v) => dispatch(DefConfAct.EditInfo_Patch({ shape_nms_angle: v }))} />
          </Row>
          <Row label="face" unit="">
            <Select style={{ width: 92 }} value={edit_info.matching_face ?? 1}
              onChange={(v) => dispatch(DefConfAct.Matching_Face_Update(v))}
              options={[{ value: 1, label: '正面' }, { value: -1, label: '反面' },
                        { value: 0, label: '兩面' }]} />
          </Row>
          <div style={{ borderTop: '1px solid ' + P.line, margin: '10px 0 8px' }} />
          <Row label="weak 弱邊" unit="1–255">
            <InputNumber min={1} max={255} step={5} style={{ width: 92 }}
              value={edit_info.shape_weak_thres ?? 50}
              onChange={(v) => dispatch(DefConfAct.EditInfo_Patch({ shape_weak_thres: v }))} />
          </Row>
          <Row label="strong 強邊" unit="1–255">
            <InputNumber min={1} max={255} step={5} style={{ width: 92 }}
              value={edit_info.shape_strong_thres ?? 80}
              onChange={(v) => dispatch(DefConfAct.EditInfo_Patch({ shape_strong_thres: v }))} />
          </Row>
          <Hint>邊緣門檻改了<b style={{ color: P.ink }}>要重新生成特徵</b>才會生效。</Hint>
        </Block>
      </div>

      {/* PINNED. Names the next action in words -- nobody should have to read a
          progress bar to work out which button to press. */}
      <div style={{ flex: '0 0 auto', paddingTop: 10, marginTop: 8,
                    borderTop: '1px solid ' + P.line }}>
        <div style={{ fontSize: 10.5, letterSpacing: '.1em', color: P.dim, fontWeight: 600,
                      marginBottom: 6 }}>{curStep < 0 ? '完成' : '下一步'}</div>
        {curStep < 0
          ? <Button data-testid="sbm2-next" data-action="close" type="primary" block
              style={{ height: 48, fontSize: 15, fontWeight: 600 }}
              data-enabled={edit_info.__shape_stale ? '0' : '1'}
              disabled={!!edit_info.__shape_stale}
              onClick={() => onClose && onClose()}>✓ 套用並離開</Button>
          : <Button data-testid="sbm2-next"
              data-action={curStep === 0 ? 'locline' : curStep === 1 ? 'include' : 'generate'}
              type="primary" block style={{ height: 48, fontSize: 15, fontWeight: 600 }}
              loading={curStep === 2 && genBusy}
              onClick={() => {
                setOpenStep(curStep);
                if (curStep === 0) pick('locline');
                else if (curStep === 1) pick('include');
                else genFeatures(true);
              }}>
              {curStep === 0 ? '✛ 設定定位線' : curStep === 1 ? '＋ 畫 include 區' : '🔵 生成特徵點'}
            </Button>}
        <div style={{ fontSize: 11.5, color: edit_info.__shape_stale ? '#ff7875' : P.dim,
                      textAlign: 'center', marginTop: 6 }}>
          {edit_info.__shape_stale ? '要先處理上面的「特徵已失效」'
            : curStep < 0 ? '設定即時套用到編輯暫存;回主編輯器按「存檔」才寫入磁碟。'
            : curStep === 0 ? '原點和 0° 軸決定特徵怎麼對齊'
            : curStep === 1 ? '只有 include 框內會抽特徵'
            : '這一步做完就可以離開了'}
        </div>
      </div>
    </div>
  </div>;
}

// A canvas-corner readout: small enough to live in the letterbox a contain-fit
// leaves, legible enough to be what somebody checks before walking away.
function Chip2({ label, value, tone }) {
  return <span style={{ fontSize: 11.5, padding: '4px 9px', borderRadius: 20,
    background: 'rgba(255,255,255,.09)', border: '1px solid rgba(255,255,255,.11)',
    color: '#d3d9e2', whiteSpace: 'nowrap' }}>
    {label} <b style={{ color: tone || '#fff', fontWeight: 600,
                        fontVariantNumeric: 'tabular-nums' }}>{value}</b></span>;
}
