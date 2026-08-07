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
const rpmToFreq = (rpm) => Math.round((rpm * PULSES_PER_REV) / 120);   // inverse of plateRpm

// Slider range. 20000 is ~40 rpm / 500 mm/s, comfortably past anything the
// camera can keep up with, so the top of the travel is a limit the machine
// meets rather than one the UI imposes. The step is coarse on purpose: 250 is
// 0.5 rpm, and finer than that is a number nobody is choosing deliberately.
const SPEED_MAX = 20000;
const SPEED_STEP = 250;
const SPEED_MARKS = { 0: '0', 10000: '20rpm', 15000: '30rpm', 20000: '40rpm' };

// Every station the plate carries a part past, in the order it meets them.
// Names and keys are exactly the firmware's stage_pulse_offset fields, so this
// table and SMM's struct can be diffed by eye.
//
// SWITCH has no "off": it is not an actuator, it is the deadline by which a
// verdict must have arrived. Everything downstream of it is a blow nozzle.
const STATIONS = [
  { key: 'CAM1', label: '相機 CAM1', on: 'CAM1_on', off: 'CAM1_off', us: 'CAM1',
    why: '相機觸發 (GPIO17)。窗寬就是曝光窗;盤在窗內走的距離就是拖影。' },
  { key: 'L1A',  label: '背光 L1A',  on: 'L1A_on',  off: 'L1A_off', us: 'L1A',
    why: '背光 (GPIO16)。必須完整覆蓋 CAM1 的窗,否則相機會在漸亮或全暗中曝光。' +
         '背光約需 300µs 才全亮,所以提前開、延後關是合理的。' },
  { key: 'CAM2', label: '相機 CAM2', on: 'CAM2_on', off: 'CAM2_off', us: 'CAM2' },
  { key: 'L2A',  label: '背光 L2A',  on: 'L2A_on',  off: 'L2A_off', us: 'L2A' },
  { key: 'SWITCH', label: 'SWITCH 期限', on: 'SWITCH',
    why: '判定期限,不是致動器。料走到這裡還沒有判定就算沒判定 —— 而且會被標成 ' +
         'UNANSWERED 或被較新的回報蓋成 SKIP。必須早於所有 SEL。' },
  { key: 'SEL1', label: 'SEL1 吹氣', on: 'SEL1_on', off: 'SEL1_off', us: 'SEL1' },
  { key: 'SEL2', label: 'SEL2 吹氣', on: 'SEL2_on', off: 'SEL2_off', us: 'SEL2' },
  { key: 'SEL3', label: 'SEL3 吹氣', on: 'SEL3_on', off: 'SEL3_off', us: 'SEL3' },
];

// The firmware stores on/off; the panel edits position/width. Kept as an
// explicit pair of converters rather than done inline, because a half-typed
// width must not be able to corrupt `off` on the device -- the conversion only
// happens on commit.
const spoToEdit = (spo, wus, plate_freq) => {
  const e = { ...spo };
  for (const st of STATIONS) {
    if (!st.off) continue;
    // Prefer the device's configured microseconds. Fall back to converting the
    // stored tick offsets only when no width has ever been set (wus === 0),
    // which is how a machine that predates the us fields still shows something
    // sensible instead of a blank field.
    const fromDev = wus && Number(wus[st.us]) > 0 ? Number(wus[st.us]) : 0;
    e['_w_' + st.key] = fromDev
      ? fromDev
      : Math.round(ticksToMs((spo[st.off] ?? 0) - (spo[st.on] ?? 0), refFreq(plate_freq)) * 1000);
  }
  return e;
};

// The speed the operator last ran this machine at, kept across reloads.
//
// Necessary because STOP has to zero plate_freq: SYS_STATE::IDLE's loop does
// PLATE_FREQ_TARGET = PLATE_FREQ_SETPOINT every pass, so leaving inspection
// mode does NOT stop the plate -- zeroing the setpoint is the only thing that
// does. Which means the configured speed is gone the moment you stop, and any
// control that wants to start again has nothing to start at.
//
// In-memory was not enough: the sidebar strip remounts, and a page reload wipes
// it, so an operator who stopped the machine yesterday met "no speed available"
// with no way forward except the setup panel.
//
// Deliberately NOT falling back to a compiled default. REF_FREQ is 30rpm; a
// machine being set up at 4.5rpm must not leap to that because someone pressed
// stop. No remembered speed means the control refuses and says so.
const SPEED_KEY = 'uinsp2.last_plate_freq';
const rememberSpeed = (pf) => {
  if (!(pf > 0)) return;
  try { window.localStorage.setItem(SPEED_KEY, String(pf)); } catch (e) { /* private mode */ }
};
const recallSpeed = () => {
  try { const v = Number(window.localStorage.getItem(SPEED_KEY)); return v > 0 ? v : 0; }
  catch (e) { return 0; }
};

// Start or stop the machine. Shared by the setup panel and the sidebar strip
// because getting this sequence subtly different in two places is how one of
// them ends up silently refusing itself.
//
// Returns a promise resolving to null on success, or a string saying why it
// declined -- callers show that, they do not swallow it.
function runSequence(api, on, speed) {
  if (!on) {
    api.exitInspMode();
    api.machineSetupUpdate({ plate_freq: 0 }, false, true);
    return Promise.resolve(null);
  }
  if (!(speed > 0)) return Promise.resolve('沒有可用的轉速,請先在設定面板設定');
  api.stepperEnable();
  api.machineSetupUpdate({ plate_freq: speed }, false, true);
  // Barrier, not politeness. machineSetupUpdate is fire-and-forget, so firing
  // enter_insp_mode straight after it races. The device answers in the order it
  // was asked, so a reply to a request queued AFTER set_setup proves set_setup
  // was consumed.
  //
  // It must be get_setup, not get_running_stat. The firmware keeps THREE plate
  // frequencies and they are not interchangeable:
  //
  //   PLATE_FREQ_SETPOINT  the configuration -- what set_setup writes, what
  //                        get_setup returns
  //   PLATE_FREQ_TARGET    what the ramp is aiming at -- what get_running_stat
  //                        returns, and 0 while IDLE
  //   PLATE_FREQ_CURRENT   the actual speed
  //
  // Checking the running stat meant checking TARGET to confirm a write to
  // SETPOINT, so in IDLE the test could never pass: the first press turned the
  // driver on, set the speed, failed its own check and silently declined to
  // enter inspection -- plate turning, switch snapping back, nothing else
  // happening.
  return api.getSetupP().then((s) => {
    if (!(s && s.plate_freq > 0)) return '轉速沒有寫進裝置,未進入檢測模式';
    return Promise.resolve(api.enterInspMode()).then(() => null);
  });
}

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

  const [names, setNames] = useState(null);   // state/err text, asked of the board
  const [runRefused, setRunRefused] = useState('');   // why RUN declined, shown inline
  // Working copy of stage_pulse_offset while it is being typed into. Adopted
  // from the device once, then owned by the editor -- rebinding on every poll
  // would overwrite a half-typed number on the next tick.
  const [spoEdit, setSpoEdit] = useState({});
  // Which field the fine-adjust bar acts on: {key,label,which:'pos'|'w'}.
  const [sel, setSel] = useState(null);
  // Set by nudge(), consumed by the effect that pushes the change. setSpoEdit
  // is async, so committing inside nudge() would send the PREVIOUS value.
  const [nudged, setNudged] = useState(false);

  const [commDiag, setCommDiag] = useState(null);
  const [pairing, setPairing] = useState(null);   // core-side frame<->object pairing health

  const [lightUntil, setLightUntil] = useState(0);   // epoch ms the board will auto-drop the hold
  const [now, setNow] = useState(Date.now());
  const mounted = useRef(true);

  const cfg = GetObjElement(CONN, ['machineSetup']) || {};
  const dev = GetObjElement(CONN, ['deviceState']) || {};
  const connected = GetObjElement(CONN, ['type']) === 'WS_CONNECTED';
  const spo = cfg.stage_pulse_offset || {};
  // Per-station widths in microseconds; {} on firmware that predates them.
  const wus = cfg.stage_pulse_width_us;
  const plate_freq = stat ? stat.plate_freq : cfg.plate_freq;
  // The CONFIGURED speed (PLATE_FREQ_SETPOINT), not the ramp's current target.
  // Width in microseconds is converted to ticks by the DEVICE against exactly
  // this value, so any us<->tick arithmetic shown here has to use it too. Using
  // the running target instead made the panel display a width 6.7x off while
  // the machine was idle -- the third time today these three variables have
  // been confused, so: get_setup.plate_freq = SETPOINT (what set_setup writes),
  // get_running_stat.plate_freq = TARGET (0 in IDLE), and CURRENT is neither.
  const setpoint_freq = (cfg.plate_freq > 0) ? cfg.plate_freq : refFreq(plate_freq);
  // Gate admission stats come from the board (get_running_stat.gate); the
  // configured cap is mirrored there too, so the panel never has to guess
  // whether its own last write actually landed.
  const gate = stat ? stat.gate : undefined;
  const gateSepUs = gate ? gate.min_sep_us : cfg.min_detect_sep_us;
  const gateHz = gateSepUs > 0 ? Math.round(1000000 / gateSepUs) : undefined;

  // Poll running stats while the panel is open.
  //
  // NEVER let two polls be outstanding at once. The reply is ~1.6 kB and the
  // link is 230400 baud, so one get_running_stat alone occupies ~70 ms of wire
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

  // Adopt the station table once, same reasoning as the sliders below.
  useEffect(() => {
    if (Object.keys(spoEdit).length === 0 && spo.CAM1_on !== undefined) setSpoEdit(spoToEdit(spo, wus, setpoint_freq));
  }, [spo.CAM1_on, spoEdit]);

  // Adopt the machine's speed once, so the slider opens where the machine
  // actually is. After that the operator owns it. A stopped plate has no speed
  // to adopt, so it opens at the production value rather than at zero -- a
  // slider that starts at 0 makes RUN a two-step action for no reason.
  useEffect(() => {
    if (speed === undefined && plate_freq !== undefined)
      setSpeed(plate_freq > 0 ? plate_freq : (recallSpeed() || REF_FREQ));
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
    if (on) rememberSpeed(speed);
    return runSequence(api, on, speed || REF_FREQ).then((why) => {
      if (mounted.current) setRunRefused(why || '');
      if (why) log.warn('[uinsp2] RUN declined:', why);
    });
  });

  const commitSpo = () => {
    const spoOut = {};
    const wOut = {};
    let changed = false;
    for (const st of STATIONS) {
      const pos = Number(spoEdit[st.on]);
      if (isFinite(pos) && spoEdit[st.on] !== '' && spoEdit[st.on] !== undefined) {
        spoOut[st.on] = pos;
        if (pos !== spo[st.on]) changed = true;
      }
      if (!st.off) continue;
      const w = Number(spoEdit['_w_' + st.key]);
      if (!isFinite(w) || spoEdit['_w_' + st.key] === '') continue;
      if (!(w > 0)) {
        log.warn('[uinsp2] refusing %s: width %s us', st.key, w);
        return;
      }
      wOut[st.us] = w;
      if (w !== Number((wus || {})[st.us])) changed = true;
    }
    if (!changed) return;
    // Widths go as microseconds and the DEVICE converts to ticks against its own
    // plate_freq. Sending ticks from here would bake in whatever speed the panel
    // happened to believe, which is the whole problem being fixed.
    run('spo', (api) => api.machineSetupUpdate({
      stage_pulse_offset: { ...spo, ...spoOut },
      stage_pulse_width_us: { ...(wus || {}), ...wOut },
    }, false, true));
  };

  // Fine adjust on the focused field. Position steps in ticks (1 tick = 12.6um,
  // the resolution that matters); width steps in microseconds, where 1us is
  // meaningless and 10us is not.
  const nudge = (d) => {
    if (!sel) return;
    const st = STATIONS.find((x) => x.key === sel.key);
    const field = sel.which === 'pos' ? st.on : '_w_' + st.key;
    const floor = sel.which === 'pos' ? 0 : 1;
    if (sel.which === 'w') d *= 10;
    const cur = Number(spoEdit[field]);
    if (!isFinite(cur)) return;
    setSpoEdit({ ...spoEdit, [field]: String(Math.max(floor, cur + d)) });
    setNudged(true);
  };

  useEffect(() => {
    if (!nudged) return;
    setNudged(false);
    commitSpo();
  }, [nudged, spoEdit]);

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
            {runRefused ? (
              <div style={{ fontSize: 11, color: '#c33' }}>⚠ {runRefused}</div>
            ) : null}
          </div>
          <div style={{ flex: '1 1 200px', minWidth: 190 }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 11, ...dim }}>
              <span>轉速</span>
              <span>
                {/* Shown for the SLIDER position, not the running speed, so the
                    consequence is visible before the plate moves.
                    "0.01mm =" is the exposure that fits inside the 0.01mm
                    width-measurement budget -- compare it against the CAMERA's
                    ExposureTime, not against any pulse width here. Smear is
                    speed x ExposureTime; the trigger pulse only has to be long
                    enough to fire the camera and light it. */}
                {speed > 0
                  ? `${plateRpm(speed).toFixed(1)} rpm · ${plateMmS(speed).toFixed(0)} mm/s · 0.01mm 需曝光 ≤ ${(10000 / plateMmS(speed)).toFixed(0)}µs`
                  : '停止'}
              </span>
            </div>
            <Slider
              min={0} max={SPEED_MAX} step={SPEED_STEP}
              value={speed ?? 0}
              marks={SPEED_MARKS}
              tooltipVisible={false}
              onChange={setSpeed}
              onAfterChange={(v) => { rememberSpeed(v); return run('freq', (api) =>
                api.machineSetupUpdate({ plate_freq: v }, false, true)); }}
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
      {/* The camera-trigger slider card that used to live here is gone: the
          station table below now edits the same two numbers (position and
          width) for every station, and two controls writing one pair of
          offsets meant the slider silently undid any L1A lead set in the
          table. One editor, one truth. */}

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
              <Why>每次配對離時鐘模型預測位置的距離。這不是量測誤差,是追蹤時鐘漂移的
                EWMA 的「落後」,大小取決於兩次配對間隔:落後 ≈ 漂移率 × 估計值年齡。
                單顆料低轉速時最糟(間隔最大、樣本最少)。</Why></span>
            {pairing.ewma_gain !== undefined && (
              <span>增益 {Number(pairing.ewma_gain).toFixed(2)}
                <Why>時間相關的 EWMA 增益 a = T/(T+0.65s)。~0.05 = 滿盤;
                  ~0.9 = 估計值剛從一段空白裡被重新錨定。</Why></span>
            )}
            <span>殘差 最大 {Math.round(pairing.resid_max_us)} µs</span>
            <span>宣告最晚 {Number(pairing.trig_wait_max_ms).toFixed(0)} ms</span>
          </div>
        )}
        {/* Two populations feed one estimate: clock-sync pulses (fired directly
            by calFireNow) and real parts (scheduled through the stage ISR). A
            standing difference between their means means the estimate is being
            pulled between two clusters and no single gain is right for both. */}
        {pairing.resid_real_n > 0 && pairing.resid_sync_n > 0 && (
          <div style={{ display: 'flex', gap: 14, flexWrap: 'wrap', ...dim }}>
            <span>殘差·真實料 平均 {Math.round(pairing.resid_real_avg_us)} µs
              <span style={dim}> (n={pairing.resid_real_n})</span></span>
            <span>殘差·直接驅動 平均 {Math.round(pairing.resid_sync_avg_us)} µs
              <span style={dim}> (n={pairing.resid_sync_n})</span></span>
            <span style={{ color: Math.abs(pairing.resid_real_avg_us - pairing.resid_sync_avg_us) > 200 ? '#c33' : '#888' }}>
              差 {Math.round(pairing.resid_real_avg_us - pairing.resid_sync_avg_us)} µs
              <Why>兩者走不同的程式路徑:校時脈衝和 keep-warm 心跳由主迴圈直接驅動腳位;
                真實料的觸發排進 stage 佇列、由步進 ISR 發出。若這個差是穩定的常數,
                時鐘估計值就是被兩個母體拉扯,任何單一增益對兩邊都不對 ——
                那是要補償的量,不是雜訊。</Why>
            </span>
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

      {/* Every station, expressed the way it is actually adjusted: WHERE it
          fires and HOW LONG it stays on. on/off is how the firmware stores it,
          but nobody thinks "move off to 672" -- they think "make the window
          wider". Width edits keep the position fixed and move `off`. */}
      <Card size="small" style={{ marginBottom: 8 }} title={<span>站點時序
        <Why>位置 = 從閘門登記算起走了多少 tick,1 tick = {MM_PER_PULSE.toFixed(4)} mm,
          與轉速無關。寬度 = 該站點持續開啟的 tick 數。括號中的時間是
          {isRef(plate_freq) ? `plate_freq ${REF_FREQ} 參考值` : '目前轉速'}下換算的。
          改完要按「存入 NVS」才會在重開機後存活。</Why></span>}>

        {/* One fine-adjust bar for the whole table, acting on whichever field
            was last focused. A slider cannot land on a single tick and one tick
            is 12.6um, which is the resolution this actually needs; per-row
            buttons would be 48 buttons. */}
        <div style={{ display: 'flex', gap: 4, alignItems: 'center', marginBottom: 8,
          padding: '4px 6px', borderRadius: 4,
          background: sel ? '#f0f6ff' : '#fafafa',
          border: sel ? '1px solid #91caff' : '1px solid #eee' }}>
          {[-100, -10, -1, 1, 10, 100].map((d) => (
            <Button key={d} size="small" disabled={!sel} style={{ padding: '0 7px' }}
              onMouseDown={(e) => e.preventDefault()}   /* keep focus on the field */
              onClick={() => nudge(d)}
            >{d > 0 ? `+${d}` : d}</Button>
          ))}
          <span style={{ fontSize: 11, marginLeft: 6, color: sel ? '#1677ff' : '#aaa' }}>
            {sel ? `${sel.label} · ${sel.which === 'pos' ? '位置 (tick)' : '寬度 (µs, 每格 ×10)'}` : '點一個欄位再用快速鈕'}
          </span>
        </div>

        <div style={{ display: 'flex', fontSize: 11, ...dim, padding: '0 0 4px 0' }}>
          <span style={{ width: 104 }}>站點</span>
          <span style={{ width: 86 }}>觸發位置</span>
          <span style={{ width: 86 }}>寬度 (µs)</span>
          <span style={{ flex: 1 }}>換算</span>
        </div>

        {STATIONS.map((st) => {
          const pos = spoEdit[st.on];
          const wid = st.off ? spoEdit['_w_' + st.key] : undefined;
          const bad = st.off && !(Number(wid) > 0);
          const isSel = (which) => sel && sel.key === st.key && sel.which === which;
          const cell = (which, val, onCh) => (
            <Input size="small" style={{ width: 82, marginRight: 4,
              borderColor: isSel(which) ? '#1677ff' : undefined }}
              value={val ?? ''}
              onFocus={() => setSel({ key: st.key, label: st.label, which })}
              onChange={(e) => onCh(e.target.value.replace(/[^0-9]/g, ''))}
              onPressEnter={commitSpo} onBlur={commitSpo} />
          );
          return (
            <div key={st.key} style={{ display: 'flex', alignItems: 'center', padding: '2px 0' }}>
              <span style={{ width: 104, fontSize: 12 }}>
                {st.label}{st.why ? <Why>{st.why}</Why> : null}
              </span>
              {cell('pos', pos, (v) => setSpoEdit({ ...spoEdit, [st.on]: v }))}
              {st.off
                ? cell('w', wid, (v) => setSpoEdit({ ...spoEdit, ['_w_' + st.key]: v }))
                : <span style={{ width: 86 }} />}
              <span style={{ flex: 1, fontSize: 11, color: bad ? '#c33' : '#888' }}>
                {Number(pos) >= 0
                  ? `${(Number(pos) * MM_PER_PULSE).toFixed(1)} mm · ${fmtMs(ticksToMs(Number(pos), refFreq(plate_freq)))}`
                  : '—'}
                {st.off ? (bad
                  ? '  ⚠ 寬度必須 > 0'
                  : `  → ${Math.ceil(Number(wid) * 2 * setpoint_freq / 1e6)} t = ${(Number(wid) * 2 * setpoint_freq / 1e6 * MM_PER_PULSE).toFixed(2)} mm${cfg.plate_freq > 0 ? '' : ' ⚠ 轉速為 0,裝置要等設定轉速後才換算'}`) : ''}
              </span>
            </div>
          );
        })}

        {/* The one cross-station rule the machine actually enforces: a verdict
            that arrives after SWITCH is no verdict at all. */}
        {spoEdit.SWITCH !== undefined && lat.max_us > 0 && (
          <div style={{ fontSize: 11, marginTop: 6,
            color: ticksToMs(Number(spoEdit.SWITCH), refFreq(plate_freq)) * 1000 < lat.max_us * 1.5
                   ? '#c33' : '#888' }}>
            SWITCH 期限 {fmtMs(ticksToMs(Number(spoEdit.SWITCH), refFreq(plate_freq)))} vs
            {' '}回報延遲最大 {(lat.max_us / 1000).toFixed(0)} ms
            {ticksToMs(Number(spoEdit.SWITCH), refFreq(plate_freq)) * 1000 < lat.max_us * 1.5
              ? ' ← 餘裕不足 1.5x,慢的那幾顆會來不及判定' : ''}
          </div>
        )}

        <div style={{ display: 'flex', gap: 6, alignItems: 'center', marginTop: 10 }}>
          <Button size="small" type="primary" loading={busy === 'save'}
            onClick={() => run('save', (api) => api.saveSetupToDevice())}
          >存入 NVS</Button>
          <Button size="small" onClick={() => setSpoEdit(spoToEdit(spo, wus, setpoint_freq))}>還原成裝置目前值</Button>
          <span style={{ ...dim, fontSize: 11 }}>
            編輯後離開欄位即套用(下一顆料起);存 NVS 才會撐過重開機
          </span>
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

// ---------------------------------------------------------------------------
// Sidebar strip for the inspection screen.
//
// The full panel above is a setup tool -- it lives in a modal and nobody wants
// it open while parts are running. This is what you watch instead: whether the
// machine is running, how fast, and the six counts. Nothing here is editable.
//
// It does NOT poll on its own if something else already is.
//
// getRunningStat() dispatches its reply into redux, so when the modal is open
// this strip is fed for free. Only when the reply stops changing does it start
// asking. That matters: one get_running_stat reply is ~1.6kB, so a second
// unconditional 1Hz poller would take another ~7% of the wire at 230400 from
// cam_trig announcements that are already measured arriving late.
//
// The link moved 115200 -> 230400 on 2026-08-07 and this reply got much faster
// than the baud change alone explains: 1518 ms -> 66 ms end to end, flat under
// 30 Hz load. The guard stays regardless -- it costs nothing and the reason it
// was added (a reply slower than the poll period collapses the link) is a
// property of blind polling, not of any particular speed.
export function UINSP_ESP32_MINI() {
  const dispatch = useDispatch();
  const API_ID = useSelector((s) => s.ConnInfo.uInspESP32_API_ID);
  const CONN = useSelector((s) => s.ConnInfo.uInspESP32_API_ID_CONN_INFO);
  const withApi = (cb) => dispatch(UIAct.EV_WS_GET_OBJ(API_ID, cb));

  const stat = GetObjElement(CONN, ['runningStat']);
  const cfg = GetObjElement(CONN, ['machineSetup']) || {};
  const seenRef = useRef({ obj: null, at: 0 });
  const mounted = useRef(true);
  const [busy, setBusy] = useState(false);
  const [why, setWhy] = useState('');
  const [rpmSel, setRpmSel] = useState(undefined);   // slider position, in rpm

  // The last speed this machine was actually seen running at.
  //
  // STOP writes plate_freq 0, so by the time someone presses RUN again the
  // configured speed is gone. Falling back to a compiled default would mean a
  // sidebar button that can spin a plate from 4.5rpm to 30rpm because the
  // operator stopped it once -- so it remembers instead, and REFUSES if it has
  // never seen a speed rather than inventing one.
  const lastSpeedRef = useRef(recallSpeed());
  const seenSpeed = (stat && stat.plate_freq > 0) ? stat.plate_freq
                  : (cfg.plate_freq > 0 ? cfg.plate_freq : 0);
  if (seenSpeed > 0 && seenSpeed !== lastSpeedRef.current) {
    lastSpeedRef.current = seenSpeed;
    rememberSpeed(seenSpeed);
  }

  useEffect(() => {
    mounted.current = true;
    let sentAt = 0;
    const tick = () => {
      const now = Date.now();
      if (stat !== seenRef.current.obj) { seenRef.current = { obj: stat, at: now }; return; }
      if (now - seenRef.current.at < 2500) return;      // somebody else is polling
      if (sentAt && now - sentAt < 5000) return;        // one in flight
      sentAt = now;
      withApi((api) => {
        if (!api || typeof api.getRunningStat !== 'function') { sentAt = 0; return; }
        api.getRunningStat().then(() => { sentAt = 0; })
          .catch((e) => { sentAt = 0; log.warn('[uinsp2mini] poll failed', e); });
      });
    };
    const h = setInterval(tick, 1000);
    return () => { mounted.current = false; clearInterval(h); };
  }, [API_ID, stat]);

  if (!CONN) return null;

  const st = stat ? stat.state : undefined;
  const cnt = (stat && stat.count) || {};
  const gate = stat ? stat.gate : undefined;
  const inError = st === 112 || st === 113;
  const starting = st === 102 || st === 103 || st === 104;
  const running = st === 101;
  const rpm = plateRpm(stat ? stat.plate_freq : cfg.plate_freq);
  const label = inError ? 'ERROR' : starting ? '啟動中' : (running && rpm > 0) ? 'RUN' : 'STOP';
  // While dragging, show the slider. Otherwise show the machine -- running
  // speed if it is turning, else the speed the next press would use.
  const rpmShown = rpmSel !== undefined ? rpmSel
                 : (rpm > 0 ? rpm : plateRpm(lastSpeedRef.current));
  const color = inError ? '#c33' : label === 'RUN' ? '#389e0d' : starting ? '#d48806' : '#888';

  // What the machine is doing, in one phrase. Kept next to the state word
  // rather than on its own line -- it is the same sentence, and it was costing
  // a whole row to say the second half of it.
  const doing = inError ? '需先在設定面板清除'
    : st === 102 ? '校時中,盤故意停著'
    : st === 103 ? '加速中'
    : st === 104 ? '空檔重新校時'
    : running ? '檢測中'
    : lastSpeedRef.current > 0 ? `按下即以 ${plateRpm(lastSpeedRef.current).toFixed(1)} rpm 啟動`
    : '尚無轉速,請先在設定面板設定';

  // Six counts on ONE row. They wrapped to two before because every cell
  // demanded a fixed 44px and the labels run to five characters; flex:1 with
  // minWidth:0 lets them share whatever the sidebar gives instead. The numbers
  // stay at a size you can read across the machine -- that is the one thing
  // not to trade away here.
  const cell = (name, v, warn) => (
    <div style={{ flex: 1, minWidth: 0 }} key={name}>
      <div style={{ fontSize: 9, color: '#888', whiteSpace: 'nowrap', lineHeight: 1.1 }}>{name}</div>
      <div style={{ fontSize: 15, fontWeight: 600, lineHeight: 1.15,
                    color: warn && v > 0 ? warn : undefined }}>
        {v ?? '—'}
      </div>
    </div>
  );

  return (
    <div style={{ margin: '2px 12px 6px 12px', textAlign: 'left' }}>
      {/* One button, and it says what pressing it DOES -- not what the machine
          currently is. A switch shows state and leaves the action implied,
          which is the wrong way round for something that starts a spinning
          plate. The state goes underneath, in small text, where it is read and
          not pressed. */}
      <Button
        block
        size="large"
        type={running || starting ? 'default' : 'primary'}
        danger={running || starting}
        loading={busy}
        disabled={inError || (!running && !starting && !(lastSpeedRef.current > 0))}
        onClick={() => {
          const on = !(running || starting);
          setBusy(true);
          dispatch(UIAct.EV_WS_GET_OBJ(API_ID, (api) => {
            if (!api) { setBusy(false); return; }
            runSequence(api, on, lastSpeedRef.current)
              .then((w) => { if (mounted.current) setWhy(w || ''); })
              .catch((e) => { if (mounted.current) setWhy(String(e)); })
              .then(() => { if (mounted.current) setBusy(false); });
          }));
        }}
        style={{ height: 34, fontSize: 15, fontWeight: 700, marginBottom: 3 }}
      >{running || starting ? '停止' : '啟動'}</Button>

      {/* State, speed, feed and what the machine is doing -- the things the
          button deliberately does not say -- on one wrapping line. */}
      <div style={{ fontSize: 11, lineHeight: 1.35, marginBottom: 3, whiteSpace: 'normal' }}>
        <b style={{ color }}>{label}</b>
        <span style={{ color: '#888' }}>
          {' · '}{rpm > 0 ? `${rpm.toFixed(1)} rpm` : '盤停止'}
          {gate ? ` · 進料 ${gate.accept}` : ''}
          {' · '}
        </span>
        <span style={{ color: inError ? '#c33' : starting ? '#d48806' : '#888' }}>{doing}</span>
      </div>
      {/* Speed, in the unit an operator thinks in.
          Applied LIVE while the machine is running; while it is stopped this
          only remembers. That asymmetry is not a nicety -- SYS_STATE::IDLE
          copies PLATE_FREQ_SETPOINT into TARGET every pass, so writing a
          non-zero plate_freq to a stopped machine STARTS THE PLATE. A speed
          slider must never be a start button. */}
      <div>
        <div style={{ display: 'flex', justifyContent: 'space-between',
                      fontSize: 10, lineHeight: 1.2, color: '#888' }}>
          <span>轉速</span>
          <span>
            {rpmShown > 0
              ? `${rpmShown.toFixed(1)} rpm · ${plateMmS(rpmToFreq(rpmShown)).toFixed(0)} mm/s`
              : '未設定'}
            {!(running || starting) && rpmShown > 0 ? ' (啟動時套用)' : ''}
          </span>
        </div>
        <Slider
          min={0} max={40} step={0.5}
          value={rpmShown}
          tooltipVisible={false}
          onChange={setRpmSel}
          onAfterChange={(v) => {
            const pf = rpmToFreq(v);
            if (!(pf > 0)) return;
            rememberSpeed(pf);
            lastSpeedRef.current = pf;
            // Only push it to the device if the plate is already turning.
            if (running || starting) {
              dispatch(UIAct.EV_WS_GET_OBJ(API_ID, (api) => {
                if (api) api.machineSetupUpdate({ plate_freq: pf }, false, true);
              }));
            }
          }}
          style={{ margin: '2px 6px 4px' }}
        />
      </div>
      {why ? <div style={{ fontSize: 11, color: '#c33', marginBottom: 3 }}>⚠ {why}</div> : null}
      {/* error_hist is the one thing here that must never be quiet -- it keeps
          its own line per error even though everything around it got tighter */}
      {stat && stat.error_hist && stat.error_hist.length > 0 && (
        <div style={{ fontSize: 11, lineHeight: 1.3, color: '#c33', marginBottom: 3 }}>
          {stat.error_hist.map((e, i) => <div key={i}>⚠ {errName(e)}</div>)}
        </div>
      )}
      <div style={{ display: 'flex', gap: 4 }}>
        {cell('SEL1', cnt.SEL1)}
        {cell('SEL2', cnt.SEL2)}
        {cell('SEL3', cnt.SEL3)}
        {cell('NA', cnt.NA)}
        {cell('SKIP', cnt.SKIP, '#c60')}
        {cell('UNANS', cnt.UNANSWERED, '#c33')}
      </div>
    </div>
  );
}
