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
import { Button, InputNumber, Divider, Select, Popconfirm } from 'antd';
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
    if (ecCanvas && typeof ecCanvas.SetStationOverlay === 'function')
      ecCanvas.SetStationOverlay({ region, clean });
  }, [ecCanvas, region, clean]);

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
      setDirty(true);
      if (aiming === 'region') setRegion(r);
      else setClean((cs) => cs.map((c, i) => (i === aiming ? { ...c, ...r } : c)));
    });
    return () => { if (ecCanvas.SetROISettingCallBack) ecCanvas.SetROISettingCallBack(undefined); };
  }, [aiming, ecCanvas]);

  const edit = (fn) => { setDirty(true); fn(); };
  const built = () => ({
    inspection_region: (region.w > 0 && region.h > 0) ? region : undefined,
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

  // whiteSpace:'normal' is not cosmetic: this panel lives inside an antd
  // Menu title, which sets `white-space:nowrap; overflow:hidden;
  // text-overflow:ellipsis`. Without opting out, every hint line is silently
  // truncated at the sidebar edge -- the instructions vanish, not the decoration.
  return <div style={{ padding: '0 8px 4px', textAlign: 'left', whiteSpace: 'normal' }}>
    {header}
    <Divider orientation="left" style={{ margin: '4px 0', fontSize: 12 }}>檢驗區域(工位)</Divider>
    <div style={{ fontSize: 11, color: '#888', marginBottom: 4, whiteSpace: 'normal', lineHeight: 1.35 }}>
      物件中心落在框外就不判定。留空(w或h=0)= 不限制,整個畫面都算。
      單位是<b>全幀感光元件像素</b>。
    </div>
    <AimBtn target="region">拉框設定</AimBtn>
    <RectFields rect={region} showNumbers={nums} onChange={(r) => edit(() => setRegion(r))} />
    {region.w > 0 && region.h > 0 ? (
      <Button size="small" danger icon={<DeleteOutlined />}
        onClick={() => edit(() => setRegion(EMPTY_REGION))}>清除(不限制)</Button>
    ) : null}

    <Divider orientation="left" style={{ margin: '8px 0 4px', fontSize: 12 }}>淨空區域</Divider>
    <div style={{ fontSize: 11, color: '#888', marginBottom: 4, whiteSpace: 'normal', lineHeight: 1.35 }}>
      低於暗門檻的面積超過上限 → 依「超出時」處理。NA = 視野被污染,這顆量不準,繞回重測。
    </div>
    {clean.map((c, i) => (
      <div key={i} style={{ border: '1px solid #333', borderRadius: 3, padding: 4, marginBottom: 4 }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
          <AimBtn target={i}>{c.name || ('淨空' + (i + 1))}</AimBtn>
          <Popconfirm title="刪除這個淨空區域?" onConfirm={() => edit(() => setClean(clean.filter((_, k) => k !== i)))}>
            <Button size="small" danger icon={<DeleteOutlined />} />
          </Popconfirm>
        </div>
        <RectFields rect={c} showNumbers={nums} onChange={(r) => edit(() => setClean(clean.map((x, k) => (k === i ? { ...x, ...r } : x))))} />
        <div style={{ display: 'flex', gap: 10, flexWrap: 'wrap', alignItems: 'center' }}>
          <NumRow label="暗" value={c.dark_thresh ?? 128}
            onChange={(v) => edit(() => setClean(clean.map((x, k) => (k === i ? { ...x, dark_thresh: v } : x))))}
            suffix="門檻(灰階)" />
          <div style={{ display: 'flex', alignItems: 'center', gap: 6, fontSize: 12 }}>
            <span style={{ color: '#888' }}>上限</span>
            <InputNumber size="small" style={{ width: 88 }} step={0.01}
              value={c.dark_area_max}
              onChange={(v) => edit(() => setClean(clean.map((x, k) => (k === i ? { ...x, dark_area_max: v } : x))))} />
            <span style={{ color: '#888' }}>mm²</span>
          </div>
          <Select size="small" style={{ width: 140 }} value={c.on_fail === 'ng' ? 'ng' : 'na'}
            onChange={(v) => edit(() => setClean(clean.map((x, k) => (k === i ? { ...x, on_fail: v } : x))))}
            options={[{ value: 'na', label: '超出→NA(繞回)' }, { value: 'ng', label: '超出→NG(吹掉)' }]} />
        </div>
      </div>
    ))}
    <Button size="small" icon={<PlusOutlined />}
      onClick={() => edit(() => setClean([...clean, { x: 0, y: 0, w: 0, h: 0, dark_thresh: 128, on_fail: 'na' }]))}>
      新增淨空區域
    </Button>

    <Divider style={{ margin: '8px 0 4px' }} />
    <div style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
      <Button size="small" type="primary" icon={<SaveOutlined />} disabled={!dirty}
        onClick={() => {
          const patch = built();
          // Push live first, then persist. If the core rejects it you find out
          // before it is on disk, not after a restart.
          if (onApply) onApply(patch);
          if (onSave) onSave({ ...(machineSetting || {}), ...patch });
          // Clearing dirty also drops the localStorage draft (see the mirror
          // effect): once it is on the machine, the machine is the source.
          setDirty(false);
        }}>套用並存檔</Button>
      <Button size="small" type={nums ? 'primary' : 'default'}
        onClick={() => setNums(!nums)}>數值</Button>
      {dirty ? (
        <Button size="small" onClick={() => {
          // Throw the draft away and go back to what the machine is running.
          lsClear();
          loadedFrom.current = null;
          setDirty(false);
        }}>放棄變更</Button>
      ) : null}
    </div>
  </div>;
}

export default StationRegionPanel;
