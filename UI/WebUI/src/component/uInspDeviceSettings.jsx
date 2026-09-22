// 裝置設定 -- every setting the uInspESP32 board accepts, in one modal.
//
// The main panel keeps the things an operator touches during a run (speed,
// gate zero, camera-rate mode, stop policy switch). Everything else the
// firmware's set_setup accepts (K_PLATE / K_GATE / K_CAM / K_SKIP in
// LegacyFirmware.cpp) lives here, grouped as the board groups them, so a
// plate that is not 240 mm or a sensor that needs a different debounce no
// longer needs a JSON edit -- and a key the firmware does not know is refused
// by the board, which this shows instead of "ack: true" doing nothing.
//
// Table-driven: FIELDS below is the whole form. Each row shows the board's
// current value as the placeholder; only fields the operator typed into are
// sent, as one set_setup per 套用. `apply(patch)` is the panel's
// machineSetupUpdate wrapper and returns a promise.
import React, { useState, useEffect } from 'react';
import Modal from 'antd/lib/modal';
import Button from 'antd/lib/button';
import Input from 'antd/lib/input';
import Select from 'antd/lib/select';
import Tooltip from 'antd/lib/tooltip';

const Why = ({ children }) => (
  <Tooltip title={<div style={{ maxWidth: 320 }}>{children}</div>}>
    <span style={{ cursor: 'help', color: '#888', border: '1px solid #bbb',
      borderRadius: '50%', fontSize: 10, lineHeight: '14px', width: 15, height: 15,
      display: 'inline-block', textAlign: 'center', marginLeft: 6 }}>?</span>
  </Tooltip>
);

// type: 'num' (integer unless step given), 'select' (options), 'bool'
// (select of 開/關). unit and tickMm (show the tick value in mm) are display.
const FIELDS = [
  { title: '盤面', why: '盤面直徑和一圈脈波數決定「1 tick = 幾 mm」,所有工位 offset、最小間距、速度換算都靠這兩個數。改了之後以 mm 顯示的數字會跟著變,offset 本身(tick)不會動。',
    items: [
      { key: 'plate_diameter_mm', label: '盤面直徑', unit: 'mm', step: 0.1, why: '量玻璃盤外徑。' },
      { key: 'pulses_per_rev', label: '一圈脈波數', unit: '脈波', why: '用 jog 走整圈數回來;韌體預設 70400 是量 40 圈 2816001 tick 得到的。' },
      { key: 'plate_accel', label: '加速度', unit: '', why: '起停的加減速斜率。太大會失步,太小起停慢。' },
      { key: 'speed_band_pct', label: '轉速容許帶', unit: '%', why: '實際轉速與設定差在這個百分比內才算「穩定」,才會進檢測。' },
      { key: 'stepper_en_active', label: 'Enable 極性', type: 'select', options: [[0, '低電位致能'], [1, '高電位致能']], why: '換驅動器才需要。' },
      { key: 'stepper_dir', label: '方向', type: 'select', options: [[0, '正'], [1, '反']], why: '盤面轉向反了就切這個。' },
    ] },
  { title: '閘門', why: '光纖閘門怎麼認一顆料。三層過濾:寬度、最小間距、最小時間間隔;通過的才給 tid、觸發相機。',
    items: [
      { key: 'pulse_min_width', label: '寬度下限', unit: 'tick', tickMm: true, why: '遮光脈衝比這短的當碎屑或抖動丟掉。0 = 不限。統計列的「擋下·太短」。' },
      { key: 'pulse_max_width', label: '寬度上限', unit: 'tick', tickMm: true, why: '比這長的當兩顆黏著或大異物丟掉。0 = 不限。統計列的「擋下·太長」。' },
      { key: 'min_detect_dist_um', label: '最小中心距', unit: 'µm', why: '和前一顆中心距小於這個值就丟掉(預設 2000)。零件規格 3 mm 一顆時 3500 會誤擋。' },
      { key: 'min_detect_sep_us', label: '最小時間間隔', unit: 'µs', why: '相機能給的最快速率;面板上的「顆/秒」就是它的倒數。' },
      { key: 'gate_debounce_rise', label: '去抖·上升', unit: 'tick', why: '光纖訊號要穩定這麼久才算遮住。' },
      { key: 'gate_debounce_fall', label: '去抖·下降', unit: 'tick', why: '要穩定這麼久才算離開。' },
      { key: 'gate_ref', label: '閘門零點', type: 'select', options: [['center', '中心'], ['trailing', '後緣']], why: '和面板上的閘門零點是同一個設定。切換後所有工位 offset 移約半顆料。' },
      { key: 'gate_cam_mode', label: '相機速率模式', type: 'select', options: [['manual', '手動'], ['auto', '自動']] },
      { key: 'gate_cam_margin_pct', label: '相機速率餘裕', unit: '%', why: '自動模式下,相機上限打幾折當閘門速率。' },
      { key: 'gate_cam_stale_ms', label: '相機回報過期', unit: 'ms', why: '相機速率超過這麼久沒更新,自動模式視為未知。' },
      { key: 'gate_proc_mode', label: '主機節流模式', type: 'select', options: [['off', '關'], ['fixed', '固定'], ['auto', '自動']] },
      { key: 'gate_proc_rate_hz', label: '主機節流速率', unit: 'Hz' },
      { key: 'gate_proc_sep_us', label: '主機節流間隔', unit: 'µs' },
      { key: 'gate_proc_iir_shift', label: '節流濾波 shift', unit: '', why: 'IIR 平滑強度,越大越慢。' },
      { key: 'gate_proc_capacity_pct', label: '主機容量比', unit: '%' },
      { key: 'gate_proc_auto', label: '節流自動', type: 'bool' },
      { key: 'gate_proc_auto_max_us', label: '自動節流上限', unit: 'µs' },
      { key: 'gate_proc_auto_rho_pct', label: '自動節流 ρ', unit: '%' },
    ] },
  { title: '相機配對', why: '影格和物件靠時間戳配對;配錯料時先看這裡。',
    items: [
      { key: 'report_match_ts', label: '時間戳配對', type: 'bool', why: '關掉就退回舊的順序配對,只在除錯時關。' },
      { key: 'cam_match_window_us', label: '配對時間窗', unit: 'µs', why: '影格時間和物件觸發時間差在這之內才配得上;窗夾在間距的一半。' },
      { key: 'cam_match_tolerance_mm', label: '配對容差', unit: 'mm', step: 0.01, why: '以距離表示的同一件事,韌體會換算成時間。' },
      { key: 'cam_recal_idle_ms', label: '時鐘重校間隔', unit: 'ms', why: '閒置多久後再打一次校正脈衝對時鐘。' },
      { key: 'cal_pulse_us', label: '校正脈衝寬', unit: 'µs' },
      { key: 'cam_drift_comp', label: '漂移補償', type: 'bool', why: '相機和板子時鐘的漂移要不要持續補償。' },
    ] },
  { title: '停機政策', why: '什麼情況停盤。只留給「可能配到錯的物件」這種追蹤問題。',
    items: [
      { key: 'skip_policy_mode', label: '模式', type: 'select', options: [['stop_only', '只停機'], ['none', '不停']] },
      { key: 'unanswered_stop_after', label: '連續無判決停機', unit: '顆', why: '連續這麼多顆到分選點都沒有判決就停。' },
      { key: 'nomatch_stop_after', label: '連續配不到停機', unit: '顆', why: '連續這麼多顆配不到影格就停。' },
    ] },
];

const fmtCur = (v) => (v === undefined || v === null) ? '—' : (typeof v === 'boolean' ? (v ? '開' : '關') : String(v));

export function DeviceSettingsModal({ open, onClose, cfg, apply, busy, mmPerPulse }) {
  const [draft, setDraft] = useState({});
  const [msg, setMsg] = useState(undefined);
  useEffect(() => { if (open) { setDraft({}); setMsg(undefined); } }, [open]);
  const c = cfg || {};
  const dirtyKeys = Object.keys(draft).filter((k) => draft[k] !== undefined && draft[k] !== '');

  const coerce = (it, v) => {
    if (it.type === 'bool') return v === 'true';
    if (it.type === 'select') return (typeof it.options[0][0] === 'number') ? Number(v) : v;
    const n = Number(v);
    if (!Number.isFinite(n)) return undefined;
    return it.step ? n : Math.round(n);
  };

  const submit = () => {
    const patch = {};
    for (const sec of FIELDS) for (const it of sec.items) {
      if (!dirtyKeys.includes(it.key)) continue;
      const v = coerce(it, draft[it.key]);
      if (v === undefined) { setMsg({ ok: false, text: `「${it.label}」不是數字` }); return; }
      patch[it.key] = v;
    }
    if (!Object.keys(patch).length) return;
    setMsg(undefined);
    Promise.resolve(apply(patch))
      .then(() => { setMsg({ ok: true, text: '已寫入 ' + Object.keys(patch).length + ' 項:' + Object.keys(patch).join(', ') }); setDraft({}); })
      .catch((e) => setMsg({ ok: false, text: '寫入失敗:' + ((e && e.message) || String(e)) }));
  };

  const row = (it) => {
    const cur = c[it.key];
    const tick = (it.tickMm && typeof cur === 'number' && mmPerPulse) ? ` = ${(cur * mmPerPulse).toFixed(2)} mm` : '';
    const changed = dirtyKeys.includes(it.key);
    let ctl;
    if (it.type === 'select' || it.type === 'bool') {
      const opts = it.type === 'bool' ? [['true', '開'], ['false', '關']] : it.options.map(([v, l]) => [String(v), l]);
      ctl = <Select size="small" style={{ width: 150 }} allowClear placeholder={fmtCur(cur)}
        value={draft[it.key] === undefined ? undefined : draft[it.key]}
        onChange={(v) => setDraft({ ...draft, [it.key]: v })}
        options={opts.map(([v, l]) => ({ value: v, label: l }))} />;
    } else {
      ctl = <Input size="small" style={{ width: 150 }} placeholder={fmtCur(cur)}
        value={draft[it.key] === undefined ? '' : draft[it.key]}
        onChange={(e) => setDraft({ ...draft, [it.key]: e.target.value })}
        addonAfter={it.unit || undefined} />;
    }
    return <tr key={it.key} style={{ background: changed ? '#fffbe6' : undefined }}>
      <td style={{ padding: '3px 6px', whiteSpace: 'nowrap' }}>{it.label}{it.why ? <Why>{it.why}</Why> : null}</td>
      <td style={{ padding: '3px 6px', color: '#888', whiteSpace: 'nowrap', fontVariantNumeric: 'tabular-nums' }}>
        {cur === undefined ? <span style={{ color: '#bbb' }}>板子未回報</span> : <>{fmtCur(cur)}{tick}</>}</td>
      <td style={{ padding: '3px 6px' }}>{ctl}</td>
    </tr>;
  };

  return <Modal open={open} visible={open} onCancel={onClose} width={760} title="裝置設定"
    footer={<div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
      <span style={{ flex: 1, textAlign: 'left', fontSize: 12, color: msg ? (msg.ok ? '#389e0d' : '#c33') : '#888' }}>
        {msg ? msg.text : (dirtyKeys.length ? `${dirtyKeys.length} 項待寫入` : '只有填了的欄位會寫入;留白 = 不動')}</span>
      <Button onClick={() => setDraft({})} disabled={!dirtyKeys.length}>清除輸入</Button>
      <Button type="primary" loading={busy} disabled={!dirtyKeys.length} onClick={submit}>套用</Button>
    </div>}>
    <div style={{ maxHeight: '70vh', overflowY: 'auto' }}>
      {mmPerPulse ? <div style={{ fontSize: 12, color: '#888', marginBottom: 6 }}>1 tick = {mmPerPulse.toFixed(4)} mm(依盤面直徑與一圈脈波數)</div> : null}
      {FIELDS.map((sec) => <div key={sec.title} style={{ marginBottom: 12 }}>
        <div style={{ fontWeight: 600, borderBottom: '1px solid #eee', padding: '4px 0', marginBottom: 2 }}>
          {sec.title}{sec.why ? <Why>{sec.why}</Why> : null}</div>
        <table style={{ width: '100%', fontSize: 12, borderCollapse: 'collapse' }}>
          <thead><tr style={{ color: '#999' }}><th style={{ textAlign: 'left', padding: '2px 6px', fontWeight: 400 }}>設定</th>
            <th style={{ textAlign: 'left', padding: '2px 6px', fontWeight: 400 }}>目前</th>
            <th style={{ textAlign: 'left', padding: '2px 6px', fontWeight: 400 }}>新值</th></tr></thead>
          <tbody>{sec.items.map(row)}</tbody>
        </table>
      </div>)}
    </div>
  </Modal>;
}

export default DeviceSettingsModal;
