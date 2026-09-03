// The last few parts, per verdict, with their pictures.
//
// Opened from the inspection screen while the machine keeps running. The
// question it answers is "what was that one?", asked three parts too late --
// so it is read newest-first and it never blocks the line.
//
// The buffer behind it (UTIL/inspSampleStore) is a SAMPLE: only parts whose
// verdict happened to arrive with a frame are in it, because the core throttles
// images and reports independently. This panel says so rather than presenting a
// gappy list as a complete one -- a count read off here would be wrong, and the
// wrongness would be invisible.
import React, { useState, useEffect, useMemo, useRef } from 'react';
import Modal from 'antd/lib/modal';
import Button from 'antd/lib/button';
import Tag from 'antd/lib/tag';
import InputNumber from 'antd/lib/input-number';
import { SaveOutlined, DeleteOutlined } from '@ant-design/icons';
import {
  SAMPLE_BUCKETS, sampleStoreSnapshot, subscribeSampleStore,
  clearSampleStore, clearSampleBucket, removeSampleEntry,
  sampleStoreCap, setSampleStoreCap,
} from 'UTIL/inspSampleStore';
import { INSPECTION_STATUS } from 'UTIL/InspectionStatus';
import { mkLog } from 'UTIL/logger';
const log = mkLog('ui.samplepanel');

const COLOUR = { OK: '#389e0d', NG: '#cf1322', NA: '#d48806' };
const LABEL = { OK: 'OK', NG: 'NG 不良', NA: 'NA 無判定' };

// ONE object URL per frame, revoked when this panel closes.
//
// A frame is shared by every part found in it, so the cache is keyed by the
// byte array itself -- four parts in one frame must not make four blobs of the
// same picture. Revoking matters more than usual here: a Blob's payload lives
// outside the JS heap, which is the whole reason the ring stores Uint8Array,
// and leaking URLs would put it back where it started.
function useFrameUrls() {
  const cache = useRef(new Map());
  useEffect(() => () => {
    cache.current.forEach((u) => { try { URL.revokeObjectURL(u); } catch (e) { /* closing */ } });
    cache.current.clear();
  }, []);
  return (img) => {
    if (!img || !img.jpegBytes) return undefined;
    const hit = cache.current.get(img.jpegBytes);
    if (hit) return hit;
    // format 1 = BGR JPEG, 2 = grayscale JPEG. Both are ordinary JPEG files.
    const url = URL.createObjectURL(new Blob([img.jpegBytes], { type: 'image/jpeg' }));
    cache.current.set(img.jpegBytes, url);
    return url;
  };
}

// Same precision on both sides of the comparison. A value at 4 dp beside a
// limit at whatever precision it happened to be typed with makes the reader do
// the alignment, and that alignment is the entire content of the row.
const num = (v) => (typeof v === 'number' && isFinite(v)) ? v.toFixed(4) : String(v);

const humanBytes = (n) => (n >= 1048576) ? (n / 1048576).toFixed(1) + ' MB'
                        : (n >= 1024) ? Math.round(n / 1024) + ' kB'
                        : n + ' B';

const hhmmss = (ms) => {
  try { const d = new Date(ms); return d.toTimeString().slice(0, 8) + '.'
    + String(d.getMilliseconds()).padStart(3, '0'); } catch (e) { return '?'; }
};

// The rows that made the verdict what it is, worst first -- that is the row
// someone opened this panel to read.
function judgeRows(entry) {
  const js = (entry && entry.judgeReports) || [];
  const rank = (s) => (s === INSPECTION_STATUS.NA || s === undefined) ? 0
                    : (s === INSPECTION_STATUS.FAILURE) ? 1 : 2;
  return js.slice().sort((a, b) => rank(a.status) - rank(b.status));
}

function Detail({ entry, url, onSave, saving, onRemove }) {
  if (!entry) return <div style={{ color: '#888', padding: 20 }}>左邊選一筆</div>;
  const img = entry.img || {};
  const ratio = (img.full_width > 0 && img.width > 0) ? (img.full_width / img.width) : 1;
  return <div>
    <div style={{ display: 'flex', gap: 10, alignItems: 'baseline', marginBottom: 8 }}>
      <Tag color={COLOUR[entry.verdict]}>{entry.verdict}</Tag>
      <span style={{ fontVariantNumeric: 'tabular-nums' }}>{hhmmss(entry.at)}</span>
      <span style={{ fontSize: 12, color: '#888' }}>
        {img.width}x{img.height}
        {ratio !== 1 ? `(全幅 ${img.full_width}x${img.full_height},降採樣 ${ratio.toFixed(2)}x)` : ''}
      </span>
      <Button size="small" icon={<SaveOutlined />} loading={saving}
        onClick={() => onSave(entry)}>存成 .xreps</Button>
      <Button size="small" danger icon={<DeleteOutlined />}
        onClick={() => onRemove(entry)}>刪除這筆</Button>
    </div>
    {url ? <img src={url} alt="" style={{ maxWidth: '100%', maxHeight: 340,
      background: '#111', display: 'block' }} /> : <div style={{ color: '#888' }}>無影像</div>}
    <div style={{ marginTop: 10, maxHeight: 200, overflow: 'auto' }}>
      <table style={{ width: '100%', fontSize: 12, borderCollapse: 'collapse' }}>
        <tbody>
          {judgeRows(entry).map((j, i) => {
            const bad = j.status === INSPECTION_STATUS.FAILURE;
            const na = j.status === INSPECTION_STATUS.NA || j.status === undefined;
            return <tr key={i} style={{ borderTop: '1px solid #333' }}>
              <td style={{ padding: '3px 6px', color: '#aaa' }}>{j.name}</td>
              <td style={{ padding: '3px 6px', textAlign: 'right',
                           fontVariantNumeric: 'tabular-nums',
                           color: bad ? COLOUR.NG : na ? COLOUR.NA : undefined }}>
                {num(j.value)}
              </td>
              <td style={{ padding: '3px 6px', fontSize: 11, color: '#888' }}>
                {j.lim ? `[${num(j.lim.LSL)}, ${num(j.lim.USL)}]` : ''}
              </td>
            </tr>;
          })}
        </tbody>
      </table>
    </div>
  </div>;
}

export function InspSamplePanel({ visible, onClose, sendBPG, savePath, defInfoFor }) {
  const [tick, setTick] = useState(0);
  const [sel, setSel] = useState(undefined);
  const [saving, setSaving] = useState(false);
  const [msg, setMsg] = useState('');
  const urlFor = useFrameUrls();

  useEffect(() => subscribeSampleStore(() => setTick((t) => t + 1)), []);
  const snap = useMemo(() => sampleStoreSnapshot(), [tick, visible]);
  const entry = sel !== undefined
    ? SAMPLE_BUCKETS.map((b) => snap[b].find((e) => e.id === sel))
        .find((e) => e !== undefined)
    : undefined;

  // SAVE WHAT IS ACTUALLY HERE, and label it as that.
  //
  // The image is the STREAMED frame, which is down-sampled -- so the camera
  // param written beside it is scaled to match. A file whose calibration
  // describes the full sensor while its picture is half that size measures
  // every length in it wrong by exactly that factor, and 載入 xrep would read
  // it back and do precisely that. The ratio is taken from full_width/width
  // rather than the header's `scale`, so it stays right whatever `scale` means.
  //
  // `reports` here is the judge rows, not a core snapshot's full report: the
  // ring deliberately drops the point clouds (97.6% of a report's bytes). The
  // note field says so inside the file, because a .xreps that looks like the
  // core's but is not would mislead whoever opens it next.
  const saveXrep = (e) => {
    if (!e || !sendBPG) return;
    setSaving(true); setMsg('');
    const stamp = new Date(e.at);
    const pad = (n, w = 2) => String(n).padStart(w, '0');
    const stem = (savePath || 'data') + '/'
      + 'sample_' + e.verdict + '_'
      + stamp.getFullYear() + pad(stamp.getMonth() + 1) + pad(stamp.getDate()) + '-'
      + pad(stamp.getHours()) + pad(stamp.getMinutes()) + pad(stamp.getSeconds()) + '_'
      + pad(stamp.getMilliseconds(), 3);

    const img = e.img || {};
    const ratio = (img.full_width > 0 && img.width > 0) ? (img.full_width / img.width) : 1;
    const cp = { ...(e.camParam || {}) };
    if (typeof cp.ppb2b === 'number' && ratio > 0) cp.ppb2b = cp.ppb2b / ratio;

    const body = {
      reports: [{ judgeReports: e.judgeReports, cx: e.cx, cy: e.cy,
                  rotate: e.rotate, isFlipped: e.isFlipped, time_ms: e.time_ms }],
      defInfo: (typeof defInfoFor === 'function') ? defInfoFor() : undefined,
      camera_param: cp,
      time_ms: e.time_ms,
      note: 'saved from the WebUI sample ring: the image is the STREAMED frame '
          + '(down-sampled ' + ratio.toFixed(3) + 'x, camera_param scaled to match) '
          + 'and reports carry judge rows only, not the core snapshot geometry.',
    };

    // Image first. A .xreps whose picture failed to write is the case playback
    // handles worst, and writing it second means a failure leaves a report with
    // no image rather than nothing at all.
    const enc = new TextEncoder();
    sendBPG('SV', 0, { filename: stem + '.jpg', make_dir: true }, e.img.jpegBytes, {
      resolve: (pkts) => {
        const SS = (pkts || []).find((p) => p.type === 'SS');
        if (!SS || SS.data.ACK !== true) {
          setSaving(false); setMsg('影像寫入失敗:' + stem + '.jpg'); return;
        }
        sendBPG('SV', 0, { filename: stem + '.xreps' },
          enc.encode(JSON.stringify(body)), {
          resolve: (pkts2) => {
            const SS2 = (pkts2 || []).find((p) => p.type === 'SS');
            setSaving(false);
            setMsg(SS2 && SS2.data.ACK === true ? ('已存 ' + stem + '.xreps')
                                                : ('報告寫入失敗:' + stem + '.xreps'));
          },
          reject: (err) => { setSaving(false); setMsg('報告寫入失敗'); log.warn(err); },
        });
      },
      reject: (err) => { setSaving(false); setMsg('影像寫入失敗'); log.warn(err); },
    });
  };

  const total = SAMPLE_BUCKETS.reduce((n, b) => n + snap[b].length, 0);

  return <Modal open={visible} visible={visible} onCancel={onClose} footer={null}
    width={1000} title="檢驗樣本(保留中,填滿即停止)" destroyOnClose>
    <div style={{ display: 'flex', gap: 10, alignItems: 'center', marginBottom: 8 }}>
      <span style={{ fontSize: 12, color: '#888' }}>每類保留</span>
      <InputNumber size="small" min={1} max={500} value={sampleStoreCap()}
        onChange={(v) => { setSampleStoreCap(v); setTick((t) => t + 1); }} style={{ width: 80 }} />
      <span style={{ fontSize: 12, color: '#888' }}>
        共 {total} 筆,{humanBytes(snap.bytes)}
      </span>
      <Button size="small" icon={<DeleteOutlined />}
        onClick={() => { clearSampleStore(); setSel(undefined); }}>全部清空</Button>
      {msg ? <span style={{ fontSize: 12, color: msg.indexOf('失敗') >= 0 ? COLOUR.NG : COLOUR.OK }}>{msg}</span> : null}
    </div>

    <div style={{ display: 'flex', gap: 12 }}>
      <div style={{ display: 'flex', gap: 8, width: 470 }}>
        {SAMPLE_BUCKETS.map((b) => (
          <div key={b} style={{ flex: 1, minWidth: 0 }}>
            <div style={{ color: COLOUR[b], fontWeight: 600, marginBottom: 2 }}>
              {LABEL[b]} <span style={{ color: '#888', fontWeight: 400 }}>
                {snap[b].length}/{snap.cap}</span>
              {snap[b].length > 0
                ? <Button type="text" size="small" style={{ float: 'right', fontSize: 11 }}
                    onClick={() => { clearSampleBucket(b); setSel(undefined); }}>清空</Button>
                : null}
            </div>
            {/* A full bucket has STOPPED collecting, and that is invisible
                otherwise -- 20 of 20 looks the same as a machine that has not
                produced another one. The turned-away count is what separates
                them. */}
            {snap.full[b]
              ? <div style={{ fontSize: 11, color: COLOUR.NA, marginBottom: 4 }}>
                  已滿,停止收集{snap.skipped[b] > 0 ? `(已略過 ${snap.skipped[b]} 筆)` : ''}
                </div>
              : <div style={{ height: 17 }} />}
            <div style={{ maxHeight: 400, overflowY: 'auto' }}>
              {snap[b].length === 0
                ? <div style={{ color: '#555', fontSize: 12, padding: 8 }}>—</div>
                : snap[b].map((e, i) => (
                  <div key={e.id} onClick={() => setSel(e.id)}
                    style={{ cursor: 'pointer', marginBottom: 4, padding: 2, position: 'relative',
                             border: '2px solid ' + (sel === e.id ? COLOUR[b] : 'transparent') }}>
                    <img src={urlFor(e.img)} alt="" style={{ width: '100%', display: 'block',
                      background: '#111' }} />
                    <div style={{ fontSize: 10, color: '#888', display: 'flex',
                                  justifyContent: 'space-between',
                                  fontVariantNumeric: 'tabular-nums' }}>
                      <span>{i + 1}. {hhmmss(e.at)}</span>
                      <a onClick={(ev) => { ev.stopPropagation();
                            removeSampleEntry(e.id);
                            if (sel === e.id) setSel(undefined); }}
                         style={{ color: COLOUR.NG }}>刪</a>
                    </div>
                  </div>))}
            </div>
          </div>))}
      </div>
      <div style={{ flex: 1, minWidth: 0, borderLeft: '1px solid #333', paddingLeft: 12 }}>
        <Detail entry={entry} url={entry ? urlFor(entry.img) : undefined}
          onSave={saveXrep} saving={saving}
          onRemove={(e) => { removeSampleEntry(e.id); setSel(undefined); }} />
      </div>
    </div>

    <div style={{ fontSize: 12, color: '#888', marginTop: 10, lineHeight: 1.8 }}>
      <b>填滿即停止</b>:每一類收滿就不再收,新的會被略過而<b>不是</b>擠掉舊的 ——
      不然以這台的速度,你看到的那一筆在你開這個視窗之前就被後面的零件推掉了。
      要繼續收就先刪掉幾筆或清空。<br/>
      這是<b>抽樣</b>,不是完整記錄:核心的影像和報告是分開節流的(影像上限 6 fps,報告不限),
      只有剛好配到影格的判定會留在這裡,所以這裡的數量不能拿來當產出統計。
      目前只在<b>全檢(FI)</b>模式收樣本 —— 檢驗(CI)的判定要等物件離開追蹤視窗才結算,
      那時畫面上的影格已經不是它了,配上去的圖會是錯的。
    </div>
  </Modal>;
}

export default InspSamplePanel;
