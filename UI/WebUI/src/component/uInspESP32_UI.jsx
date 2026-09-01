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
import { PerifStatusPanel } from '../perif/PerifStatus';
import { usePerifConn, getPerifAPI, perifGetObj } from '../perif/PerifAPI';
import { useSelector, useDispatch } from 'react-redux';
import Button from 'antd/lib/button';
import Input from 'antd/lib/input';
import InputNumber from 'antd/lib/input-number';
import Tag from 'antd/lib/tag';
import Card from 'antd/lib/card';
import Collapse from 'antd/lib/collapse';
import Divider from 'antd/lib/divider';
import Modal from 'antd/lib/modal';
import Slider from 'antd/lib/slider';
import Switch from 'antd/lib/switch';
import Radio from 'antd/lib/radio';
import Tooltip from 'antd/lib/tooltip';
import Popover from 'antd/lib/popover';
import CameraOutlined from '@ant-design/icons/CameraOutlined';
import ReloadOutlined from '@ant-design/icons/ReloadOutlined';
import CaretRightOutlined from '@ant-design/icons/CaretRightOutlined';
import HistoryOutlined from '@ant-design/icons/HistoryOutlined';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import { GetObjElement } from 'UTIL/MISC_Util';
import { mkLog } from 'UTIL/logger';
import { compactN, exactN } from '../perif/fmt.mjs';
import message from 'antd/lib/message';
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
// What the run control starts at when nothing else has supplied a speed. The
// stop path zeroes the plate setpoint (see runSequence), so this is the normal
// case after any stop -- not an edge case.
const DEFAULT_START_FREQ = 8000;

// Matches LIGHT_HOLD_MAX_MS in the firmware, which clamps anything larger.
// Two minutes is not enough to set exposure/gain/focus without the light
// dying mid-adjustment; five is the most the board will hold.
// Take one lit frame using the BOARD's trigger, with the camera armed for it.
//
// The board puts its pulse on the camera's LINE0. The camera only listens to
// that line in TriggerMode(2), and the core switches to 2 solely when an
// inspection session starts -- outside one it sits on TriggerMode(1), software
// source. So the pulse went out, nothing listened, and there was no frame and no
// error anywhere: the core log did not even show an attempt.
//
// The other route -- asking the core to grab via EX, the way 檢測快照 does --
// does not work from here and cannot: EX needs a registered CI session with
// frames already flowing (wiringPanel: "Caller must have CI registered and
// trigger_mode:0"), and this panel has none. Measured on the bench: board
// trigger produced an image, core trigger produced nothing.
//
// The restore runs on BOTH paths, and treats a refusal as done. A camera left
// armed for a hardware line nobody drives is worse than a failed snapshot --
// everything that expects free-run then silently has no frames.
function snapWithBoardTrigger(api, dispatch, CORE_ID) {
  // ONE REQUEST, AND THE CORE OWNS THE CAMERA.
  //
  // This used to run the sequence itself: ST trigger_mode 2, ask the board to
  // flash and pulse, ST trigger_mode back. The restore was a hardcoded 1, and
  // mode 1 is the mode that DISCARDS frames -- so pressed from inside a running
  // inspection it dropped the camera out of the session's mode into the one
  // that throws every frame away. The board kept flashing, no image arrived,
  // and the clock calibration could not finish because it needs one frame per
  // sync pulse. From the line: "the light flashes but nothing comes in, and
  // refreshing the page fixes it" -- refreshing re-enters the session, which
  // re-issues the right mode.
  //
  // The panel could not have got this right. Which mode to restore depends on
  // whether a session is running and which kind, and this panel does not know
  // that -- the core does. So the panel now asks for what it wants, a lit
  // frame, and the core saves the mode, switches, drives the board and puts it
  // back on a deadline (see cam_snap_lit in wiringPanel).
  return new Promise((resolve, reject) => {
    dispatch(UIAct.EV_WS_SEND_BPG(CORE_ID, 'SC', 0, { type: 'cam_snap_lit' },
      undefined, {
        resolve: (pkts) => {
          const p = (pkts || []).find((q) => q && q.data && q.data.type === 'cam_snap_lit');
          const d = p && p.data;
          // An older core does not know this command and answers nothing that
          // matches. Say so instead of reporting a snapshot that never happened.
          if (!d) { reject(new Error('這個核心版本沒有 cam_snap_lit(需要更新 core)')); return; }
          if (d.err) { reject(new Error(d.err)); return; }
          resolve(d);
        },
        reject: (e) => reject(e instanceof Error ? e : new Error(String(e))),
      }));
  });
}

const LIGHT_HOLD_MS = 300000;
const refFreq = (plate_freq) => (plate_freq > 0 ? plate_freq : REF_FREQ);
const isRef = (plate_freq) => !(plate_freq > 0);

// Plate geometry, must match _PLAT_PULSE_PER_TURN in the firmware.
//
// 70400, MEASURED 2026-08-12 -- 2816001 ticks over 40 revolutions, one tick of
// residual, and 70400/2 = 35200 steps = 3200 microsteps x 11:1. The 60000 that
// stood here (and in the firmware) was a rough estimate 17.3% out, so every mm
// and every rpm this panel showed was wrong by that much.
//
// The 240mm is still an assumption: parts do not ride at the plate's rim, so
// mm-per-pulse for a PART is smaller than this. The pulse count is exact now;
// the millimetres are as good as that diameter. The stage timer ticks at 2x plate_freq, which makes
// plate_freq the only speed the board exposes -- and a bare "15000" tells an
// operator nothing. rpm and mm/s are the same fact in units someone can act on,
// and mm/s is the one that sets the exposure budget: 0.01 mm of smear at
// 377 mm/s is 26 us, which is a decision, not a statistic.
const PULSES_PER_REV = 70400;
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
  // blow: the window may be CENTRED on its position instead of starting at it.
  // Only the nozzles get this. A blow has to be open before the part arrives
  // (solenoid travel + air transit) and stay open after it, so its position
  // wants to mean "where the part is" -- geometry, fixed -- while the margins
  // either side are pneumatics and get retuned with air pressure. Forward-only
  // forces both into one number. The camera does not have that problem: its
  // trigger edge IS the event and everything after it is exposure.
  { key: 'SEL1', label: 'SEL1 吹氣', on: 'SEL1_on', off: 'SEL1_off', us: 'SEL1', blow: true },
  { key: 'SEL2', label: 'SEL2 吹氣', on: 'SEL2_on', off: 'SEL2_off', us: 'SEL2', blow: true },
  { key: 'SEL3', label: 'SEL3 吹氣', on: 'SEL3_on', off: 'SEL3_off', us: 'SEL3', blow: true },
];

// The firmware stores on/off; the panel edits position/width. Kept as an
// explicit pair of converters rather than done inline, because a half-typed
// width must not be able to corrupt `off` on the device -- the conversion only
// happens on commit.
const spoToEdit = (spo, wus, plate_freq, ctr) => {
  const e = { ...spo };
  for (const st of STATIONS) {
    if (!st.off) continue;
    // A centred station's *_on is DERIVED by the device, so the field has to
    // show the centre instead -- otherwise editing the position would read
    // back the leading edge and walk the window backwards on every commit.
    if (st.blow && ctr && Number(ctr[st.us]) > 0) e[st.on] = Number(ctr[st.us]);
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

// --- statistics history ------------------------------------------------------
//
// The device's counters are cumulative since the last reset and it keeps no
// history of its own, so zeroing them used to destroy the only copy of what a
// batch did. Every reset now takes a snapshot first: the snapshot IS the batch.
//
// It lives in localStorage, i.e. in this browser, not in the machine. That is
// honest about what it is -- an operator's notepad, not a production record --
// and it is why it is capped at 20 rather than kept forever.
const HIST_KEY = 'visSele.uinsp.statHist.v1';
const HIST_MAX = 20;
const recallHist = () => {
  try {
    const v = JSON.parse(window.localStorage.getItem(HIST_KEY) || '[]');
    return Array.isArray(v) ? v : [];
  } catch (e) { return []; }
};
const storeHist = (list) => {
  try { window.localStorage.setItem(HIST_KEY, JSON.stringify(list.slice(0, HIST_MAX))); }
  catch (e) { /* private mode / quota */ }
  return list.slice(0, HIST_MAX);
};
// Local time, because it is read next to a wall clock and a shift log.
const fmtWhen = (t) => {
  const d = new Date(t);
  const p = (n) => String(n).padStart(2, '0');
  return `${p(d.getMonth() + 1)}/${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}`;
};
// One row of the history table, and the current-counts row above it -- same
// columns, because the point of showing "now" there is that you can read it
// against the rows below without translating.
// Column widths for the reset-history table, in ONE place.
//
// They were five copies of 52/60/96 across four rows, which is four chances for
// the header to stop lining up with the body. The numeric columns are wide
// enough for a grouped nine-digit count ("1,982,530") at 12px, because these
// cells now print counts in full -- see exactN.
const HIST_W = { label: 88, n: 78, feed: 84 };
// The machine at a glance, one tap away from the strip.
//
// The strip abbreviates on purpose (see compactN): five characters is what
// keeps three counts legible in a third of a sidebar each. What was missing was
// anywhere to read the exact digits -- they were on `title`, which a mouse can
// reach and a finger cannot, and this panel runs on a touchscreen.
//
// Having opened a surface with room on it, the counts are not the only thing
// worth putting there. The numbers below are the ones that let someone standing
// at the machine answer "is it healthy" without opening three panels, and they
// are grouped by the question they answer rather than by where they came from.
//
// The one worth explaining is 判定期限. A part is photographed at CAM1_on and
// must have a verdict by the time it reaches SWITCH; that gap is the whole
// budget the inspection gets, and the report latency is what it actually
// spends. When the margin between them goes to zero, parts start arriving at
// SWITCH undecided -- which is device error 2 and the SKIP counter, and by then
// it is being read as a sorting fault rather than as the machine simply running
// out of time. Stated as a margin, it is a number that can be watched BEFORE it
// becomes a defect.
function CountsBubble({ cnt, gate, selOK, selNG, rate, stat, cfg, onResetStat, statSince }) {
  const n0v = (v) => (typeof v === 'number' && isFinite(v) ? v : 0);
  const has = (v) => typeof v === 'number' && isFinite(v);

  const rows = selOK && selNG
    ? [['NG', n0v(cnt[selNG]), '#c33', selNG],
       ['OK', n0v(cnt[selOK]), '#389e0d', selOK]]
    : [['SEL1', n0v(cnt.SEL1), undefined, 'SEL1'],
       ['SEL2', n0v(cnt.SEL2), undefined, 'SEL2'],
       ['SEL3', n0v(cnt.SEL3), undefined, 'SEL3']];
  rows.push(['NA', n0v(cnt.NA), undefined, 'NA']);
  const judged = rows.reduce((a, [, v]) => a + v, 0);

  const lat  = (stat && stat.report_latency) || {};
  const pipe = (stat && stat.pipe) || {};
  // WHERE THE PAIRING ACTUALLY HAPPENS.
  //
  // This section used to read matched / pending / drops / offset_ms out of the
  // core's perif_pairing. The core does not have them any more and has not for
  // a while: the frame<->object match moved into the firmware, which is the
  // side that fired the trigger and therefore knows the object and the instant.
  // The core's item still exists and still carries link health, the verdict
  // path and the latency histogram -- so nothing threw, every field simply read
  // undefined, n0v turned that into 0, and the panel reported perfect silence.
  //
  // cam_sync is the board's own account of the same thing, and this strip is
  // already polling it in get_running_stat.
  const sync = (stat && stat.cam_sync) || null;
  const health = (stat && stat.health) || {};
  // FLAT NAMES. cfg is the FLATTENED setup (machineSetupReSync runs it through
  // uinspFlatten before publishing), so cfg.skip_policy does not exist -- the
  // keys are unanswered_stop_after and nomatch_stop_after. Reading the grouped
  // shape gave undefined, and the panel rendered "連續 0 / —" for both
  // thresholds: the live count with no bound to read it against, which is the
  // state this row was added to end. Seen on the machine, not in review.
  // (skip_policy_mode a few hundred lines below already used the flat name --
  // one file, two conventions, and only one of them was right.)
  const skipStop = cfg && cfg.unanswered_stop_after;
  const nomatchStop = cfg && cfg.nomatch_stop_after;
  const hz   = n0v(cfg && cfg.plate_freq);
  const off  = (cfg && cfg.stage_pulse_offset) || {};
  // Budget = camera to deadline. Both are stage-timer ticks at 2x plate_freq.
  const budgetMs = (hz > 0 && has(off.SWITCH) && has(off.CAM1_on))
    ? ticksToMs(off.SWITCH - off.CAM1_on, hz) : NaN;
  // TWO MARGINS, because one of them is a latch.
  //
  // This showed budget minus the SLOWEST report ever seen, and called it 餘裕.
  // One slow reply -- and the first frame after a def build or a shape train is
  // always slow -- pins it negative for the rest of the session, whatever the
  // machine is doing now. Seen tonight: 643ms budget, 491.0 average, 6682.8
  // worst, margin -6040 and red, while the average had +152ms to spare.
  //
  // The worst case is still the one that produces a defect, so it stays. It
  // just cannot be the only number, or "how is it running" has no answer on
  // this panel. Average first, worst second.
  //
  // AND cam_*, not the bare pair, because they are measured from different
  // instants. The firmware keeps two clocks on every report and says so at the
  // serialiser: avg_us/max_us run from trig_us, "Registration wall time ... for
  // the gate->report latency stat" -- the GATE. cam_avg_us/cam_max_us run from
  // cam_us, and carry the comment "From the camera trigger: the electronics
  // alone, directly comparable to the CAM->SWITCH budget. avg_us above is NOT."
  //
  // This panel read the gate pair under a heading that says 相機 → SWITCH and
  // subtracted it from a budget measured CAM1_on -> SWITCH. The difference is
  // the part's ride from the gate to the camera, which is MECHANICAL: at 10000
  // (tick rate 20000/s) and CAM1_on at 8010 ticks it is ~400ms, and it DOUBLES
  // when the plate is slowed down. So the margin got worse the more you backed
  // off the speed, and the reading looked like inspection compute time getting
  // slower under load when it was the part taking longer to walk to the camera.
  // Reported from the line in exactly those words.
  //
  // Fall back to the gate pair only when the board is too old to send cam_*,
  // and label it, rather than silently showing a number that means something
  // else under the same heading.
  const camLat = has(lat.cam_avg_us);
  const avgUs = camLat ? lat.cam_avg_us : lat.avg_us;
  const maxUs = camLat ? lat.cam_max_us : lat.max_us;
  const spentMs  = has(maxUs) ? maxUs / 1000 : NaN;
  const spentAvgMs = has(avgUs) ? avgUs / 1000 : NaN;
  const marginMs = (isFinite(budgetMs) && isFinite(spentMs)) ? budgetMs - spentMs : NaN;
  const marginAvgMs = (isFinite(budgetMs) && isFinite(spentAvgMs)) ? budgetMs - spentAvgMs : NaN;

  const Row = ({ label, sub, value, color, warn }) => (
    <div style={{ display: 'flex', gap: 12, justifyContent: 'space-between' }}>
      <span style={{ color: color || undefined, fontWeight: color ? 600 : 400 }}>
        {label}{sub ? <span style={{ color: '#888', fontWeight: 400 }}> {sub}</span> : null}
      </span>
      <span style={{ fontVariantNumeric: 'tabular-nums',
                     color: warn ? '#c33' : undefined, fontWeight: warn ? 600 : undefined }}>
        {value}
      </span>
    </div>
  );
  const Head = ({ children }) => (
    <div style={{ color: '#888', fontSize: 11, marginTop: 6, borderTop: '1px solid #eee',
                  paddingTop: 3 }}>{children}</div>
  );

  return (
    // SCROLLS RATHER THAN OVERFLOWS. This grew from a strip of bin counts into
    // five sections, and a Popover does not clip -- on a short screen the last
    // sections simply render past the bottom of the window with no way to reach
    // them. The Surface Go is the machine this panel is used on, and 逾時餘量
    // and 時間配對 are at the end.
    //
    // 78vh, not a fixed pixel height: the popover is anchored to a control part
    // way down the page, so what is left below it is a fraction of the window,
    // not a constant.
    <div data-testid="uinsp-counts-bubble"
         style={{ fontSize: 13, lineHeight: 1.8, minWidth: 240,
                  maxHeight: '78vh', overflowY: 'auto', overflowX: 'hidden',
                  // Room for the scrollbar so it never sits on top of the
                  // numbers it is scrolling.
                  paddingRight: 4 }}>
      {rows.map(([name, v, color, sel]) => (
        <div key={name} data-bin={name} data-value={v}>
          <Row label={name} sub={sel !== name ? sel : ''} value={exactN(v)} color={color} />
        </div>
      ))}
      <Row label="已判定合計" value={exactN(judged)} />
      {gate && has(gate.accept)
        ? <Row label="進料" sub="gate" value={exactN(gate.accept)} /> : null}

      <Head>速度</Head>
      {rate ? (<>
        <Row label="進料" value={`${rate.g.toFixed(1)} /s`} />
        <Row label="檢測" value={`${rate.i.toFixed(1)} /s`} />
        <Row label="OK"   value={`${rate.o.toFixed(1)} /s`} color="#389e0d" />
      </>) : <Row label="—" value="尚未取樣" />}
      {hz > 0 ? <Row label="轉速"
                     value={`${plateRpm(hz).toFixed(1)} rpm · ${plateMmS(hz).toFixed(0)} mm/s`} /> : null}

      <Head>判定期限（相機 → SWITCH）{camLat ? '' : ' · 舊韌體:以閘門起算'}</Head>
      <Row label="可用時間" value={isFinite(budgetMs) ? `${budgetMs.toFixed(0)} ms` : '—'} />
      <Row label="回報 平均 / 最慢"
           value={has(avgUs) ? `${(avgUs/1000).toFixed(1)} / ${(maxUs/1000).toFixed(1)} ms` : '—'} />
      {/* The gate pair, kept but named for what it is. It includes the ride
          from the gate to the camera, so it grows when the plate slows down --
          useful as a total dwell, useless as a budget. */}
      {camLat && has(lat.avg_us)
        ? <Row label="含進料段" sub="閘門起算"
               value={`${(lat.avg_us/1000).toFixed(1)} / ${(lat.max_us/1000).toFixed(1)} ms`} />
        : null}
      {/* Red once the slowest report has eaten the budget: past zero, a part
          reaches SWITCH with nothing decided about it. */}
      <Row label="餘裕 平均 / 最慢"
           value={isFinite(marginAvgMs) || isFinite(marginMs)
             ? `${isFinite(marginAvgMs) ? marginAvgMs.toFixed(0) : '—'} / `
               + `${isFinite(marginMs) ? marginMs.toFixed(0) : '—'} ms`
             : '—'}
           warn={isFinite(marginAvgMs) && marginAvgMs <= 0} />
      {/* THE TWO WAYS RUNNING OUT OF TIME ACTUALLY SHOWS UP, and how much room
          is left before either stops the line. A late verdict lands on one side
          or the other depending on whether its object was still there: past
          SWITCH it is an unjudged part (device error 2, the unanswered counter);
          past the sweep the object is gone and the report matches nothing.
          Both are bounded by a consecutive count, and neither bound was visible
          anywhere -- so the first sign of either was the machine stopping. */}
      <Head>逾時餘量</Head>
      <Row label="無判決 連續 / 上限"
           value={`${n0v(health.consec_unanswered)} / ${has(skipStop) ? skipStop : '—'}`}
           warn={has(skipStop) && n0v(health.consec_unanswered) >= skipStop - 1} />
      <Row label="對不上物件 連續 / 上限"
           value={`${n0v(cnt.NOMATCH_CONSEC)} / ${has(nomatchStop) ? nomatchStop : '—'}`}
           warn={has(nomatchStop) && n0v(cnt.NOMATCH_CONSEC) >= nomatchStop - 1} />
      {/* The host-side admission layer, shown only when it is switched on. The
          average is the number that says whether the loop is settled: parked at
          the floor is the loop working; well above it with rej_load climbing is
          something else turning parts away. */}
      {has(gate && gate.proc_sep_us) && gate.proc_sep_us > 0 ? (<>
        <Row label="進料抑制 已擋" value={exactN(n0v(gate.rej_load))} />
        <Row label="濾波間隔 / 下限"
             value={`${(n0v(gate.proc_avg_us)/1000).toFixed(0)} / `
                  + `${(gate.proc_sep_us/1000).toFixed(0)} ms`} />
      </>) : null}
      {has(pipe.waiting) || has(pipe.registered)
        ? <Row label="在製 等待 / 登記"
               value={`${n0v(pipe.waiting)} / ${n0v(pipe.registered)}`} /> : null}
      {/* THE AVERAGE IS SINCE THE LAST RESET, AND THE MAXIMUM IS A HIGH-WATER.
          avg_us is REP_LAT_SUM_US / REP_LAT_N and max_us is a peak-hold, both
          running since the board booted -- so the pair can only ever climb, and
          slowing the plate down cannot bring either back. A number that only
          goes up reads as a machine that is degrading whether or not it is.
          The window has to be resettable for the reading to mean anything, and
          it has to SAY which window it is. */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 8, padding: '4px 0 2px' }}>
        <span style={{ fontSize: 11, color: '#8c8c8c', flex: '1 1 auto' }}>
          {statSince ? `統計自 ${statSince} 起` : '統計自開機起'}</span>
        <Button size="small" data-testid="uinsp-reset-latency"
                onClick={onResetStat}>重置統計</Button>
      </div>

      <Head>時間配對（裝置）</Head>
      {sync ? (<>
        {/* ZERO RESIDUAL AND NO RESIDUAL ARE NOT THE SAME READING, and this
            printed them the same way. n0v turns an absent value into 0, and 0
            is this metric's IDEAL -- so a pairing that never ran displayed as
            perfect timing. With nothing established it says so instead. */}
        <Row label="殘差 現在 / 最大"
             value={n0v(sync.established) > 0
               ? `${Math.round(n0v(sync.resid_us))} / ${Math.round(n0v(sync.resid_max_us))} µs`
               : '尚未配對'} />
        {/* The number the match window should be set against: how much of it a
            frame actually needed. Hundreds of µs against a window in the
            thousands is real margin; creeping toward the window is not. */}
        <Row label="用掉視窗 / 視窗"
             value={has(sync.window_us)
               ? `${Math.round(n0v(sync.delta_max_us))} / ${Math.round(sync.window_us)} µs`
               : '—'}
             warn={n0v(sync.window_us) > 0
                   && n0v(sync.delta_max_us) > n0v(sync.window_us) * 0.5} />
        <Row label="已配對" value={exactN(n0v(sync.established))}
             warn={n0v(sync.established) === 0} />
        {/* rejected is a frame that landed outside the window; two in a row and
            the machine stops on CAM_CLOCK_LOST. rebuilds counts the times it
            reached that. Both zero is the normal reading. */}
        <Row label="拒絕 / 重建"
             value={`${n0v(sync.rejected)} / ${n0v(sync.rebuilds)}`}
             warn={n0v(sync.rejected) > 0 || n0v(sync.rebuilds) > 0} />
        {has(sync.offset_us)
          ? <Row label="時鐘偏移" value={`${(sync.offset_us / 1000).toFixed(1)} ms`}
                 warn={sync.valid === false} /> : null}
      </>) : <Row label="—" value="裝置尚未回報" />}

      {(n0v(cnt.SKIP) > 0 || n0v(cnt.UNANSWERED) > 0) ? (<>
        <Head>未判定</Head>
        <Row label="SKIP" value={exactN(n0v(cnt.SKIP))} warn />
        <Row label="UNANS" value={exactN(n0v(cnt.UNANSWERED))} warn />
      </>) : null}
    </div>
  );
}

const histRow = { display: 'flex', alignItems: 'center', fontSize: 12,
                  padding: '3px 0', lineHeight: 1.3 };
const n0 = (v) => (typeof v === 'number' ? v : 0);
const fmtDur = (ms) => {
  if (!(ms > 0)) return '';
  const m = Math.round(ms / 60000);
  return m < 60 ? `${m} 分` : `${Math.floor(m / 60)} 時 ${m % 60} 分`;
};

// Start or stop the machine. Shared by the setup panel and the sidebar strip
// because getting this sequence subtly different in two places is how one of
// them ends up silently refusing itself.
//
// Returns a promise resolving to null on success, or a string saying why it
// declined -- callers show that, they do not swallow it.
// Exported because InspectionUI has to stop the machine on the way out, and the
// comment above is the reason it must not write its own version of this.
export function runSequence(api, on, speed) {
  if (!on) {
    api.exitInspMode();
    api.machineSetupUpdate({ plate_freq: 0 }, false, true);
    return Promise.resolve(null);
  }
  // No usable speed is no longer a refusal.
  //
  // Stopping zeroes the plate SETPOINT just above -- deliberately, because the
  // firmware re-asserts TARGET from the setpoint on every IDLE pass, so a
  // non-zero setpoint would keep the plate turning after a stop. But every
  // caller then came back to a machine with no speed, and the run control
  // refused with 「沒有可用的轉速」. Measured on the bench: run 快速驗證, leave
  // it, and full inspection could not be started at all -- which presents as
  // "the machine will not finish clock calibration", because calibration never
  // gets to begin.
  //
  // So the start path supplies one. DEFAULT_START_FREQ, not the last remembered
  // value: a machine that was being set up slowly must not leap to that speed
  // because somebody pressed start (the note above SPEED_KEY is about exactly
  // that), and a fixed, stated number is the one an operator can predict.
  // Ask the BOARD before inventing a speed.
  //
  // The first version of this defaulted straight to DEFAULT_START_FREQ, and
  // that turned a fallback into an override: anything that had set the plate
  // speed by another route -- the soak harness sets it over the board console
  // before pressing start -- was silently replaced at start. Measured: a run
  // asked for 14667 (25 rpm), the board acked it, and the machine ran the whole
  // ten minutes at 13.6 rpm because this line wrote 8000 over it.
  //
  // So the order is: what the caller asked for, then what the board already
  // holds, then the default. The default still exists -- a stop zeroes the
  // setpoint, so a machine with no speed at all is the normal case, and
  // refusing there is what blocked calibration.
  return Promise.resolve(
    (speed > 0) ? speed : api.getSetupP().then(
      (st) => ((st && st.plate_freq > 0) ? st.plate_freq : DEFAULT_START_FREQ),
      () => DEFAULT_START_FREQ)
  ).then((useSpeed) => {
  api.stepperEnable();
  api.machineSetupUpdate({ plate_freq: useSpeed }, false, true);
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
  });   // end of the resolved-speed continuation opened above
}

// A "?" that carries the explanation instead of a paragraph that carries it
// permanently. The prose is worth keeping -- most of it is a fact that cost a
// day to learn -- but it belongs one hover away, not between the operator and
// the number.
const Why = ({ children }) => (
  <Tooltip title={<div style={{ maxWidth: 320 }}>{children}</div>}>
    {/* stopPropagation because these sit inside FoldCard's clickable title.
        Reaching for the explanation of a section must not also close it. */}
    <span onClick={(e) => e.stopPropagation()}
      style={{ cursor: 'help', color: '#888', border: '1px solid #bbb',
      borderRadius: '50%', fontSize: 10, lineHeight: '14px', width: 15, height: 15,
      display: 'inline-block', textAlign: 'center', marginLeft: 6 }}>?</span>
  </Tooltip>
);

// A Card that folds.
//
// One ADVANCED collapse wrapped all seven of these, so the choice was the
// whole page or none of it -- and the page is long enough that finding the one
// section you came for meant scrolling past six you did not. antd's Card has
// no fold of its own, and rebuilding each as a Collapse.Panel would restyle
// every one, so this gates the BODY and leaves the Card exactly as it was.
//
// Closed by default: these are bring-up and diagnosis, opened deliberately.
// `defaultOpen` is for the ones worth seeing without a click.
//
// display:none rather than unmounting, so a half-typed value in a section is
// still there when it is reopened -- several of these hold working copies
// (the stage-timing table's spoDraft) that a remount would discard.
const FoldCard = ({ title, defaultOpen = false, style, extra, children }) => {
  const [open, setOpen] = useState(!!defaultOpen);
  return (
    <Card size="small" style={style} extra={extra}
      bodyStyle={open ? undefined : { display: 'none' }}
      title={
        <span onClick={() => setOpen(!open)}
              style={{ cursor: 'pointer', userSelect: 'none', display: 'flex',
                       alignItems: 'center', gap: 6 }}>
          <CaretRightOutlined rotate={open ? 90 : 0}
            style={{ fontSize: 11, opacity: 0.6, flexShrink: 0, transition: 'transform .2s' }} />
          <span style={{ flex: 1, minWidth: 0 }}>{title}</span>
        </span>
      }>
      {children}
    </Card>
  );
};

export function UINSP_ESP32_UI({ pollMs = 1000 }) {
  const dispatch = useDispatch();
  const API_ID = useSelector((s) => s.ConnInfo.uInspESP32_API_ID);
  const CONN = usePerifConn(useSelector((s) => s.ConnInfo.uInspESP32_API_ID));
  const CORE_ID = useSelector((s) => s.ConnInfo.CORE_ID);
  const withApi = (cb) => cb(getPerifAPI(API_ID));

  const [stat, setStat] = useState(undefined);
  const [busy, setBusy] = useState('');
  // Slider position. It is NOT bound to plate_freq, because RUN/STOP sets the
  // device speed to 0 -- binding them would snap the slider to zero the moment
  // the machine stops and lose the speed the operator had chosen. So the slider
  // holds the target, the device holds the actual, and the panel says when the
  // two disagree.
  const [speed, setSpeed] = useState(undefined);
  const [hzInput, setHzInput] = useState('');        // gate fire-rate cap, in parts/s
  const [procHzInput, setProcHzInput] = useState(''); // host throughput cap, in parts/s
  const [stopAfterInput, setStopAfterInput] = useState('');
  const [nomatchAfterInput, setNomatchAfterInput] = useState('');

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
  const [fw, setFw] = useState(undefined);   // board build identity; null = asked, too old to answer
  const [runRefused, setRunRefused] = useState('');   // why RUN declined, shown inline
  // Working copy of stage_pulse_offset while it is being typed into. Adopted
  // from the device once, then owned by the editor -- rebinding on every poll
  // would overwrite a half-typed number on the next tick.
  const [spoEdit, setSpoEdit] = useState({});
  // Which field the fine-adjust bar acts on: {key,label,which:'pos'|'w'}.
  const [sel, setSel] = useState(null);
  // Station timing is read far more often than it is changed, and every field
  // in it applies on blur -- so a stray click plus a keystroke moves a real
  // trigger on a running machine with no confirmation anywhere. The lock is
  // deliberately not persisted: it re-arms every time the panel is opened,
  // because "I unlocked this an hour ago" is not consent for the next edit.
  const [spoUnlock, setSpoUnlock] = useState(false);
  // Set by nudge(), consumed by the effect that pushes the change. setSpoEdit
  // is async, so committing inside nudge() would send the PREVIOUS value.
  const [nudged, setNudged] = useState(false);
  // Station placement (jog). Only the arm speed is UI state -- everything else
  // is read from the device, because the device is the one that knows where the
  // plate actually stopped.
  // 1000, not 3000. Measured 2026-08-12: the catch is where the accuracy is
  // lost, and it is lost to the COAST, which grows as f^2 (496 ticks at 1000,
  // 40530 at 9000 -- 0.68 of a revolution). Over that distance the part does
  // not stay put on the plate: driving the same offset after catching at 3000
  // vs 9000 put it 1.77 mm apart, ~141 ticks. The sensor's own lag accounts for
  // 0.13 mm of that, so the rest is the part sliding.
  //
  // The jog MOVE is speed-independent (+-6 ticks at every arm speed). Only the
  // catch degrades, so this is the one speed worth keeping low.
  const [jogArmFreq, setJogArmFreq] = useState(1000);
  // The goto target and its speed ceiling. jogGo is seeded from whichever
  // station is selected -- picking a station and then asking "where is that?"
  // is the whole workflow, so retyping the number it already knows is friction.
  const [jogGo, setJogGo] = useState(0);
  const [jogGoFreq, setJogGoFreq] = useState(600);

  const [commDiag, setCommDiag] = useState(null);
  const [pairing, setPairing] = useState(null);   // core-side frame<->object pairing health

  // Whole-config export / import. cfgBusy names the operation in flight so the
  // three buttons spin independently; cfgReport holds either an import
  // verification result or a get_schema answer, and is cleared before each run
  // so a stale report can never be read as belonging to the new one.
  const [cfgBusy, setCfgBusy] = useState(null);
  const [cfgReport, setCfgReport] = useState(null);
  const cfgFileRef = useRef(null);

  const [lightUntil, setLightUntil] = useState(0);   // epoch ms the board will auto-drop the hold
  const [snapWhy, setSnapWhy] = useState('');       // why the last plate snapshot did not happen
  const [now, setNow] = useState(Date.now());
  const mounted = useRef(true);

  const cfg = GetObjElement(CONN, ['machineSetup']) || {};
  const dev = GetObjElement(CONN, ['deviceState']) || {};
  // cfg is the board's own settings, filled only by a get_setup reply.
  // Empty means "not read", which is NOT the same as "read, and it said
  // compile defaults" -- see the tag below.
  const cfgRead = Object.keys(GetObjElement(CONN, ['machineSetup']) || {}).length > 0;
  const cfgTries = GetObjElement(CONN, ['cfgResyncTries']) || 0;
  const connected = GetObjElement(CONN, ['type']) === 'WS_CONNECTED';
  const spo = cfg.stage_pulse_offset || {};
  // Per-station widths in microseconds; {} on firmware that predates them.
  const wus = cfg.stage_pulse_width_us;
  // Window centres in ticks; 0/absent = that station keeps the forward-only
  // shape. {} on firmware that predates them, which reads as "all forward".
  const ctr = cfg.stage_pulse_center || {};
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
  // The second layer, in the same units as the first so the two can be read
  // against each other: it only ever makes sense BELOW the camera cap.
  // ONE READING, WHATEVER THE MODE. The device resolves mode + rate itself and
  // reports both, so the panel never inverts a microsecond interval to find out
  // what it is doing -- which was the one arithmetic step every caller had to
  // repeat and only had to get wrong once.
  // Layer one, read the same way as layer two: the device resolves the mode and
  // reports what is actually in force, so the panel never derives one from the
  // other. min_sep_us is what was configured; min_sep_eff_us is what the gate is
  // enforcing, and under cam_mode auto they are different numbers.
  const camMode = (gate && gate.cam_mode) || cfg.gate_cam_mode || 'manual';
  const camFps = gate && gate.cam_fps_limit > 0 ? gate.cam_fps_limit : undefined;
  const camStale = !!(gate && gate.cam_fps_stale);
  const effSepUs = gate && gate.min_sep_eff_us > 0 ? gate.min_sep_eff_us : undefined;
  const effHz = effSepUs ? 1000000 / effSepUs : undefined;
  const procMode = (gate && gate.proc_mode) || cfg.gate_proc_mode || 'off';
  const procHz = gate && gate.proc_rate_hz > 0 ? gate.proc_rate_hz : undefined;
  const procAvgHz = (gate && gate.proc_avg_us > 0)
    ? 1000000 / gate.proc_avg_us : undefined;
  // What the loop is doing, for the auto row. rho is the number that says
  // whether it has headroom; svc is what it thinks the host costs per part.
  const procRho = gate && gate.proc_rho_pct > 0 ? gate.proc_rho_pct : undefined;
  const procSvcMs = gate && gate.proc_svc_us > 0 ? gate.proc_svc_us / 1000 : undefined;
  // The window MEAN beside the median the loop sizes itself from. The distance
  // between them is the spikiness, and it is worth showing: a throttle sized
  // from a typical part and one sized from a tail behave very differently, and
  // until now nothing on this screen said which was happening.
  const procSvcMeanMs = gate && gate.proc_svc_mean_us > 0
    ? gate.proc_svc_mean_us / 1000 : undefined;
  const procCapN = (gate && gate.proc_auto_cap_n) | 0;
  // WHAT THE LOOP IS DOING, not just where it ended up. These were added to the
  // firmware and never put on screen, so the advice "watch proc_probe_up_n"
  // meant reading JSON. Separately, because together they say something a
  // single number cannot: only 探測 climbing is a loop still finding headroom,
  // only 退讓 climbing is one being pushed back, and both climbing is one
  // hunting around an edge it has already found.
  const probeUp = (gate && gate.proc_probe_up_n) | 0;
  const backoff = (gate && gate.proc_backoff_n) | 0;
  // The report latency as a RATE, so it reads against the parts/s field. cam_*
  // and not the bare pair, for the same reason the 判定期限 rows use it: the
  // gate pair carries the part's mechanical ride to the camera, which is not
  // work the host does and does not bound how fast it can be fed.
  const rlat = (stat && stat.report_latency) || {};
  // THE CONSTRAINT EVERY OTHER KNOB IN THIS CARD SERVES.
  //
  // A part is photographed at CAM1_on and must be judged by SWITCH. That gap is
  // the whole budget, it is set by the plate speed, and it is what decides
  // whether an unjudged part is a rare event or the normal outcome. Every other
  // control here is a way of keeping the report inside it -- so it is shown
  // first, not filed in a different card from the settings that answer to it.
  const spoNow = (cfg && cfg.stage_pulse_offset) || {};
  const budgetMs = (setpoint_freq > 0 && spoNow.SWITCH > 0 && spoNow.CAM1_on > 0)
    ? ticksToMs(spoNow.SWITCH - spoNow.CAM1_on, setpoint_freq) : undefined;
  const spentAvgMs = rlat.cam_avg_us > 0 ? rlat.cam_avg_us / 1000 : undefined;
  const spentMaxMs = rlat.cam_max_us > 0 ? rlat.cam_max_us / 1000 : undefined;
  const marginAvgMs = (budgetMs !== undefined && spentAvgMs !== undefined)
    ? budgetMs - spentAvgMs : undefined;
  const marginMaxMs = (budgetMs !== undefined && spentMaxMs !== undefined)
    ? budgetMs - spentMaxMs : undefined;
  const healthNow = (stat && stat.health) || {};
  // The device's own answer to "would save_setup be accepted right now".
  // Absent means yes. Undefined on old firmware, which reads as savable on
  // purpose -- see the button.
  const persistDeny = healthNow.cfg_persist_deny;
  const cntNow = (stat && stat.count) || {};
  const reportHz = rlat.cam_avg_us > 0 ? 1000000 / rlat.cam_avg_us : undefined;
  const reportWorstHz = rlat.cam_max_us > 0 ? 1000000 / rlat.cam_max_us : undefined;

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

      // Same once-per-open reasoning: the build cannot change without a
      // reflash, and a reflash reopens the link. Firmware older than
      // get_fw_version does not know the command, so a refusal here is not a
      // fault -- it is itself the answer "too old to say", and setFw(null)
      // records that so the tag can distinguish it from "not asked yet".
      if (typeof api.getFwVersion === 'function') {
        api.getFwVersion()
          .then((r) => { if (mounted.current) setFw(r && r.git ? r : null); })
          .catch(() => { if (mounted.current) setFw(null); });
      }
    });
  }, [API_ID]);

  // Adopt the station table once, same reasoning as the sliders below.
  useEffect(() => {
    if (Object.keys(spoEdit).length === 0 && spo.CAM1_on !== undefined) setSpoEdit(spoToEdit(spo, wus, setpoint_freq, ctr));
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
    // Centres, for the nozzles that are in centre mode. The position typed for
    // such a station IS the centre, so it goes here and the device derives both
    // edges from it; *_on is still sent above but the device overwrites it.
    const cOut = {};
    for (const st of STATIONS) {
      if (!st.blow) continue;
      const on = Number(ctr[st.us]) > 0;
      const v = on ? Number(spoEdit[st.on]) : 0;
      cOut[st.us] = isFinite(v) ? v : 0;
      if (cOut[st.us] !== Number(ctr[st.us] || 0)) changed = true;
    }
    if (!changed) return;
    // Widths go as microseconds and the DEVICE converts to ticks against its own
    // plate_freq. Sending ticks from here would bake in whatever speed the panel
    // happened to believe, which is the whole problem being fixed.
    run('spo', (api) => api.machineSetupUpdate({
      stage_pulse_offset: { ...spo, ...spoOut },
      stage_pulse_width_us: { ...(wus || {}), ...wOut },
      stage_pulse_center: { ...ctr, ...cOut },
    }, false, true));
  };

  // Turn centre mode on or off for one nozzle WITHOUT moving the blow.
  //
  // Switching representation must be a no-op on the plate, or nobody can try it
  // on a running machine -- and a blow that jumps half its own width when you
  // tick a box is exactly the kind of change that gets reverted and never
  // revisited. So: on, the centre is seeded from the current leading edge plus
  // half the width; off, the position falls back to that same leading edge.
  const setCentered = (st, want) => {
    const wUs = Number(spoEdit['_w_' + st.key]) || Number((wus || {})[st.us]) || 0;
    if (want && !(wUs > 0)) {
      log.warn('[uinsp2] %s: centre needs a width first', st.key);
      return;
    }
    const halfTicks = Math.ceil(wUs * 2 * setpoint_freq / 1e6 / 2);
    const lead = Number(spo[st.on]) || 0;          // what the device fires at now
    const next = want ? lead + halfTicks : lead;
    setSpoEdit({ ...spoEdit, [st.on]: String(next) });
    run('spo', (api) => api.machineSetupUpdate({
      stage_pulse_center: { ...ctr, [st.us]: want ? next : 0 },
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
  // jog, straight from get_running_stat. `disp` is the signed travel of the
  // held part from its gate edge -- the same units and origin as
  // stage_pulse_offset, which is what makes "設為此站" a copy and not a
  // conversion.
  const jogInfo   = (stat && stat.jog) || {};
  const jogState  = jogInfo.state || 0;
  const jogDisp   = Number(jogInfo.disp) || 0;
  const jogMoving = !!jogInfo.moving;
  const jogHold   = jogState === 2;
  const jogOn     = jogState !== 0;
  // Selecting a station fills in where it fires, so "show me this station" is
  // one click instead of reading a number out of the table and typing it back.
  // Only on the SELECTION changing -- not on every spoEdit keystroke, or the
  // field could not be typed into.
  const selPosKey = (sel && sel.which === 'pos'
    && (STATIONS.find((x) => x.key === sel.key) || {}).on) || null;
  useEffect(() => {
    if (!selPosKey) return;
    const v = Number(spoEdit[selPosKey] ?? (spo || {})[selPosKey]);
    if (isFinite(v)) setJogGo(v);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [selPosKey]);
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
        {/* Three states, because there are three.
            cfg_from_nvs is a fact the BOARD reports in its get_setup reply --
            so before that reply lands there is nothing to report, and a
            two-state tag has to guess. It guessed "compile defaults", which
            reads as "the board's NVS is empty" and sends whoever is standing
            at the machine to go and look at the hardware. On a fresh PC whose
            renderer froze for 153 seconds the reply never landed at all, the
            config resync gave up, and the tag confidently described a board
            whose NVS was perfectly intact the whole time. Say "not read yet"
            when that is what is true. */}
        {cfgRead ? (
          <Tag color={dev.cfg_from_nvs ? 'green' : 'orange'}>
            {dev.cfg_from_nvs ? '設定來自 NVS' : '設定非 NVS(編譯預設值)'}
          </Tag>
        ) : (
          <Tooltip title={'尚未收到板子的 get_setup 回應,所以還不知道它的設定從哪裡來。'
                        + '這不代表 NVS 是空的。主機會一直重試(前 10 次每秒,之後每 5 秒)。'
                        + (cfgTries > 10 ? ' 重試很多次仍無回應:先看主執行緒有沒有被卡住(下載診斷紀錄裡的 [stall]),再看序列鏈路。' : '')}>
            <Tag color={cfgTries > 10 ? 'red' : 'default'}>
              設定尚未讀取{cfgTries ? ` (重試 ${cfgTries})` : ''}
            </Tag>
          </Tooltip>
        )}
        {dev.machine_id ? <Tag>{dev.machine_id}</Tag> : null}
        {/* Which build is on the chip. The short hash is what fits; the whole
            identity goes in the tooltip, because the value of this tag is
            being able to paste it next to a `git log` line. A "-dirty" suffix
            is deliberately not hidden: it means the image is NOT the commit it
            names, which is exactly when trusting the hash misleads. */}
        {fw === undefined ? null : fw === null ? (
          <Tooltip title="這塊板子的韌體早於 get_fw_version,問不出建置身分。重新燒錄後才會顯示。">
            <Tag color="default">韌體版本未知</Tag>
          </Tooltip>
        ) : (
          <Tooltip title={`git ${fw.git}
build ${fw.build}`}>
            <Tag color={String(fw.git).endsWith('-dirty') ? 'orange' : 'blue'}>
              韌體 {String(fw.git).slice(0, 12)}
            </Tag>
          </Tooltip>
        )}
      </div>

      {/* ---- main row: what someone standing at the machine needs ---------- */}
      <Card size="small" style={{ marginBottom: 8, borderColor: runColor }}
        bodyStyle={{ padding: '10px 12px' }}>
        {/* Link health, incl. the core-side verdict counters (tx_fail /
            dropped_no_channel). The core has counted them since 0338671f;
            this is the first place an operator can see "the UI is green but
            nothing is being sorted" as it happens. */}
        <PerifStatusPanel id={API_ID} />
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

      {/* IO safe mode. Above everything, because nothing below it can run.

          The device leaves the eight actuator pins as inputs until a config
          says what ON means -- see DEV_COMPLETE_CHECKLIST "IO safe mode". Left
          unexplained this looks like a machine that refuses to start for no
          reason, so the reason is the first thing on the panel, and the way
          out is right under it.

          The switches write polarity, which is a WIRING fact, not a
          preference: getting one wrong energises that output whenever it
          should be off. So they are laid out one per output with the actual
          levels shown, and nothing here guesses a value on the operator's
          behalf -- a "fix it for me" button would just be the compiled default
          under a friendlier name, and the compiled default is what put the
          machine here. */}
      {dev.io_armed === false && (
        <Card size="small" style={{ marginBottom: 8, borderColor: '#c33' }}>
          <b style={{ color: '#c33' }}>輸出未啟用 —— 安全模式</b>
          <div style={{ marginTop: 4 }}>
            八個輸出腳位維持高阻抗(不驅動),檢測模式會被拒絕。
            原因:<b> {dev.io_safe_why || '設定未定義輸出極性'}</b>
          </div>
          <div style={{ ...dim, marginTop: 4 }}>
            韌體不會替沒定義的輸出猜一個預設值:這台機器八個輸出全是低準位導通,
            而編譯預設有七個是相反的 —— 那正是燈和氣閥自己打開的那次。
          </div>
          {(() => {
            const TAB = ['L1A', 'CAM1', 'L2A', 'CAM2', 'SEL1', 'SEL2', 'SEL3', 'FEEDER'];
            // io_on_level is in SETTABLE_KEYS, so the read-only sync files it
            // under machineSetup, not deviceState. io_armed/io_safe_why are not
            // settable and land in deviceState -- two different objects for two
            // halves of the same card.
            const cur = cfg.io_on_level || dev.io_on_level || {};
            const push = (name, lv) => run('iopol', (api) =>
              api.machineSetupUpdate({ io_on_level: { ...cur, [name]: lv } },
                                     false, true));
            return (
              <div style={{ display: 'flex', gap: 12, flexWrap: 'wrap',
                            marginTop: 8 }}>
                {TAB.map((n) => (
                  <span key={n} style={{ display: 'flex', alignItems: 'center',
                                         gap: 4 }}>
                    <b>{n}</b>
                    <Switch size="small" loading={busy === 'iopol'}
                      checkedChildren="HIGH" unCheckedChildren="LOW"
                      checked={cur[n] === 1}
                      onChange={(v) => push(n, v ? 1 : 0)} />
                    {/* A name the device does not drive cannot be set from
                        here, and saying so beats a control that does nothing. */}
                    {cur[n] === undefined && (
                      <span style={{ color: '#c33' }}>缺</span>)}
                  </span>
                ))}
              </div>
            );
          })()}
          <div style={{ ...dim, marginTop: 6 }}>
            設定完成後輸出立即啟用,但<b>不會自動存入 NVS</b> —— 重開機會回到安全模式,
            確認正確後請按存檔。
          </div>
        </Card>
      )}

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
        {/* PULLED OUT OF THE CARDS, because it belongs to all of them.
            Every settable field on this panel writes RAM and takes effect on
            the next part; this is the only thing that makes any of it survive a
            power cycle. It lived inside two collapsed cards, which meant the
            step that keeps the work was reachable only from the card you
            happened to be in -- and tuning done in a third card was lost with
            no error, looking like the machine had drifted overnight.

            Enabled state comes from the DEVICE, not from a rule copied here:
            health.cfg_persist_deny is computed by the same function save_setup
            calls, so the button cannot disagree with what would actually
            happen. Absent = savable. Unknown (old firmware, no stat yet) stays
            ENABLED -- a greyed-out button with no explanation is worse than a
            press that comes back with the firmware's own refusal text. */}
        <Tooltip title={persistDeny
          ? `裝置現在不接受存檔:${persistDeny}`
          : '把目前所有設定寫進板子的 NVS,才會撐過重開機'}>
          <span>
            <Button size="small" type={persistDeny ? 'default' : 'primary'}
              loading={busy === 'savenvs'} disabled={!!persistDeny}
              data-testid="uinsp-save-nvs"
              onClick={() => run('savenvs', (api) => api.saveSetupToDevice())}
            >存入 NVS</Button>
          </span>
        </Tooltip>
        {persistDeny && (
          <span style={{ alignSelf: 'center', fontSize: 11, color: '#c60' }}>
            {persistDeny === 'must be in IDLE or INSPECTION_MODE_READY' ? '要先停下檢測才能存檔'
             : persistDeny === 'set plate_freq to 0 first' ? '要先把轉速設為 0 才能存檔'
             : persistDeny === 'plate still moving; wait until SYS_STEP_COUNT stops' ? '轉盤還在轉,停穩才能存檔'
             : persistDeny}
          </span>
        )}
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

      {/* Commissioning a board, and the escape hatch for anything this panel
          does not give a control.
          The firmware declares 30 settable keys and the hand-built cards below
          reach eight of them. On a bench that is a gap; on a machine in the
          field it is a dead end -- the value you need is the one nobody built
          a box for. A whole-config export/import closes it for every key at
          once, and keeps closing it when the firmware grows a new one.
          It is also the answer to "how do I set up a replacement board":
          export from the working machine, import into the new one. */}
      <FoldCard style={{ marginBottom: 8 }} title={<span>設定備份 / 移機
        <Why>匯出目前裝置上的完整設定成 JSON,或把 JSON 寫回裝置。換板、備份、
          以及這個面板沒有做控制項的欄位,都靠這個。<br/><br/>
          匯入會<b>寫回後重讀比對</b>:韌體對它不認得的鍵一律回 ack:true 然後忽略,
          所以只寫不驗的匯入是不會失敗的匯入 —— 在新板子上那是最糟的性質。<br/><br/>
          寫入後仍在 RAM,要斷電保留必須再按「存入 NVS」。</Why></span>}>
        <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap', alignItems: 'center' }}>
          <Button size="small" loading={cfgBusy === 'export'} onClick={() => {
            const api = getPerifAPI(API_ID);
            if (!api) { message.error('裝置未連線'); return; }
            setCfgBusy('export'); setCfgReport(null);
            api.exportSetupP()
              .then((doc) => {
                const id = doc.machine_id || 'uInspESP32';
                const blob = new Blob([JSON.stringify(doc, null, 2)], { type: 'application/json' });
                const a = document.createElement('a');
                a.href = URL.createObjectURL(blob);
                a.download = `uinsp_setup_${id}.json`;
                document.body.appendChild(a); a.click();
                document.body.removeChild(a); URL.revokeObjectURL(a.href);
                message.success(`已匯出 ${Object.keys(doc).length} 個設定欄位`);
              })
              .catch((e) => message.error('匯出失敗: ' + String(e)))
              .then(() => setCfgBusy(null));
          }}>匯出 JSON</Button>

          <Button size="small" loading={cfgBusy === 'import'} onClick={() => cfgFileRef.current && cfgFileRef.current.click()}>
            匯入 JSON…
          </Button>
          <input ref={cfgFileRef} type="file" accept="application/json,.json"
            style={{ display: 'none' }}
            onChange={(ev) => {
              const f = ev.target.files && ev.target.files[0];
              ev.target.value = '';                 // so re-picking the same file fires again
              if (!f) return;
              const api = getPerifAPI(API_ID);
              if (!api) { message.error('裝置未連線'); return; }
              setCfgBusy('import'); setCfgReport(null);
              f.text()
                .then((txt) => JSON.parse(txt))
                .then((doc) => api.importSetupP(doc))
                .then((r) => {
                  setCfgReport(r);
                  if (r.mismatch.length) message.warning(`${r.written.length} 個欄位已寫入,但 ${r.mismatch.length} 個沒有生效`);
                  else message.success(`${r.written.length} 個欄位已寫入並確認`);
                })
                .catch((e) => { message.error('匯入失敗: ' + String(e)); setCfgReport(null); })
                .then(() => setCfgBusy(null));
            }} />

          {/* What is still on a compiled default -- the silent half. */}
          <Button size="small" loading={cfgBusy === 'schema'} onClick={() => {
            const api = getPerifAPI(API_ID);
            if (!api) { message.error('裝置未連線'); return; }
            setCfgBusy('schema');
            api.getSchemaP()
              .then((s) => setCfgReport({ schema: s }))
              .catch((e) => message.error('查詢失敗: ' + String(e)))
              .then(() => setCfgBusy(null));
          }}>檢查預設值</Button>
        </div>

        {cfgReport && cfgReport.mismatch && (
          <div style={{ marginTop: 8, fontSize: 12 }}>
            <div>寫入 {cfgReport.written.length} 個欄位。</div>
            {cfgReport.unknown.length > 0 && (
              <div style={{ color: '#c60' }}>⚠ 檔案裡有 {cfgReport.unknown.length} 個此韌體不認得的鍵,已略過:
                {' '}{cfgReport.unknown.join(', ')}</div>
            )}
            {cfgReport.mismatch.length > 0 ? (
              <div style={{ color: '#c33', marginTop: 4 }}>
                ⚠ 以下欄位寫入後讀回來不一樣(裝置拒絕或夾限了):
                {cfgReport.mismatch.map((m) => (
                  <div key={m.key} style={{ marginLeft: 8 }}>
                    {m.key}: 想要 {JSON.stringify(m.wanted)} → 實際 {JSON.stringify(m.got)}
                  </div>
                ))}
              </div>
            ) : <div style={{ color: '#389e0d', marginTop: 4 }}>✓ 全部讀回確認一致</div>}
          </div>
        )}
        {cfgReport && cfgReport.schema && (
          <div style={{ marginTop: 8, fontSize: 12 }}>
            <div>io_armed: <b style={{ color: cfgReport.schema.io_armed ? '#389e0d' : '#c33' }}>
              {String(cfgReport.schema.io_armed)}</b>
              {cfgReport.schema.io_safe_why ? ` (${cfgReport.schema.io_safe_why})` : ''}</div>
            <div style={{ marginTop: 4 }}>
              走編譯預設值的欄位:<b>{cfgReport.schema.defaulted_n ?? 0}</b> 個
              {Array.isArray(cfgReport.schema.defaulted) && cfgReport.schema.defaulted.length > 0 && (
                <div style={{ color: '#c60', marginLeft: 8 }}>
                  {cfgReport.schema.defaulted.join(', ')}
                </div>
              )}
            </div>
            <div style={{ color: '#888', marginTop: 4 }}>
              預設值不會報錯,它只是安靜地生效。io_on_level 的編譯預設極性和本機相反。
            </div>
          </div>
        )}
      </FoldCard>

      <FoldCard style={{ marginBottom: 8 }} title={<span>背光 (相機設定用)
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

          {/* The plate, photographed. The compact status strip has had this
              button; this panel -- the one someone opens to SET the camera up --
              did not, so lighting the plate and taking the picture of it were in
              two different places.

              camSnapWithLight owns the whole sequence: hold the light, settle,
              fire the camera pulse, settle, drop the light. It drops the light
              on the failure path too, which is why this does not reuse the
              buttons above -- a hold left on by a failed snapshot is a backlight
              built for 600us pulses sitting lit until its own timeout. */}
          <Button size="small" type="primary" icon={<CameraOutlined />}
            loading={busy === 'snap'} disabled={running}
            title="點燈、由板子送觸發、熄燈。相機會先被切到硬體觸發再切回去。"
            onClick={() => run('snap', (api) => {
              setSnapWhy('');
              return snapWithBoardTrigger(api, dispatch, CORE_ID)
                .then((r) => { setLightUntil(0); return r; },
                  (e) => { setLightUntil(0);
                    setSnapWhy('拍照失敗:' + String(e && e.message ? e.message : e)); throw e; });
            })}
          >拍一張盤面</Button>
          <span style={dim}>
            {running
              ? '檢測模式中無法手動點燈'
              : lightUntil > now
                ? `亮著 — ${Math.ceil((lightUntil - now) / 1000)} 秒後自動熄滅`
                : '熄滅'}
          </span>
        </div>
        {snapWhy ? <div style={{ color: '#cf1322', marginTop: 6 }}>{snapWhy}</div> : null}
      </FoldCard>

      {/* ONE CARD, BECAUSE THESE SETTINGS ARE ONE DECISION.
          The feed caps, the judging deadline and the stop thresholds were in
          three different places, and they cannot be chosen apart: the deadline
          is set by the plate speed, whether it is met is set by the feed rate,
          and the stop thresholds decide what happens when it is not. Tuning one
          while looking at another card is how a machine ends up stopping on a
          threshold that was chosen for a speed it no longer runs at.

          The order below is the project's priority order, stated by the owner
          on 2026-08-31: never mis-judge, then best effort, then avoid stopping.
          An uncertain part is NA and rides round again -- that already satisfies
          the first rule, so stopping is a separate and much stronger action,
          reserved for losing track of WHICH part a report belongs to. */}
      <FoldCard style={{ marginBottom: 8 }} title={<span>運作調節
        <Why>三個目標,依優先序:<b>不可檢錯</b> &gt; best effort &gt; <b>盡量不停機</b>。
          有疑問就 NA 讓料回流 —— 沒有致動就是再轉一圈,料不會掉,第一條就已經滿足了。
          所以停機只留給「可能配到錯的物件」這種追蹤問題,不是給「這顆判不出來」。
          這張卡把判定期限、兩層進料節流、停機門檻放在一起,因為它們是同一個決定:
          期限由轉速決定,達不達得到由進料速率決定,達不到怎麼辦由門檻決定。</Why></span>}>

        {/* The deadline, first, because it is what the rest is for. */}
        <div style={{ display: 'flex', gap: 14, flexWrap: 'wrap', marginBottom: 10,
                      padding: '6px 8px', background: '#fafafa', borderRadius: 4 }}>
          <span>判定期限
            <Why>相機觸發到分選點的時間,由轉速和 CAM1_on/SWITCH 的間距決定。
              回報時間是相機起算的(cam_avg_us),跟這個預算同一個起點才能相減。</Why>
            {' '}<b>{budgetMs !== undefined ? `${budgetMs.toFixed(0)} ms` : '—'}</b></span>
          <span>回報 平均/最慢 <b>
            {spentAvgMs !== undefined ? spentAvgMs.toFixed(0) : '—'} / </b>
            <b>{spentMaxMs !== undefined ? spentMaxMs.toFixed(0) : '—'} ms</b></span>
          {/* Average first. The worst is a high-water mark that one slow frame
              after a def build pins negative for the rest of the session, so on
              its own it says nothing about how the machine is running now. */}
          <span>餘裕 平均/最慢 <b style={{
              color: marginAvgMs !== undefined && marginAvgMs <= 0 ? '#c33'
                   : (marginMaxMs !== undefined && marginMaxMs <= 0 ? '#c60' : '#389e0d') }}>
            {marginAvgMs !== undefined ? marginAvgMs.toFixed(0) : '—'} / {marginMaxMs !== undefined ? marginMaxMs.toFixed(0) : '—'} ms</b></span>
          {marginAvgMs !== undefined && marginAvgMs <= 0 && (
            <span style={{ color: '#c33' }}>← 平均就來不及,料會一顆顆變 NA</span>
          )}
        </div>
        {/* Layer one, in the same three-state shape as layer two below. The two
            are one idea asked twice -- what limits the feed -- and an operator
            should not have to learn two vocabularies for it. */}
        <div style={{ marginBottom: 4 }}>進料上限（相機）
          <Why>閘門每登記一個物件就會觸發相機一次。要求得比相機能給的快,就會有
            觸發卻沒有影格 —— 那不會配錯(配對靠時間戳,視窗夾在間距的一半),
            但那顆料沒有回報:不致動、回流、計入無判決,持續下去撞停機門檻。<br/><br/>
            <b>自動</b>用相機自己回答的上限(ResultingFrameRate),它會隨 ROI 和
            曝光即時改變 —— 那正是手填的數字維持不住的原因:改 ROI 的時候沒有人
            會回頭想到閘門。</Why></div>
        <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap', alignItems: 'center' }}>
          <Radio.Group size="small" value={camMode} disabled={busy === 'cammode'}
            onChange={(e) => run('cammode', (api) => api.machineSetupUpdate(
              { gate_cam_mode: e.target.value }, false, true))}>
            <Radio.Button value="manual">手動</Radio.Button>
            <Radio.Button value="auto">自動</Radio.Button>
          </Radio.Group>

          {camMode === 'manual' && (<>
            <Input style={{ width: 140 }} addonAfter="顆/秒"
              placeholder={gateHz !== undefined ? String(gateHz) : ''}
              value={hzInput} onChange={(e) => setHzInput(e.target.value)} />
            <Button size="small" loading={busy === 'gate'}
              disabled={!(Number(hzInput) > 0)}
              onClick={() => run('gate', (api) => api.machineSetupUpdate(
                { min_detect_sep_us: Math.round(1000000 / Number(hzInput)) }, false, true))}
            >套用</Button>
            {camFps !== undefined && (
              <a onClick={() => setHzInput(String(Math.floor(camFps * 0.9)))}>
                填相機上限的 90% ({(camFps * 0.9).toFixed(1)})</a>
            )}
          </>)}
        </div>

        <div style={{ ...dim, marginTop: 6 }}>
          {camMode === 'manual'
            ? `固定在 ${gateHz !== undefined ? gateHz : '—'} 顆/秒`
              + (camFps !== undefined ? ` · 相機上限 ${camFps.toFixed(1)} fps` : '')
            : (camFps !== undefined
                ? `跟隨相機 ${camFps.toFixed(1)} fps × 90% = `
                  + `${effHz !== undefined ? effHz.toFixed(1) : '—'} 顆/秒`
                : '等待相機上限 —— 需要 core 連線')}
        </div>

        {/* THE MANUAL NUMBER BEING ABOVE THE CAMERA IS THE CASE THIS EXISTS FOR,
            and it is invisible without saying it: on this bench the configured
            70.0/s sat above a 68.9 fps camera. */}
        {camMode === 'manual' && camFps !== undefined && gateHz > camFps && (
          <div style={{ color: '#c33', marginTop: 4 }}>
            高於相機上限 {camFps.toFixed(1)} fps —— 會有觸發沒影格,那些料變成無判決。
            改用自動,或填 {(camFps * 0.9).toFixed(0)}。
          </div>
        )}
        {/* Stale is not an error and must not read as one: the board keeps using
            the SLOWER of the last camera figure and the manual cap, so nothing
            unsafe happens. What it means is that nobody is refreshing it. */}
        {camMode === 'auto' && camStale && (
          <div style={{ color: '#c60', marginTop: 4 }}>
            相機上限已過期 —— 板子改用較慢的那個值。core 或 UI 沒有在更新它。
          </div>
        )}

        <div style={{ marginTop: 10, marginBottom: 4 }}>進料節流（主機）
          <Why>上面那條看相機能給多快,這條看主機算得多快。失效方式不同:相機來不及
            就是沒影格,主機來不及還是會回答,只是遲到 —— 遲到就是 NA 回流,再嚴重
            就撞到下面的停機門檻。擋掉的料一樣回流,不會掉。<br/><br/>
            <b>自動</b>是預設的選擇:板子量自己的服務時間,把利用率壓在目標值,
            料號換了、ROI 改了、電腦換了都不用重設。<b>手動</b>只在你要固定住一個
            數字的時候用。</Why></div>
        <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap', alignItems: 'center' }}>
          <Radio.Group
            size="small"
            value={procMode}
            disabled={busy === 'procmode'}
            onChange={(e) => {
              const m = e.target.value;
              // manual with nothing typed yet takes the report average as its
              // starting point, because that is the number the field would be
              // filled from anyway and an empty manual mode throttles nothing.
              const hz = m !== 'manual' ? undefined
                : (Number(procHzInput) > 0 ? Math.round(Number(procHzInput))
                   : (reportHz ? Math.max(1, Math.floor(reportHz)) : undefined));
              if (m === 'manual' && !hz) { setProcHzInput(''); return; }
              run('procmode', (api) => api.machineSetupUpdate(
                hz ? { gate_proc_mode: m, gate_proc_rate_hz: hz }
                   : { gate_proc_mode: m }, false, true));
            }}>
            <Radio.Button value="off">關閉</Radio.Button>
            <Radio.Button value="manual">手動</Radio.Button>
            <Radio.Button value="auto">自動</Radio.Button>
          </Radio.Group>

          {procMode === 'manual' && (<>
            <Input
              style={{ width: 140 }}
              addonAfter="顆/秒"
              placeholder={procHz !== undefined ? String(procHz) : ''}
              value={procHzInput}
              onChange={(e) => setProcHzInput(e.target.value)}
            />
            <Button size="small" loading={busy === 'procrate'}
              disabled={!(Number(procHzInput) > 0)}
              onClick={() => run('procrate', (api) => api.machineSetupUpdate(
                { gate_proc_rate_hz: Math.round(Number(procHzInput)) }, false, true))}
            >套用</Button>
            {reportHz !== undefined && (
              <a onClick={() => setProcHzInput(String(Math.max(1, Math.floor(reportHz))))}>
                填目前回報平均 {reportHz.toFixed(1)}/s</a>
            )}
          </>)}
        </div>

        {/* What it is actually doing, in one line, phrased for the mode it is
            in. A mode that reads the same in all three states is a mode
            somebody has to test by changing it. */}
        <div style={{ ...dim, marginTop: 6 }}>
          {procMode === 'off' && '沒有主機節流 —— 進料只受相機上限管。'}
          {procMode === 'manual' && (procHz !== undefined
            ? `固定在 ${procHz} 顆/秒${gate && gate.rej_load > 0 ? ` · 已擋 ${gate.rej_load}` : ''}`
            : '尚未設定速率')}
          {procMode === 'auto' && (procHz !== undefined
            ? `自動找到 ${procHz} 顆/秒`
              + (procSvcMs !== undefined
                  ? ` · 服務 ${procSvcMs.toFixed(0)}ms(中值)`
                    + (procSvcMeanMs !== undefined && procSvcMeanMs > procSvcMs * 1.25
                        ? ` 平均 ${procSvcMeanMs.toFixed(0)}ms` : '')
                  : '')
              + (procRho !== undefined ? ` · 利用率 ${procRho}%` : '')
              + ` · 探測 ${probeUp} / 退讓 ${backoff}`
              + (gate && gate.rej_load > 0 ? ` · 已擋 ${gate.rej_load}` : '')
            : '量測中 —— 需要幾秒的回報才會有值')}
        </div>
        {/* THE ONE THING THAT DISTINGUISHES A WORKING LOOP FROM A RUNAWAY.
            A controller pinned at its bound reports no unjudged parts, no halt
            and no production, which looks like success. Only this says
            otherwise, so it is never folded into the line above. */}
        {procMode === 'auto' && procCapN > 0 && (
          <div style={{ color: '#c33', marginTop: 4 }}>
            自動節流已觸頂 {procCapN} 次 —— 它在盡量慢卻還是跟不上,查相機或主機是不是卡住了
          </div>
        )}
        {procMode !== 'off' && procHz !== undefined && gateHz !== undefined && procHz >= gateHz && (
          <div style={{ color: '#c33', marginTop: 4 }}>
            高於相機上限 {gateHz} 顆/秒,永遠不會作用
          </div>
        )}

        <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap', marginTop: 8 }}>
          {/* The camera ceiling, measured rather than assumed: shrinking the ROI
              height raises it, which is the lever for running parts closer
              together. The gate cap has to stay under this -- above it you get
              triggers with no frames -- unanswered parts, not mis-paired ones
              (the window is clamped to half the separation), so the cost is
              throughput and, sustained, the stop threshold. */}
          {/* The camera's own answer for the ROI and exposure it is carrying.
              Available while IDLE, which is the difference that matters: the
              measured cam_max_fps below needs a run to exist, and this field is
              filled in before one. Both are shown when both exist -- they
              answer different questions, and a gap between them is itself
              information (the sensor could go faster than this run drove it). */}
          {pairing && pairing.cam_fps_limit > 0 && (
            <span style={{ alignSelf: 'center',
              color: gateHz > pairing.cam_fps_limit ? '#c33' : '#888' }}>
              相機上限 {Number(pairing.cam_fps_limit).toFixed(1)} fps
              {gateHz > pairing.cam_fps_limit ? ' ← 閘門開得比相機快' : ''}
              <a style={{ marginLeft: 6 }}
                 onClick={() => setHzInput(String(Math.floor(pairing.cam_fps_limit * 0.9)))}>
                填 90%</a>
            </span>
          )}
          {pairing && pairing.cam_max_fps > 0 && (
            <span style={{ alignSelf: 'center',
              color: gateHz > pairing.cam_max_fps ? '#c33' : '#888' }}>
              本次實測 {Number(pairing.cam_max_fps).toFixed(1)} fps
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

        {/* WHAT HAPPENS WHEN THE DEADLINE IS MISSED ANYWAY -- the last block,
            because it is the last resort. Two thresholds, and they are not the
            same kind of thing:

            unanswered  = nobody judged this part. It was never actuated, so it
                          is already safe; it rides round again. Stopping here
                          protects nothing about quality, it only says "several
                          in a row means the host has stopped answering".
            nomatch     = a verdict arrived that cannot be placed against an
                          object. THAT is the one that touches rule 1, because
                          a report placed against the wrong object is a wrong
                          sort. The firmware already tolerates the harmless
                          shapes (no candidate, outside the window) and stops
                          immediately when the clock itself is invalid.

            Both live here with their LIVE consecutive counts beside them, so a
            threshold is never chosen without seeing what the machine actually
            does against it. */}
        <div style={{ borderTop: '1px solid #eee', marginTop: 10, paddingTop: 8 }}>
          <div style={{ marginBottom: 6 }}>漏判處置
            <Why>「漏判」是料走到分選點時還沒有檢測結果。它不會掉 —— 沒有動作就是
              再轉一圈。連續漏判代表主機或相機不再回答了。看的是「連續」幾顆,不是比例。
              門檻訂太緊,機器只是暫時跟不上、正在自己調節的時候就會停 —— 那正是
              「盡量不停機」要避免的;先開上面的均速節流,再把這個放寬。</Why>
          </div>
          {(() => {
            const mode = cfg.skip_policy_mode
              || (dev.skip_policy && dev.skip_policy.mode);
            const stop = mode === 'stop_only' || mode === 'slow_and_stop';
            const push = (m) => run('skippol', (api) =>
              api.machineSetupUpdate({ skip_policy_mode: m }, false, true));
            const cu = healthNow.consec_unanswered;
            const cn = cntNow.NOMATCH_CONSEC;
            const lim = cfg.unanswered_stop_after;
            const nlim = cfg.nomatch_stop_after;
            return (<>
              <div style={{ display: 'flex', gap: 14, flexWrap: 'wrap',
                            alignItems: 'center', marginBottom: 8 }}>
                <span>
                  <Switch size="small" checked={stop} loading={busy === 'skippol'}
                    onChange={(v) => push(v ? 'stop_only' : 'none')} />
                  {' '}連續漏判即停機
                </span>
                <span style={dim}>mode = {mode || '—'}</span>
                {/* The firmware says this out loud rather than refusing it:
                    "none" is legitimate on a bench and a machine that silently
                    declines a setting is worse than one that tells you. */}
                {!stop && (
                  <span style={{ color: '#c33' }}>
                    已關閉 —— 漏判的料會一直無聲通過
                  </span>
                )}
              </div>
              <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap',
                            alignItems: 'center', marginBottom: 6 }}>
                <Input style={{ width: 175 }} addonBefore="無判決 連續"
                  addonAfter="顆停" placeholder={lim !== undefined ? String(lim) : ''}
                  value={stopAfterInput}
                  onChange={(e) => setStopAfterInput(e.target.value)} />
                <Button loading={busy === 'stopafter'}
                  disabled={!(Number(stopAfterInput) >= 1)}
                  onClick={() => run('stopafter', (api) => api.machineSetupUpdate(
                    { unanswered_stop_after: Math.round(Number(stopAfterInput)) },
                    false, true))}>套用</Button>
                <span style={{ alignSelf: 'center', ...dim,
                  color: lim > 0 && cu >= lim - 1 ? '#c33' : undefined }}>
                  目前連續 {cu !== undefined ? cu : '—'} / {lim !== undefined ? lim : '—'}
                </span>
              </div>
              <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap',
                            alignItems: 'center' }}>
                <Input style={{ width: 175 }} addonBefore="對不上 連續"
                  addonAfter="顆停" placeholder={nlim !== undefined ? String(nlim) : ''}
                  value={nomatchAfterInput}
                  onChange={(e) => setNomatchAfterInput(e.target.value)} />
                <Button loading={busy === 'nomatchafter'}
                  disabled={!(Number(nomatchAfterInput) >= 1)}
                  onClick={() => run('nomatchafter', (api) => api.machineSetupUpdate(
                    { nomatch_stop_after: Math.round(Number(nomatchAfterInput)) },
                    false, true))}>套用</Button>
                <span style={{ alignSelf: 'center', ...dim,
                  color: nlim > 0 && cn >= nlim - 1 ? '#c33' : undefined }}>
                  目前連續 {cn !== undefined ? cn : '—'} / {nlim !== undefined ? nlim : '—'}
                </span>
                {/* Rule 1 lives on this line, so it says so. */}
                <span style={dim}>← 這條牽涉到判錯,不要放太寬</span>
              </div>
            </>);
          })()}
        </div>

        {/* EVERY FIELD IN THIS CARD IS RAM UNTIL THIS BUTTON IS PRESSED.
            set_setup takes effect on the next part and does not touch flash;
            save_setup is what survives a reboot. The station-timing card has
            had this button and this sentence for a while, and this card had
            neither -- so tuning done here would have come back after a power
            cycle looking like the machine had drifted. The same button, in
            both places, rather than one card quietly depending on the other. */}
        <div style={{ marginTop: 10, borderTop: '1px solid #eee', paddingTop: 8,
                      ...dim, fontSize: 11 }}>
          上面的欄位套用後立刻生效(下一顆料起),但只在 RAM。要撐過重開機,用面板
          最上面的「存入 NVS」—— 那顆按鈕管的是整個面板,所以不放在任何一張卡裡面。
          存在板子上,不是存在這台電腦上;換板要用「設定備份 / 移機」。
        </div>
      </FoldCard>

      {/* Every station, expressed the way it is actually adjusted: WHERE it
          fires and HOW LONG it stays on. on/off is how the firmware stores it,
          but nobody thinks "move off to 672" -- they think "make the window
          wider". Width edits keep the position fixed and move `off`. */}
      <FoldCard style={{ marginBottom: 8 }} title={<span>站點時序
        <Why>位置 = 從閘門登記算起走了多少 tick,1 tick = {MM_PER_PULSE.toFixed(4)} mm,
          與轉速無關。寬度 = 該站點持續開啟的 tick 數。括號中的時間是
          {isRef(plate_freq) ? `plate_freq ${REF_FREQ} 參考值` : '目前轉速'}下換算的。
          改完要按「存入 NVS」才會在重開機後存活。

          跑機中改位置,盤上的料會有一部分用舊值、一部分用新值 —— 已經過
          SWITCH 的料用舊的,還沒到的用新的,所以會有幾顆分錯。這是刻意接受
          的:調位置本來就是停產調機在做的事,不值得為它多做一套排空流程。
          韌體那邊同樣標註在 STAGE_PULSE_OFFSET_publish 的註解裡。
          (轉速不適用這條 —— 轉速是生產中會動的,大幅變速會先把管線排空。)</Why></span>}
        extra={<span style={{ fontSize: 11, whiteSpace: 'nowrap',
                              color: spoUnlock ? '#c60' : '#888' }}>
          {spoUnlock ? '可編輯' : '唯讀'}{' '}
          <Switch size="small" checked={spoUnlock} onChange={setSpoUnlock} />
        </span>}>

        {/* Station placement. Turns "guess an offset, run, watch where the blow
            lands, guess again" into a measurement: catch a part at the gate,
            drive it to an offset, look at it, and take the number.

            Every move is sent ABSOLUTE and the device computes the relative
            one. This panel cannot know where the plate stopped -- braking
            distance is not predictable from here -- so the +/- buttons send
            disp+d, not d. */}
        <div style={{ display: 'flex', gap: 4, alignItems: 'center', marginBottom: 8,
          flexWrap: 'wrap', padding: '4px 6px', borderRadius: 4,
          background: jogOn ? '#fff7e6' : '#fafafa',
          border: jogOn ? '1px solid #ffd591' : '1px solid #eee' }}>
          <span style={{ fontSize: 11, color: '#888', width: 34 }}>對位</span>
          {jogState === 0 ? (
            <Button size="small" type="primary" ghost disabled={!spoUnlock}
              onClick={() => run('jog', (api) => api.jogArm(jogArmFreq))}>
              放料抓取
            </Button>
          ) : (
            <Button size="small" danger
              onClick={() => run('jog', (api) => api.jogEnd())}>釋放</Button>
          )}
          <InputNumber size="small" style={{ width: 76 }} controls={false}
            value={jogArmFreq} min={300} max={15000} step={250}
            disabled={jogState !== 0}
            onChange={(v) => setJogArmFreq(Number(v) || 3000)} />
          <span style={{ fontSize: 11, color: '#aaa' }}>抓取轉速</span>

          {jogHold ? <>
            <span style={{ fontSize: 12, fontWeight: 600, marginLeft: 8 }}>
              {jogDisp} tick
            </span>
            <span style={{ fontSize: 11, color: '#888' }}>
              ({(jogDisp * MM_PER_PULSE).toFixed(2)} mm){jogMoving ? ' · 移動中' : ''}
            </span>

            <span style={{ fontSize: 11, color: '#888', marginLeft: 8 }}>前往</span>
            <InputNumber size="small" style={{ width: 92 }} controls={false}
              value={jogGo} step={10} disabled={jogMoving}
              onChange={(v) => setJogGo(Number(v))}
              onPressEnter={() => run('jog', (api) => api.jogGoto(jogGo, jogGoFreq))} />
            <InputNumber size="small" style={{ width: 68 }} controls={false}
              value={jogGoFreq} min={60} max={15000} step={100} disabled={jogMoving}
              onChange={(v) => setJogGoFreq(Number(v) || 600)} />
            <span style={{ fontSize: 11, color: '#aaa' }}>速度</span>
            <Button size="small" type="primary" disabled={jogMoving || !isFinite(jogGo)}
              onClick={() => run('jog', (api) => api.jogGoto(jogGo, jogGoFreq))}>前往</Button>

            {/* The payoff: the measured position becomes the station's offset.
                Same units, same origin (the gate edge), so no conversion. */}
            <Button size="small" type="primary" ghost
              disabled={jogMoving || !spoUnlock || !sel || sel.which !== 'pos'}
              title={sel && sel.which === 'pos'
                ? `把 ${jogDisp} 寫進 ${sel.label} 的觸發位置` : '先點一個站點的「觸發位置」欄位'}
              onClick={() => {
                const st = STATIONS.find((x) => x.key === sel.key);
                setSpoEdit({ ...spoEdit, [st.on]: String(jogDisp) });
                setNudged(true);   // same commit path the fine-adjust bar uses
              }}>設為 {sel && sel.which === 'pos' ? sel.label : '此站'}</Button>
          </> : (
            <span style={{ fontSize: 11, color: '#c60' }}>
              {jogState === 1 ? '盤轉動中,等待料通過閘門…' : '需先解鎖'}
            </span>
          )}
        </div>

        {/* One fine-adjust bar for the whole table, acting on whichever field
            was last focused. A slider cannot land on a single tick and one tick
            is 12.6um, which is the resolution this actually needs; per-row
            buttons would be 48 buttons. */}
        <div style={{ display: 'flex', gap: 4, alignItems: 'center', marginBottom: 8,
          padding: '4px 6px', borderRadius: 4,
          background: sel ? '#f0f6ff' : '#fafafa',
          border: sel ? '1px solid #91caff' : '1px solid #eee' }}>
          {[-100, -10, -1, 1, 10, 100].map((d) => (
            <Button key={d} size="small" disabled={!sel || !spoUnlock} style={{ padding: '0 7px' }}
              onMouseDown={(e) => e.preventDefault()}   /* keep focus on the field */
              onClick={() => nudge(d)}
            >{d > 0 ? `+${d}` : d}</Button>
          ))}
          <span style={{ fontSize: 11, marginLeft: 6, color: sel ? '#1677ff' : '#aaa' }}>
            {!spoUnlock ? '唯讀 — 右上角切換才能編輯'
              : sel ? `${sel.label} · ${sel.which === 'pos' ? '位置 (tick)' : '寬度 (µs, 每格 ×10)'}`
              : '點一個欄位再用快速鈕'}
          </span>
        </div>

        <div style={{ display: 'flex', fontSize: 11, ...dim, padding: '0 0 4px 0' }}>
          <span style={{ width: 104 }}>站點</span>
          <span style={{ width: 86 }}>觸發位置</span>
          <span style={{ width: 86 }}>寬度 (µs)</span>
          <span style={{ width: 58 }}>中心</span>
          <span style={{ flex: 1 }}>換算</span>
        </div>

        {STATIONS.map((st) => {
          const pos = spoEdit[st.on];
          const wid = st.off ? spoEdit['_w_' + st.key] : undefined;
          const bad = st.off && !(Number(wid) > 0);
          const centered = !!(st.blow && Number(ctr[st.us]) > 0);
          const isSel = (which) => sel && sel.key === st.key && sel.which === which;
          const cell = (which, val, onCh) => (
            <Input size="small" style={{ width: 82, marginRight: 4,
              borderColor: isSel(which) ? '#1677ff' : undefined }}
              disabled={!spoUnlock}
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
              <span style={{ width: 58 }}>
                {st.blow ? (
                  <Tooltip title={centered
                    ? '位置是窗口的中心:前後各展開一半寬度。調氣壓只動寬度,料的位置不動。'
                    : '位置是窗口的起點,寬度只往後長(今天的行為)。切換不會移動吹氣窗。'}>
                    <Switch size="small" disabled={!spoUnlock} checked={centered}
                      onChange={(v) => setCentered(st, v)} />
                  </Tooltip>
                ) : null}
              </span>
              <span style={{ flex: 1, fontSize: 11, color: bad ? '#c33' : '#888' }}>
                {Number(pos) >= 0
                  ? `${(Number(pos) * MM_PER_PULSE).toFixed(1)} mm · ${fmtMs(ticksToMs(Number(pos), refFreq(plate_freq)))}`
                  : '—'}
                {/* Both edges, spelled out. A centre is only useful if you can
                    see what it buys either side of the part. */}
                {centered && Number(wid) > 0 ? (() => {
                  const t = Math.ceil(Number(wid) * 2 * setpoint_freq / 1e6);
                  const half = Math.floor(t / 2);
                  const a = Math.max(0, Number(pos) - half);
                  return `  [${a} … ${a + t}] t`;
                })() : ''}
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
          <Button size="small" onClick={() => setSpoEdit(spoToEdit(spo, wus, setpoint_freq, ctr))}>還原成裝置目前值</Button>
          <span style={{ ...dim, fontSize: 11 }}>
            編輯後離開欄位即套用(下一顆料起);要撐過重開機,用面板最上面的「存入 NVS」
          </span>
        </div>
      </FoldCard>

      {pairing && (
      <FoldCard style={{ marginBottom: 8 }} title={<span>影格配對(核心端)
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
      </FoldCard>
      )}

      <FoldCard title="統計" style={{ marginBottom: 8 }}>
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
      </FoldCard>

      <FoldCard title="診斷">
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
      </FoldCard>

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
  const CORE_ID = useSelector((s) => s.ConnInfo.CORE_ID);
  const CONN = usePerifConn(useSelector((s) => s.ConnInfo.uInspESP32_API_ID));
  const withApi = (cb) => cb(getPerifAPI(API_ID));

  const stat = GetObjElement(CONN, ['runningStat']);
  const cfg = GetObjElement(CONN, ['machineSetup']) || {};
  // Read-only device state -- machine_id lives here now that the firmware
  // derives it and it is no longer a settable key. The reset snapshot stamps
  // it, so a filed batch says which machine produced it.
  const dev = GetObjElement(CONN, ['deviceState']) || {};
  const seenRef = useRef({ obj: null, at: 0 });
  const mounted = useRef(true);
  const [busy, setBusy] = useState(false);
  const [snapping, setSnapping] = useState(false);
  const [clearing, setClearing] = useState(false);
  const [why, setWhy] = useState('');
  const [rpmSel, setRpmSel] = useState(undefined);   // slider position, in rpm
  // Statistics history, and the modal that is the only way to zero the counts.
  //
  // An arm-then-fire button in the strip was wrong: both presses land on the
  // same pixel, so the guard is only a timing accident away from not being one.
  // The confirmation is in a different PLACE now -- open a modal, then press
  // something else -- which is the kind a slip cannot walk through.
  const [histOpen, setHistOpen] = useState(false);
  const [hist, setHist] = useState(recallHist);
  const [rsting, setRsting] = useState(false);
  // Which outlet is OK and which is NG, from the core's conn_info.
  //
  // Asked rarely rather than per poll: the wiring cannot change without a
  // reconnect, and this strip is deliberately frugal. It retries while the
  // answer is missing and then stops, so a core that is not up yet still
  // resolves once it is. It goes to the CORE over the websocket and never
  // touches the device's serial link, so it costs nothing the strip was
  // protecting.
  // "Asked rarely" was the intent; the effect did the opposite.
  //
  // It ran ask() in its body with [outlets] as the dependency, and the reply
  // called setOutlets(pv) with a FRESH object every time. New reference ->
  // dependency changed -> effect re-runs -> ask() -> reply -> ... a loop
  // bounded only by the websocket round-trip. Measured on the bench with the
  // Inspection UI up: **420 GS/s** to the core, 99.7% of all inbound packets,
  // every one of them rebuilding perif_pairing JSON under the core's locks
  // while the machine was inspecting. It also swamped the packet sampler in
  // the core log, which is how it was found at all.
  //
  // It could only run away once the answer arrived: the `if (pv.cat_ok &&
  // pv.cat_ng)` guard means a machine whose outlet wiring is not known yet
  // never calls setOutlets, so the loop does not start. That is why this
  // survived -- it is invisible until the machine is fully wired.
  //
  // Now: ask once on mount, and let the 10s timer retry only while the answer
  // is still missing. The ref exists so that timer reads the CURRENT answer
  // without the state having to be a dependency.
  const [outlets, setOutlets] = useState(null);
  const outletsRef = useRef(null);
  useEffect(() => {
    let live = true;
    const ask = () => {
      dispatch(UIAct.EV_WS_SEND_BPG(CORE_ID, "GS", 0, { items: ["perif_pairing"] },
        undefined, {
          resolve: (pkts) => {
            const gs = pkts.find((p) => p.type === "GS");
            const pv = gs && gs.data && gs.data.perif_pairing;
            if (live && pv && pv.cat_ok && pv.cat_ng) { outletsRef.current = pv; setOutlets(pv); }
          },
          reject: () => {},
        }));
    };
    ask();
    const h = setInterval(() => { if (!outletsRef.current) ask(); }, 10000);
    return () => { live = false; clearInterval(h); };
  }, [CORE_ID]);
  // Live pairing residue for the bubble -- fetched ONLY while it is open.
  //
  // The cost of this query is not theoretical: the runaway above measured
  // 420 GS/s, 99.7% of everything the core received, rebuilding perif_pairing
  // JSON under the core's locks while the machine was inspecting. A poll that
  // runs so a bubble MIGHT be opened would pay that every second of every
  // shift, on a machine that is often a fanless tablet, to answer a question
  // nobody asked.
  //
  // The one-a-second GS poll that used to live here is gone with the section it
  // fed. It asked the core for perif_pairing so the bubble could show pairing
  // health; the pairing moved into the firmware, and the bubble now reads
  // cam_sync out of get_running_stat, which this strip already polls. Nothing
  // replaced the poll because nothing needed it -- the core's perif_pairing is
  // still read elsewhere (cam_max_fps, link health) on the existing path.
  const [bubbleOpen, setBubbleOpen] = useState(false);
  // When the latency window was last zeroed, so the panel can say which window
  // the numbers describe. Local: the board does not timestamp the reset, and a
  // clock the operator can read beats one they cannot.
  const [statSince, setStatSince] = useState(null);
  const resetLatency = () => {
    const api = getPerifAPI(API_ID);
    if (!api) return;
    api.resetLatencyStat().then((r) => {
      if (r && r.ack === false) { log.warn('[stat] reset refused', r); return; }
      setStatSince(new Date().toTimeString().slice(0, 5));
    }, (e) => log.warn('[stat] reset failed', e));
  };

  // Presses on the reset button, within a 5s window. Three, because inside a
  // modal the risk is no longer a stray click (you had to open it) -- it is
  // pressing the wrong button while looking at the history you came to read.
  const [rstHits, setRstHits] = useState(0);
  useEffect(() => {
    if (!rstHits) return;
    const h = setTimeout(() => setRstHits(0), 5000);
    return () => clearTimeout(h);
  }, [rstHits]);
  useEffect(() => { if (!histOpen) setRstHits(0); }, [histOpen]);
  // Same shape for "clear the history" -- hidden in the modal, and confirmed,
  // because it throws away every batch at once rather than one.
  const [delHits, setDelHits] = useState(0);
  useEffect(() => {
    if (!delHits) return;
    const h = setTimeout(() => setDelHits(0), 5000);
    return () => clearTimeout(h);
  }, [delHits]);

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

  // --- recent throughput -----------------------------------------------------
  //
  // The device reports cumulative counters, so a rate is a difference over
  // time. A raw difference between two polls is unreadable: the poll period
  // itself jitters (2.5-5s, and only when a reply lands), and at ~9 parts/s one
  // part more or less swings the number by 10%. So it is smoothed.
  //
  // The filter is an EMA in the TIME domain, not per sample: alpha is derived
  // from dt against a 6s time constant. That matters because the sample period
  // is not constant -- a fixed per-sample alpha would make the filter faster or
  // slower depending on how the link happened to be behaving, i.e. the display
  // would react differently to the same machine at different link speeds.
  //
  // Counters only ever go up. A decrease means someone cleared them (or the
  // board rebooted), and blending across that discontinuity would print one
  // enormous negative spike -- so the filter restarts instead.
  const RATE_TAU = 6.0;                       // seconds to ~63% of a step
  const rateRef = useRef(null);               // last sample + filter state
  const [rate, setRate] = useState(null);     // what is displayed

  useEffect(() => {
    if (!stat) return;
    const c = stat.count || {};
    const n = (v) => (typeof v === 'number' ? v : 0);
    const smp = {
      t: Date.now(),
      // Feed: parts the gate let onto the plate.
      g: n(stat.gate && stat.gate.accept),
      // Inspected: every part that reached SWITCH and was accounted for --
      // including NA/SKIP/UNANSWERED. This is throughput, not yield; a part
      // that went round again still cost the machine a slot.
      i: n(c.SEL1) + n(c.SEL2) + n(c.SEL3) + n(c.NA) + n(c.SKIP) + n(c.UNANSWERED),
      o: n(c.SEL3),
    };
    const p = rateRef.current;
    const dt = p ? (smp.t - p.t) / 1000 : 0;
    if (!p || smp.g < p.g || smp.i < p.i || smp.o < p.o) {
      rateRef.current = { ...smp, rg: 0, ri: 0, ro: 0 };   // first sample, or cleared
      setRate(null);
      return;
    }
    if (dt < 0.3) return;                                  // same reply seen twice
    const a = 1 - Math.exp(-dt / RATE_TAU);
    const mix = (prev, d) => prev + a * (d / dt - prev);
    const nx = {
      ...smp,
      rg: mix(p.rg, smp.g - p.g),
      ri: mix(p.ri, smp.i - p.i),
      ro: mix(p.ro, smp.o - p.o),
    };
    rateRef.current = nx;
    setRate({ g: nx.rg, i: nx.ri, o: nx.ro });
  }, [stat]);

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
  // Which outlet is OK and which is NG, from the core's conn_info -- never
  // assumed here. Undefined until the first GS answers, and the counters render
  // as raw selectors until then, because a mislabelled NG is worse than an
  // unlabelled one.
  const selOK = outlets ? `SEL${outlets.cat_ok}` : undefined;
  const selNG = outlets ? `SEL${outlets.cat_ng}` : undefined;
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
  // null where the phrase would only repeat what is already on screen. The
  // ordinary idle case used to read "按下即以 21.0 rpm 啟動" -- and the speed
  // row three lines down already says the rpm AND "(啟動時套用)", which is the
  // same sentence. It was the longest string here, so it was also the one that
  // wrapped the status line to two rows, in the state the panel sits in most
  // of the time. The states that say something the rest of the strip does not
  // still get their phrase.
  const doing = inError ? '需先在設定面板清除'
    : st === 102 ? '校時中,盤故意停著'
    : st === 103 ? '加速中'
    : st === 104 ? '空檔重新校時'
    : running ? '檢測中'
    : lastSpeedRef.current > 0 ? null
    : '尚無轉速,請先在設定面板設定';

  // File the current counts as a batch, then zero the device.
  //
  // The snapshot is written only AFTER the device confirms -- if it refuses,
  // nothing was zeroed and inventing a history entry would be inventing a
  // batch. `since` is the previous reset, which is what makes a row a period
  // rather than a timestamp.
  const doReset = () => {
    if (rstHits + 1 < 3) { setRstHits(rstHits + 1); return; }
    // Refuse rather than guess. Zeroing the device destroys the counts, so this
    // row is the only surviving record of the batch -- and it used to be filed
    // under a hardcoded NG=SEL2/OK=SEL3 that is wrong on a cat_ng 1 machine.
    // A wrong archived batch is worse than a delayed reset, and the wiring
    // arrives within seconds of the panel opening.
    if (!selOK || !selNG) { setRstHits(0); setWhy('尚未取得分料口接線,無法歸零'); return; }
    setRstHits(0); setRsting(true); setWhy('');
    const prev = hist[0];
    // Whichever selector the wiring does not name travels with the batch under
    // its own name -- a count there means a part went to a bin this machine
    // does not have, and which bin that is matters when you go looking.
    const oth = ['SEL1', 'SEL2', 'SEL3'].find((s) => s !== selOK && s !== selNG);
    // Record the whole picture, not the two columns the table happens to show.
    //
    // Zeroing DESTROYS the counts on the device, so this row is the only
    // surviving record of the batch -- and it was keeping NG/OK/NA/feed and
    // throwing the rest away. Two things were missing that cannot be
    // reconstructed afterwards:
    //
    //   the WIRING. A row saying "NG 3124" does not say NG meant SEL1. Re-wire
    //   the machine, or read the row on another one, and the number is no
    //   longer interpretable -- the same ambiguity that already forced the
    //   `h.SEL1` legacy special-case in the table below.
    //
    //   the RAW selector counts. NG/OK/OTH is a mapped view; if the mapping
    //   was wrong at the time (it has been), the underlying numbers are gone
    //   and the mistake is unrecoverable rather than merely visible.
    //
    // The mapped fields stay exactly as they were so old rows and the existing
    // table keep working; everything new is additive.
    const snap = {
      v: 2,
      t: Date.now(), since: prev ? prev.t : null,
      // --- as before: the mapped view the table renders -------------------
      NG: n0(cnt[selNG]), OK: n0(cnt[selOK]), NA: n0(cnt.NA),
      OTH: oth ? { s: oth, n: n0(cnt[oth]) } : null,
      SKIP: n0(cnt.SKIP), UNANS: n0(cnt.UNANSWERED), feed: n0(gate && gate.accept),
      // --- new: what the mapped view cannot be reconstructed from ---------
      wiring: { ng: selNG, ok: selOK },
      sel: { SEL1: n0(cnt.SEL1), SEL2: n0(cnt.SEL2), SEL3: n0(cnt.SEL3) },
      machine: dev.machine_id || cfg.machine_id || null,
      // Context the batch was produced under. Cheap to keep, impossible to
      // recover: "that shift ran at 25 rpm and faulted twice" is exactly the
      // question asked of an archived batch.
      rpm: rpm > 0 ? Math.round(rpm * 10) / 10 : 0,
      state: st === undefined ? null : st,
      errs: (stat && Array.isArray(stat.error_hist) && stat.error_hist.length)
              ? stat.error_hist.slice(0, 8) : null,
      gate: gate ? { in: n0(gate.in), out: n0(gate.out), edges: n0(gate.edges) } : null,
    };
    perifGetObj(API_ID, ((api) => {
      if (!api) { setRsting(false); setWhy('裝置未連線'); return; }
      Promise.resolve(api.resetRunningStat())
        .then(() => { if (mounted.current) setHist(storeHist([snap, ...hist])); })
        .catch((e) => { if (mounted.current) setWhy('歸零失敗: ' + String(e)); })
        .then(() => { if (mounted.current) setRsting(false); });
    }));
  };

  // Three counts on ONE row, and the colour carries the meaning so the row can
  // be read without reading it -- red is the bin you care about, green is the
  // one you want big, grey is the one that means "went round again".
  //
  // flex:1 + minWidth:0 on the Tag, and margin:0 to kill antd's default 8px
  // right margin, which otherwise fights the flex gap and pushes the last tag
  // out of a sidebar that has no room to give.
  //
  // No word in the box. The whole width goes to the number, because the number
  // is what runs to six digits and the colour already says which bin it is.
  // `named` survives for ONE case: the raw SEL1/2/3 fallback, where nothing
  // distinguishes the three but their names -- colour cannot carry a meaning
  // the machine has not told us yet. There the name rides ABOVE the number
  // instead of beside it, so it costs height (which this row has) rather than
  // width (which it does not).
  //
  // tabular-nums so the three boxes do not jitter as digits change under a
  // proportional face; title carries the exact value compactN rounded away,
  // for a mouse. For a finger, the whole row opens CountsBubble.
  // data-testid / data-bin / data-sel / data-value: hooks for the regression
  // probes, and the reason they carry SEMANTICS rather than just marking the
  // node.
  //
  // A test that finds this cell by walking the DOM for text starting with a
  // Chinese label is coupled to the layout, the wording and the position --
  // all three of which are things a UI change is allowed to move. Worse, the
  // thing worth asserting here is not "a number is displayed" but "the cell
  // claiming to be NG is reading the selector the wiring says is NG", and no
  // amount of DOM archaeology recovers that: the mapping is exactly what the
  // rendered text has thrown away. So publish it. data-bin is the claim
  // (NG/OK/NA/SELn), data-sel is which physical outlet it resolved to, and
  // data-value is the raw count as a number, for a probe that would rather not
  // parse thousands separators back out of the rendered text.
  const tag = (name, v, color, named, sel) => (
    <Tag key={name} color={color}
         data-testid="uinsp-count" data-bin={name}
         data-sel={sel || name} data-value={typeof v === 'number' ? v : ''}
         title={typeof v === 'number' ? `${name}: ${v.toLocaleString()}` : name}
         style={{ flex: 1, minWidth: 0, margin: 0, padding: named ? '1px 2px' : '0 2px',
                  textAlign: 'center', lineHeight: named ? 1.05 : '22px',
                  overflow: 'hidden' }}>
      {named ? <div style={{ fontSize: 9, opacity: 0.75, letterSpacing: 0.2 }}>{name}</div> : null}
      {/* compactN stays. The strip is glanced at, and a five-character cap is
          what keeps three counts legible in a third of a sidebar each. The
          exact digits live one tap away -- see CountsBubble below, which exists
          because `title` is a hover affordance and this machine has a
          touchscreen, so on the panel it is standing on, the exact value had
          nowhere to be read at all. */}
      <div style={{ fontSize: named ? 14 : 15, fontWeight: 600,
                    fontVariantNumeric: 'tabular-nums' }}>
        {v === undefined || v === null ? '—' : compactN(v)}
      </div>
    </Tag>
  );

  return (
    // maxWidth/overflow are load-bearing. This strip renders inside an antd
    // Menu title, which is `white-space:nowrap` and does not constrain its
    // children, so a `block` button sizes itself against an unbounded box and
    // drags the whole strip wider than the sidebar -- everything then clips at
    // the left edge and the counts read as "NANS".
    <div style={{ margin: '2px 12px 4px 12px', textAlign: 'left',
                  maxWidth: '100%', overflow: 'hidden', whiteSpace: 'normal' }}>
      {/* One button, and it says what pressing it DOES -- not what the machine
          currently is. A switch shows state and leaves the action implied,
          which is the wrong way round for something that starts a spinning
          plate. The state goes underneath, in small text, where it is read and
          not pressed. */}
      {/* One row, 50:25:25. The three things you press on this panel: start it,
          look at it, and clear what stopped it. Fixed proportions rather than
          conditional rendering -- a button that vanishes when the machine state
          changes moves the other two under the operator's finger. They disable
          instead. */}
      {/* The flex sizing lives on the WRAPPERS, not the buttons. antd's Tooltip
          inserts a <span> around a DISABLED child (so the tip still has
          something to hang off), and that span becomes the flex item -- so
          `flex:1` on the Button applies to the wrong element and only the
          disabled one falls out of the row. Sizing the wrappers is immune to
          whatever antd decides to wrap. */}
      <div style={{ display: 'flex', gap: 4, marginBottom: 3, alignItems: 'stretch' }}>
        <div style={{ flex: 2, minWidth: 0, display: 'flex' }}>
        <Button
          style={{ width: '100%', height: 34, fontSize: 15, fontWeight: 700, padding: 0 }}
          size="large"
          // Solid on both states, red while it is turning. A ghost/outline stop
          // reads as the secondary option, and stopping a spinning plate is
          // never the secondary option -- it is the one you want to hit without
          // looking.
          type="primary"
          danger={running || starting}
          loading={busy}
          disabled={inError || (!running && !starting && !(lastSpeedRef.current > 0))}
          onClick={() => {
            const on = !(running || starting);
            setBusy(true);
            perifGetObj(API_ID, ((api) => {
              if (!api) { setBusy(false); return; }
              runSequence(api, on, lastSpeedRef.current)
                .then((w) => { if (mounted.current) setWhy(w || ''); })
                .catch((e) => { if (mounted.current) setWhy(String(e)); })
                .then(() => { if (mounted.current) setBusy(false); });
            }));
          }}
          // Transport icons, not words. It keeps the row one visual language
          // with the other two, and a triangle/square pair is read faster than a
          // label at a glance across the machine. The word is not lost: the
          // status line directly underneath still says STOP or RUN, which is
          // where the machine's state belongs -- the button says what pressing
          // it DOES, and the line says what the machine IS.
          // A filled white square, not antd's outlined BorderOutlined. On a solid
          // red field a hollow square reads as an empty box; the filled one is
          // the transport-stop glyph everyone already knows.
          icon={running || starting
            ? <span style={{ display: 'inline-block', width: 13, height: 13,
                             background: '#fff', verticalAlign: 'middle' }} />
            : <CaretRightOutlined />}
        />
        </div>

        {/* A lit frame. Disabled while running, because the firmware refuses a
            manual light hold once the stage machine drives those pins -- and it
            is right to: a hold would be stomped within milliseconds. */}
        {/* Tooltip wraps the DIV, not the button. Given a disabled child antd
            injects its own inline-block <span> to hang the tip on, and a
            width:100% button inside an inline-block span resolves against
            content width -- so the button collapsed to 16px and sat 4px low.
            With the div as the child there is no injected span at all. */}
        <Tooltip title="拍一張(含背光)">
        <div style={{ flex: 1, minWidth: 0, display: 'flex' }}>
          <Button
            style={{ width: '100%', height: 34, padding: 0 }}
            icon={<CameraOutlined />} loading={snapping}
            disabled={running || starting}
            onClick={() => {
              setSnapping(true); setWhy('');
              perifGetObj(API_ID, ((api) => {
                if (!api || typeof api.camSnapWithLight !== 'function') {
                  setSnapping(false); setWhy('韌體或介面不支援 camSnapWithLight'); return;
                }
                snapWithBoardTrigger(api, dispatch, CORE_ID)
                  .catch((e) => { if (mounted.current) setWhy('拍照失敗: ' + String(e && e.message ? e.message : e)); })
                  .then(() => { if (mounted.current) setSnapping(false); });
              }));
            }} />
        </div>
        </Tooltip>

        {/* Clearing the error was only possible from the setup panel, which is a
            modal the operator has to go and find -- for the one action that gets
            a stopped line moving again. */}
        <Tooltip title="清除錯誤">
        <div style={{ flex: 1, minWidth: 0, display: 'flex' }}>
          <Button
            style={{ width: '100%', height: 34, padding: 0 }}
            icon={<ReloadOutlined />} loading={clearing}
            danger={inError}
            type={inError ? 'primary' : 'default'}
            disabled={!inError && !(stat && stat.error_hist && stat.error_hist.length)}
            onClick={() => {
              setClearing(true); setWhy('');
              perifGetObj(API_ID, ((api) => {
                if (!api) { setClearing(false); return; }
                Promise.resolve(api.clearError())
                  .then(() => api.clearErrorHistory())
                  .catch((e) => { if (mounted.current) setWhy('清除失敗: ' + String(e)); })
                  .then(() => { if (mounted.current) setClearing(false); });
              }));
            }} />
        </div>
        </Tooltip>
      </div>

      {/* State on the left, the shift's feed total on the right, and the phrase
          only when there is one.
          Three changes from the run-on line this replaces. The rpm is gone --
          the speed row directly below states it, and stating it twice cost the
          width that made this wrap. `進料` is right-aligned out of the sentence
          instead of sitting inside it: it is a cumulative counter, not a fact
          about the current state, and reading "STOP · 盤停止 · 進料 19239" as
          one clause invites the 19239 to be read as part of the condition. And
          it stays on compactN, unlike the verdict tiles and the history table,
          which now print in full. Not an oversight: this sits inline in a
          header that is already fighting for width, and it is a feed total
          rather than a number anyone copies onto a sheet. The exact value is on
          the title. If it ever needs to be read exactly, it wants its own row,
          not a wider clause. */}
      <div style={{ display: 'flex', alignItems: 'baseline', gap: 6,
                    fontSize: 11, lineHeight: 1.35, marginBottom: 3,
                    whiteSpace: 'normal' }}>
        <span style={{ flex: 1, minWidth: 0 }}>
          <b style={{ color }}>{label}</b>
          <span style={{ color: '#888' }}>
            {' · '}{rpm > 0 ? `${rpm.toFixed(1)} rpm` : '盤停止'}
          </span>
          {doing ? (<>
            <span style={{ color: '#888' }}>{' · '}</span>
            <span style={{ color: inError ? '#c33' : starting ? '#d48806' : '#888' }}>{doing}</span>
          </>) : null}
        </span>
        {gate ? (
          <span style={{ color: '#888', flexShrink: 0,
                         fontVariantNumeric: 'tabular-nums' }}
                title={`進料累計 ${n0(gate.accept).toLocaleString()}`}>
            進料 {compactN(n0(gate.accept))}
          </span>
        ) : null}
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
              perifGetObj(API_ID, ((api) => {
                if (api) api.machineSetupUpdate({ plate_freq: pf }, false, true);
              }));
            }
          }}
          style={{ margin: '0 6px 2px' }}
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
      {/* The verdict, in the operator's words. WHICH air valve fires is wiring,
          and this panel must not guess it: it used to read NG=SEL2 / OK=SEL3,
          and on a machine wired cat_ng 1 / cat_ok 3 that put every real reject
          under a warning saying "not wired on this machine" while the NG column
          showed 0. The mapping now comes from conn_info via the core, and when
          the core has not answered yet the raw selectors are shown rather than
          labelled wrongly. */}
      {/* Hover OR click, because the two are different users. A mouse gets the
          per-tile title; a finger gets nothing from a title, and this panel
          runs on a touchscreen -- so the exact counts had no reachable home at
          all. One bubble for the whole row rather than one per tile: the
          question is almost never "what is NG exactly", it is "read me the
          numbers", and three taps to answer that is two too many. */}
      <Popover trigger={['hover', 'click']} placement="right"
               overlayStyle={{ maxWidth: 320 }}
               visible={bubbleOpen} onVisibleChange={setBubbleOpen}
               content={<CountsBubble cnt={cnt} gate={gate} selOK={selOK} selNG={selNG}
                                      rate={rate} stat={stat} cfg={cfg}
                                      statSince={statSince} onResetStat={resetLatency} />}>
        <div style={{ display: 'flex', gap: 4, cursor: 'pointer' }}
             data-testid="uinsp-counts-row">
          {selOK && selNG ? (<>
            {tag('NG', cnt[selNG], 'red',   undefined, selNG)}
            {tag('OK', cnt[selOK], 'green', undefined, selOK)}
          </>) : (<>
            {tag('SEL1', cnt.SEL1, undefined, true)}
            {tag('SEL2', cnt.SEL2, undefined, true)}
            {tag('SEL3', cnt.SEL3, undefined, true)}
          </>)}
          {tag('NA', cnt.NA)}
        </div>
      </Popover>
      {/* The selectors conn_info does NOT name get no column: they read 0
          forever on a given machine and a permanent 0 teaches people to stop
          looking. They are not unwatched though -- a count on one of them means
          a part was sorted into a bin this machine does not have.
          Which ones those are is read from the wiring, never assumed: the
          hardcoded "SEL1 is unwired" that stood here was wrong on this very
          machine and hid 106 real rejects behind a warning. */}
      {selOK && selNG && ['SEL1', 'SEL2', 'SEL3']
        .filter((s) => s !== selOK && s !== selNG && cnt[s] > 0)
        .map((s) => (
          <div key={s} style={{ fontSize: 11, lineHeight: 1.3, color: '#c60', marginTop: 2 }}
               title={`${s}: ${n0(cnt[s]).toLocaleString()}`}>
            ⚠ {s} {compactN(n0(cnt[s]))}(此機未接線)
          </div>
        ))}
      {/* SKIP and UNANSWERED lost their columns -- they read 0 all day and were
          costing two of six slots. They are NOT dropped: both mean a part went
          past with nobody's judgement on it, and SKIP does that without the
          device raising anything. So they stay silent while they are zero and
          take a whole line the moment they are not. */}
      {(cnt.SKIP > 0 || cnt.UNANSWERED > 0) && (
        <div style={{ fontSize: 11, lineHeight: 1.3, color: '#c60', marginTop: 2 }}
             title={`SKIP ${n0(cnt.SKIP).toLocaleString()} · UNANSWERED ${n0(cnt.UNANSWERED).toLocaleString()}`}>
          ⚠ 無判定通過
          {cnt.SKIP > 0 ? ` · SKIP ${compactN(n0(cnt.SKIP))}` : ''}
          {cnt.UNANSWERED > 0 ? ` · UNANS ${compactN(n0(cnt.UNANSWERED))}` : ''}
        </div>
      )}
      {/* The two counters that mean a part was NOT sorted the way it was
          judged -- and neither had anywhere to appear. The board reports both
          in get_running_stat.count; this panel rendered neither, so on a
          machine hitting them the operator watched NG/OK/NA/SEL1-3 sit
          perfectly still while product went the wrong way.

          SEL_SUPPRESSED  a verdict whose actuation was scheduled and not
                          delivered. Its guard is
                          PLATE_RUNNING && !SYS_STEPPER_DISABLED && !DRY_RUN,
                          and stopping the plate does not reach it -- on a real
                          machine the way in is de-energising the driver while
                          the plate turns, which lets a loaded plate coast and
                          throw parts.
          SEL1_NO_QUOTA   an NG the actuation quota ate: guard passed,
                          SEL1_ACT_COUNTDOWN spent, so no blow AND no SEL1_Count.
                          A reject that stayed in the good stream, untraced --
                          SEL1_Count is what the bin is reconciled against.

          Red, not the #c60 used above: those say "nobody judged this part",
          these say "the judgement did not reach the part". Same silent-while-
          zero rule as SKIP/UNANSWERED, because that is what makes a line
          appearing here mean something. */}
      {(cnt.SEL_SUPPRESSED > 0 || cnt.SEL1_NO_QUOTA > 0) && (
        <div style={{ fontSize: 11, lineHeight: 1.3, color: '#c00', fontWeight: 600,
                      marginTop: 2 }}
             data-testid="uinsp-unsorted"
             data-suppressed={n0(cnt.SEL_SUPPRESSED)}
             data-noquota={n0(cnt.SEL1_NO_QUOTA)}
             title={`SEL_SUPPRESSED ${n0(cnt.SEL_SUPPRESSED).toLocaleString()}`
                    + ` · SEL1_NO_QUOTA ${n0(cnt.SEL1_NO_QUOTA).toLocaleString()}`}>
          ⚠ 判定未落實到零件
          {cnt.SEL_SUPPRESSED > 0 ? ` · 未送達 ${compactN(n0(cnt.SEL_SUPPRESSED))}` : ''}
          {cnt.SEL1_NO_QUOTA > 0 ? ` · NG未吹除 ${compactN(n0(cnt.SEL1_NO_QUOTA))}` : ''}
        </div>
      )}
      {/* Rate, not total. The totals above say what the shift has done; this
          says what the machine is doing right now, which is the number you
          watch while turning the speed up. Three of them because the gaps
          between them are the diagnosis: feed >> inspected means parts are
          arriving faster than the camera can judge them, and inspected >> OK
          means it is judging them and not liking them. */}
      {/* The history button lives HERE, not on the counter row above.
          There it was a fourth item competing with the counts for a sidebar's
          width, and the counts are the thing that runs to six digits. This row
          is text at fontSize 10 with slack at its right end, so the button is
          free here -- and "件/秒" moved onto the numbers, where the unit
          belongs, instead of floating at the far right detached from them. */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 4, marginTop: 2,
                    fontSize: 10, color: '#888', lineHeight: 1.2 }}>
        {rate ? (<>
          <span style={{ flex: 1, minWidth: 0 }}>進料 {rate.g.toFixed(1)}/s</span>
          <span style={{ flex: 1, minWidth: 0 }}>檢測 {rate.i.toFixed(1)}/s</span>
          <span style={{ flex: 1, minWidth: 0, color: '#389e0d' }}>OK {rate.o.toFixed(1)}/s</span>
        </>) : <span style={{ flex: 1 }} />}
        {/* Not a reset button. Reset is something that happens inside the
            history, because the history is what a reset produces. */}
        <Button size="small" type="text" title="統計歷史 / 歸零"
          icon={<HistoryOutlined />}
          style={{ padding: '0 2px', height: 16, fontSize: 11, flexShrink: 0,
                   lineHeight: 1 }}
          onClick={() => setHistOpen(true)}
        />
      </div>

      {/* The history, and the only place the counters can be zeroed. */}
      <Modal visible={histOpen} onCancel={() => setHistOpen(false)} footer={null}
             width={480} title="統計歷史" destroyOnClose>
        {/* What is on the machine right now -- i.e. exactly what pressing the
            button below would file away. Shown in the same shape as a history
            row so the two can be compared without translating. */}
        {/* The SAME wiring the strip uses, not a second guess at it.
            This row read cnt.SEL2 as NG and cnt.SEL3 as OK. That is the exact
            hardcoding the comment above the counter row says was removed -- it
            was removed there and left standing here. On a machine wired
            cat_ng 1 / cat_ok 3 the NG column then showed SEL2, which is always
            zero, so every reject the operator was about to archive read as
            none. And this row is the preview of what "歸零統計" files away,
            so the wrong number is the one they check before committing.
            doReset() already builds its snapshot from selNG/selOK; this makes
            what is shown agree with what is stored. */}
        {/* data-testid on the ROW and on each cell: this is the pair the
            regression probe compares against the strip, and finding it by
            scanning for a div whose text begins with 目前 matched the modal
            container instead -- layout-, language- and position-coupled, and
            wrong on the first try. Each cell publishes which bin it claims and
            which selector that resolved to, because the mapping is precisely
            what the rendered digits have discarded. */}
        <div data-testid="uinsp-hist-current"
             style={{ ...histRow, fontWeight: 600, borderBottom: '1px solid #eee' }}>
          <span style={{ width: HIST_W.label }}>目前</span>
          <span data-testid="uinsp-hist-cell" data-bin="NG" data-sel={selNG || ''}
                data-value={typeof cnt[selNG] === 'number' ? cnt[selNG] : ''}
                style={{ width: HIST_W.n, textAlign: 'right', color: '#c33' }}>
            {selNG ? exactN(n0(cnt[selNG])) : '—'}</span>
          <span data-testid="uinsp-hist-cell" data-bin="OK" data-sel={selOK || ''}
                data-value={typeof cnt[selOK] === 'number' ? cnt[selOK] : ''}
                style={{ width: HIST_W.n, textAlign: 'right', color: '#389e0d' }}>
            {selOK ? exactN(n0(cnt[selOK])) : '—'}</span>
          <span data-testid="uinsp-hist-cell" data-bin="NA" data-sel="NA"
                data-value={typeof cnt.NA === 'number' ? cnt.NA : ''}
                style={{ width: HIST_W.n, textAlign: 'right' }}>{exactN(n0(cnt.NA))}</span>
          <span data-testid="uinsp-hist-cell" data-bin="feed" data-sel="gate"
                data-value={gate && typeof gate.accept === 'number' ? gate.accept : ''}
                style={{ width: HIST_W.feed, textAlign: 'right' }}>{exactN(n0(gate && gate.accept))}</span>
          <span style={{ flex: 1 }} />
        </div>
        <div style={{ ...histRow, fontSize: 10, color: '#888' }}>
          {/* Name the outlet in the header. Two columns of bare NG/OK is what
              let a wrong mapping sit here unnoticed -- with the selector
              printed, a column reading zero all shift is checkable against the
              machine instead of believed. */}
          <span style={{ width: HIST_W.label }}>歸零時間</span>
          <span style={{ width: HIST_W.n, textAlign: 'right' }}>NG{selNG ? ` ${selNG}` : ''}</span>
          <span style={{ width: HIST_W.n, textAlign: 'right' }}>OK{selOK ? ` ${selOK}` : ''}</span>
          <span style={{ width: HIST_W.n, textAlign: 'right' }}>NA</span>
          <span style={{ width: HIST_W.feed, textAlign: 'right' }}>進料</span>
          <span style={{ flex: 1, textAlign: 'right' }}>區間</span>
        </div>

        <div style={{ maxHeight: 320, overflowY: 'auto' }}>
          {hist.length === 0
            ? <div style={{ color: '#888', padding: '12px 0' }}>還沒有紀錄。第一次歸零就會建立一筆。</div>
            : hist.map((h) => (
              <div key={h.t} style={{ ...histRow, borderBottom: '1px solid #f5f5f5' }}>
                <span style={{ width: HIST_W.label }}>{fmtWhen(h.t)}</span>
                {/* Rows are safe: doReset resolved selNG/selOK at snapshot
                    time, so h.NG/h.OK already name the right bin. Only the
                    "目前" row above had to read the wiring live. */}
                <span style={{ width: HIST_W.n, textAlign: 'right',
                               color: h.NG > 0 ? '#c33' : undefined }}
                      title={String(n0(h.NG))}>{exactN(n0(h.NG))}</span>
                <span style={{ width: HIST_W.n, textAlign: 'right', color: '#389e0d' }}
                      title={String(n0(h.OK))}>{exactN(n0(h.OK))}</span>
                <span style={{ width: HIST_W.n, textAlign: 'right' }}
                      title={String(n0(h.NA))}>{exactN(n0(h.NA))}</span>
                <span style={{ width: HIST_W.feed, textAlign: 'right' }}
                      title={String(n0(h.feed))}>{exactN(n0(h.feed))}</span>
                <span style={{ flex: 1, textAlign: 'right', fontSize: 10, color: '#888' }}>
                  {h.since ? fmtDur(h.t - h.since) : ''}
                  {/* The quiet ones travel with the batch they happened in --
                      they are the reason to keep a record at all. */}
                  {/* h.SEL1 is how rows written before the wiring was known
                      recorded the same thing -- kept so old batches still
                      show it, even though what they called SEL1 was only ever
                      assumed to be the spare one. */}
                  {(h.SKIP > 0 || h.UNANS > 0 || (h.OTH && h.OTH.n > 0) || h.SEL1 > 0) && (
                    <span style={{ color: '#c60' }}>
                      {h.SKIP > 0 ? ` SKIP${h.SKIP}` : ''}
                      {h.UNANS > 0 ? ` UNANS${h.UNANS}` : ''}
                      {h.OTH && h.OTH.n > 0 ? ` ${h.OTH.s}${h.OTH.n}` : ''}
                      {h.SEL1 > 0 ? ` SEL1${h.SEL1}` : ''}
                    </span>
                  )}
                </span>
              </div>
            ))}
        </div>

        {/* Get the record OUT of the browser.
            Zeroing destroys the counts on the device, so these rows are the
            only surviving account of each batch -- and they live in
            localStorage, capped at 20, on one machine's browser. Clearing site
            data, a reinstall, or twenty resets and the record is gone. The
            table shows the mapped columns; the file carries everything the
            snapshot stores, including the wiring and the raw selector counts
            the table has no room for. */}
        {hist.length > 0 && (
          <div style={{ marginTop: 10, display: 'flex', alignItems: 'center', gap: 8 }}>
            <Button size="small" onClick={() => {
              const payload = {
                exported: new Date().toISOString(),
                machine: dev.machine_id || cfg.machine_id || null,
                wiring: { ng: selNG || null, ok: selOK || null },
                note: 'uInspESP32 batch history. Rows with v:2 carry per-selector '
                    + 'raw counts and the OK/NG wiring in force when they were filed; '
                    + 'earlier rows carry the mapped columns only.',
                rows: hist,
              };
              const blob = new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json' });
              const a = document.createElement('a');
              a.href = URL.createObjectURL(blob);
              a.download = `uinsp_hist_${payload.machine || 'unknown'}.json`;
              document.body.appendChild(a); a.click();
              document.body.removeChild(a); URL.revokeObjectURL(a.href);
            }}>匯出歷史 JSON</Button>
            <span style={{ fontSize: 11, color: '#888' }}>
              {hist.length}/{HIST_MAX} 筆 · 只存在這台瀏覽器,滿了會擠掉最舊的
            </span>
          </div>
        )}

        {/* Three presses inside a modal you had to open. The risk here is not a
            stray click -- it is pressing this while reading the table you came
            for, which one press would not catch and a second press in the same
            spot barely would. The count resets after 5s of not pressing. */}
        <Button type="primary" danger block loading={rsting}
                style={{ marginTop: 14 }}
                onClick={doReset}>
          {rstHits === 0 ? '送入歷史並歸零'
            : `再按 ${3 - rstHits} 次` }
        </Button>
        <div style={{ color: '#888', fontSize: 11, marginTop: 4 }}>
          目前的計數會存成一筆紀錄,然後裝置歸零。只保留最近 {HIST_MAX} 筆,
          存在這台瀏覽器裡(不在機器上)。
        </div>

        <div style={{ marginTop: 10, textAlign: 'right' }}>
          <Button type="text" size="small" style={{ fontSize: 11, color: '#aaa' }}
                  disabled={hist.length === 0}
                  onClick={() => {
                    if (delHits + 1 < 3) { setDelHits(delHits + 1); return; }
                    setDelHits(0); setHist(storeHist([]));
                  }}>
            {delHits === 0 ? '清除歷史' : `再按 ${3 - delHits} 次清除全部`}
          </Button>
        </div>
      </Modal>
    </div>
  );
}
