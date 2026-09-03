// Kept samples, in the operator's own filter groups.
//
// Opened from the inspection screen while the machine keeps running. The
// question it answers is "what was that one?", asked three parts too late.
//
// The buffer behind it (UTIL/inspSampleStore) fills and STOPS, first match wins,
// and a sample matching no group is dropped -- so this panel has two halves that
// matter equally: the samples, and the groups that decided which ones exist. A
// group list is a question; showing the answers without the question is how
// somebody concludes the machine made no bad parts for an hour.
import React, { useState, useEffect, useMemo, useRef } from 'react';
import Modal from 'antd/lib/modal';
import Button from 'antd/lib/button';
import Tag from 'antd/lib/tag';
import Input from 'antd/lib/input';
import Select from 'antd/lib/select';
import InputNumber from 'antd/lib/input-number';
import Tooltip from 'antd/lib/tooltip';
import { SaveOutlined, DeleteOutlined, PlusOutlined,
         UpOutlined, DownOutlined } from '@ant-design/icons';
import {
  SAMPLE_CONDS, SAMPLE_PRESETS, SAMPLE_CAP_DEFAULT,
  sampleStoreSnapshot, subscribeSampleStore, sampleGroups, setSampleGroups,
  clearSampleStore, clearSampleGroup, removeSampleEntry, overallVerdict,
} from 'UTIL/inspSampleStore';
import { INSPECTION_STATUS } from 'UTIL/InspectionStatus';
import { RepDisplay } from '../RepDisplayUI.js';
import { mkLog } from 'UTIL/logger';
const log = mkLog('ui.samplepanel');

const COLOUR = { OK: '#389e0d', NG: '#cf1322', NA: '#d48806' };
const CONDLABEL = { OK: 'OK', NG: 'NG', NA: 'NA', '*': '不管' };

// Same precision on both sides of a comparison. A value at 4 dp beside a limit
// at whatever precision it was typed with makes the reader align the decimal
// points, and that alignment is the entire content of the row.
const num = (v) => (typeof v === 'number' && isFinite(v)) ? v.toFixed(4) : String(v);

const humanBytes = (n) => (n >= 1048576) ? (n / 1048576).toFixed(1) + ' MB'
                        : (n >= 1024) ? Math.round(n / 1024) + ' kB'
                        : n + ' B';

const hhmmss = (ms) => {
  try { const d = new Date(ms); return d.toTimeString().slice(0, 8) + '.'
    + String(d.getMilliseconds()).padStart(3, '0'); } catch (e) { return '?'; }
};

// ONE object URL per frame, revoked when this panel closes.
//
// A frame is shared by every part found in it, so the cache is keyed by the byte
// array itself -- four parts in one frame must not make four blobs of the same
// picture. Revoking matters more than usual: a Blob's payload lives outside the
// JS heap, which is the whole reason the store keeps Uint8Array, and leaking
// URLs would put it back where it started.
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

// The rows that made the verdict what it is, worst first -- that is the row
// someone opened this panel to read.
function judgeRows(entry) {
  const js = (entry && entry.judgeReports) || [];
  const rank = (s) => (s === INSPECTION_STATUS.FAILURE) ? 0
                    : (s === INSPECTION_STATUS.SUCCESS) ? 2 : 1;
  return js.slice().sort((a, b) => rank(a.status) - rank(b.status));
}

// A one-line rendering of what a group asks for, so a column header says what it
// is rather than only what someone called it.
function groupSummary(g, measures) {
  const bits = [];
  if (g.overall !== '*') bits.push('整體=' + g.overall);
  Object.keys(g.conds || {}).forEach((id) => {
    const m = (measures || []).find((x) => String(x.id) === id);
    bits.push((m ? m.name : '#' + id) + '=' + g.conds[id]);
  });
  return bits.length ? bits.join(' 且 ') : '全部收(無條件)';
}

function Detail({ entry, onSave, saving, onRemove, defCopy }) {
  if (!entry) return <div style={{ color: '#888', padding: 20 }}>左邊選一筆</div>;
  const img = entry.img || {};
  const ratio = (img.full_width > 0 && img.width > 0) ? (img.full_width / img.width) : 1;
  return <div>
    <div style={{ display: 'flex', gap: 10, alignItems: 'baseline', marginBottom: 8, flexWrap: 'wrap' }}>
      <Tag color={COLOUR[entry.verdict]}>{entry.verdict}</Tag>
      <span style={{ fontVariantNumeric: 'tabular-nums' }}>{hhmmss(entry.at)}</span>
      <span style={{ fontSize: 12, color: '#888' }}>
        {entry.groupName} · {img.width}x{img.height}
        {ratio !== 1 ? `(全幅 ${img.full_width}x${img.full_height},降採樣 ${ratio.toFixed(2)}x)` : ''}
      </span>
      <Button size="small" icon={<SaveOutlined />} loading={saving}
        onClick={() => onSave(entry)}>存成 .xreps</Button>
      <Button size="small" danger icon={<DeleteOutlined />}
        onClick={() => onRemove(entry)}>刪除這筆</Button>
    </div>
    {/* THE OVERLAY, not a flat picture.
        Same component the report-playback screen uses: given the def, the camera
        param and the report it draws the search points, the fitted lines and
        circles and the caliper hits over the frame. A picture plus a table says
        WHICH measurement failed; this says where on the part it went wrong,
        which is the question someone opened this panel with. */}
    <div style={{ height: 330, background: '#111' }}>
      <RepDisplay def={defCopy} camera_param={entry.camParam}
        reports={[entry.report]} image={entry.img} IGNORE_IMAGE_FIT_TO_SCREEN />
    </div>
    <div style={{ marginTop: 8, maxHeight: 170, overflow: 'auto' }}>
      <table style={{ width: '100%', fontSize: 12, borderCollapse: 'collapse' }}>
        <tbody>
          {judgeRows(entry).map((j, i) => {
            const bad = j.status === INSPECTION_STATUS.FAILURE;
            const na = j.status !== INSPECTION_STATUS.FAILURE
                    && j.status !== INSPECTION_STATUS.SUCCESS;
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

// ---- the group editor ------------------------------------------------------
//
// Order is a SETTING here, not presentation: first match wins, so a broad group
// above a narrow one silently starves it. Hence the arrows, and hence the note.
function GroupEditor({ measures, onDone }) {
  const [draft, setDraft] = useState(() => sampleGroups());
  const [preset, setPreset] = useState(SAMPLE_PRESETS[0].name);

  const patch = (i, k, v) => setDraft((d) => d.map((g, j) => (j === i ? { ...g, [k]: v } : g)));
  const setCond = (i, id, v) => setDraft((d) => d.map((g, j) => {
    if (j !== i) return g;
    const conds = { ...g.conds };
    if (v === '*') delete conds[String(id)]; else conds[String(id)] = v;
    return { ...g, conds };
  }));
  const move = (i, d) => setDraft((arr) => {
    const j = i + d;
    if (j < 0 || j >= arr.length) return arr;
    const c = arr.slice();
    const t = c[i]; c[i] = c[j]; c[j] = t;
    return c;
  });
  const add = () => {
    const p = SAMPLE_PRESETS.find((x) => x.name === preset) || SAMPLE_PRESETS[0];
    setDraft((d) => d.concat([{ name: p.name, cap: SAMPLE_CAP_DEFAULT,
                                overall: p.overall, conds: { ...p.conds } }]));
  };

  return <div>
    <div style={{ display: 'flex', gap: 8, alignItems: 'center', marginBottom: 10 }}>
      <Select size="small" value={preset} onChange={setPreset} style={{ width: 190 }}
        options={SAMPLE_PRESETS.map((p) => ({ value: p.name, label: p.name }))} />
      <Button size="small" icon={<PlusOutlined />} onClick={add}>加入</Button>
      <Button size="small" type="primary"
        onClick={() => { setSampleGroups(draft); onDone(); }}>儲存</Button>
      <Button size="small" onClick={onDone}>取消</Button>
      <span style={{ fontSize: 12, color: '#888' }}>
        由上往下比,<b>第一組命中就收</b> —— 把窄的條件放上面,不然它永遠收不到東西。
      </span>
    </div>

    {draft.length === 0
      ? <div style={{ color: '#888', padding: 16 }}>
          還沒有任何篩選組,所以什麼都不會收。上面挑一個預設方案按「加入」。
        </div>
      : draft.map((g, i) => (
        <div key={g.id || i} style={{ border: '1px solid #333', borderRadius: 4,
                                      padding: 8, marginBottom: 8 }}>
          <div style={{ display: 'flex', gap: 8, alignItems: 'center', marginBottom: 6 }}>
            <span style={{ color: '#888', width: 18 }}>{i + 1}</span>
            <Input size="small" style={{ width: 180 }} value={g.name}
              onChange={(e) => patch(i, 'name', e.target.value)} />
            <span style={{ fontSize: 12, color: '#888' }}>保留</span>
            <InputNumber size="small" min={1} max={500} value={g.cap}
              style={{ width: 70 }} onChange={(v) => patch(i, 'cap', v)} />
            <span style={{ fontSize: 12, color: '#888' }}>整體判定</span>
            <Select size="small" style={{ width: 90 }} value={g.overall}
              onChange={(v) => patch(i, 'overall', v)}
              options={SAMPLE_CONDS.map((c) => ({ value: c, label: CONDLABEL[c] }))} />
            <Button size="small" icon={<UpOutlined />} onClick={() => move(i, -1)} />
            <Button size="small" icon={<DownOutlined />} onClick={() => move(i, 1)} />
            <Button size="small" danger icon={<DeleteOutlined />}
              onClick={() => setDraft((d) => d.filter((_, j) => j !== i))} />
          </div>
          {(measures || []).length === 0
            ? <div style={{ fontSize: 12, color: '#888' }}>
                這份設定檔沒有量測項目可以設條件(只能用整體判定)。
              </div>
            : <div style={{ display: 'flex', flexWrap: 'wrap', gap: 6 }}>
                {measures.map((m) => (
                  <span key={m.id} style={{ display: 'inline-flex', alignItems: 'center',
                                            gap: 4, fontSize: 12 }}>
                    <span style={{ color: '#aaa', maxWidth: 150, overflow: 'hidden',
                                   textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}
                          title={m.name}>{m.name}</span>
                    <Select size="small" style={{ width: 78 }}
                      value={g.conds[String(m.id)] || '*'}
                      onChange={(v) => setCond(i, m.id, v)}
                      options={SAMPLE_CONDS.map((c) => ({ value: c, label: CONDLABEL[c] }))} />
                  </span>))}
              </div>}
        </div>))}
  </div>;
}

export function InspSamplePanel({ visible, onClose, sendBPG, savePath, defInfoFor, measures }) {
  const [tick, setTick] = useState(0);
  const [sel, setSel] = useState(undefined);
  const [saving, setSaving] = useState(false);
  const [msg, setMsg] = useState('');
  const [editing, setEditing] = useState(false);
  const urlFor = useFrameUrls();

  useEffect(() => subscribeSampleStore(() => setTick((t) => t + 1)), []);
  const snap = useMemo(() => sampleStoreSnapshot(), [tick, visible, editing]);

  const entry = useMemo(() => {
    if (sel === undefined) return undefined;
    for (const g of snap.groups) {
      const hit = g.items.find((e) => e.id === sel);
      if (hit) return hit;
    }
    return undefined;
  }, [sel, snap]);

  // A FRESH copy per selection. RepDisplay's rootDefInfoLoading deletes
  // featureSet_sha1 off the object it is given, and handing it the live def
  // would strip the digest the editor hard-blocks on.
  const defCopy = useMemo(() => {
    if (!entry || typeof defInfoFor !== 'function') return undefined;
    try { return JSON.parse(JSON.stringify(defInfoFor())); }
    catch (e) { log.warn('[samples] def for the overlay failed', e); return undefined; }
  }, [entry && entry.id]);

  // SAVE WHAT IS ACTUALLY HERE, and label it as that.
  //
  // The image is the STREAMED frame, which is down-sampled -- so the camera
  // param written beside it is scaled to match. A file whose calibration
  // describes the full sensor while its picture is a third that size measures
  // every length in it wrong by exactly that factor, and 載入 xrep would read it
  // back and do precisely that. The ratio comes from full_width/width rather
  // than the header's `scale`, so it stays right whatever `scale` means.
  const saveXrep = (e) => {
    if (!e || !sendBPG) return;
    setSaving(true); setMsg('');
    const stamp = new Date(e.at);
    const pad = (n, w = 2) => String(n).padStart(w, '0');
    const stem = (savePath || 'data') + '/sample_' + e.verdict + '_'
      + stamp.getFullYear() + pad(stamp.getMonth() + 1) + pad(stamp.getDate()) + '-'
      + pad(stamp.getHours()) + pad(stamp.getMinutes()) + pad(stamp.getSeconds()) + '_'
      + pad(stamp.getMilliseconds(), 3);

    const img = e.img || {};
    const ratio = (img.full_width > 0 && img.width > 0) ? (img.full_width / img.width) : 1;
    const cp = { ...(e.camParam || {}) };
    if (typeof cp.ppb2b === 'number' && ratio > 0) cp.ppb2b = cp.ppb2b / ratio;

    const body = {
      reports: [e.report],
      defInfo: (typeof defInfoFor === 'function') ? defInfoFor() : undefined,
      camera_param: cp,
      time_ms: e.time_ms,
      note: 'saved from the WebUI sample buffer (group "' + e.groupName + '"): the '
          + 'image is the STREAMED frame, down-sampled ' + ratio.toFixed(3)
          + 'x, with camera_param scaled to match.',
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

  const total = snap.groups.reduce((n, g) => n + g.items.length, 0);

  return <Modal open={visible} visible={visible} onCancel={onClose} footer={null}
    width={1240} title="檢驗樣本(依篩選組保留,填滿即停止)" destroyOnClose>
    <div style={{ display: 'flex', gap: 10, alignItems: 'center', marginBottom: 8, flexWrap: 'wrap' }}>
      <Button size="small" type={editing ? 'primary' : 'default'}
        onClick={() => setEditing((v) => !v)}>篩選設定</Button>
      <span style={{ fontSize: 12, color: '#888' }}>
        {snap.groups.length} 組,共 {total} 筆,{humanBytes(snap.bytes)}
      </span>
      <Button size="small" icon={<DeleteOutlined />}
        onClick={() => { clearSampleStore(); setSel(undefined); }}>全部清空</Button>
      {msg ? <span style={{ fontSize: 12,
        color: msg.indexOf('失敗') >= 0 ? COLOUR.NG : COLOUR.OK }}>{msg}</span> : null}
    </div>

    {editing
      ? <GroupEditor measures={measures} onDone={() => setEditing(false)} />
      : <>
        {snap.groups.length === 0
          ? <div style={{ color: '#888', padding: 20 }}>
              還沒有任何篩選組,所以什麼都不會收 —— 樣本是照你設定的條件挑的,沒有條件就沒有樣本。
              按上面的「篩選設定」加一組(可以直接用 NG/OK/NA 預設方案)。
            </div>
          : <div style={{ display: 'flex', gap: 12 }}>
            <div style={{ display: 'flex', gap: 8, width: 560, overflowX: 'auto' }}>
              {snap.groups.map((g, gi) => (
                <div key={g.id} style={{ width: 150, flex: '0 0 150px' }}>
                  <div style={{ fontWeight: 600, marginBottom: 2,
                                overflow: 'hidden', textOverflow: 'ellipsis',
                                whiteSpace: 'nowrap' }} title={g.name}>
                    <span style={{ color: '#888', fontWeight: 400 }}>{gi + 1}.</span> {g.name}
                    <span style={{ color: '#888', fontWeight: 400 }}> {g.items.length}/{g.cap}</span>
                  </div>
                  <Tooltip title={groupSummary(g, measures)}>
                    <div style={{ fontSize: 10, color: '#777', marginBottom: 2,
                                  overflow: 'hidden', textOverflow: 'ellipsis',
                                  whiteSpace: 'nowrap' }}>
                      {groupSummary(g, measures)}
                    </div>
                  </Tooltip>
                  {/* A full group has STOPPED collecting, and that is invisible
                      otherwise -- 20 of 20 looks the same as a machine that has
                      not produced another one. The turned-away count is what
                      separates them. */}
                  {g.full
                    ? <div style={{ fontSize: 11, color: COLOUR.NA }}>
                        已滿,停止收集{g.skipped > 0 ? `(略過 ${g.skipped})` : ''}
                      </div>
                    : <div style={{ height: 17 }} />}
                  {g.items.length > 0
                    ? <Button type="text" size="small" style={{ fontSize: 11, padding: 0 }}
                        onClick={() => { clearSampleGroup(g.id); setSel(undefined); }}>清空這組</Button>
                    : <div style={{ height: 22 }} />}
                  <div style={{ maxHeight: 380, overflowY: 'auto' }}>
                    {g.items.length === 0
                      ? <div style={{ color: '#555', fontSize: 12, padding: 6 }}>—</div>
                      : g.items.map((e, i) => (
                        <div key={e.id} onClick={() => setSel(e.id)}
                          style={{ cursor: 'pointer', marginBottom: 4, padding: 2,
                                   border: '2px solid ' + (sel === e.id
                                     ? (COLOUR[e.verdict] || '#1890ff') : 'transparent') }}>
                          <img src={urlFor(e.img)} alt="" style={{ width: '100%',
                            display: 'block', background: '#111' }} />
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
              <Detail entry={entry} defCopy={defCopy} onSave={saveXrep} saving={saving}
                onRemove={(e) => { removeSampleEntry(e.id); setSel(undefined); }} />
            </div>
          </div>}
      </>}

    <div style={{ fontSize: 12, color: '#888', marginTop: 10, lineHeight: 1.8 }}>
      <b>第一組命中就收</b>,一筆只會進一個組;<b>沒有符合任何一組的就丟掉</b>。
      <b>填滿即停止</b> —— 收滿的組會略過新的而不是擠掉舊的,不然以這台的速度,
      你看到的那一筆在你開這個視窗之前就被後面的零件推掉了。設定存在這台瀏覽器裡(localStorage)。<br/>
      這是<b>抽樣</b>,不是完整記錄:核心的影像和報告是分開節流的(影像上限 6 fps,報告不限),
      只有剛好配到影格的判定會留在這裡,所以這裡的數量不能拿來當產出統計。
      目前只在<b>全檢(FI)</b>模式收樣本 —— 檢驗(CI)的判定要等物件離開追蹤視窗才結算,
      那時畫面上的影格已經不是它了,配上去的圖會是錯的。
    </div>
  </Modal>;
}

export default InspSamplePanel;
