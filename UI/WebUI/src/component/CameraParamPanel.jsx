// The one camera-parameter editor. Used by the main menu's Camera modal
// (script.jsx) and by the calibration page (CalibrationUI.js), which is where
// this code started life -- two copies of exposure/gain/gamma/blacklevel would
// drift, and only one of them would learn the things written below.
//
// THREE FACTS THAT SHAPE THIS PANEL:
//
// 1. The camera does not read back. `GS camera_info` reports identity, mmpp and
//    calibration state -- no exposure, no gain. So the values shown here come
//    from data/default_camera_setting.json (the same file the core loads at
//    boot via CameraSettingFromFile), NOT from the sensor. Two people tuning
//    from two browsers will not see each other.
//
// 2. A green ACK does not mean the sensor changed. Every driver setter can
//    refuse (the base class NAKs anything unimplemented) and CameraSetup used
//    to discard the status. It now accumulates refused names into
//    camera_info.setup_failed -- the ONLY place a refused setter is visible.
//    This panel surfaces it, because "I set exposure and nothing happened" is
//    otherwise indistinguishable from a lighting problem.
//
// 3. Each push costs an acquisition restart. wiringPanel's CameraSetup does
//    StopAquisition() ... StartAquisition() around the setters, so one push per
//    keystroke would stop and start the camera a dozen times per edit. Hence
//    the debounce. It is also why the panel says not to do this mid-run.
//
// ROI IS DELIBERATELY NOT EDITABLE HERE. The stored crop lives under
// "InspectionROI" and is written by exactly ONE gesture -- an operator
// finishing a crop in the Inspection UI (save_insp_roi). Every other path sets
// the runtime word "ROI", which the loader strips from the file on purpose:
// that separation is what stops an abandoned full-sensor selection from
// permanently erasing the machine's crop. Making it a field here would put
// that bug back. It is shown read-only so the operator can still SEE it.
import React, { useState, useEffect, useRef, useCallback } from 'react';
import Card from 'antd/lib/card';
import Button from 'antd/lib/button';
import InputNumber from 'antd/lib/input-number';
import Tag from 'antd/lib/tag';
import Tooltip from 'antd/lib/tooltip';
import { mkLog } from 'UTIL/logger';
const log = mkLog('ui.camparam');

export const CAMERA_SETTING_PATH = 'data/default_camera_setting.json';

// Only what the core's CameraSetup actually applies AND that is safe to nudge
// from a menu. trigger_mode is excluded on purpose: re-applying it on a
// full-frame camera is the documented way to kill this Hikrobot until
// DeviceReset (CameraLayer_Aravis, 2026-08-11).
const FIELDS = [
  { key: 'exposure',   label: '曝光 exposure (µs)', min: 1,   max: 1000000, step: 100,  dflt: 10000, width: 120 },
  { key: 'gain',       label: '增益 gain',           min: 0,   max: 48,      step: 0.1,  dflt: 1.0,   width: 90  },
  { key: 'gamma',      label: 'gamma',               min: 0.1, max: 4,       step: 0.05, dflt: 1.0,   width: 90  },
  { key: 'blacklevel', label: 'blacklevel',          min: 0,   max: 1000,    step: 1,    dflt: 0,     width: 90  },
];

const PUSH_DEBOUNCE_MS = 300;

/**
 * @param send  (tl, prop, obj, bin, cbs) => void   -- already bound to CORE_ID
 * @param camInfo  optional camera_info[0] object; only used to show
 *                 setup_failed / InspectionROI context. Panel works without it.
 * @param title    card title override
 * @param onPush   optional (key, value) notification, for pages that mirror
 *                 the value elsewhere (calibration's exposure readout).
 */
export function CameraParamPanel({ send, camInfo, title, onPush, style }) {
  const [vals, setVals] = useState(() =>
    FIELDS.reduce((o, f) => { o[f.key] = f.dflt; return o; }, {}));
  const [loaded, setLoaded] = useState(false);
  const [saveState, setSaveState] = useState('');   // '' | 'saving' | 'ok' | 'fail'
  const [inspROI, setInspROI] = useState(undefined);
  const timers = useRef({});

  // Read the file. This is the ONLY source of initial values -- see note 1.
  const loadFromFile = useCallback(() => {
    setLoaded(false);
    send('LD', 0, { filename: CAMERA_SETTING_PATH }, undefined, {
      resolve: (pkts) => {
        const fl = pkts.find((p) => p.type === 'FL');
        const cfg = fl && fl.data;
        if (cfg && typeof cfg === 'object') {
          setVals((prev) => {
            const next = { ...prev };
            FIELDS.forEach((f) => { if (cfg[f.key] != null) next[f.key] = cfg[f.key]; });
            return next;
          });
          setInspROI(Array.isArray(cfg.InspectionROI) ? cfg.InspectionROI : null);
        }
        setLoaded(true);
      },
      reject: () => { log.warn('[cam-param] cannot read', CAMERA_SETTING_PATH); setLoaded(true); },
    });
  }, [send]);

  useEffect(() => { loadFromFile(); }, []);

  useEffect(() => () => {
    Object.values(timers.current).forEach((t) => clearTimeout(t));
  }, []);

  // Debounced live push -- see note 3. Per-key timers so editing exposure does
  // not swallow a gain change made in the same window.
  const pushLive = (key, val) => {
    clearTimeout(timers.current[key]);
    timers.current[key] = setTimeout(() => {
      send('ST', 0, { CameraSetting: { [key]: val } });
      log.info('[cam-param] push', { [key]: val });
      if (onPush) onPush(key, val);
    }, PUSH_DEBOUNCE_MS);
  };

  const onEdit = (f, raw) => {
    let x = raw == null || Number.isNaN(+raw) ? f.dflt : +raw;
    if (x < f.min) x = f.min;
    if (x > f.max) x = f.max;
    setVals((p) => ({ ...p, [f.key]: x }));
    pushLive(f.key, x);
  };

  // Read-modify-write: pull the file, override only OUR fields, write back.
  // Anything else in there (InspectionROI, mirror, transpose, vendor keys)
  // survives -- this panel must never be able to erase the machine's crop.
  const save = () => {
    setSaveState('saving');
    send('LD', 0, { filename: CAMERA_SETTING_PATH }, undefined, {
      resolve: (pkts) => {
        const fl = pkts.find((p) => p.type === 'FL');
        const base = (fl && fl.data && typeof fl.data === 'object') ? fl.data : {};
        const merged = { ...base, ...vals };
        const payload = new TextEncoder().encode(JSON.stringify(merged, null, 2));
        send('SV', 0, { filename: CAMERA_SETTING_PATH }, payload, {
          resolve: (pkts2) => {
            const ss = pkts2.find((p) => p.type === 'SS');
            const ok = !!(ss && ss.data && ss.data.ACK);
            setSaveState(ok ? 'ok' : 'fail');
            log[ok ? 'info' : 'warn']('[cam-param] save', { ok, merged });
            setTimeout(() => setSaveState(''), 2500);
          },
          reject: () => { setSaveState('fail'); setTimeout(() => setSaveState(''), 2500); },
        });
      },
      reject: () => {
        log.warn('[cam-param] save aborted: could not read', CAMERA_SETTING_PATH);
        setSaveState('fail');
        setTimeout(() => setSaveState(''), 2500);
      },
    });
  };

  // Note 2: the driver refused these. Empty string = everything applied.
  const failed = (camInfo && camInfo.setup_failed) ? String(camInfo.setup_failed) : '';
  const failedSet = new Set(failed ? failed.split(',') : []);

  return (
    <Card size="small" style={style}
      title={<span>{title || '相機參數'}{loaded ? null : <span style={{ color: '#aaa' }}> (讀取中…)</span>}</span>}
      extra={
        <span>
          <Button size="small" onClick={loadFromFile} disabled={!loaded}>重讀檔案</Button>
          <Button size="small" type="primary" style={{ marginLeft: 6 }}
            onClick={save} disabled={!loaded || saveState === 'saving'}>
            {saveState === 'saving' ? '存檔中…' : saveState === 'ok' ? '已存檔' : '存檔'}
          </Button>
        </span>
      }>
      <div style={{ display: 'flex', gap: 16, flexWrap: 'wrap', alignItems: 'center' }}>
        {FIELDS.map((f) => (
          <label key={f.key} style={{ whiteSpace: 'nowrap' }}>
            {f.label}:
            <InputNumber value={vals[f.key]} min={f.min} max={f.max} step={f.step}
              style={{ marginLeft: 6, width: f.width }} disabled={!loaded}
              onChange={(v) => onEdit(f, v)} />
            {failedSet.has(f.key) && (
              <Tooltip title="驅動拒絕了這個設定 — 值沒有進到感測器。camera_info.setup_failed 是唯一看得到它的地方。">
                <Tag color="red" style={{ marginLeft: 6 }}>未套用</Tag>
              </Tooltip>
            )}
          </label>
        ))}
      </div>

      {saveState === 'fail' && (
        <div style={{ color: '#cf1322', marginTop: 8 }}>
          存檔失敗 — {CAMERA_SETTING_PATH} 沒有寫成功，重開核心後會回到舊值。
        </div>
      )}

      <div style={{ fontSize: 12, color: '#888', marginTop: 10, lineHeight: 1.7 }}>
        <div>
          值來自 <code>{CAMERA_SETTING_PATH}</code>，<b>不是從相機回讀</b>——相機不回報
          曝光/增益。改動會即時推到相機（{PUSH_DEBOUNCE_MS}ms 去抖），但只有<b>存檔</b>
          才會在重開核心後留著。
        </div>
        <div>
          每次推送都會讓相機 stop/start acquisition：<b>機器在跑產時不要動這裡</b>。
        </div>
        {inspROI !== undefined && (
          <div>
            檢測裁切 InspectionROI：<code>{inspROI ? `[${inspROI.join(', ')}]` : '（未設定＝全感測器）'}</code>
            {' '}— 唯讀。只能在檢測畫面框選後儲存，這樣才不會被別的「開全幅」動作洗掉。
          </div>
        )}
        {failed && (
          <div style={{ color: '#cf1322' }}>
            驅動拒絕套用：<b>{failed}</b>（camera_info.setup_failed）
          </div>
        )}
      </div>
    </Card>
  );
}

export default CameraParamPanel;
