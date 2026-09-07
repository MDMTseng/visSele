// Kept inspection samples -- what the CORE is holding, drawn with the overlay.
//
// The first version of this panel buffered frames in the browser and was
// removed: the display stream is down-sampled and lossy, and the overlay did
// not land on it (docs/INSP_SAMPLE_BUFFER_2026-09-04.md). The buffer now lives
// in core memory, filled on the snapshot thread from the full-resolution frame
// and the report that came from it, so there is no pairing question and no
// scale mismatch. This panel is a viewer: SL lists, SG fetches one record.
//
// Groups are the question ("measure 10 NG but measure 3 OK"), configured in
// machine_setting.json (INSP_SAMPLE_GROUPS) or pushed live over ST; first
// match wins, fill-and-stop unless a group opted into rotating.
import React, { useState, useEffect, useMemo } from 'react';
import Modal from 'antd/lib/modal';
import Button from 'antd/lib/button';
import Tag from 'antd/lib/tag';
import { ReloadOutlined, DeleteOutlined } from '@ant-design/icons';
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

// The rows that made the verdict what it is, worst first.
function judgeRows(obj) {
  const js = (obj && obj.judgeReports) || [];
  const rank = (s) => (s === INSPECTION_STATUS.NA || s === undefined) ? 0
                    : (s === INSPECTION_STATUS.FAILURE) ? 1 : 2;
  return js.slice().sort((a, b) => rank(a.status) - rank(b.status));
}

function Detail({ rec }) {
  // Hooks first, unconditionally (rules of hooks); the early returns follow.
  const recId = rec && rec.id;
  const image = useMemo(() => imageFromRecord(rec), [recId]);
  // A FRESH def copy per record: RepDisplay's rootDefInfoLoading deletes
  // featureSet_sha1 off whatever it is handed.
  const defCopy = useMemo(() => { try { return rec && rec.def ? JSON.parse(JSON.stringify(rec.def)) : undefined; } catch (e) { return undefined; } }, [recId]);
  if (!rec) return <div style={{ color: '#888', padding: 20 }}>左邊選一筆</div>;
  if (rec.error) return <div style={{ color: COLOUR.NG, padding: 20 }}>{rec.error}</div>;
  const reports = (rec.report && rec.report.reports) || [];
  const camParam = reports[0] && reports[0].cam_param;
  const objs = (reports[0] && reports[0].reports) || [];
  return <div>
    <div style={{ display: 'flex', gap: 10, alignItems: 'baseline', marginBottom: 8 }}>
      <Tag color={COLOUR[rec.group_verdict] || '#555'}>{rec.group}</Tag>
      <span style={{ fontVariantNumeric: 'tabular-nums' }}>{hhmmss(rec.ts_ms)}</span>
      <span style={{ fontSize: 12, color: '#888' }}>{rec.w}x{rec.h} 全解析度 · {objs.length} 顆</span>
    </div>
    {/* The overlay, from the same component the playback screen uses: the def,
        the camera param and the report draw search points, fitted lines and
        circles and caliper hits over the frame the measurement was taken from. */}
    <div style={{ height: 360, background: '#111' }}>
      <RepDisplay def={defCopy} camera_param={camParam} reports={reports} image={image}
        IGNORE_IMAGE_FIT_TO_SCREEN />
    </div>
    <div style={{ marginTop: 10, maxHeight: 220, overflow: 'auto' }}>
      {objs.map((o, oi) => <table key={oi} style={{ width: '100%', fontSize: 12, borderCollapse: 'collapse', marginBottom: 6 }}>
        <tbody>
          <tr><td colSpan={3} style={{ color: '#aaa', padding: '2px 6px' }}>物件 {oi + 1}{o.trust && o.trust.code ? <span style={{ color: COLOUR.NA }}>  · trust {o.trust.code}</span> : null}</td></tr>
          {judgeRows(o).map((j, i) => {
            const bad = j.status === INSPECTION_STATUS.FAILURE;
            const na = j.status === INSPECTION_STATUS.NA || j.status === undefined;
            return <tr key={i} style={{ borderTop: '1px solid #333' }}>
              <td style={{ padding: '3px 6px', color: '#aaa' }}>[{j.id}] {j.name}</td>
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

export default function InspSamplePanel({ visible, onClose, sendBPG }) {
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
  useEffect(() => { if (visible) { refresh(); setSel(undefined); setRec(undefined); } }, [visible]);

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
      <span style={{ flex: 1 }} />
      <span style={{ fontSize: 11, color: '#666' }}>群組在 machine_setting.json 的 INSP_SAMPLE_GROUPS 設定;先符合的群組收走該幀</span>
    </div>
    {groups.length === 0
      ? <div style={{ color: '#888', padding: 20 }}>{list ? '核心沒有設定任何樣本群組(INSP_SAMPLE_GROUPS 為空),沒有在保留。' : '讀取中…'}</div>
      : <div style={{ display: 'flex', gap: 12 }}>
        <div style={{ display: 'flex', gap: 8, width: 480 }}>
          {groups.map((g, gi) => (
            <div key={gi} style={{ flex: 1, minWidth: 0 }}>
              <div style={{ color: COLOUR[g.verdict] || '#ccc', fontWeight: 600, marginBottom: 2 }}>
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
                  ? <div style={{ color: '#555', fontSize: 12, padding: 8 }}>—</div>
                  : g.items.slice().reverse().map((it) => (
                    <div key={it.id} onClick={() => open(it, g)}
                      style={{ cursor: 'pointer', padding: '4px 6px', marginBottom: 3, borderRadius: 4,
                               background: sel === it.id ? '#2a3f5f' : '#1b1b1b', fontSize: 12,
                               display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ fontVariantNumeric: 'tabular-nums' }}>{hhmmss(it.ts_ms)}</span>
                      <Tag color={COLOUR[it.verdict]} style={{ margin: 0 }}>{it.verdict}</Tag>
                    </div>))}
              </div>
            </div>))}
        </div>
        <div style={{ flex: 1, minWidth: 0 }}>
          {sel !== undefined && rec === undefined ? <div style={{ color: '#888', padding: 20 }}>讀取中…</div> : <Detail rec={rec} />}
        </div>
      </div>}
  </Modal>;
}
