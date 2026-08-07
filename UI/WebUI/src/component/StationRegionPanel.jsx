// The station: inspection region + clean-space regions.
//
// A NEW block. Nothing here touches the def editor, because none of this
// belongs to the def: it describes where the part sits when the camera fires
// and which patch of plate has to be empty -- mechanics, not product. A def
// carrying it would be redrawn per product for no reason and would be wrong the
// moment it was copied to another machine. So it lives in machine_setting.json
// and is authored here, on the live image, in InspectionUI.
//
// Coordinates are FULL-SENSOR PIXELS throughout -- the same frame the core's
// `inspection_region` and the lens model use. Not mm, and not the def's object
// frame: those move when the calibration or the product moves, and the station
// does not.
//
//   inspection_region : {x,y,w,h}     -- an object whose CENTRE falls outside is
//                                        dropped by the locator before any work
//   clean_regions     : [{x,y,w,h,name,dark_thresh,dark_area_max,on_fail}]
//
// The whole component is kept in one file on purpose: the drag mode, the
// numeric fields, the overlay push and the save are one interaction, and
// splitting them would mean threading canvas state through three modules to
// gain nothing.
import React, { useState, useEffect, useRef } from 'react';
import { useSelector } from 'react-redux';
import { Button, InputNumber, Divider, Select, Popconfirm, Tooltip } from 'antd';
import { AimOutlined, DeleteOutlined, SaveOutlined, PlusOutlined } from '@ant-design/icons';
import log from 'loglevel';

const EMPTY_REGION = { x: 0, y: 0, w: 0, h: 0 };

// localStorage, keyed per browser, is the working copy.
//
// The regions describe THIS machine's station, and the browser is on it, so a
// half-finished setup surviving an F5 is worth more than it costs. It also
// decouples authoring from the save round-trip: you can draw the boxes with the
// core down, or before you are ready to commit them.
//
// machine_setting.json stays authoritative -- it is what the core reads. The
// local copy only wins while it is DIRTY, i.e. edited and not yet saved; once
// saved, both agree and the machine setting takes over again. That ordering
// matters: a stale local draft must never quietly override what the machine is
// actually running.
const LS_KEY = 'visSele.station.draft.v1';

// The camera ROI origin, and why everything here has to know about it.
//
// Regions are stored in FULL-SENSOR px so the station does not move when
// somebody changes the camera crop -- it is a physical place on the machine.
// The canvas, however, draws the streamed image as if it were the whole frame:
// img_info.offsetX is 0 and full_width equals width even under a 1136x640 crop,
// so canvas coordinates are ROI-LOCAL.
//
// Those two agreed exactly while the ROI was 0,0 and diverged the moment one was
// set -- the saved boxes rendered a thousand pixels off the image, and a fresh
// drag would have stored coordinates a thousand pixels wrong in the other
// direction. So convert at the boundary: canvas + origin = stored, stored -
// origin = canvas.
//
// The origin comes from the WebUI's own record of the ROI it asked for
// (LS_INSP_ROI, written as FullSensorROI in InspectionUI). CAVEAT: that is the
// REQUESTED rectangle. The camera snaps it to its alignment increments -- asked
// 1017.47,331.94, got 1016,328 -- so a few px of residual remain. The right
// long-term source is the core's own sampler->getOriginOffset(), which is what
// the region filter actually compares against; it just is not sent to the UI
// today.
// FALLBACK ONLY. The core now sends its own sampler->getOriginOffset() in the
// report's `station` block -- the exact value the region filter adds -- so the
// panel and the filter cannot drift apart. This path is what you get before the
// first report arrives, or against a core too old to send it.
const LS_ROI_KEY = 'LS_INSP_ROI';
function roiOriginFromLS() {
  try {
    const v = JSON.parse(localStorage.getItem(LS_ROI_KEY) || 'null');
    if (Array.isArray(v) && v.length >= 2 && isFinite(v[0]) && isFinite(v[1]))
      return { x: Math.round(v[0]), y: Math.round(v[1]) };
  } catch (e) { log.warn('[station] LS_INSP_ROI unreadable', e); }
  return { x: 0, y: 0 };
}
const toStored = (r, o) => ({ ...r, x: Math.round(r.x + o.x), y: Math.round(r.y + o.y) });
const toCanvas = (r, o) => (r && r.w > 0 && r.h > 0)
  ? { ...r, x: r.x - o.x, y: r.y - o.y } : r;
const lsLoad = () => {
  try { const s = localStorage.getItem(LS_KEY); return s ? JSON.parse(s) : null; }
  catch (e) { log.warn('[station] localStorage read failed', e); return null; }
};
const lsSave = (v) => {
  try { localStorage.setItem(LS_KEY, JSON.stringify(v)); }
  catch (e) { log.warn('[station] localStorage write failed', e); }
};
const lsClear = () => { try { localStorage.removeItem(LS_KEY); } catch (e) { /* ignore */ } };

// A drag gives two opposite corners in any order; a region is an origin + size.
function rectFromDrag(info) {
  const a = info && info.start && info.start.pix;
  const b = info && info.end && info.end.pix;
  if (!a || !b) return null;
  const x = Math.round(Math.min(a.x, b.x)), y = Math.round(Math.min(a.y, b.y));
  const w = Math.round(Math.abs(b.x - a.x)), h = Math.round(Math.abs(b.y - a.y));
  if (w < 2 || h < 2) return null;      // a click, not a drag
  return { x, y, w, h };
}

function NumRow({ label, value, onChange, suffix }) {
  return <div style={{ display: 'flex', alignItems: 'center', gap: 6, fontSize: 12 }}>
    <span style={{ width: 16, color: '#888' }}>{label}</span>
    <InputNumber size="small" style={{ width: 78 }} value={value} step={1}
      onChange={(v) => onChange(Math.round(v || 0))} />
    {suffix ? <span style={{ color: '#888' }}>{suffix}</span> : null}
  </div>;
}

// The numbers are for debugging, not for the operator: a station is set by
// dragging a box round the part, and nobody types 1378 to do that. Shown as a
// read-only one-liner, with the editable fields behind a toggle -- they still
// matter when you need to nudge an edge by 10px or reproduce a value from a log.
function RectFields({ rect, onChange, showNumbers }) {
  const set = (k) => (v) => onChange({ ...rect, [k]: v });
  if (!showNumbers) {
    return <div style={{ fontSize: 11, color: '#888', margin: '2px 0 4px' }}>
      {(rect.w > 0 && rect.h > 0) ? `${rect.w}×${rect.h} px @${rect.x},${rect.y}` : '尚未框選'}
    </div>;
  }
  return <div style={{ display: 'flex', gap: 10, flexWrap: 'wrap', margin: '2px 0 4px' }}>
    <NumRow label="x" value={rect.x} onChange={set('x')} />
    <NumRow label="y" value={rect.y} onChange={set('y')} />
    <NumRow label="w" value={rect.w} onChange={set('w')} />
    <NumRow label="h" value={rect.h} onChange={set('h')} />
  </div>;
}

/**
 * props:
 *   ecCanvas        the INSP_CanvasComponent instance (InspectionUI holds it)
 *   machineSetting  state.UIData.machine_custom_setting
 *   onApply(patch)  push live to the core   (ST { MachineSetting })
 *   onSave(setting) persist                 (SV data/machine_setting.json)
 */
export function StationRegionPanel({ ecCanvas, machineSetting, onApply, onSave }) {
  const draft = useRef(lsLoad());
  const [region, setRegion] = useState(() => ({ ...EMPTY_REGION, ...((draft.current && draft.current.region) || {}) }));
  const [clean, setClean]   = useState(() => (draft.current && draft.current.clean) || []);
  const [dirty, setDirty]   = useState(() => !!(draft.current && draft.current.dirty));
  const [aiming, setAiming] = useState(null);   // null | 'region' | <clean index>
  const [open, setOpen]     = useState(false);  // collapsed by default: sidebar space
  const [nums, setNums]     = useState(false);  // xywh fields: debugging, not operating
  const loadedFrom = useRef(null);

  // What the core is actually doing, straight from the report it already sends.
  const station = useSelector((st) =>
    (st && st.UIData && st.UIData.edit_info && st.UIData.edit_info.station) || null);
  const origin = (station && Array.isArray(station.roi_origin))
    ? { x: Math.round(station.roi_origin[0]), y: Math.round(station.roi_origin[1]) }
    : roiOriginFromLS();
  // The drag callback is installed once per aiming session, so it must not close
  // over a stale origin.
  const originRef = useRef(origin);
  originRef.current = origin;

  // Adopt whatever the core last told us, but never stomp on edits in progress.
  useEffect(() => {
    if (!machineSetting || dirty) return;
    const sig = JSON.stringify([machineSetting.inspection_region, machineSetting.clean_regions]);
    if (sig === loadedFrom.current) return;
    loadedFrom.current = sig;
    setRegion({ ...EMPTY_REGION, ...(machineSetting.inspection_region || {}) });
    setClean(Array.isArray(machineSetting.clean_regions) ? machineSetting.clean_regions : []);
  }, [machineSetting, dirty]);

  // Mirror every edit to localStorage. Only while dirty -- a clean panel is just
  // showing what the machine already has, and storing that would resurrect it as
  // a "draft" that outranks a change made from somewhere else.
  useEffect(() => {
    if (dirty) lsSave({ region, clean, dirty: true });
    else lsClear();
  }, [region, clean, dirty]);

  // Mirror to the canvas whenever anything moves.
  useEffect(() => {
    if (ecCanvas && typeof ecCanvas.SetStationOverlay === 'function') {
      // Geometry only. The state (verdict, clean/dirty) is read by the canvas
      // straight out of the image-paired snapshot -- pushing it from here raced
      // the image, because a useEffect and a componentWillUpdate do not order.
      const o = origin;
      ecCanvas.SetStationOverlay({
        region: toCanvas(region, o),
        // `key` matches the core's per-region name; `name` is what gets drawn.
        // Folding them together made the box caption read "clean1".
        clean: clean.map((c, i) => ({ ...toCanvas(c, o),
                                      key: c.name || ('clean' + (i + 1)),
                                      name: c.name || ('淨空' + (i + 1)) })),
      });
    }
  }, [ecCanvas, region, clean, origin.x, origin.y]);

  // Drag-to-set. The canvas clears its own callback after one drag, so aiming
  // is a one-shot: press the target, drag once, done. That is deliberate --
  // leaving the canvas in ROI mode swallows pan/zoom, and an operator who
  // wandered off mid-setup would find the image frozen with no explanation.
  useEffect(() => {
    if (aiming === null || !ecCanvas) return;
    ecCanvas.SetROISettingCallBack((info) => {
      const r = rectFromDrag(info);
      setAiming(null);
      if (!r) { log.debug('[station] drag too small, ignored'); return; }
      const o = originRef.current;
      const rs = toStored(r, o);           // canvas px -> full-sensor px
      log.debug('[station] drag', r, '+ roi origin', o, '=', rs);
      setDirty(true);
      if (aiming === 'region') setRegion((p) => ({ ...p, ...rs }));
      else setClean((cs) => cs.map((c, i) => (i === aiming ? { ...c, ...rs } : c)));
    });
    return () => { if (ecCanvas.SetROISettingCallBack) ecCanvas.SetROISettingCallBack(undefined); };
  }, [aiming, ecCanvas]);

  const edit = (fn) => { setDirty(true); fn(); };
  const built = () => ({
    inspection_region: (region.w > 0 && region.h > 0)
      ? { ...region, fit: region.fit === 'center' ? 'center' : 'contain' }
      : undefined,
    clean_regions: clean.length ? clean : undefined,
  });

  // Arming without a canvas handle is the worst possible failure here: the
  // button lights up, the operator drags, the image pans, and nothing says why.
  // That is exactly what happened when this panel was mounted somewhere that
  // never received the canvas. Refuse to arm, and say so.
  const canAim = !!(ecCanvas && typeof ecCanvas.SetROISettingCallBack === 'function');
  const AimBtn = ({ target, children }) => (
    <Button size="small" icon={<AimOutlined />} disabled={!canAim}
      title={canAim ? undefined : '畫布尚未就緒'}
      type={aiming === target ? 'primary' : 'default'}
      onClick={() => setAiming(aiming === target ? null : target)}>
      {aiming === target ? '在影像上拉框…' : children}
    </Button>
  );

  // Collapsed by default. This is setup, not monitoring: it is touched when the
  // machine is being commissioned and not once per shift, so it must not sit
  // between the operator and the counters they actually watch. The one-line
  // summary is enough to tell at a glance that a region IS set -- which is the
  // only thing about it that matters while parts are running.
  const summary = (region.w > 0 && region.h > 0)
    ? `${region.w}×${region.h} @${region.x},${region.y}`
    : '未設定(不限制)';
  const header = (
    <div onClick={() => setOpen(!open)}
      style={{ display: 'flex', alignItems: 'center', gap: 6, cursor: 'pointer',
               fontSize: 11, padding: '2px 8px', userSelect: 'none' }}>
      <span style={{ color: '#888' }}>{open ? '▾' : '▸'}</span>
      <span style={{ color: '#888' }}>工位</span>
      <span style={{ color: (region.w > 0 && region.h > 0) ? '#00b0ff' : '#888' }}>{summary}</span>
      {clean.length ? <span style={{ color: '#ffab00' }}>· 淨空 {clean.length}</span> : null}
      {dirty ? <span style={{ color: '#d48806' }}>· 未存檔</span> : null}
    </div>
  );

  if (!open) return header;
  // The prose that used to sit between every control is behind a "?" now. It was
  // three paragraphs for two controls, and once a station is set up nobody reads
  // it again -- but the day you do need it, it is a hover away instead of gone.
  const Q = ({ children }) => (
    <Tooltip title={<div style={{ maxWidth: 300, fontSize: 12 }}>{children}</div>}>
      <span style={{ cursor: 'help', color: '#888', border: '1px solid #bbb',
        borderRadius: '50%', fontSize: 10, lineHeight: '13px', width: 14, height: 14,
        display: 'inline-block', textAlign: 'center', marginLeft: 4 }}>?</span>
    </Tooltip>
  );
  const Row = ({ children, gap = 6 }) => (
    <div style={{ display: 'flex', alignItems: 'center', gap, flexWrap: 'wrap',
                  margin: '3px 0' }}>{children}</div>
  );

  // Only worth screen space when it DISAGREES. In sync it is one tick; out of
  // sync it is the most important line in the panel, because the boxes look
  // right and the machine is doing something else.
  const inSync = station && station.region
    && Math.round(station.region.x) === region.x && Math.round(station.region.y) === region.y
    && Math.round(station.region.w) === region.w && Math.round(station.region.h) === region.h;

  return <div style={{ padding: '0 8px 4px', textAlign: 'left', whiteSpace: 'normal' }}>
    {header}

    <Row>
      <AimBtn target="region">拉框設定</AimBtn>
      <Select size="small" style={{ width: 112 }}
        value={region.fit === 'center' ? 'center' : 'contain'}
        onChange={(v) => edit(() => setRegion({ ...region, fit: v }))}
        options={[{ value: 'contain', label: '整顆在框內' },
                  { value: 'center',  label: '只看中心點' }]} />
      <Q>
        <b>整顆在框內</b>:外接框全部要在框內。把框畫得舒服地包住一顆,鄰居就進不來
        (它偏移的距離小於自己的寬度)。代價是零件偏移到凸出框外時會變 NA、繞回重測。<br/><br/>
        <b>只看中心點</b>:只要中心在框內就算。當間距小於零件寬度時,框會被迫小於零件本身。<br/><br/>
        物件中心/外接框落在框外就不判定。w 或 h = 0 表示不限制。單位是<b>全幀感光元件像素</b>。
      </Q>
      {region.w > 0 && region.h > 0 ? (
        <Button size="small" danger type="text" icon={<DeleteOutlined />}
          style={{ padding: '0 4px' }}
          onClick={() => edit(() => setRegion(EMPTY_REGION))} />
      ) : null}
    </Row>
    {nums ? <RectFields rect={region} showNumbers onChange={(r) => edit(() => setRegion(r))} /> : null}
    {station && !inSync && station.region ? (
      <div style={{ fontSize: 11, color: '#d48806', margin: '2px 0' }}>
        核心跑的是 {Math.round(station.region.w)}×{Math.round(station.region.h)}
        {' @'}{Math.round(station.region.x)},{Math.round(station.region.y)} — 尚未套用
      </div>
    ) : null}
    {station && !station.region ? (
      <div style={{ fontSize: 11, color: '#888', margin: '2px 0' }}>核心目前不限制區域</div>
    ) : null}

    <Divider orientation="left" style={{ margin: '6px 0 2px', fontSize: 12 }}>
      淨空區域
      <Q>低於暗門檻的面積超過上限 → 依「超出時」處理。<br/>
         NA = 視野被污染,這顆量不準,繞回重測。<br/>NG = 這顆本身不良,吹掉。</Q>
    </Divider>

    {clean.map((c, i) => {
      const m = station && Array.isArray(station.clean)
        ? station.clean.find((z) => z.name === (c.name || ('clean' + (i + 1)))) : null;
      return (
        <div key={i} style={{ borderLeft: '2px solid #ffab00', paddingLeft: 5, marginBottom: 5 }}>
          <Row>
            <AimBtn target={i}>{c.name || ('淨空' + (i + 1))}</AimBtn>
            {m ? <span style={{ fontSize: 11, color: m.dirty ? '#c33' : '#389e0d' }}>
              {m.dirty ? '有雜物 ' : '乾淨 '}{Number(m.dark_area_mm2).toFixed(4)}mm²
            </span> : <span style={{ fontSize: 11, color: '#888' }}>{c.w > 0 ? '等待影像' : '尚未框選'}</span>}
            <Popconfirm title="刪除?" onConfirm={() => edit(() => setClean(clean.filter((_, k) => k !== i)))}>
              <Button size="small" danger type="text" icon={<DeleteOutlined />}
                style={{ padding: '0 4px', marginLeft: 'auto' }} />
            </Popconfirm>
          </Row>
          <Row gap={4}>
            <span style={{ fontSize: 11, color: '#888' }}>暗</span>
            <InputNumber size="small" style={{ width: 58 }} value={c.dark_thresh ?? 128}
              onChange={(v) => edit(() => setClean(clean.map((x, k) => (k === i ? { ...x, dark_thresh: Math.round(v || 0) } : x))))} />
            <span style={{ fontSize: 11, color: '#888' }}>≤</span>
            <InputNumber size="small" style={{ width: 68 }} step={0.01} value={c.dark_area_max}
              onChange={(v) => edit(() => setClean(clean.map((x, k) => (k === i ? { ...x, dark_area_max: v } : x))))} />
            <span style={{ fontSize: 11, color: '#888' }}>mm²</span>
            <Select size="small" style={{ width: 96 }} value={c.on_fail === 'ng' ? 'ng' : 'na'}
              onChange={(v) => edit(() => setClean(clean.map((x, k) => (k === i ? { ...x, on_fail: v } : x))))}
              options={[{ value: 'na', label: '→NA' }, { value: 'ng', label: '→NG' }]} />
          </Row>
          {nums ? <RectFields rect={c} showNumbers
            onChange={(r) => edit(() => setClean(clean.map((x, k) => (k === i ? { ...x, ...r } : x))))} /> : null}
        </div>
      );
    })}

    <Row>
      <Button size="small" type="text" icon={<PlusOutlined />} style={{ padding: '0 4px' }}
        onClick={() => edit(() => setClean([...clean, { x: 0, y: 0, w: 0, h: 0, dark_thresh: 128, on_fail: 'na' }]))}>
        新增淨空區域</Button>
    </Row>

    <Row>
      <Button size="small" type="primary" icon={<SaveOutlined />} disabled={!dirty}
        onClick={() => {
          const patch = built();
          if (onApply) onApply(patch);
          if (onSave) onSave({ ...(machineSetting || {}), ...patch });
          setDirty(false);
        }}>套用並存檔</Button>
      {dirty ? <Button size="small" onClick={() => { lsClear(); loadedFrom.current = null; setDirty(false); }}>放棄</Button> : null}
      <Button size="small" type={nums ? 'primary' : 'text'} style={{ padding: '0 6px', marginLeft: 'auto' }}
        onClick={() => setNums(!nums)}>數值</Button>
    </Row>
  </div>;
}
