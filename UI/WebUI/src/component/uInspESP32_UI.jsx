// Control panel for the uInspESP32 sorter.
//
// A separate file from rdxComponent.jsx on purpose: that one holds the
// uInspMEGA / SLID / CNC panels, and this board speaks a different dialect
// (plate_freq + stage_pulse_offset + an explicit inspection-mode state machine,
// rather than pulse_hz + res_count). Keeping it apart means the 1st-gen panels
// stay untouched while this one grows.
//
// Everything here goes through uInspESP32_API (script.jsx). Note that reading
// is free but writing is not: the board owns its configuration in NVS, so a
// value typed here is pushed only when you press its button, and only
// "Save to NVS" makes it survive a reboot.
import React, { useState, useEffect, useRef } from 'react';
import { useSelector, useDispatch } from 'react-redux';
import Button from 'antd/lib/button';
import Input from 'antd/lib/input';
import Tag from 'antd/lib/tag';
import Card from 'antd/lib/card';
import Divider from 'antd/lib/divider';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import { GetObjElement } from 'UTIL/MISC_Util';
import { mkLog } from 'UTIL/logger';
const log = mkLog('ui.uinsp2');

// From FirmwareTypes.hpp (SYS_STATE). Anything unlisted is shown raw rather
// than guessed at.
const STATE_NAME = {
  0: 'BOOT',
  100: 'IDLE',
  101: 'INSPECTION',
  110: 'PAUSED',
  112: 'ERROR',
};

// GEN_ERROR_CODE, same header. Worth spelling out: "2" on its own tells an
// operator nothing, and this is the code they will actually meet.
const ERR_NAME = {
  0: 'RESET',
  1: 'result matched no object',
  2: 'object reached SWITCH with no verdict',
  3: 'result counter error',
  4: 'result pulse time out of sync',
  5: 'result has no timestamp',
  10: 'cam_trig could not be sent',
  11: 'serial protocol error',
  12: 'host link timeout',
  255: 'SEL actuation limit reached',
};

const stateName = (s) => (STATE_NAME[s] !== undefined ? `${STATE_NAME[s]} (${s})` : `state ${s}`);
const errName = (e) => (ERR_NAME[e] !== undefined ? `${e}: ${ERR_NAME[e]}` : `code ${e}`);

// The stage timer ticks at 2x plate_freq, so every offset in
// stage_pulse_offset is (ticks / (2*plate_freq)) seconds. Showing the ms value
// next to the raw tick count is the whole point of this panel: 1500 ticks means
// nothing, "50 ms of air" means something, and it changes with plate speed.
const ticksToMs = (ticks, plate_freq) =>
  plate_freq > 0 && ticks !== undefined ? (ticks * 1000) / (2 * plate_freq) : NaN;

const fmtMs = (ms) => (isFinite(ms) ? `${ms.toFixed(ms < 10 ? 2 : 0)} ms` : '—');

// A stopped plate makes every offset infinite in time, so the panel would go
// blank exactly when someone is sitting there reading the timing. Fall back to
// the production speed and label it, rather than showing nothing.
const REF_FREQ = 15000;
const refFreq = (plate_freq) => (plate_freq > 0 ? plate_freq : REF_FREQ);
const isRef = (plate_freq) => !(plate_freq > 0);

export function UINSP_ESP32_UI({ pollMs = 1000 }) {
  const dispatch = useDispatch();
  const API_ID = useSelector((s) => s.ConnInfo.uInspESP32_API_ID);
  const CONN = useSelector((s) => s.ConnInfo.uInspESP32_API_ID_CONN_INFO);
  const withApi = (cb) => dispatch(UIAct.EV_WS_GET_OBJ(API_ID, cb));

  const [stat, setStat] = useState(undefined);
  const [busy, setBusy] = useState('');
  const [freqInput, setFreqInput] = useState('');
  const [commDiag, setCommDiag] = useState(null);
  const mounted = useRef(true);

  const cfg = GetObjElement(CONN, ['machineSetup']) || {};
  const dev = GetObjElement(CONN, ['deviceState']) || {};
  const connected = GetObjElement(CONN, ['type']) === 'WS_CONNECTED';
  const spo = cfg.stage_pulse_offset || {};
  const plate_freq = stat ? stat.plate_freq : cfg.plate_freq;

  // Poll running stats while the panel is open. The board answers
  // get_running_stat in ~8 ms, so 1 Hz is free -- and without it the counters
  // are a still frame, which is exactly the wrong thing when you are watching
  // for a fault.
  useEffect(() => {
    mounted.current = true;
    const tick = () => {
      withApi((api) => {
        if (!api || typeof api.getRunningStat !== 'function') return;
        api.getRunningStat()
          .then((r) => { if (mounted.current) setStat(r); })
          .catch(() => {});
      });
    };
    tick();
    const h = setInterval(tick, pollMs);
    return () => { mounted.current = false; clearInterval(h); };
  }, [API_ID, pollMs]);

  const run = (label, fn) => {
    setBusy(label);
    withApi((api) => {
      let p;
      try { p = fn(api); } catch (e) { log.warn('[uinsp2]', label, e); }
      Promise.resolve(p).catch((e) => log.warn('[uinsp2]', label, 'failed', e))
        .finally(() => { if (mounted.current) setBusy(''); });
    });
  };

  const runCommDiag = (n = 20) => {
    setCommDiag({ running: true, i: 0, total: n });
    withApi((api) => {
      if (!api || typeof api.diagnoseComm !== 'function') {
        setCommDiag({ error: 'diagnoseComm unavailable' });
        return;
      }
      api.diagnoseComm(n, (u) => {
        if (!mounted.current) return;
        setCommDiag(u.done ? { running: false, ...u } : { running: true, ...u });
      });
    });
  };

  const inError = stat && stat.state === 112;
  const running = stat && stat.state === 101;
  const cnt = (stat && stat.count) || {};
  const lat = (stat && stat.report_latency) || {};
  const health = (stat && stat.health) || {};
  const pipe = (stat && stat.pipe) || {};

  const kv = { display: 'flex', justifyContent: 'space-between', gap: 12, padding: '1px 0' };
  const dim = { color: '#888' };

  return (
    <div style={{ minWidth: 460 }}>
      <div style={{ marginBottom: 8 }}>
        <Tag color={connected ? 'green' : 'red'}>{connected ? '已連線' : '未連線'}</Tag>
        <Tag color={inError ? 'red' : running ? 'blue' : 'default'}>
          {stat ? stateName(stat.state) : '—'}
        </Tag>
        <Tag color={dev.cfg_from_nvs ? 'green' : 'orange'}>
          {dev.cfg_from_nvs ? '設定來自 NVS' : '設定非 NVS(編譯預設值)'}
        </Tag>
        {dev.machine_id ? <Tag>{dev.machine_id}</Tag> : null}
      </div>

      {stat && stat.error_hist && stat.error_hist.length > 0 && (
        <Card size="small" style={{ marginBottom: 8, borderColor: '#c33' }}>
          <b style={{ color: '#c33' }}>錯誤紀錄</b>
          {stat.error_hist.map((e, i) => <div key={i}>{errName(e)}</div>)}
        </Card>
      )}

      <Card size="small" title="運轉" style={{ marginBottom: 8 }}>
        <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap', marginBottom: 8 }}>
          <Input
            style={{ width: 130 }}
            addonBefore="plate_freq"
            placeholder={plate_freq !== undefined ? String(plate_freq) : ''}
            value={freqInput}
            onChange={(e) => setFreqInput(e.target.value)}
          />
          <Button
            loading={busy === 'freq'}
            disabled={freqInput === '' || isNaN(Number(freqInput))}
            onClick={() => run('freq', (api) =>
              api.machineSetupUpdate({ plate_freq: Number(freqInput) }, false, true))}
          >套用轉速</Button>
          <Button danger loading={busy === 'stop'}
            onClick={() => run('stop', (api) => {
              api.exitInspMode();
              return api.machineSetupUpdate({ plate_freq: 0 }, false, true);
            })}
          >停止</Button>
        </div>
        <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap' }}>
          <Button type="primary" loading={busy === 'enter'} disabled={running}
            onClick={() => run('enter', (api) => { api.stepperEnable(); return api.enterInspMode(); })}
          >進入檢測模式</Button>
          <Button loading={busy === 'exit'} disabled={!running}
            onClick={() => run('exit', (api) => api.exitInspMode())}
          >離開檢測模式</Button>
          <Button loading={busy === 'clear'} disabled={!inError}
            onClick={() => run('clear', (api) => { api.clearError(); return api.clearErrorHistory(); })}
          >清除錯誤</Button>
        </div>
        <div style={{ display: 'flex', gap: 6, marginTop: 8 }}>
          <Button size="small" onClick={() => run('sen', (api) => api.stepperEnable())}>驅動器 ON</Button>
          <Button size="small" onClick={() => run('sdis', (api) => api.stepperDisable())}>驅動器 OFF</Button>
          <Button size="small" onClick={() => run('rst', (api) => api.resetRunningStat())}>歸零統計</Button>
        </div>
      </Card>

      <Card size="small" title="統計" style={{ marginBottom: 8 }}>
        <div style={kv}><span>SEL1 / SEL2 / SEL3</span>
          <b>{cnt.SEL1 ?? '—'} / {cnt.SEL2 ?? '—'} / {cnt.SEL3 ?? '—'}</b></div>
        <div style={kv}><span>NA</span><b>{cnt.NA ?? '—'}</b></div>
        <div style={kv}>
          <span>UNANSWERED</span>
          <b style={{ color: cnt.UNANSWERED ? '#c33' : undefined }}>{cnt.UNANSWERED ?? '—'}</b>
        </div>
        <Divider style={{ margin: '6px 0' }} />
        {/* Measured from GATE registration, not from the camera trigger --
            trig_us is stamped beside gate_pulse. So it contains the transport
            time from gate to camera as well as the vision loop. */}
        <div style={kv}><span>回報延遲 平均 / 最大 <span style={dim}>(自閘門起算)</span></span>
          <b>{lat.avg_us ? (lat.avg_us / 1000).toFixed(0) : '—'} / {lat.max_us ? (lat.max_us / 1000).toFixed(0) : '—'} ms</b></div>
        {/* The deadline this must beat, in the same gate-relative frame. */}
        <div style={kv}><span>SWITCH 期限</span>
          <b>{fmtMs(ticksToMs(spo.SWITCH, refFreq(plate_freq)))}
            {isRef(plate_freq) ? <span style={dim}> @{REF_FREQ}</span> : null}</b></div>
        <div style={kv}><span>在途 / 等待</span>
          <b>{pipe.registered ?? '—'} / {pipe.waiting ?? '—'}</b></div>
      </Card>

      <Card size="small"
        title={`時序 (ticks — 括號為 ${isRef(plate_freq) ? `plate_freq ${REF_FREQ} 參考值` : "目前轉速"} 下的時間)`}
        style={{ marginBottom: 8 }}>
        {[['相機/光源 CAM1', spo.CAM1_on, spo.CAM1_off],
          ['SWITCH 判定期限', spo.SWITCH, undefined],
          ['SEL1 吹氣', spo.SEL1_on, spo.SEL1_off],
          ['SEL2 吹氣', spo.SEL2_on, spo.SEL2_off]].map(([name, on, off]) => (
          <div style={kv} key={name}>
            <span>{name}</span>
            <b>
              {on ?? '—'}{off !== undefined ? `→${off}` : ''}
              <span style={dim}>
                {' '}({fmtMs(ticksToMs(on, refFreq(plate_freq)))}
                {off !== undefined ? `, 寬 ${fmtMs(ticksToMs(off - on, refFreq(plate_freq)))}` : ''})
              </span>
            </b>
          </div>
        ))}
        <div style={{ marginTop: 8 }}>
          <Button size="small" loading={busy === 'save'}
            onClick={() => run('save', (api) => api.saveSetupToDevice())}
          >存入 NVS</Button>
        </div>
      </Card>

      <Card size="small" title="診斷">
        <div style={kv}><span>CRC ok / fail</span>
          <b>{health.rx_crc_ok ?? '—'} / <span style={{ color: health.rx_crc_fail ? '#c33' : undefined }}>{health.rx_crc_fail ?? '—'}</span></b></div>
        <div style={kv}><span>rbuf peak</span><b>{health.rbuf_peak ?? '—'}</b></div>
        <div style={kv}><span>min heap</span><b>{health.min_heap ?? '—'}</b></div>
        <div style={kv}><span>uptime</span><b>{health.uptime_s ?? '—'} s</b></div>
        <div style={{ marginTop: 8 }}>
          <Button size="small" onClick={() => runCommDiag(20)} loading={commDiag && commDiag.running}>
            通訊診斷 (20 次)
          </Button>
          {commDiag && !commDiag.running && commDiag.avg !== undefined && (
            <span style={{ marginLeft: 8 }}>
              min {commDiag.min} / avg {commDiag.avg} / p95 {commDiag.p95} / max {commDiag.max} ms
              {commDiag.fails ? <b style={{ color: '#c33' }}> ({commDiag.fails} 失敗)</b> : null}
            </span>
          )}
          {commDiag && commDiag.error && <span style={{ marginLeft: 8, color: '#c33' }}>{commDiag.error}</span>}
        </div>
      </Card>
    </div>
  );
}

export default UINSP_ESP32_UI;
