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
import Collapse from 'antd/lib/collapse';
import Divider from 'antd/lib/divider';
import Slider from 'antd/lib/slider';
import Switch from 'antd/lib/switch';
import Tooltip from 'antd/lib/tooltip';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import { GetObjElement } from 'UTIL/MISC_Util';
import { mkLog } from 'UTIL/logger';
const log = mkLog('ui.uinsp2');

// From FirmwareTypes.hpp SMM_STATE_DECLARE. The panel used to carry five of
// these and invented one (110/PAUSED, which does not exist), so a machine
// sitting in CAL showed "state 102" -- a number that means nothing to the
// person watching it wait. Kept in the same order as the firmware macro so the
// two can be diffed by eye; get_state_names (below) replaces this at runtime
// when the board is new enough to answer it.
const STATE_NAME = {
  0: 'INIT',
  100: 'IDLE',
  101: 'READY',            // inspecting
  102: 'CAL',              // clock calibration, plate deliberately still
  103: 'SPINUP',           // ramping to the target speed
  104: 'RECAL',            // re-calibrating during an idle gap
  112: 'ERROR',
  113: 'FATAL',
  140: 'TEST',
  200: 'NOP',
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
  11: 'serial protocol error — a malformed/stray byte on the link; LATCHED, needs 清除錯誤',
  12: 'host link timeout',
  13: 'camera clock lost',
  14: 'startup clock calibration did not converge',
  15: 'plate never reached target speed',
  255: 'SEL actuation limit reached',
};

// The short local name wins WHEN THERE IS ONE, and the board's answer fills the
// gaps. Deliberately that way round: the bug was coverage, not wording -- 102
// had no entry at all -- and the firmware's own identifiers are the full enum
// names (INSPECTION_MODE_READY), which are worse in a tag than READY. So the
// board guarantees nothing is ever unnamed again; the local table only makes
// the common ones short.
const stateName = (s, names) => {
  const n = STATE_NAME[s] || (names && names.state && names.state[s]);
  return n !== undefined ? `${n} (${s})` : `state ${s}`;
};
const errName = (e, names) => {
  const n = (names && names.err && names.err[e]) || ERR_NAME[e];
  return n !== undefined ? `${e}: ${n}` : `code ${e}`;
};

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

// Plate geometry, measured on the machine (_PLAT_PULSE_PER_TURN in the
// firmware): 60000 stage pulses per revolution of a 240mm plate, so one pulse
// is 0.0126 mm of travel. The stage timer ticks at 2x plate_freq, which makes
// plate_freq the only speed the board exposes -- and a bare "15000" tells an
// operator nothing. rpm and mm/s are the same fact in units someone can act on,
// and mm/s is the one that sets the exposure budget: 0.01 mm of smear at
// 377 mm/s is 26 us, which is a decision, not a statistic.
const PULSES_PER_REV = 60000;
const MM_PER_PULSE = (240 * Math.PI) / PULSES_PER_REV;
const plateRpm = (pf) => (pf > 0 ? (2 * pf * 60) / PULSES_PER_REV : 0);
const plateMmS = (pf) => (pf > 0 ? 2 * pf * MM_PER_PULSE : 0);

// Slider range. 20000 is ~40 rpm / 500 mm/s, comfortably past anything the
// camera can keep up with, so the top of the travel is a limit the machine
// meets rather than one the UI imposes. The step is coarse on purpose: 250 is
// 0.5 rpm, and finer than that is a number nobody is choosing deliberately.
const SPEED_MAX = 20000;
const SPEED_STEP = 250;
const SPEED_MARKS = { 0: '0', 10000: '20rpm', 15000: '30rpm', 20000: '40rpm' };

// A "?" that carries the explanation instead of a paragraph that carries it
// permanently. The prose is worth keeping -- most of it is a fact that cost a
// day to learn -- but it belongs one hover away, not between the operator and
// the number.
const Why = ({ children }) => (
  <Tooltip title={<div style={{ maxWidth: 320 }}>{children}</div>}>
    <span style={{ cursor: 'help', color: '#888', border: '1px solid #bbb',
      borderRadius: '50%', fontSize: 10, lineHeight: '14px', width: 15, height: 15,
      display: 'inline-block', textAlign: 'center', marginLeft: 6 }}>?</span>
  </Tooltip>
);

export function UINSP_ESP32_UI({ pollMs = 1000 }) {
  const dispatch = useDispatch();
  const API_ID = useSelector((s) => s.ConnInfo.uInspESP32_API_ID);
  const CONN = useSelector((s) => s.ConnInfo.uInspESP32_API_ID_CONN_INFO);
  const CORE_ID = useSelector((s) => s.ConnInfo.CORE_ID);
  const withApi = (cb) => dispatch(UIAct.EV_WS_GET_OBJ(API_ID, cb));

  const [stat, setStat] = useState(undefined);
  const [busy, setBusy] = useState('');
  // Slider position. It is NOT bound to plate_freq, because RUN/STOP sets the
  // device speed to 0 -- binding them would snap the slider to zero the moment
  // the machine stops and lose the speed the operator had chosen. So the slider
  // holds the target, the device holds the actual, and the panel says when the
  // two disagree.
  const [speed, setSpeed] = useState(undefined);
  const [hzInput, setHzInput] = useState('');        // gate fire-rate cap, in parts/s

  // When the last poll actually answered. Age, not a failure count, because the
  // failure mode here is silence: a request whose reply never comes back leaves
  // its promise unsettled forever, so nothing ever rejects and a counter of
  // rejections stays at zero while the panel is completely dead. That is
  // exactly what happened -- green 已連線 (latched at CONNECT, never rechecked)
  // beside a status that never filled in, indistinguishable from a panel opened
  // half a second ago.
  const lastOkRef = useRef(0);
  const openedRef = useRef(Date.now());

  // Camera trigger position and exposure-window width, in stage ticks. Same
  // pattern as the speed slider: local while dragging, pushed on release.
  const [camOn, setCamOn] = useState(undefined);
  const [camW, setCamW] = useState(undefined);

  const [names, setNames] = useState(null);   // state/err text, asked of the board

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

  // Poll running stats while the panel is open.
  //
  // NEVER let two polls be outstanding at once. The reply is ~1.6 kB and the
  // link is 115200 baud, so one get_running_stat alone occupies ~140 ms of wire
  // -- and it shares that wire with cam_trig announcements, which the core has
  // measured arriving up to 115 ms late. A blind 1 Hz interval turns a slow
  // link into a dead one: the reply takes longer than the period, the next
  // request is queued anyway, and the queue grows faster than the device can
  // drain it. That is congestion collapse, and the symptom is exactly "the
  // ESP32 stopped answering" -- from a device that never stopped working.
  useEffect(() => {
    mounted.current = true;
    // When the outstanding request was sent, 0 when idle. A timestamp rather
    // than a boolean because a reply that never comes leaves its promise
    // unsettled forever -- a plain in-flight flag would then be stuck true and
    // the panel would never poll again, turning one lost reply into a permanent
    // outage. After STALL_MS we give up on that reply and send a fresh one.
    let sentAt = 0;
    const STALL_MS = 5000;
    const tick = () => {
      withApi((api) => {
        if (!api || typeof api.getRunningStat !== 'function') return;
        if (sentAt && Date.now() - sentAt < STALL_MS) return;  // wire still busy
        sentAt = Date.now();
        api.getRunningStat()
          .then((r) => {
            sentAt = 0;
            if (mounted.current) { setStat(r); lastOkRef.current = Date.now(); }
          })
          // Never swallow this. The silent catch that used to sit here meant a
          // link that was rejecting outright left no trace at all.
          .catch((e) => { sentAt = 0; log.warn('[uinsp2] get_running_stat failed', e); });
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

  // Asked once per panel open, not per poll: the table cannot change without a
  // reflash, and a reflash reopens the link. Firmware that does not know the
  // command answers ack:false or nothing at all, and the built-in table stands.
  useEffect(() => {
    withApi((api) => {
      if (!api || typeof api.getStateNames !== 'function') return;
      api.getStateNames()
        .then((r) => { if (r && r.state && mounted.current) setNames(r); })
        .catch((e) => log.warn('[uinsp2] get_state_names unavailable', e));
    });
  }, [API_ID]);

  // Adopt the board's trigger window once, then the sliders own it -- rebinding
  // on every poll would fight the drag.
  useEffect(() => {
    if (camOn === undefined && spo.CAM1_on !== undefined) {
      setCamOn(spo.CAM1_on);
      setCamW(Math.max(1, (spo.CAM1_off ?? spo.CAM1_on) - spo.CAM1_on));
    }
  }, [spo.CAM1_on, spo.CAM1_off, camOn]);

  // Adopt the machine's speed once, so the slider opens where the machine
  // actually is. After that the operator owns it. A stopped plate has no speed
  // to adopt, so it opens at the production value rather than at zero -- a
  // slider that starts at 0 makes RUN a two-step action for no reason.
  useEffect(() => {
    if (speed === undefined && plate_freq !== undefined) setSpeed(plate_freq > 0 ? plate_freq : REF_FREQ);
  }, [plate_freq, speed]);

  const run = (label, fn) => {
    setBusy(label);
    withApi((api) => {
      let p;
      try { p = fn(api); } catch (e) { log.warn('[uinsp2]', label, e); }
      Promise.resolve(p).catch((e) => log.warn('[uinsp2]', label, 'failed', e))
        .finally(() => { if (mounted.current) setBusy(''); });
    });
  };

  // RUN does the whole thing -- driver on, speed applied, inspection entered --
  // because "RUN" on a switch means the machine processes parts, and a version
  // that enters inspection mode on a stationary plate would satisfy the label
  // while doing nothing. STOP is the inverse and stops the plate: an operator
  // reaching for STOP wants the plate still, not merely unjudged.
  const toggleRun = (on) => run('run', (api) => {
    if (!on) {
      api.exitInspMode();
      return api.machineSetupUpdate({ plate_freq: 0 }, false, true);
    }
    api.stepperEnable();
    api.machineSetupUpdate({ plate_freq: speed || REF_FREQ }, false, true);
    // Barrier, not politeness. machineSetupUpdate is fire-and-forget, so firing
    // enter_insp_mode straight after it races: the board enters CAL while
    // plate_freq is still 0, the stage timer never ticks, the calibration
    // pulses never fire, and it fails to converge -- err 14, machine in ERROR.
    // Measured, not theorised: that is exactly what the first version did.
    // The device answers in the order it was asked, so a reply to a request
    // queued AFTER set_setup proves set_setup was consumed.
    return api.getRunningStat().then((s) => {
      if (!(s && s.plate_freq > 0)) {
        log.warn('[uinsp2] refusing RUN: plate_freq did not take', s && s.plate_freq);
        return undefined;   // entering CAL at 0 is a guaranteed err 14
      }
      return api.enterInspMode();
    });
  });

  // CAM1 is the camera's trigger line, L1A is the backlight strobe -- two pins,
  // two independent offsets, and they default to the same window. Moving one
  // without the other exposes the sensor into the dark, so the panel moves them
  // as one: the whole group shifts by the same delta and takes the same width.
  //
  // The full stage_pulse_offset object is sent, not just the two keys. The
  // firmware writes only what it is given (JSON_SETIF_ABLE), so a partial one
  // would be correct on the device -- but machineSetupUpdate merges shallowly,
  // so the panel's own copy would lose SWITCH and every SEL offset and the
  // timing card below would go blank.
  const applyCam = (on, w) => run('cam', (api) => api.machineSetupUpdate({
    stage_pulse_offset: {
      ...spo,
      CAM1_on: on, CAM1_off: on + w,
      L1A_on:  on, L1A_off:  on + w,
    },
  }, false, true));

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

  // Four missed polls. Long enough that one slow round-trip does not flicker
  // the panel, short enough that an operator is not reading stale counters.
  const staleMs = now - (lastOkRef.current || openedRef.current);
  const noReply = staleMs > 4 * pollMs;

  // RUN means the board is in INSPECTION *and* the plate is turning. Either one
  // alone is a machine that is not processing parts, and calling that RUN is
  // how someone stands in front of a stopped plate waiting for counts.
  //
  // But there is a third thing, and leaving it out was the panel's own fault:
  // after the switch is flipped the FIRMWARE runs a sequence -- CAL (clock
  // calibration, plate deliberately still), then SPINUP (ramp), then READY --
  // and that takes tens of seconds. Showing plain "STOP" throughout reads as
  // "the switch did nothing", which is exactly how it was read.
  const rpm = plateRpm(plate_freq);
  const st = stat ? stat.state : undefined;
  const starting = st === 102 || st === 103 || st === 104;
  const runState = inError ? 'ERROR' : starting ? '啟動中' : (running && rpm > 0) ? 'RUN' : 'STOP';
  const runColor = runState === 'ERROR' ? '#c33'
    : runState === 'RUN' ? '#389e0d' : starting ? '#d48806' : '#888';
  // What the machine is actually doing right now, in the operator's terms.
  const runHint = inError ? '錯誤中,需先清除'
    : st === 102 ? '校時中 — 盤故意停著'
    : st === 103 ? '加速中 — 正在到達設定轉速'
    : st === 104 ? '空檔重新校時'
    : running ? '檢測中' : '未進入檢測模式';

  // The six numbers an operator watches. SKIP and UNANSWERED are both parts
  // that went through without a verdict, kept apart because only one of them
  // raises an error on the device -- SKIP is the quiet one, and quiet is the
  // reason it belongs on the main row rather than three cards down.
  const Cell = ({ label, value, warn, why }) => (
    <div style={{ minWidth: 46 }}>
      <div style={{ fontSize: 11, whiteSpace: 'nowrap', ...dim }}>
        {label}{why ? <Why>{why}</Why> : null}
      </div>
      <div style={{ fontSize: 20, lineHeight: '24px', fontWeight: 600,
        color: warn && value > 0 ? warn : undefined }}>
        {value ?? '—'}
      </div>
    </div>
  );

  return (
    <div style={{ minWidth: 460 }}>
      <div style={{ marginBottom: 8 }}>
        <Tag color={connected ? 'green' : 'red'}>{connected ? '已連線' : '未連線'}</Tag>
        <Tag color={inError ? 'red' : running ? 'blue' : 'default'}>
          {stat ? stateName(stat.state, names) : "—"}
        </Tag>
        {noReply && (
          <Tag color="red">無回應 {Math.round(staleMs / 1000)}s</Tag>
        )}
        <Tag color={dev.cfg_from_nvs ? 'green' : 'orange'}>
          {dev.cfg_from_nvs ? '設定來自 NVS' : '設定非 NVS(編譯預設值)'}
        </Tag>
        {dev.machine_id ? <Tag>{dev.machine_id}</Tag> : null}
      </div>

      {/* ---- main row: what someone standing at the machine needs ---------- */}
      <Card size="small" style={{ marginBottom: 8, borderColor: runColor }}
        bodyStyle={{ padding: '10px 12px' }}>
        <div style={{ display: 'flex', gap: 20, alignItems: 'flex-start',
          flexWrap: 'wrap', marginBottom: 10 }}>
          <div>
            <div style={{ fontSize: 11, ...dim }}>狀態</div>
            <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
              <Switch
                // Checked through the whole start-up sequence, not only at
                // READY: the switch is "what was asked for", and springing back
                // to off during CAL would say the command was refused.
                checked={runState === 'RUN' || starting}
                loading={busy === 'run' || starting}
                disabled={noReply || inError}
                checkedChildren="RUN" unCheckedChildren="STOP"
                onChange={toggleRun}
              />
              <span style={{ fontSize: 22, lineHeight: '26px', fontWeight: 700, color: runColor }}>
                {runState}
              </span>
            </div>
            {/* The switch otherwise reflects the board, not the click: it sits
                where the last poll put it, so a command that did not take shows
                as the switch falling back rather than as a lie on screen. */}
            <div style={{ fontSize: 11, color: starting ? '#d48806' : '#888' }}>
              {runHint}
            </div>
          </div>
          <div style={{ flex: '1 1 200px', minWidth: 190 }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 11, ...dim }}>
              <span>轉速</span>
              <span>
                {/* Shown for the SLIDER position, not the running speed, so the
                    consequence is visible before the plate moves. 0.01mm is the
                    width-measurement budget, and it is the exposure that has to
                    fit inside it. */}
                {speed > 0
                  ? `${plateRpm(speed).toFixed(1)} rpm · ${plateMmS(speed).toFixed(0)} mm/s · 0.01mm = ${(10000 / plateMmS(speed)).toFixed(0)}µs`
                  : '停止'}
              </span>
            </div>
            <Slider
              min={0} max={SPEED_MAX} step={SPEED_STEP}
              value={speed ?? 0}
              marks={SPEED_MARKS}
              tooltipVisible={false}
              onChange={setSpeed}
              onAfterChange={(v) => run('freq', (api) =>
                api.machineSetupUpdate({ plate_freq: v }, false, true))}
              /* The bottom margin is the mark labels' room. antd reserves it via
                 .ant-slider-with-marks; an inline `margin` shorthand wipes that
                 out and the marks land on top of whatever follows. */
              style={{ margin: '2px 6px 24px 6px' }}
            />
            {/* Only while running. Stopped, the device is at 0 by definition and
                the slider is holding the target -- flagging that as a mismatch
                would mean the panel cries wolf every time the machine is idle,
                which is most of the time someone is looking at it. */}
            {running && plate_freq !== speed && (
              <div style={{ fontSize: 11, color: '#c60' }}>
                裝置目前 plate_freq {plate_freq ?? '—'}(拖曳放開才送出)
              </div>
            )}
          </div>
          <div>
            <div style={{ fontSize: 11, ...dim }}>進料</div>
            <div style={{ fontSize: 20, lineHeight: '24px', fontWeight: 600 }}>
              {gate ? gate.accept : '—'}
            </div>
            <div style={{ fontSize: 11, ...dim }}>
              上限 {gateHz !== undefined ? `${gateHz}/s` : '—'}
            </div>
          </div>
        </div>
        <div style={{ display: 'flex', gap: 9, flexWrap: 'wrap' }}>
          <Cell label="SEL1" value={cnt.SEL1} />
          <Cell label="SEL2" value={cnt.SEL2} />
          <Cell label="SEL3" value={cnt.SEL3} />
          <Cell label="NA" value={cnt.NA} />
          <Cell label="NA(SKIP)" value={cnt.SKIP} warn="#c60"
            why={<>被較新的回報蓋過。回報一顆料時,所有比它舊、還沒判定的料都會被標成
              SKIP。若它們自己的判定在 SWITCH 之前趕到,SKIP 會被覆蓋、什麼都沒少;
              沒趕到的話,那顆料就沒有判定通過了 —— 而且跟 UNANSWERED 不同,
              SKIP 完全不會報錯。所以這是「無聲通過」的誠實數字。</>} />
          <Cell label="NA(UNANS)" value={cnt.UNANSWERED} warn="#c33"
            why="料走到 SWITCH 時仍然沒有任何判定。裝置會為此報錯。" />
        </div>
      </Card>

      {stat && stat.error_hist && stat.error_hist.length > 0 && (
        <Card size="small" style={{ marginBottom: 8, borderColor: '#c33' }}>
          <b style={{ color: '#c33' }}>錯誤紀錄</b>
          {stat.error_hist.map((e, i) => <div key={i}>{errName(e, names)}</div>)}
        </Card>
      )}

      {/* The switch and the slider are the run control now. What is left are the
          two actions that are not a mode: clearing a fault, and zeroing the
          counters someone is watching. */}
      <div style={{ display: 'flex', gap: 6, marginBottom: 8 }}>
        <Button size="small" danger={inError} loading={busy === 'clear'} disabled={!inError}
          onClick={() => run('clear', (api) => { api.clearError(); return api.clearErrorHistory(); })}
        >清除錯誤</Button>
        <Button size="small" loading={busy === 'rst'}
          onClick={() => run('rst', (api) => api.resetRunningStat())}>歸零統計</Button>
      </div>

      {/* Where the camera fires, in the only frame that matters: distance along
          the plate from the gate that registered the part. Not in ADVANCED,
          because "the trigger is not on the object" is the thing you are here
          to fix, and it is fixed by watching the image while you drag. */}
      {camOn !== undefined && (
      <Card size="small" style={{ marginBottom: 8 }} title={<span>相機觸發位置
        <Why>觸發位置是「從閘門登記算起,盤走了多少 tick」。1 tick = 0.0126 mm,
          所以這是空間上的位移,跟轉速無關 —— 改轉速不會讓觸發跑掉。
          CAM1(相機觸發)和 L1A(背光)一起移動,否則相機會在暗的時候曝光。</Why></span>}>
        <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 11, ...dim }}>
          <span>位置</span>
          <span>
            {camOn} ticks · <b style={{ color: '#000' }}>{(camOn * MM_PER_PULSE).toFixed(2)} mm</b>
            {' '}· {fmtMs(ticksToMs(camOn, refFreq(plate_freq)))}
          </span>
        </div>
        <Slider
          min={0} max={spo.SWITCH || 30000} step={5}
          value={camOn} tooltipVisible={false}
          onChange={setCamOn}
          onAfterChange={(v) => applyCam(v, camW)}
          style={{ margin: '2px 6px 8px 6px' }}
        />
        <div style={{ display: 'flex', gap: 4, alignItems: 'center', marginBottom: 10 }}>
          {/* A slider cannot land on one tick, and one tick is 12.6 um -- which
              is the resolution this adjustment actually needs. */}
          {[-100, -10, -1, 1, 10, 100].map((d) => (
            <Button key={d} size="small" style={{ padding: '0 7px' }}
              onClick={() => { const v = Math.max(0, camOn + d); setCamOn(v); applyCam(v, camW); }}
            >{d > 0 ? `+${d}` : d}</Button>
          ))}
          <span style={{ ...dim, fontSize: 11, marginLeft: 4 }}>
            ticks — 1 tick = {MM_PER_PULSE.toFixed(4)} mm
          </span>
        </div>

        <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 11, ...dim }}>
          <span>曝光窗寬度</span>
          <span>
            {camW} ticks · {(camW * MM_PER_PULSE).toFixed(3)} mm
            {' '}· {Math.round(ticksToMs(camW, refFreq(plate_freq)) * 1000)} µs
          </span>
        </div>
        <Slider
          min={1} max={120} step={1}
          value={camW} tooltipVisible={false}
          onChange={setCamW}
          onAfterChange={(v) => applyCam(camOn, v)}
          style={{ margin: '2px 6px 8px 6px' }}
        />

        {/* Two limits worth meeting before the image looks wrong rather than
            after. The trigger floor is the camera's; the smear budget is the
            0.01mm the width measurement is allowed to lose. */}
        {(() => {
          const us = ticksToMs(camW, refFreq(plate_freq)) * 1000;
          const smear = camW * MM_PER_PULSE;
          const warn = [];
          if (us < 100) warn.push(`窗只有 ${Math.round(us)}µs — 相機觸發下限約 100µs,可能根本不會拍`);
          else if (us < 300) warn.push(`窗 ${Math.round(us)}µs — 低於 300µs 背光到不了全亮`);
          if (smear > 0.01) warn.push(`盤在窗內走 ${smear.toFixed(3)} mm — 超過 0.01mm 量測預算`);
          return warn.length ? (
            <div style={{ fontSize: 11, color: '#c60', marginBottom: 8 }}>
              {warn.map((w, i) => <div key={i}>⚠ {w}</div>)}
            </div>
          ) : null;
        })()}

        <div style={{ display: 'flex', gap: 6, alignItems: 'center', flexWrap: 'wrap' }}>
          <Button size="small" loading={busy === 'spo_save'}
            onClick={() => run('spo_save', (api) => api.saveSetupToDevice())}
          >存入 NVS</Button>
          <span style={{ ...dim, fontSize: 11 }}>
            拖曳放開就立刻生效(下一顆料起);不存 NVS 的話重開機會回到舊值
          </span>
        </div>
      </Card>
      )}

      {/* Everything below is setup, tuning and diagnosis -- read during
          bring-up, not while parts are running. Collapsed by default so the
          numbers above are the panel, not the header of a long page. */}
      <Collapse ghost style={{ marginLeft: -16, marginRight: -16 }}>
        <Collapse.Panel key="adv" header={<b>ADVANCED</b>}>

      {pairing && (
      <Card size="small" style={{ marginBottom: 8 }} title={<span>影格配對(核心端)
        <Why>每張影格屬於哪一顆料。配錯不會有任何錯誤碼 —— 料照樣被回答、照樣被分選,
          只是判定落在別顆身上。這裡是唯一看得出來的地方。</Why></span>}>
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
          <span>配對失敗·影格
            <Why>影格找不到對應的觸發,那顆料就沒有判定。與「配對失敗·觸發」刻意不相加:
              一個是影格找不到料,一個是料等不到影格,同一件事從兩邊看時會 1:1 對上。</Why>
            {' '}<b style={{ color: pairing.no_candidate > 0 ? '#c33' : undefined }}>
            {pairing.no_candidate}</b></span>
          <span>配對失敗·觸發
            <Why>觸發發出去了,影格從來沒回來 —— 那顆料會被回報成 NA。</Why>
            {' '}<b style={{ color: pairing.stale > 0 ? '#c60' : undefined }}>
            {pairing.stale}</b></span>
          <span>佇列滿溢 <b style={{ color: pairing.drops > 0 ? '#c33' : undefined }}>
            {pairing.drops}</b></span>
          {/* Not failures. covered_by_skip = orphans we stayed quiet about
              because a later report had already swept them on the device;
              dumped = frames discarded before inspection because their
              announcement was lost, so nothing could ever claim them. Both are
              work avoided, and both are only legible next to the failures. */}
          <span style={dim}>裝置已掃 {pairing.covered_by_skip ?? 0}
            <Why>不是失敗。裝置端較新的回報已經把這些孤兒掃過了,所以主機刻意不出聲。</Why></span>
          <span style={dim}>提前丟棄 {pairing.dumped ?? 0}
            <Why>不是失敗。影格的宣告掉了,沒有任何東西能認領它,所以在檢測前就丟掉。
              這是省下的工,只有跟失敗數擺在一起才讀得懂。</Why></span>
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
            <span>殘差 現在 {Math.round(pairing.resid_last_us)} µs
              <Why>每次配對離時鐘模型預測位置的距離。幾十 µs = 模型是對的;
                往容差爬 = 時鐘偏移漂得比追蹤還快。</Why></span>
            <span>殘差 最大 {Math.round(pairing.resid_max_us)} µs</span>
            <span>宣告最晚 {Number(pairing.trig_wait_max_ms).toFixed(0)} ms</span>
          </div>
        )}
      </Card>
      )}

      <Card size="small" style={{ marginBottom: 8 }} title={<span>進料節流(閘門)
        <Why>閘門每登記一個物件就會觸發相機一次。要求得比相機能給的快,就會出現
          「有觸發、沒影格」—— 那會讓主機的配對永久錯位,不是只掉一顆料。
          這裡把進料速率壓在相機之下。</Why></span>}>
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
        <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap', marginBottom: 8 }}>
          {/* Disabling the gate does not lose parts -- they ride round again,
              exactly as they do for any rate/distance rejection. What it buys is
              a lane with no real detections in it, so a calibration pulse can be
              fired and measured without a part landing mid-measurement. */}
          <Button
            danger={gate && gate.disabled}
            loading={busy === 'gatedis'}
            onClick={() => run('gatedis', (api) => api.setGateDisable(!(gate && gate.disabled)))}
          >{gate && gate.disabled ? '閘門已停用 — 恢復' : '停用閘門(僅忽略真實感測)'}</Button>
        <Why>停用閘門不會掉料 —— 料會再轉一圈回來,跟任何速率/距離擋下的情況一樣。
          換到的是一條沒有真實偵測的通道,校時脈衝可以在沒有料闖進來的情況下量測。</Why>
          <Button
            loading={busy === 'phantom'}
            onClick={() => run('phantom', (api) => api.trigPhantomPulse())}
          >注入假脈衝</Button>
          {gate && (
            <span style={{ alignSelf: 'center', ...dim }}>
              轉速{gate.freq_stable ? '已穩定' : '未穩定'}
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
            <span>擋下·速率
              <Why>限速器在做它的事:這些料留在盤上,下一圈會再回來。這不是錯誤,
                但必須看得見 —— 沒有它,「掉了的料」跟「故意跳過的料」長得一模一樣。</Why>
              {' '}<b style={{ color: gate.rej_rate > 0 ? '#c60' : undefined }}>
              {gate.rej_rate}</b></span>
            <span>擋下·距離 <b>{gate.rej_dist}</b></span>
            <span>擋下·忙碌 <b style={{ color: gate.rej_busy > 0 ? '#c33' : undefined }}>
              {gate.rej_busy}</b></span>
          </div>
        )}
      </Card>

      <Card size="small" title="統計" style={{ marginBottom: 8 }}>
        {/* SEL/NA/SKIP/UNANSWERED now live on the main row above. What is left
            here is the timing question they cannot answer: a verdict that is
            correct but late is still an unjudged part, and the only way to see
            that coming is the latency next to the deadline it has to beat. */}
        {/* Measured from GATE registration, not from the camera trigger --
            trig_us is stamped beside gate_pulse. So it contains the transport
            time from gate to camera as well as the vision loop. */}
        <div style={kv}><span>回報延遲 平均 / 最大 <span style={dim}>(自閘門起算)</span>
          <Why>從閘門登記算起,不是從相機觸發 —— trig_us 是跟 gate_pulse 一起蓋章的。
            所以它同時包含閘門到相機的傳輸時間和視覺運算。要比下面的 SWITCH 期限早,
            判定正確但太晚,那顆料一樣沒有判定。</Why></span>
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

      <Card size="small" style={{ marginBottom: 8 }} title={<span>背光 (相機設定用)
        <Why>相機設定用的常亮。裝置在 IDLE 以外會拒絕 —— 檢測模式下這些腳位歸 stage
          任務所有,點了也會被蓋掉 —— 而且會逾時自動熄滅:為 600µs 閃燈設計的背光,
          不一定撐得住連續點亮。</Why></span>}>
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

        </Collapse.Panel>
      </Collapse>
    </div>
  );
}

export default UINSP_ESP32_UI;
