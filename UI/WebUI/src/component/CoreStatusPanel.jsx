// The 運算核心 modal's body. Was a raw JSON dump of whatever the 1Hz status
// poll last returned; this makes the numbers readable and adds the handful of
// core knobs that had no UI at all.
//
// WHAT IS AND IS NOT CONFIGURABLE HERE, AND WHY:
//
// The core has no config file for runtime behaviour -- every knob is a
// session-lifetime variable set over ST, and NONE of them read back (GS
// reports counters and queues, never settings). So this panel can only show
// what THIS BROWSER last sent, and it says so rather than pretending to
// display core state. Two consequences worth knowing before trusting a value:
//
//   * CI/FI RESET SOME OF THEM. Starting an inspection sets saveInspFailSnap,
//     saveInspNASnap and SKIP_NA_DATA_VIEW back to false (wiringPanel, the
//     CI/FI branch). InspectionUI re-pushes the NG pair right after starting,
//     from 設定/machine_custom_setting -- which is why NG snapshot settings
//     are NOT duplicated here: two writers, one of them re-firing on every
//     inspection start, is how a setting becomes unexplainable.
//   * Entering the def editor pushes IMG_STREAMING_JPEG_QUALITY=85
//     unconditionally (DefConfUI). A quality set here does not survive a trip
//     through 量測設定.
//
// The fields grouped as 本次連線 keep their value until the core restarts; the
// ones marked 每次開始檢驗會重設 do not. That distinction is the whole reason
// this panel is laid out in two blocks.
import React, { useState } from 'react';
import Card from 'antd/lib/card';
import Button from 'antd/lib/button';
import InputNumber from 'antd/lib/input-number';
import Switch from 'antd/lib/switch';
import Tooltip from 'antd/lib/tooltip';
import Progress from 'antd/lib/progress';
import { GetObjElement } from 'UTIL/MISC_Util';
import { mkLog } from 'UTIL/logger';
const log = mkLog('ui.core');

// Core defaults, from wiringPanel.cpp's initialisers. Shown until this browser
// pushes something -- an honest starting point, not a read-back.
const DEFAULTS = {
  IMG_STREAMING_MAX_FPS: 20,
  IMG_STREAMING_JPEG_QUALITY: 85,
  IMG_STREAMING_SKIP_NA: false,
  INSP_NA_SNAP: true,
};

function QueueBar({ name, q, hint }) {
  const size = GetObjElement(q, ['size']);
  const cap = GetObjElement(q, ['capacity']);
  if (size === undefined) return null;
  const pct = cap > 0 ? Math.round((size / cap) * 100) : 0;
  // A queue that is merely non-empty is normal at frame rate; one that is
  // FULL is the shape the 2026-08-10 stall took (producer outrunning the
  // consumer until every frame waits). Colour on the ratio, not on size.
  const status = pct >= 90 ? 'exception' : pct >= 50 ? 'active' : 'normal';
  return (
    <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginBottom: 4 }}>
      <Tooltip title={hint}><span style={{ width: 120, fontFamily: 'monospace' }}>{name}</span></Tooltip>
      <Progress percent={pct} status={status} showInfo={false} style={{ flex: 1, margin: 0 }} />
      <span style={{ width: 70, textAlign: 'right', fontFamily: 'monospace' }}>{size}/{cap}</span>
    </div>
  );
}

function Counter({ label, value, hint, warnAbove = 0 }) {
  const n = Number(value || 0);
  return (
    <div>
      <Tooltip title={hint}><span style={{ color: '#888' }}>{label}</span></Tooltip>
      {': '}
      <b style={{ color: n > warnAbove ? '#cf1322' : undefined, fontFamily: 'monospace' }}>{n}</b>
    </div>
  );
}

/**
 * @param info  the 1Hz GS status object (CORE_ID_CONN_INFO.info)
 * @param send  (tl, prop, obj, bin, cbs) => void, bound to CORE_ID
 */
export function CoreStatusPanel({ info, send }) {
  const [cfg, setCfg] = useState(DEFAULTS);
  const [dumpState, setDumpState] = useState('');   // '' | 'busy' | 'ok' | 'fail'

  const push = (key, val) => {
    setCfg((p) => ({ ...p, [key]: val }));
    send('ST', 0, { [key]: val });
    log.info('[core-cfg] push', { [key]: val });
  };

  // SC log_dump: ask the drainer to write the WHOLE ring out. This is the only
  // way to get the verbose lines out of a running core -- disk persist defaults
  // to WARN-and-above, so anything below it exists in RAM and nowhere else
  // until someone asks for this.
  //
  // On-demand dumps go to latest_dump.dump under a FIXED name (inspd_log_main
  // passes fixed_name=on_demand); only real crashes get crash_<utc>.dump. So
  // pressing this twice keeps only the second one -- the UI says so, because
  // "I dumped it earlier" is otherwise a false memory.
  const logDump = () => {
    setDumpState('busy');
    send('SC', 0, { type: 'log_dump' }, undefined, {
      resolve: (pkts) => {
        const ss = pkts.find((p) => p.type === 'SS');
        const ok = !!(ss && ss.data && ss.data.ACK);
        setDumpState(ok ? 'ok' : 'fail');
        setTimeout(() => setDumpState(''), 4000);
      },
      reject: () => { setDumpState('fail'); setTimeout(() => setDumpState(''), 4000); },
    });
  };

  const q = GetObjElement(info, ['precess_queue_status']);

  return (
    <>
      <Card size="small" title="佇列深度">
        <QueueBar name="inspQueue"     q={GetObjElement(q, ['inspQueue'])}
          hint="等待檢驗的影格。長期貼滿＝檢驗跟不上相機。" />
        <QueueBar name="datViewQueue"  q={GetObjElement(q, ['datViewQueue'])}
          hint="等待送到 WebUI 的影像。貼滿代表編碼/網路跟不上,不影響判定。" />
        <QueueBar name="inspSnapQueue" q={GetObjElement(q, ['inspSnapQueue'])}
          hint="等待寫檔的 NG/NA 快照。貼滿時新快照會被丟掉(下方計數)。" />
        {q === undefined && <span style={{ color: '#888' }}>（尚未取得狀態）</span>}
      </Card>

      <Card size="small" title="快照遺失計數" style={{ marginTop: 10 }}>
        <div style={{ fontSize: 12, lineHeight: 1.9 }}>
          <Counter label="佇列滿而略過" value={info && info.snap_queue_skip_count}
            hint="inspSnapQueue 滿,這張 NG/NA 沒有存下來。" />
          <Counter label="資料夾滿後刪舊" value={info && info.save_snap_folder_full_delete_count}
            hint="達到 INSP_NG_SNAP_MAX_NUM,刪掉最舊的一張騰位置。" />
          <Counter label="磁碟將滿而略過" value={info && info.save_snap_disk_low_skip_count}
            hint="磁碟空間不足,直接不存。這個數字在動＝機器已經沒有在留證據。" />
        </div>
      </Card>

      <Card size="small" title="影像串流（本次連線,重開核心即回預設）" style={{ marginTop: 10 }}>
        <div style={{ display: 'flex', gap: 16, flexWrap: 'wrap', alignItems: 'center' }}>
          <label>最高 FPS:
            <Tooltip title="核心推給 WebUI 的影像上限。降低它可以直接省下編碼與傳輸成本,判定完全不受影響。">
              <InputNumber value={cfg.IMG_STREAMING_MAX_FPS} min={1} max={60} step={1}
                style={{ marginLeft: 6, width: 80 }}
                onChange={(v) => push('IMG_STREAMING_MAX_FPS', Math.max(1, v || 1))} />
            </Tooltip>
          </label>
          <label>JPEG 品質:
            <Tooltip title="0 = 不壓縮(原始 RGBA,最貴)。1-100 為 JPEG 品質。注意:進入量測設定會被無條件改回 85。">
              <InputNumber value={cfg.IMG_STREAMING_JPEG_QUALITY} min={0} max={100} step={5}
                style={{ marginLeft: 6, width: 80 }}
                onChange={(v) => push('IMG_STREAMING_JPEG_QUALITY', Math.min(100, Math.max(0, v || 0)))} />
            </Tooltip>
          </label>
        </div>
      </Card>

      <Card size="small" title="每次開始檢驗會重設" style={{ marginTop: 10 }}>
        <div style={{ display: 'flex', gap: 24, flexWrap: 'wrap', alignItems: 'center' }}>
          <label>
            <Switch size="small" checked={cfg.IMG_STREAMING_SKIP_NA}
              onChange={(c) => push('IMG_STREAMING_SKIP_NA', c)} />
            <span style={{ marginLeft: 8 }}>不傳 NA 影像</span>
          </label>
          <label>
            <Switch size="small" checked={cfg.INSP_NA_SNAP}
              onChange={(c) => push('INSP_NA_SNAP', c)} />
            <span style={{ marginLeft: 8 }}>儲存 NA 快照</span>
          </label>
        </div>
        <div style={{ fontSize: 12, color: '#888', marginTop: 8, lineHeight: 1.7 }}>
          這兩項在核心裡於每次 CI/FI 開始時被歸零,所以只在<b>當前這段檢驗</b>內有效。
          NG 快照的開關與上限不放這裡——它由「設定」頁擁有,而且每次開始全檢都會重推一次;
          兩個地方都能寫會讓值變得無法解釋。
        </div>
      </Card>

      <Card size="small" title="診斷" style={{ marginTop: 10 }}>
        <Button size="small" onClick={logDump} loading={dumpState === 'busy'}>
          傾印核心日誌 (log_dump)
        </Button>
        {dumpState === 'ok'   && <span style={{ marginLeft: 10, color: '#389e0d' }}>已寫出 latest_dump.dump</span>}
        {dumpState === 'fail' && <span style={{ marginLeft: 10, color: '#cf1322' }}>核心沒有 ACK</span>}
        <div style={{ fontSize: 12, color: '#888', marginTop: 8, lineHeight: 1.7 }}>
          日誌存檔預設只留 WARN 以上,其餘只活在 RAM 環形緩衝區裡。這個按鈕把<b>整個環</b>
          （含 INFO/DEBUG）寫到資料目錄下的 <code>latest_dump.dump</code>,核心不會中斷。
          出問題當下按它,不要等重開。<b>檔名固定,再按一次會蓋掉上一份</b>——要留就先改名。
          （真正的崩潰才會寫成 crash_&lt;utc&gt;.dump。）
          瀏覽器端的紀錄請用抽屜上方的「下載診斷紀錄」,那是另一份東西。
        </div>
        <div style={{ fontSize: 12, marginTop: 8, fontFamily: 'monospace', color: '#888' }}>
          <div>binary: {GetObjElement(info, ['binary_path']) || '—'}</div>
          <div>data:   {GetObjElement(info, ['data_path']) || '—'}</div>
        </div>
      </Card>
    </>
  );
}

export default CoreStatusPanel;
