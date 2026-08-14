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
import { Button, InputNumber, Divider, Select, Popconfirm, Tooltip, Switch } from 'antd';
import { AimOutlined, DeleteOutlined, SaveOutlined, PlusOutlined } from '@ant-design/icons';
import log from 'loglevel';

const EMPTY_REGION = { x: 0, y: 0, w: 0, h: 0 };

// There is no local draft. machine_setting.json is the only source.
//
// This used to mirror every edit into localStorage (`visSele.station.draft.v1`)
// and restore it on mount WITH its dirty flag, and the adopt-from-core effect
// bails out while dirty. The two together meant a draft outranked the machine
// indefinitely, across reloads and browser restarts, with nothing on screen
// saying so except whether a "放棄" button happened to be rendered.
//
// The cost was not hypothetical. On 2026-08-12 the panel showed no clean
// regions while the core was enforcing two of them out of the file, and the
// inspection returned NA for a "dirty" clean region that the operator believed
// had been deleted. Neither side was lying; they were reading different data.
//
// A per-browser copy of a MACHINE setting has no owner. Editing now lives only
// in React state: it survives until the panel unmounts, and a reload is a
// discard. That is the honest contract -- what you see is what the machine has,
// unless you are mid-edit in this very session.

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
// The origin is the core's own `sampler->getOriginOffset()`, sent in every
// report's `station` block. It is the exact value the region filter adds, so
// the drawn box and the box being enforced cannot drift apart.
//
// It used to fall back to the WebUI's record of the ROI it REQUESTED
// (localStorage `LS_INSP_ROI`), and that was measurably wrong: the camera snaps
// a requested rectangle to its alignment increments -- asked 1017.47,331.94,
// got 1016,428 -- so the panel drew the station a few px off the one the core
// compares against. A per-browser guess about a machine's geometry has no
// business overriding the machine's own answer, so it is gone.
//
// Before the first report there is no origin to have. {0,0} is the honest
// answer for that window: the panel has nothing to place a box against yet.
const ORIGIN_UNKNOWN = { x: 0, y: 0 };
const toStored = (r, o) => ({ ...r, x: Math.round(r.x + o.x), y: Math.round(r.y + o.y) });
const toCanvas = (r, o) => (r && r.w > 0 && r.h > 0)
  ? { ...r, x: r.x - o.x, y: r.y - o.y } : r;
// One-shot cleanup of the draft key this panel used to write. Without it a
// browser that has one keeps it forever -- harmless now that nothing reads it,
// but it would reappear as a mystery in devtools years from now.
try { localStorage.removeItem('visSele.station.draft.v1'); } catch (e) { /* ignore */ }

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
 *   onBypass(bool)  stop/resume enforcing   (ST { InspAreaBypass })
 */
export function StationRegionPanel({ ecCanvas, machineSetting, onApply, onSave, onBypass }) {
  // Empty until the core's machine setting arrives. Never pre-seeded from a
  // stored draft -- see the note at the top of this file.
  const [region, setRegion] = useState(EMPTY_REGION);
  const [clean, setClean]   = useState([]);
  const [dirty, setDirty]   = useState(false);
  const [aiming, setAiming] = useState(null);   // null | 'region' | <clean index>
  const [open, setOpen]     = useState(false);  // collapsed by default: sidebar space
  const [nums, setNums]     = useState(false);  // xywh fields: debugging, not operating
  const loadedFrom = useRef(null);

  // What the core is actually doing, straight from the report it already sends.
  const station = useSelector((st) =>
    (st && st.UIData && st.UIData.edit_info && st.UIData.edit_info.station) || null);
  // The report is the truth, but it only exists while an inspection is running.
  //
  // Reading the switch purely from `station.area_bypass` looked right and was
  // unusable: with no session there are no reports, so the switch sat at "off"
  // no matter what the core had been told. The core logged the change; the UI
  // showed nothing. That is indistinguishable from a dead control.
  //
  // So: echo what we asked for until a report can answer, and let the report
  // win the moment one arrives. `echo` is not a second source of truth -- it is
  // only consulted when there is no truth to be had, and it never suppresses a
  // report that disagrees (which is how another browser's change, or a core
  // restart that cleared the flag, shows up here).
  const [bypassEcho, setBypassEcho] = useState(false);
  const reported = station ? !!station.area_bypass : null;
  const bypassed = reported !== null ? reported : bypassEcho;
  const bypassUnconfirmed = reported === null;
  const origin = (station && Array.isArray(station.roi_origin))
    ? { x: Math.round(station.roi_origin[0]), y: Math.round(station.roi_origin[1]) }
    : ORIGIN_UNKNOWN;
  // The drag callback is installed once per aiming session, so it must not close
  // over a stale origin.
  const originRef = useRef(origin);
  originRef.current = origin;

  // Adopt whatever the core last told us, but never stomp on edits in progress.
  //
  // `dirty` now only ever means "edited in THIS mounted session", so the window
  // in which the panel can disagree with the machine is bounded by the edit
  // itself -- it cannot outlive a reload the way the stored draft could.
  useEffect(() => {
    if (!machineSetting || dirty) return;
    const sig = JSON.stringify([machineSetting.inspection_region, machineSetting.clean_regions]);
    if (sig === loadedFrom.current) return;
    loadedFrom.current = sig;
    setRegion({ ...EMPTY_REGION, ...(machineSetting.inspection_region || {}) });
    setClean(Array.isArray(machineSetting.clean_regions) ? machineSetting.clean_regions : []);
  }, [machineSetting, dirty]);

  // Mirror to the canvas whenever anything moves.
  useEffect(() => {
    if (ecCanvas && typeof ecCanvas.SetStationOverlay === 'function') {
      // Bypassed -> draw nothing. A box on the image is a claim that the core
      // is selecting by it, and while the bypass is on that claim is false;
      // leaving it drawn is the same silent-disagreement bug the origin
      // conversion above exists to prevent, just with the whole gate instead of
      // a few pixels. `undefined` is the canvas's own "draw nothing" input.
      //
      // Except while aiming: the drag-to-set flow needs to show the box being
      // dragged, or you drag one and nothing appears, which reads as a broken
      // control. Setting a region up while the bypass is on is a normal thing
      // to do -- that is when the machine is not there.
      if (bypassed && aiming === null) { ecCanvas.SetStationOverlay(undefined); return; }
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
  }, [ecCanvas, region, clean, origin.x, origin.y, bypassed, aiming]);

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
  // "Cleared" has to be a VALUE, not a missing key.
  //
  // These two used to emit `undefined` for the empty case, and JSON.stringify
  // drops an undefined key entirely. That is fine for onApply -- the core reads
  // a missing key as "clear" (load_clean_regions builds an empty vector and
  // assigns it unconditionally; load_insp_region likewise) -- but onSave sends
  // `{ ...machineSetting, ...patch }`, and a key that is not in the patch cannot
  // override the one already in machineSetting. So deleting the LAST clean
  // region applied live and then saved the old regions straight back, and the
  // next core start read them from the file again.
  //
  // Deleting the last one was the single state the panel could not persist.
  // `[]` and `null` both survive the spread and both read as cleared: the core
  // takes an empty array through the same loop (zero iterations) and a null
  // through `cJSON_IsObject` == false, and this panel reloads them via
  // `Array.isArray(...) ? ... : []` and `{ ...EMPTY_REGION, ...(x || {}) }`.
  const built = () => ({
    inspection_region: (region.w > 0 && region.h > 0)
      ? { ...region, fit: region.fit === 'center' ? 'center' : 'contain' }
      : null,
    clean_regions: clean,
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
      {/* The panel is collapsed by default, so this has to be legible without
          opening it. A machine that has quietly stopped enforcing its station
          is exactly the state nobody should have to expand a panel to find. */}
      {/* Kept short on purpose: the sidebar is ~225px and the summary already
          carries the geometry and the clean-region count, so a longer label
          wraps the header onto a second line. */}
      {bypassed ? <span style={{ color: '#c33', fontWeight: 'bold' }}>· 判定暫停</span> : null}
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

  // The bypass control. Its state is read from the REPORT, never from local
  // state, for the same reason the geometry is: this is a property of the core,
  // and a switch that remembers what this browser last clicked would say "off"
  // on a machine that another browser -- or the previous session, since the core
  // latches it until restart -- left on. The switch shows what is happening.
  const bypassRow = (
    <Row>
      <Switch size="small" checked={bypassed}
        onChange={(v) => { setBypassEcho(v); if (onBypass) onBypass(v); }} />
      <span style={{ fontSize: 12, color: bypassed ? '#c33' : '#888' }}>暫停區域判定</span>
      {/* Says which of the two it is showing. Without this the switch cannot
          distinguish "the core confirms this" from "nobody has reported yet". */}
      {bypassUnconfirmed ? (
        <span style={{ fontSize: 11, color: '#888' }}>(未執行檢測,尚無回報)</span>
      ) : null}
      <Q>
        暫時停用<b>工位框</b>與<b>淨空區域</b>兩項判定,讓不在工位上的影像也能量測。<br/><br/>
        用於<b>沒有機台</b>時的開發與除錯:存檔影像裡的零件不在工位上,工位框會把每顆都丟掉
        (報告變空的),淨空區域則因為畫面完全不同而判定有雜物,每一幀都變成 NA。看起來像是
        規格檔壞了,其實兩項都只是在描述機台本身。<br/><br/>
        <b>只存在於這次執行</b>:不寫入 machine_setting.json,核心重啟即恢復。但在重啟前會一直
        維持停用狀態,包含別的瀏覽器連進來的時候。
      </Q>
    </Row>
  );

  return <div style={{ padding: '0 8px 4px', textAlign: 'left', whiteSpace: 'normal' }}>
    {header}

    {bypassRow}
    {/* Stated as a banner and not just a switch position: while this is on the
        machine is not doing the job the station exists for. It also has to say
        the overlay is gone -- an empty image is otherwise indistinguishable
        from a station that was never set up. */}
    {bypassed ? (
      <div style={{ fontSize: 11, color: '#c33', border: '1px solid #c33',
                    borderRadius: 3, padding: '3px 6px', margin: '3px 0' }}>
        工位框與淨空區域<b>目前不生效</b>,畫面上的框也一併隱藏(設定仍在,按「拉框設定」
        會暫時顯示)。核心重啟即恢復。
      </div>
    ) : null}

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
      {/* Discard = drop the in-session edit and re-adopt the machine's own
          setting. Clearing loadedFrom is what forces that re-adopt: the effect
          skips work when the signature is unchanged, and it has not changed. */}
      {dirty ? <Button size="small" onClick={() => { loadedFrom.current = null; setDirty(false); }}>放棄</Button> : null}
      <Button size="small" type={nums ? 'primary' : 'text'} style={{ padding: '0 6px', marginLeft: 'auto' }}
        onClick={() => setNums(!nums)}>數值</Button>
    </Row>
  </div>;
}
