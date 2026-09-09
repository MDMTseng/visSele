// Kept inspection samples -- what the CORE is holding, drawn with the overlay.
//
// The first version of this panel buffered frames in the browser and was
// removed: the display stream is down-sampled and lossy, and the overlay did
// not land on it (docs/INSP_SAMPLE_BUFFER_2026-09-04.md). The buffer now lives
// in core memory, filled on the snapshot thread from the full-resolution frame
// and the report that came from it, so there is no pairing question and no
// scale mismatch. This panel is a viewer: SL lists, SG fetches one record.
//
// Groups are the question ("measure 10 NG but measure 3 OK"), edited here and
// remembered per def file name in localStorage (UTIL/inspSampleGroups.js);
// pushed to the core over ST. First match wins, fill-and-stop unless a group
// opted into rotating.
import React, { useState, useEffect, useMemo } from 'react';
import Modal from 'antd/lib/modal';
import Button from 'antd/lib/button';
import Tag from 'antd/lib/tag';
import Input from 'antd/lib/input';
import InputNumber from 'antd/lib/input-number';
import Select from 'antd/lib/select';
import Switch from 'antd/lib/switch';
import { ReloadOutlined, DeleteOutlined, SettingOutlined, PlusOutlined, CloseOutlined, SaveOutlined } from '@ant-design/icons';
import { loadSampleGroups, saveSampleGroups, pushSampleGroups, SAMPLE_WANTS } from 'UTIL/inspSampleGroups';
import { INSPECTION_STATUS } from 'UTIL/InspectionStatus';
import { RepDisplay } from '../RepDisplayUI.js';
import { mkLog } from 'UTIL/logger';
const log = mkLog('ui.samplepanel');

const COLOUR = { OK: '#389e0d', NG: '#cf1322', NA: '#d48806', '*': '#888' };
const num = (v) => (typeof v === 'number' && isFinite(v)) ? v.toFixed(4) : String(v);
const humanBytes = (n) => (n >= 1048576) ? (n / 1048576).toFixed(1) + ' MB'
                        : (n >= 1024) ? Math.round(n / 1024) + ' kB' : n + ' B';
const hhmmss = (ms) => {
  try { const d = new Date(ms); return d.toTimeString().slice(0, 8) + '.'
    + String(d.getMilliseconds()).padStart(3, '0'); } catch (e) { return '?'; }
};

// base64 -> bytes, in the shape the canvas already decodes (the IM frame
// object): jpegBytes + the size header fields. scale 1: this is the full frame.
function imageFromRecord(rec) {
  if (!rec || !rec.jpg_b64) return undefined;
  const bin = atob(rec.jpg_b64);
  const bytes = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
  return { format: rec.jpg_fmt || 2, jpeg_quality: 0, width: rec.w, height: rec.h,
           full_width: rec.w, full_height: rec.h, scale: 1, offsetX: 0, offsetY: 0,
           jpegBytes: bytes, image: bytes };
}

// Write one kept record to disk as the pair the playback screen and the
// editor's quick verify read: <stem>.xreps (the same JSON the core's
// saveInspectionSample writes -- reports = the frame's objects, defInfo,
// camera_param = the frame's cam_param, time_ms) and <stem>.jpg (the frame,
// bytes as kept). Both go through SV with a binary payload, the same path
// that saves defs. The stem is <def>-<record time>, so a record saved twice
// lands on the same files.
function stampOf(ms) {
  const d = new Date(ms || Date.now());
  const p2 = (n) => String(n).padStart(2, '0');
  return d.getFullYear() + p2(d.getMonth() + 1) + p2(d.getDate()) + '-' + p2(d.getHours()) + '-' + p2(d.getMinutes()) + '-' + p2(d.getSeconds()) + '_' + String(d.getMilliseconds()).padStart(3, '0');
}
function saveRecordAsXreps(rec, sendBPG, dir, defName) {
  const frames = (rec.report && rec.report.reports) || [];
  const frame = frames[0] || {};
  const body = {
    reports: frame.reports || [],
    defInfo: rec.def,
    camera_param: frame.cam_param,
    time_ms: rec.ts_ms || Date.now(),
  };
  const stem = (dir || 'data').replace(/\/+$/, '') + '/' + (defName || 'sample') + '-' + stampOf(rec.ts_ms);
  const put = (filename, bytes) => new Promise((resolve, reject) =>
    sendBPG('SV', 0, { filename, make_dir: true }, bytes, {
      resolve: (pkts) => { const SS = (pkts || []).find((x) => x.type === 'SS'); (SS && SS.data && SS.data.ACK) ? resolve() : reject(new Error('write refused: ' + filename)); },
      reject: (e) => reject(e instanceof Error ? e : new Error(String(e))),
    }));
  const bin = atob(rec.jpg_b64 || '');
  const img = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) img[i] = bin.charCodeAt(i);
  return put(stem + '.xreps', new TextEncoder().encode(JSON.stringify(body)))
    .then(() => (img.length ? put(stem + '.jpg', img) : undefined))
    .then(() => stem);
}

// The rows that made the verdict what it is, worst first.
function judgeRows(obj) {
  const js = (obj && obj.judgeReports) || [];
  const rank = (s) => (s === INSPECTION_STATUS.NA || s === undefined) ? 0
                    : (s === INSPECTION_STATUS.FAILURE) ? 1 : 2;
  return js.slice().sort((a, b) => rank(a.status) - rank(b.status));
}

function Detail({ rec, sendBPG, saveDir, defName }) {
  // Hooks first, unconditionally (rules of hooks); the early returns follow.
  const recId = rec && rec.id;
  const [saving, setSaving] = useState(false);
  const [saved, setSaved] = useState(undefined);
  useEffect(() => { setSaved(undefined); }, [recId]);
  const image = useMemo(() => imageFromRecord(rec), [recId]);
  // A FRESH def copy per record: RepDisplay's rootDefInfoLoading deletes
  // featureSet_sha1 off whatever it is handed.
  const defCopy = useMemo(() => { try { return rec && rec.def ? JSON.parse(JSON.stringify(rec.def)) : undefined; } catch (e) { return undefined; } }, [recId]);
  if (!rec) return <div style={{ color: '#888', padding: 20 }}>左邊選一筆</div>;
  if (rec.error) return <div style={{ color: COLOUR.NG, padding: 20 }}>{rec.error}</div>;
  // The record carries the whole frame envelope: report.reports[0] is the
  // frame (type, cam_param, ...) and ITS .reports are the objects. RepDisplay
  // wants the objects, the same level the playback screen hands it from an
  // .xreps file -- passing the frame list drew every overlay at the wrong
  // place (measured 2026-09-07: the frame's cx/cy read as an object's).
  const frames = (rec.report && rec.report.reports) || [];
  const camParam = frames[0] && frames[0].cam_param;
  const objs = (frames[0] && frames[0].reports) || [];
  return <div>
    <div style={{ display: 'flex', gap: 10, alignItems: 'baseline', marginBottom: 8 }}>
      <Tag color={COLOUR[rec.group_verdict] || '#555'}>{rec.group}</Tag>
      <span style={{ fontVariantNumeric: 'tabular-nums' }}>{hhmmss(rec.ts_ms)}</span>
      <span style={{ fontSize: 12, color: '#888' }}>{rec.w}x{rec.h} 全解析度 · {objs.length} 顆</span>
      <span style={{ flex: 1 }} />
      <Button size="small" icon={<SaveOutlined />} loading={saving} onClick={() => {
        setSaving(true);
        saveRecordAsXreps(rec, sendBPG, saveDir, defName)
          .then((stem) => setSaved({ ok: true, text: '已存 ' + stem + '.xreps / .jpg' }))
          .catch((e) => setSaved({ ok: false, text: '存檔失敗:' + (e && e.message || e) }))
          .finally(() => setSaving(false));
      }}>存成 xreps</Button>
    </div>
    {saved ? <div style={{ fontSize: 12, color: saved.ok ? '#389e0d' : COLOUR.NG, marginBottom: 6 }}>{saved.text}</div> : null}
    {/* The overlay, from the same component the playback screen uses: the def,
        the camera param and the report draw search points, fitted lines and
        circles and caliper hits over the frame the measurement was taken from. */}
    <div style={{ height: 360, background: '#111' }}>
      <RepDisplay def={defCopy} camera_param={camParam} reports={objs} image={image}
        IGNORE_IMAGE_FIT_TO_SCREEN />
    </div>
    <div style={{ marginTop: 10, maxHeight: 220, overflow: 'auto' }}>
      {objs.map((o, oi) => <table key={oi} style={{ width: '100%', fontSize: 12, borderCollapse: 'collapse', marginBottom: 6 }}>
        <tbody>
          <tr><td colSpan={3} style={{ color: '#666', padding: '2px 6px' }}>物件 {oi + 1}{o.trust && o.trust.code ? <span style={{ color: COLOUR.NA }}>  · trust {o.trust.code}</span> : null}</td></tr>
          {judgeRows(o).map((j, i) => {
            const bad = j.status === INSPECTION_STATUS.FAILURE;
            const na = j.status === INSPECTION_STATUS.NA || j.status === undefined;
            return <tr key={i} style={{ borderTop: '1px solid #e5e5e5' }}>
              <td style={{ padding: '3px 6px', color: '#555' }}>[{j.id}] {j.name}</td>
              <td style={{ padding: '3px 6px', textAlign: 'right', fontVariantNumeric: 'tabular-nums',
                           color: bad ? COLOUR.NG : na ? COLOUR.NA : undefined }}>{num(j.value)}</td>
              <td style={{ padding: '3px 6px', fontSize: 11, color: '#888' }}>{bad ? 'NG' : na ? 'NA' : 'OK'}</td>
            </tr>;
          })}
        </tbody>
      </table>)}
    </div>
  </div>;
}

const WANT_LABEL = { OK: 'OK', NG: 'NG', NA: 'NA', '*': '不限' };
const wantOptions = SAMPLE_WANTS.map((w) => ({ value: w, label: WANT_LABEL[w] }));

// Group editor. Edits a local copy; 套用 writes localStorage (per def name)
// AND pushes the list to the core, which restarts the buffer -- so say so.
function GroupEditor({ defName, measures, initial, sendBPG, onApplied, onCancel }) {
  const [groups, setGroups] = useState(() => JSON.parse(JSON.stringify(initial || [])));
  const upd = (i, patch) => setGroups(groups.map((g, k) => (k === i ? { ...g, ...patch } : g)));
  const setMeasure = (i, id, want) => {
    const m = { ...(groups[i].measures || {}) };
    if (want === '*' || want === undefined) delete m[id]; else m[id] = want;
    upd(i, { measures: m });
  };
  const add = () => setGroups([...groups, { name: `群組${groups.length + 1}`, cap: 10, rotate: false, verdict: 'NG', measures: {} }]);
  const remove = (i) => setGroups(groups.filter((_, k) => k !== i));
  const measureName = (id) => { const m = (measures || []).find((x) => String(x.id) === String(id)); return m ? `[${m.id}] ${m.name || ''}` : `[${id}]`; };
  const usedIds = (g) => Object.keys(g.measures || {});
  const freeMeasures = (g) => (measures || []).filter((m) => !usedIds(g).includes(String(m.id)));

  return <div>
    <div style={{ fontSize: 12, color: '#888', marginBottom: 8 }}>
      套用到 <b>{defName || '(未載入 def)'}</b>;設定存在這台瀏覽器,依 def 檔名記住。條件由上而下,先符合的群組收走該幀;沒有符合的不保留。
    </div>
    {groups.map((g, i) => <div key={i} style={{ border: '1px solid #ddd', borderRadius: 4, padding: 8, marginBottom: 8 }}>
      <div style={{ display: 'flex', gap: 8, alignItems: 'center', flexWrap: 'wrap' }}>
        <Input size="small" style={{ width: 140 }} value={g.name} onChange={(e) => upd(i, { name: e.target.value })} placeholder="名稱" />
        <span style={{ fontSize: 12 }}>張數</span>
        <InputNumber size="small" min={1} max={200} value={g.cap} onChange={(v) => upd(i, { cap: v })} style={{ width: 70 }} />
        <span style={{ fontSize: 12 }}>幀判定</span>
        <Select size="small" style={{ width: 80 }} value={g.verdict} options={wantOptions} onChange={(v) => upd(i, { verdict: v })} />
        <Switch size="small" checked={g.rotate === true} onChange={(c) => upd(i, { rotate: c })} />
        <span style={{ fontSize: 12 }}>{g.rotate ? '滿了換掉最舊的' : '滿了就停'}</span>
        <span style={{ flex: 1 }} />
        <Button size="small" type="text" icon={<CloseOutlined />} onClick={() => remove(i)} />
      </div>
      <div style={{ marginTop: 6, display: 'flex', gap: 6, alignItems: 'center', flexWrap: 'wrap' }}>
        <span style={{ fontSize: 12, color: '#888' }}>量測條件:</span>
        {usedIds(g).length === 0 ? <span style={{ fontSize: 12, color: '#999' }}>(無,只看幀判定)</span> : null}
        {usedIds(g).map((id) => <span key={id} style={{ fontSize: 12, display: 'inline-flex', alignItems: 'center', gap: 4, background: '#f0f0f0', padding: '2px 6px', borderRadius: 3 }}>
          {measureName(id)}
          <Select size="small" style={{ width: 70 }} value={g.measures[id]} options={wantOptions.filter((o) => o.value !== '*')} onChange={(v) => setMeasure(i, id, v)} />
          <CloseOutlined style={{ cursor: 'pointer', color: '#888' }} onClick={() => setMeasure(i, id, '*')} />
        </span>)}
        {freeMeasures(g).length > 0
          ? <Select size="small" style={{ width: 170 }} placeholder="＋ 加量測條件" value={undefined}
              options={freeMeasures(g).map((m) => ({ value: String(m.id), label: measureName(m.id) }))}
              onChange={(id) => setMeasure(i, id, 'NG')} />
          : null}
      </div>
    </div>)}
    <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
      <Button size="small" icon={<PlusOutlined />} onClick={add}>新增群組</Button>
      <span style={{ flex: 1 }} />
      <Button size="small" onClick={onCancel}>取消</Button>
      <Button size="small" type="primary" disabled={!defName} onClick={() => {
        saveSampleGroups(defName, groups);
        pushSampleGroups(sendBPG, groups).then(() => onApplied(groups));
      }}>套用(核心緩衝會重新開始)</Button>
    </div>
  </div>;
}

export default function InspSamplePanel({ visible, onClose, sendBPG, defName, measures, saveDir }) {
  const [editing, setEditing] = useState(false);
  const [list, setList] = useState(undefined);
  const [sel, setSel] = useState(undefined);
  const [rec, setRec] = useState(undefined);
  const [busy, setBusy] = useState(false);

  const ask = (tag, body) => new Promise((resolve, reject) =>
    sendBPG(tag, 0, body, undefined, { resolve: (pkts) => {
      const p = (pkts || []).find((x) => x.type === tag);
      p ? resolve(p.data) : reject(new Error('no ' + tag + ' reply'));
    }, reject }));

  const refresh = () => { setBusy(true); ask('SL', {}).then(setList).catch((e) => log.warn('[samples] SL', e)).finally(() => setBusy(false)); };
  useEffect(() => { if (visible) { refresh(); setSel(undefined); setRec(undefined); setEditing(false); } }, [visible]);

  const open = (item, group) => {
    setSel(item.id); setRec(undefined);
    ask('SG', { id: item.id })
      .then((d) => setRec({ ...d, group_verdict: item.verdict }))
      .catch((e) => { log.warn('[samples] SG', e); setRec({ id: item.id, error: '讀不到這筆(緩衝可能已清空)' }); });
  };
  const clear = (name) => {
    sendBPG('ST', 0, { INSP_SAMPLE_CLEAR: name === undefined ? true : name }, undefined,
      { resolve: () => { setSel(undefined); setRec(undefined); refresh(); }, reject: () => refresh() });
  };

  const groups = (list && list.groups) || [];
  const total = groups.reduce((n, g) => n + (g.count || 0), 0);
  const bytes = groups.reduce((n, g) => n + (g.items || []).reduce((m, i) => m + (i.jpg_bytes || 0), 0), 0);

  return <Modal open={visible} visible={visible} onCancel={onClose} footer={null}
    width={1040} title="檢驗樣本(核心保留中)" destroyOnClose>
    <div style={{ display: 'flex', gap: 10, alignItems: 'center', marginBottom: 8 }}>
      <Button size="small" icon={<ReloadOutlined />} loading={busy} onClick={refresh}>重新整理</Button>
      <span style={{ fontSize: 12, color: '#888' }}>
        共 {total} 筆,{humanBytes(bytes)}
        {list && list.dropped_full > 0 ? `,已滿略過 ${list.dropped_full} 幀` : ''}
      </span>
      <Button size="small" icon={<DeleteOutlined />} onClick={() => clear()}>全部清空</Button>
      <Button size="small" icon={<SettingOutlined />} type={editing ? 'primary' : 'default'} onClick={() => setEditing(!editing)}>設定群組</Button>
      <span style={{ flex: 1 }} />
      <span style={{ fontSize: 11, color: '#666' }}>{defName ? `依 def「${defName}」記住` : ''}</span>
    </div>
    {editing
      ? <GroupEditor defName={defName} measures={measures} initial={loadSampleGroups(defName)} sendBPG={sendBPG}
          onCancel={() => setEditing(false)}
          onApplied={() => { setEditing(false); setSel(undefined); setRec(undefined); refresh(); }} />
    : groups.length === 0
      ? <div style={{ color: '#888', padding: 20 }}>{list ? '這個 def 沒有設定樣本群組,核心沒有在保留。按「設定群組」開始。' : '讀取中…'}</div>
      : <div style={{ display: 'flex', gap: 12 }}>
        <div style={{ display: 'flex', gap: 8, width: 480 }}>
          {groups.map((g, gi) => (
            <div key={gi} style={{ flex: 1, minWidth: 0 }}>
              <div style={{ color: COLOUR[g.verdict] || '#444', fontWeight: 600, marginBottom: 2 }}>
                {g.name} <span style={{ color: '#888', fontWeight: 400 }}>{g.count}/{g.cap}{g.rotate ? ' ↻' : ''}</span>
                {g.count > 0 ? <Button type="text" size="small" style={{ float: 'right', fontSize: 11 }}
                  onClick={() => clear(g.name)}>清空</Button> : null}
              </div>
              {/* A full fill-and-stop group has STOPPED collecting -- say so. */}
              {!g.rotate && g.count >= g.cap
                ? <div style={{ fontSize: 11, color: COLOUR.NA, marginBottom: 4 }}>已滿,停止收集</div>
                : <div style={{ height: 17 }} />}
              <div style={{ maxHeight: 420, overflowY: 'auto' }}>
                {(g.items || []).length === 0
                  ? <div style={{ color: '#999', fontSize: 12, padding: 8 }}>—</div>
                  : g.items.slice().reverse().map((it) => (
                    <div key={it.id} onClick={() => open(it, g)}
                      style={{ cursor: 'pointer', padding: '4px 6px', marginBottom: 3, borderRadius: 4,
                               background: sel === it.id ? '#d6e4ff' : '#f0f0f0', fontSize: 12,
                               display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ fontVariantNumeric: 'tabular-nums' }}>{hhmmss(it.ts_ms)}</span>
                      <Tag color={COLOUR[it.verdict]} style={{ margin: 0 }}>{it.verdict}</Tag>
                    </div>))}
              </div>
            </div>))}
        </div>
        <div style={{ flex: 1, minWidth: 0 }}>
          {sel !== undefined && rec === undefined ? <div style={{ color: '#888', padding: 20 }}>讀取中…</div> : <Detail rec={rec} sendBPG={sendBPG} saveDir={saveDir} defName={defName} />}
        </div>
      </div>}
  </Modal>;
}
