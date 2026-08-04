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

// Matches LIGHT_HOLD_MAX_MS in the firmware, which clamps anything larger.
// Two minutes is not enough to set exposure/gain/focus without the light
// dying mid-adjustment; five is the most the board will hold.
const LIGHT_HOLD_MS = 300000;
const refFreq = (plate_freq) => (plate_freq > 0 ? plate_freq : REF_FREQ);
const isRef = (plate_freq) => !(plate_freq > 0);

export function UINSP_ESP32_UI({ pollMs = 1000 }) {
  const dispatch = useDispatch();
  const API_ID = useSelector((s) => s.ConnInfo.uInspESP32_API_ID);
  const CONN = useSelector((s) => s.ConnInfo.uInspESP32_API_ID_CONN_INFO);
  const CORE_ID = useSelector((s) => s.ConnInfo.CORE_ID);
  const withApi = (cb) => dispatch(UIAct.EV_WS_GET_OBJ(API_ID, cb));

  const [stat, setStat] = useState(undefined);
  const [busy, setBusy] = useState('');
  const [freqInput, setFreqInput] = useState('');
  const [hzInput, setHzInput] = useState('');        // gate fire-rate cap, in parts/s

  const [commDiag, setCommDiag] = useState(null);
  const [pairing, setPairing] = useState(null);   // core-side frame<->object pairing health

  const [lightUntil, setLightUntil] = useState(0);   // epoch ms the board will auto-drop the hold
  const [now, setNow] = useState(Date.now());
  const mounted = useRef(true);

  const cfg = GetObjElement(CONN, ['machineSetup']) || {};
  const dev = GetObjElement(CONN, ['deviceState']) || {};
  const connected = GetObjElement(CONN, ['type']) === 'WS_CONNECTED';
  const spo = cfg.stage_pulse_offset || {};
  const plate_freq = stat ? stat.plate_freq : cfg.plate_freq;
  // Gate admission stats come from the board (get_running_stat.gate); the
  // configured cap is mirrored there too, so the panel never has to guess
  // whether its own last write actually landed.
  const gate = stat ? stat.gate : undefined;
  const gateSepUs = gate ? gate.min_sep_us : cfg.min_detect_sep_us;
  const gateHz = gateSepUs > 0 ? Math.round(1000000 / gateSepUs) : undefined;

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
      // Pairing health lives in the CORE, not the device -- the device can only
      // ever say "answered / unanswered", never "answered with the right
      // verdict". So it comes over the core's GS channel, not get_running_stat.
      dispatch(UIAct.EV_WS_SEND_BPG(CORE_ID, "GS", 0, { items: ["perif_pairing"] },
        undefined, {
          resolve: (pkts) => {
            const gs = pkts.find((p) => p.type === "GS");
            const pv = gs && gs.data && gs.data.perif_pairing;
            if (pv && mounted.current) setPairing(pv);
          },
          reject: () => {},
        }));
    };
    tick();
    const h = setInterval(tick, pollMs);
    const h2 = setInterval(() => { if (mounted.current) setNow(Date.now()); }, 500);
    return () => { mounted.current = false; clearInterval(h); clearInterval(h2); };
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

      {pairing && (
      <Card size="small" title="影格配對(核心端)" style={{ marginBottom: 8 }}>
        <div style={{ marginBottom: 6, ...dim }}>
          每張影格屬於哪一顆料。配錯不會有任何錯誤碼 —— 料照樣被回答、照樣被分選,
          只是判定落在別顆身上。這裡是唯一看得出來的地方。
        </div>
        <div style={{ display: 'flex', gap: 14, flexWrap: 'wrap', marginBottom: 6 }}>
          <span>模式 <b>{pairing.mode}</b>
            {pairing.mode === 'timestamp' && !pairing.offset_valid
              ? <span style={dim}> (校時中)</span> : null}</span>
          <span>已配對 <b>{pairing.matched}</b> / {pairing.rx}</span>
          <span>待配 <b>{pairing.pending}</b></span>
        </div>
        <div style={{ display: 'flex', gap: 14, flexWrap: 'wrap', marginBottom: 6 }}>
          {/* Two counts, deliberately not summed: no_candidate is a FRAME that
              could not be placed (its part goes unjudged), stale is a TRIGGER
              whose frame never arrived (that part is reported NA). They pair up
              1:1 when it is the same event seen from both sides. */}
          <span>配對失敗·影格 <b style={{ color: pairing.no_candidate > 0 ? '#c33' : undefined }}>
            {pairing.no_candidate}</b></span>
          <span>配對失敗·觸發 <b style={{ color: pairing.stale > 0 ? '#c60' : undefined }}>
            {pairing.stale}</b></span>
          <span>佇列滿溢 <b style={{ color: pairing.drops > 0 ? '#c33' : undefined }}>
            {pairing.drops}</b></span>
          {pairing.rx > 0 && (
            <span style={dim}>失敗率 {((pairing.no_candidate / pairing.rx) * 100).toFixed(1)}%</span>
          )}
        </div>
        {pairing.mode === 'timestamp' && (
          <div style={{ display: 'flex', gap: 14, flexWrap: 'wrap', ...dim }}>
            {/* resid is how far each match sat from where the clock model said
                it should be. Tens of us = the model is right. Creeping toward
                the tolerance = the offset is drifting faster than it is tracked. */}
            <span>時鐘差 {Number(pairing.offset_ms).toFixed(1)} ms</span>
            <span>殘差 現在 {Math.round(pairing.resid_last_us)} µs</span>
            <span>殘差 最大 {Math.round(pairing.resid_max_us)} µs</span>
            <span>宣告最晚 {Number(pairing.trig_wait_max_ms).toFixed(0)} ms</span>
          </div>
        )}
      </Card>
      )}

      <Card size="small" title="進料節流(閘門)" style={{ marginBottom: 8 }}>
        <div style={{ marginBottom: 6, ...dim }}>
          閘門每登記一個物件就會觸發相機一次。要求得比相機能給的快,就會出現「有觸發、沒影格」
          —— 那會讓主機的配對永久錯位,不是只掉一顆料。這裡把進料速率壓在相機之下。
        </div>
        <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap', marginBottom: 8 }}>
          <Input
            style={{ width: 150 }}
            addonBefore="上限"
            addonAfter="顆/秒"
            placeholder={gateHz !== undefined ? String(gateHz) : ''}
            value={hzInput}
            onChange={(e) => setHzInput(e.target.value)}
          />
          <Button
            loading={busy === 'gate'}
            disabled={!(Number(hzInput) > 0)}
            onClick={() => run('gate', (api) => api.machineSetupUpdate(
              { min_detect_sep_us: Math.round(1000000 / Number(hzInput)) }, false, true))}
          >套用</Button>
          <span style={{ alignSelf: 'center', ...dim }}>
            目前 {gateHz !== undefined ? `${gateHz} 顆/秒` : '—'}
            {gateSepUs !== undefined ? ` (min_detect_sep_us=${gateSepUs})` : ''}
          </span>
          {/* The camera ceiling, measured rather than assumed: shrinking the ROI
              height raises it, which is the lever for running parts closer
              together. The gate cap has to stay under this -- above it you get
              triggers with no frames, which is exactly what breaks the pairing. */}
          {pairing && pairing.cam_max_fps > 0 && (
            <span style={{ alignSelf: 'center',
              color: gateHz > pairing.cam_max_fps ? '#c33' : '#888' }}>
              相機實測上限 {Number(pairing.cam_max_fps).toFixed(1)} fps
              {gateHz > pairing.cam_max_fps ? ' ← 閘門開得比相機快' : ''}
            </span>
          )}
        </div>
        {gate && (
          <div style={{ display: 'flex', gap: 14, flexWrap: 'wrap' }}>
            <span>通過 <b>{gate.accept}</b></span>
            {/* rej_rate > 0 is the limiter doing its job: those parts stayed on
                the plate and come round again. It is not an error, but it must
                be visible -- "missing parts" and "deliberately skipped parts"
                look identical without it. */}
            <span>擋下·速率 <b style={{ color: gate.rej_rate > 0 ? '#c60' : undefined }}>
              {gate.rej_rate}</b></span>
            <span>擋下·距離 <b>{gate.rej_dist}</b></span>
            <span>擋下·忙碌 <b style={{ color: gate.rej_busy > 0 ? '#c33' : undefined }}>
              {gate.rej_busy}</b></span>
          </div>
        )}
      </Card>

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
        {/* SKIP is the one that does not announce itself. Reporting an object
            marks every OLDER still-unjudged object as SKIP, so any out-of-order
            report (a stale-trigger NA fill is always for an older id than the
            verdicts around it) sweeps its predecessors. If their own verdict
            lands before the selector it overwrites the SKIP and nothing is
            lost; if it does not, the part goes through unjudged -- and unlike
            UNANSWERED, SKIP raises no error at all. So this is the honest
            count of parts that passed without a verdict. */}
        <div style={kv}>
          <span>SKIP <span style={dim}>(被較新的回報蓋過)</span></span>
          <b style={{ color: cnt.SKIP ? '#c60' : undefined }}>{cnt.SKIP ?? '—'}</b>
        </div>
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

      {/* Steady light for camera setup. The board refuses the hold outside IDLE
          (in INSPECTION the stage tasks own these pins and would stomp it) and
          drops it on a timeout, because a backlight sized for a 600us strobe is
          not necessarily rated for continuous duty. */}
      <Card size="small" title="背光 (相機設定用)" style={{ marginBottom: 8 }}>
        <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap', alignItems: 'center' }}>
          {['L1A', 'L2A'].map((ch) => (
            <Button key={ch} size="small" loading={busy === 'lt' + ch} disabled={running}
              onClick={() => run('lt' + ch, (api) =>
                api.light(ch, true, LIGHT_HOLD_MS).then((r) => {
                  if (r && r.ack === false) { log.warn('[light] refused', r); return; }
                  setLightUntil(Date.now() + ((r && r.timeout_ms) || LIGHT_HOLD_MS));
                }))}
            >{ch} 開</Button>
          ))}
          <Button size="small" danger loading={busy === 'ltoff'}
            onClick={() => run('ltoff', (api) => {
              setLightUntil(0);
              return Promise.all([api.light('L1A', false), api.light('L2A', false)]);
            })}
          >全部關</Button>
          <span style={dim}>
            {running
              ? '檢測模式中無法手動點燈'
              : lightUntil > now
                ? `亮著 — ${Math.ceil((lightUntil - now) / 1000)} 秒後自動熄滅`
                : '熄滅'}
          </span>
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
