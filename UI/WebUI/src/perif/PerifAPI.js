// Peripheral device APIs (uInspMEGA / uInspESP32 / SLID / CNC) + their
// connection-state store — extracted from script.jsx's component closure
// (2026-08-16).
//
// Design decisions, in order of importance:
//
// 1. NO REDUX for link state. Device-link state is machine state, not UI
//    editing state: it needs no undo/dirty semantics, and pushing every 3s
//    PING through the root store made connection jitter a whole-tree
//    re-render event. The store here is a plain module object with the
//    useSyncExternalStore contract (subscribe/getSnapshot); components use
//    usePerifLink(id) and re-render only when THEIR device changes.
//    (Migration complete: consumers read usePerifLink/usePerifConn; the
//    legacy WS_* bridge and the ConnInfo reducer cases are gone.)
//
// 2. ONE file. The classes are coupled, stateful and share the base's
//    request/response machinery — splitting them into per-device files would
//    scatter that coupling, not remove it.
//
// 3. Dependency injection instead of the `comp` closure: the classes used to
//    capture the root React component to reach ACT_WS_SEND_BPG/CORE_ID.
//    initPerifModule({sendBPG, dispatch, getState}) severs that; the classes
//    are constructible (and testable) without mounting the app.
//
// 4. Reconnect backoff. The old watchdog retried a dead device every 3s
//    forever; consecutive failures now back off 3s -> 6s -> 12s -> 24s,
//    capped at 30s, reset on success. The PING cadence is unchanged.
//
// 5. A SUSPECT state with a face. The core has reported link health
//    (perif_pairing.link: suspect / tx_fail / dropped_no_channel — the
//    "machine is NOT sorting" counters) since 0338671f and no UI ever read
//    it. One central poll here feeds it into the link store for
//    PerifStatus.jsx to show.

import { useState, useEffect, useMemo } from 'react';
import { regroup as uinspRegroup, flatten as uinspFlatten } from '../uinspCfg';
import { GetObjElement, xstate_GetCurrentMainState } from 'UTIL/MISC_Util';
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';
import { mkLog } from 'UTIL/logger';

const log = mkLog('perif.api');
const perifLog = mkLog('ui.uinsp2');

// ---- injected dependencies -------------------------------------------------

const deps = {
  sendBPG: null,   // (tl, prop, obj, bin, promiseCBs) -> void ; targets the CORE ws
  getState: null,  // Redux getState -- SLID's EM_STOP rules read UIData
};

export function initPerifModule(d) {
  deps.sendBPG = d.sendBPG;
  deps.getState = d.getState || (() => ({}));
  startLinkHealthPoll();
}

// ---- link-state store (useSyncExternalStore contract) ----------------------

// links[id] = { state: 'DISCONNECTED'|'CONNECTING'|'CONNECTED'|'SUSPECT',
//               connInfo, machineStatus, machineSetup, deviceState,
//               runningStat, linkHealth (core perif_pairing.link), ... }
let links = {};
const listeners = new Set();

function publish(id, patch) {
  const prev = links[id] || { state: 'DISCONNECTED' };
  links = { ...links, [id]: { ...prev, ...patch } };
  listeners.forEach((fn) => { try { fn(); } catch (e) { /* listener's problem */ } });
}

export function subscribePerifLinks(fn) { listeners.add(fn); return () => listeners.delete(fn); }
export function getPerifLinksSnapshot() { return links; }
export function getPerifLink(id) { return links[id]; }

// React hook: re-renders the caller only when THIS device's entry changes
// identity (publish() always replaces the entry object). React 16 has no
// useSyncExternalStore; the subscribe/setState pattern below is its manual
// equivalent (the re-subscribe on id + the immediate setSnap cover the gap
// between render and effect).
export function usePerifLink(id) {
  const [snap, setSnap] = useState(() => links[id]);
  useEffect(() => {
    const unsub = subscribePerifLinks(() => setSnap(links[id]));
    setSnap(links[id]);   // catch a publish that landed before this effect ran
    return unsub;
  }, [id]);
  return snap;
}

// Legacy-shaped view of a link, for consumers written against the old Redux
// ConnInfo.*_CONN_INFO objects ({type:"WS_CONNECTED", machineStatus, ...}).
// SUSPECT maps to WS_CONNECTED on purpose: every legacy gate is
// `type!=="WS_CONNECTED" -> hide the panel`, and a doubtful link is exactly
// when the operator needs the panel MOST -- the doubt is carried in the
// separate `suspect` flag (and drawn by PerifStatus), not by hiding things.
export function linkToLegacyConn(l) {
  if (!l || (l.state === 'DISCONNECTED' && l.connInfo === undefined)) return undefined;
  return {
    type: (l.state === 'CONNECTED' || l.state === 'SUSPECT') ? 'WS_CONNECTED' : 'WS_DISCONNECTED',
    suspect: l.state === 'SUSPECT',
    machineStatus: l.machineStatus,
    machineSetup: l.machineSetup,
    deviceState: l.deviceState,
    runningStat: l.runningStat,
    default_pulse_hz: l.default_pulse_hz,
    result_count_rate_recent: l.result_count_rate_recent,
    linkHealth: l.linkHealth,
  };
}

export function usePerifConn(id) {
  // Memoized on the store snapshot: the raw version minted a FRESH object
  // every render, which turned any useEffect depending on this value into a
  // self-sustaining loop wherever the effect also setState()s -- MAINUI's
  // readiness effect did exactly that and spun at commit speed for the whole
  // duration of "camera reconnecting" on a uInsp machine. The link snapshot
  // object only changes identity on publish(), so this is the correct key.
  const link = usePerifLink(id);
  return useMemo(() => linkToLegacyConn(link), [link]);
}

// ---- API-object registry ---------------------------------------------------
// Replaces the ACT_WS_GET_OBJ round-trip through Redux for peripheral ids:
// the object is right here, synchronously.
const registry = {};
export function registerPerifAPI(id, api) { registry[id] = api; }
export function getPerifAPI(id) { return registry[id]; }
// Call-shape twin of dispatch(EV_WS_GET_OBJ(id, cb)) for mechanical migration:
// same paren count, synchronous.
export function perifGetObj(id, cb) { return cb(getPerifAPI(id)); }

// ---- central core-side link-health poll ------------------------------------
// One poller for the whole module (not one per widget): perif_pairing.link is
// global core state (there is one perifCH), so it lands on every registered
// link. 30s cadence: transitions are pushed by the core's perif-state
// doorbell (pgID 0xCA12 -> pokeLinkHealthNow below), so the poll is only a
// safety net for non-primary peers and older cores without the watcher.
let healthTimer = null;
// One in-flight query, ever (the pokeNow-fork lesson from the camera poll):
// a doorbell landing while a query is airborne is coalesced into pokePending
// and honoured when the reply arrives, never run in parallel.
let healthInFlight = false;
let healthPokePending = false;
function queryLinkHealthNow() {
  if (!deps.sendBPG) return;
  if (Object.keys(registry).length === 0) return;
  if (healthInFlight) { healthPokePending = true; return; }
  healthPokePending = false;
  healthInFlight = true;
  const done = () => {
    healthInFlight = false;
    if (healthPokePending) queryLinkHealthNow();
  };
  deps.sendBPG('GS', 0, { items: ['perif_pairing'] }, undefined, {
    resolve: (pkts) => {
      try {
        const gs = (pkts || []).find((p) => p.type === 'GS');
        const lk = gs && gs.data && gs.data.perif_pairing && gs.data.perif_pairing.link;
        if (!lk) return;
        Object.keys(registry).forEach((id) => {
          const cur = links[id];
          // Core-side suspect only demotes a link the module believes is up;
          // a link that is DISCONNECTED locally stays DISCONNECTED.
          const patch = { linkHealth: lk };
          if (cur && cur.state === 'CONNECTED' && lk.suspect) { patch.state = 'SUSPECT'; patch.suspectSrc = 'core'; }
          // Only promote a SUSPECT this poll itself demoted. The local PING
          // watchdog (suspectSrc 'local') holds SUSPECT while pings go
          // unanswered; promoting it here made the chip flap green/orange
          // during exactly the outage it exists to show.
          if (cur && cur.state === 'SUSPECT' && !lk.suspect && cur.suspectSrc !== 'local') { patch.state = 'CONNECTED'; patch.suspectSrc = null; }
          publish(id, patch);
        });
      } finally { done(); }
    },
    reject: () => { done(); },
  });
}
// The core's perif-state doorbell says "link summary changed, re-read NOW".
// Wired from WSDataDispatch (script.jsx), same as the camera doorbell.
export function pokeLinkHealthNow() { queryLinkHealthNow(); }
function startLinkHealthPoll() {
  if (healthTimer) return;
  healthTimer = setInterval(queryLinkHealthNow, 30000);
}

// ---- reconnect backoff -----------------------------------------------------
const BACKOFF_BASE_MS = 3000;
const BACKOFF_CAP_MS = 30000;

// ============================================================================
// Base class — request/response over the core's PD proxy, PING watchdog,
// setting-file sync. Moved verbatim from script.jsx except for: injected
// deps, publish() alongside the legacy dispatch, and the backoff.
// ============================================================================

export class Perif_API_Base {
  constructor(id, settingFilePath, pg_id_channel) {
    this.id = id;
    this.settingFilePath = settingFilePath;
    this.pg_id_channel = pg_id_channel;

    this.CONN_ID = undefined;
    this.connInfo = undefined;
    this.inReconnection = false;

    this.trackingWindow = {};
    this.idCounter = 10;
    this.PINGCount = 0;

    this.machineInfo = undefined;

    this._failN = 0;             // consecutive failed connects, drives backoff
    this._nextAttemptAt = 0;

    this.checkReconnectionInterval = setInterval(() => this.checkReConnection(), 3000);
    this.runPINGInterval = setInterval(() => this._sendPing(), 3000);

    publish(this.id, { state: 'DISCONNECTED' });
  }

  // ---- subclass hooks ------------------------------------------------

  // uInspMEGA's get_setup reply carries no "ack" field, so the check cannot
  // be unconditional -- devices that do send one opt in and avoid storing a
  // nak as if it were settings.
  resyncRequiresAck() { return false; }

  // PING reply with the envelope fields already stripped.
  onPingStatus(machineStatus) {
    publish(this.id, { machineStatus });
  }

  onSetupFileLoaded(machInfo) {}
  onSetupFileSaved() {}
  onBeforeSetupPush() {}

  // ---- connection ----------------------------------------------------

  cleanUpTrackingWindow() {
    Object.keys(this.trackingWindow).forEach((key) => {
      const reject = this.trackingWindow[key].reject;
      if (reject !== undefined) reject('CONNECTION ERROR');
      delete this.trackingWindow[key];
    });
  }

  cleanUpConnection() { this.cleanUpTrackingWindow(); }

  connect(connInfo) {
    if (this.inReconnection == true) return false;

    publish(this.id, { state: 'CONNECTING', connInfo });
    this.connInfo = connInfo;
    this.inReconnection = true;
    // Not every machine keeps its configuration on the host. See
    // loadSettingFileOnConnect.
    if (this.loadSettingFileOnConnect()) this.LoadFileToMachine();
    deps.sendBPG('PD', 0, { type: 'CONNECT', ...connInfo, _PGID_: this.pg_id_channel, _PGINFO_: { keep: true } }, undefined, {
      resolve: (stacked_pkts, action_channal) => {
        const PD = stacked_pkts.find((pkt) => pkt.type == 'PD');
        this.inReconnection = false;
        if (PD !== undefined) {
          const PD_data = PD.data;
          switch (PD_data.type) {
            case 'MESSAGE': {
              const msg = PD_data.msg;
              const msg_id = msg !== undefined ? msg.id : undefined;
              const trwin = this.trackingWindow[msg_id];
              if (trwin !== undefined) {
                if (trwin.resolve !== undefined) trwin.resolve(msg);
                delete this.trackingWindow[msg_id];
              }
            }
              break;
            case 'DISCONNECT':
              this.CONN_ID = undefined;
              this.cleanUpConnection();
              this._failN++;
              this._nextAttemptAt = Date.now() + Math.min(BACKOFF_CAP_MS, BACKOFF_BASE_MS * Math.pow(2, this._failN - 1));
              publish(this.id, { state: 'DISCONNECTED' });
              break;
            case 'CONNECT':
              this.CONN_ID = PD_data.CONN_ID;
              this._failN = 0;
              this._nextAttemptAt = 0;
              publish(this.id, { state: 'CONNECTED', CONN_ID: this.CONN_ID });

              if (this.machineSetup !== undefined) {
                this.onBeforeSetupPush();
                this.send({ type: 'set_setup', ...this.machineSetup },
                  (ret) => { this.machineSetupReSync(); }, (e) => console.log(e));
              } else {
                this.machineSetupReSync();
              }
              break;
          }
        }
      },
      reject: (e) => {
        this.CONN_ID = undefined;
        this.inReconnection = false;
        this.cleanUpConnection();
        this._failN++;
        this._nextAttemptAt = Date.now() + Math.min(BACKOFF_CAP_MS, BACKOFF_BASE_MS * Math.pow(2, this._failN - 1));
        publish(this.id, { state: 'DISCONNECTED' });
      },
    });
  }

  checkReConnection() {
    if (this.connInfo === undefined || this.CONN_ID !== undefined || this.inReconnection == true) return;
    if (Date.now() < this._nextAttemptAt) return;   // backing off
    this.connect(this.connInfo);
  }

  // ---- setting file ----------------------------------------------------

  saveMachineSetupIntoFile(filename = this.settingFilePath) {
    deps.sendBPG('SV', 0,
      { filename },
      new TextEncoder().encode(JSON.stringify(this.machineSetup, null, 4)),
      {
        resolve: (res) => { this.onSetupFileSaved(); },
        reject: (res) => {},
      });
  }

  // Does connect() push the host-side settings file INTO the machine?
  //
  // LoadFileToMachine is not a read: it ends in machineSetupUpdate(...,true)
  // -> set_setup, so the file becomes the machine's configuration. That is
  // right for a device whose settings live on the host, and wrong for one
  // that owns its own -- which is why it is a question and not a constant.
  loadSettingFileOnConnect() { return true; }

  LoadFileToMachine(filename = this.settingFilePath) {
    new Promise((resolve, reject) => {
      log.info('LoadSettingToMachine step2');
      deps.sendBPG('LD', 0, { filename }, undefined, { resolve, reject });
      setTimeout(() => reject('Timeout'), 1000);
    }).then((pkts) => {
      log.info('LoadSettingToMachine>> step3', pkts);
      if (pkts[0].type != 'FL') return;
      const machInfo = pkts[0].data;
      this.machineSetupUpdate(machInfo, true);
      this.onSetupFileLoaded(machInfo);
    }).catch((err) => {
      log.info('LoadSettingToMachine>> step3-error', err);
    });
  }

  // ---- machine setup ---------------------------------------------------

  machineSetupUpdate(newMachineInfo, doReplace = false) {
    this.machineSetup = doReplace == true ? newMachineInfo : { ...this.machineSetup, ...newMachineInfo };
    publish(this.id, { machineSetup: this.machineSetup });
    this.send(uinspRegroup({ type: 'set_setup', ...newMachineInfo }),
      (ret) => {
        log.debug('[machine-setup] set_setup ack', ret);
        //HACK: just assume it will work
      }, (e) => log.warn('[machine-setup] set_setup failed', e));
  }

  machineSetupReSync() {
    log.debug('[machine-setup] resync request');
    this.send({ type: 'get_setup' },
      (ret) => {
        if (this.resyncRequiresAck() && ret['ack'] != true) {
          log.warn('[machine-setup] get_setup nak', ret);
          return;
        }
        delete ret['type'];
        delete ret['id'];
        delete ret['st'];
        delete ret['ack'];
        // Flatten before anything downstream looks at it: SETTABLE_KEYS is a
        // list of flat names, so a grouped reply matched none of them and the
        // entire configuration was filed as read-only device state.
        this.machineSetup = uinspFlatten(ret);
        this.machineSetupUpdate(this.machineSetup, true);
      }, (e) => console.log(e));
  }

  getMachineSetup() { return this.machineSetup; }

  // ---- request / response ---------------------------------------------

  findAvailableID() {
    let id = this.idCounter;
    while (this.trackingWindow[id] !== undefined) {
      this.idCounter++;
      if (this.idCounter > 999999) this.idCounter = 0;
      id = this.idCounter;
    }
    return id;
  }

  send(data, resolve, reject) {
    if (this.CONN_ID === undefined) { reject('CONN ID is not set'); return; }

    if (data.id !== undefined) {
      if (this.trackingWindow[data.id] !== undefined) reject(`ID ${data.id} collision`);
    } else {
      data.id = this.findAvailableID();
    }
    this.trackingWindow[data.id] = { resolve, reject };

    deps.sendBPG('PD', 0, { msg: data, CONN_ID: this.CONN_ID, type: 'MESSAGE' }, undefined, {
      resolve: (d) => d,
      reject: (d) => console.log(d),
    });
  }

  // Promise flavour of send(), for the command helpers subclasses add.
  sendP(data) { return new Promise((resolve, reject) => this.send(data, resolve, reject)); }

  // ---- PING watchdog ---------------------------------------------------

  _sendPing() {
    if (this.CONN_ID === undefined) return;

    if (this.PINGCount >= 2) {
      //time to disconnect
      this.PINGCount = 0;
      publish(this.id, { state: 'SUSPECT', suspectSrc: 'local' });   // face first, then reconnect
      this.connect(this.connInfo);
      return;
    }
    this.PINGCount++;
    this.triggerPing();
  }

  triggerPing() {
    this.send({ type: 'PING' }, (ret) => {
      delete ret['type'];
      delete ret['id'];
      delete ret['st'];
      this.onPingStatus({ ...ret });
      this.PINGCount = 0;
    }, (errorInfo) => console.log(errorInfo));
  }

  // Round-trip latency probe for the peripheral link: WebUI -> core PD packet
  // -> perifCH serial -> device -> reply -> back. Sends `count` SEQUENTIAL
  // PINGs (same transport a light/PIN_CONF command takes) and times each
  // resolve, then reports min/avg/p95/max + per-sample list via onUpdate.
  // Drives the WebUI "通訊診斷" button so field comm-delay can be measured.
  diagnoseComm(count, onUpdate) {
    count = count || 20;
    onUpdate = onUpdate || (() => {});
    const self = this;
    let samples = [], fails = 0, i = 0, startedAt = new Date().getTime();
    function finish() {
      const sorted = [...samples].sort((a, b) => a - b);
      const sum = samples.reduce((a, b) => a + b, 0);
      onUpdate({
        done: true, total: count, n: samples.length, fails,
        min: sorted.length ? sorted[0] : null,
        max: sorted.length ? sorted[sorted.length - 1] : null,
        avg: samples.length ? Math.round(sum / samples.length) : null,
        p95: sorted.length ? sorted[Math.min(sorted.length - 1, Math.floor(sorted.length * 0.95))] : null,
        elapsed: new Date().getTime() - startedAt,
        samples,
      });
    }
    function next() {
      if (i >= count || self.CONN_ID === undefined) { finish(); return; }
      i++;
      const t0 = new Date().getTime(); let settled = false;
      const to = setTimeout(() => {
        if (settled) return; settled = true; fails++;
        onUpdate({ done: false, i, total: count, last: null, timeout: true }); next();
      }, 3000);
      self.send({ type: 'PING' }, () => {
        if (settled) return; settled = true; clearTimeout(to);
        samples.push(new Date().getTime() - t0);
        onUpdate({ done: false, i, total: count, last: samples[samples.length - 1] });
        next();
      }, (e) => {
        if (settled) return; settled = true; clearTimeout(to); fails++;
        onUpdate({ done: false, i, total: count, last: null, error: String(e) });
        next();
      });
    }
    next();
  }
}

// ============================================================================
// uInspMEGA (Arduino MEGA + W5500). Speaks pulse_hz / res_count and reports
// sorting throughput derived from successive PING replies.
// ============================================================================

export class uInsp_API extends Perif_API_Base {
  constructor(id, pg_id_channel = 10024) {
    super(id, 'data/uInspSetting.json', pg_id_channel);

    this.pre_res_count = undefined;
    this.res_count_start_time = undefined;
    this.res_count_pre_time = undefined;
    this.res_count_rate_overall = undefined;
    this.res_count_rate_recent = undefined;
  }

  onSetupFileLoaded(machInfo) {
    this.default_pulse_hz = machInfo.pulse_hz;
    publish(this.id, { default_pulse_hz: this.default_pulse_hz });
  }

  onSetupFileSaved() {
    publish(this.id, { default_pulse_hz: this.machineSetup.pulse_hz });
  }

  onBeforeSetupPush() {
    publish(this.id, { default_pulse_hz: this.default_pulse_hz });
  }

  onPingStatus(machineStatus) {
    const res_count = machineStatus.res_count || { OK: 0, NG: 0, NA: 0 };

    const currentTime_ms = new Date().getTime();
    if (this.pre_res_count !== undefined) {
      if (this.pre_res_count.OK <= res_count.OK &&
        this.pre_res_count.NG <= res_count.NG &&
        this.pre_res_count.NA <= res_count.NA &&
        this.res_count_pre_time !== undefined &&
        this.res_count_start_time !== undefined) {
        const period_s = (currentTime_ms - this.res_count_pre_time) / 1000;
        const period_overall_s = (currentTime_ms - this.res_count_start_time) / 1000;
        const period_pre_s = period_overall_s - period_s;
        const OK_rate = (res_count.OK - this.pre_res_count.OK) / period_s;
        const NG_rate = (res_count.NG - this.pre_res_count.NG) / period_s;
        const NA_rate = (res_count.NA - this.pre_res_count.NA) / period_s;
        this.res_count_rate_overall = {
          OK: (this.res_count_rate_overall.OK * period_pre_s + OK_rate * period_s) / period_overall_s,
          NG: (this.res_count_rate_overall.NG * period_pre_s + NG_rate * period_s) / period_overall_s,
          NA: (this.res_count_rate_overall.NA * period_pre_s + NA_rate * period_s) / period_overall_s,
        };

        const maxRange = 20;
        const offset = 1;
        const alpha = period_s > maxRange ? 1 : ((period_s + offset) / (maxRange + offset));
        this.res_count_rate_recent = {
          OK: (this.res_count_rate_recent.OK * (1 - alpha) + OK_rate * alpha),
          NG: (this.res_count_rate_recent.NG * (1 - alpha) + NG_rate * alpha),
          NA: (this.res_count_rate_recent.NA * (1 - alpha) + NA_rate * alpha),
        };
        this.res_count_pre_time = currentTime_ms;
        this.pre_res_count = { ...res_count };
      } else {
        this.pre_res_count = undefined;
      }
    }

    if (this.pre_res_count === undefined) {
      this.pre_res_count = { ...res_count };
      this.res_count_start_time =
      this.res_count_pre_time = currentTime_ms;

      this.res_count_rate_overall = { OK: 0, NG: 0, NA: 0 };
      this.res_count_rate_recent = { OK: 0, NG: 0, NA: 0 };
    }

    publish(this.id, { machineStatus, result_count_rate_recent: this.res_count_rate_recent });
  }
}

// ============================================================================
// uInspESP32 (Peripheral/uInspESP32). Distinct protocol from uInspMEGA:
// plateFreq rather than pulse_hz, stage_pulse_offset for the per-machine
// camera/light/selector timing, and an explicit inspection-mode state
// machine. get_setup answers with ack, so the resync check is worth having.
// ============================================================================

// Consecutive RESET attempts before falling back to a port reopen. Two is
// enough for a one-off line glitch; more would just delay the real recovery
// when the device is genuinely gone (unplugged, crashed, powered down).
const LINK_RESYNC_MAX = 2;

export class uInspESP32_API extends Perif_API_Base {
  constructor(id, settingFilePath = 'data/uInspESP32Setting.json', pg_id_channel = 10027) {
    super(id, settingFilePath, pg_id_channel);
    this.runningStat = undefined;
    this.deviceState = {};
    this.cfg = {};
    this._resyncTries = 0;
    this._linkResyncTries = 0;
  }

  // The v2 board owns its configuration in NVS -- reading is free, writing
  // is not (see the note at the top of uInspESP32_UI.jsx). So connect()
  // must NOT push a host-side file into it.
  //
  // data/uInspESP32Setting.json has never existed on this machine, so every
  // connect logged "Cannot read file from: data/uInspESP32Setting.json" and
  // the push failed harmlessly. That failure was the only thing preventing
  // it: had the file been created -- the obvious "fix" for the log spam --
  // each connect would have overwritten the board's live NVS config with
  // whatever the host file happened to hold. That is the io_on_level wipe,
  // and on an active-low machine it inverts every output.
  //
  // The board is the source of truth. get_setup already syncs the host copy
  // from it, and the operator writes back deliberately via 存入 NVS.
  loadSettingFileOnConnect() { return false; }

  resyncRequiresAck() { return true; }

  // Ignore the real gate sensor while phantom pulses still register.
  // Lets a calibration pulse be fired into a clean lane -- no real part can
  // land in the middle of the measurement. Parts are not lost; they ride
  // round again.
  setGateDisable(on) { return this.sendP({ type: 'set_gate_disable', on: !!on }); }
  trigPhantomPulse() { return this.sendP({ type: 'trig_phantom_pulse' }); }

  // QA/bring-up: flip the frame<->object pairing algorithm at runtime.
  setPairingMode(mode) {
    deps.sendBPG('PD', 0, { type: 'PAIRING_MODE', mode, CONN_ID: this.CONN_ID },
      undefined, { resolve: (d) => d, reject: (d) => d });
  }

  // Try to un-wedge the serial link before resorting to reopening the port.
  //
  // The base watchdog reconnects after two unanswered PINGs, and reconnect
  // reopens the port, and opening the port toggles DTR, which hard-resets
  // the ESP32. So one corrupted byte on the wire costs a board reset, every
  // object in flight, and the whole run. Observed exactly that: a bad frame,
  // two silent PINGs, channel torn down 9s later, run over at 196 parts.
  //
  // The device parser leaves its framing-error state on one thing only --
  // the RESET_PACKET byte sequence -- which is why a reconnect works: it
  // sends RESET on the way up. The core can send that alone, without
  // touching the port, so try that first and give the link a cycle to come
  // back. Only if it stays silent is the expensive reset justified.
  _sendPing() {
    if (this.CONN_ID === undefined) return;

    if (this.PINGCount >= 2) {
      if (this._linkResyncTries < LINK_RESYNC_MAX) {
        this._linkResyncTries++;
        perifLog.warn('[link] PING unanswered -- trying RESET before reconnect',
          { attempt: this._linkResyncTries, max: LINK_RESYNC_MAX });
        publish(this.id, { state: 'SUSPECT', suspectSrc: 'local' });
        deps.sendBPG('PD', 0, { type: 'RESYNC', CONN_ID: this.CONN_ID },
          undefined, { resolve: (d) => d, reject: (d) => d });
        this.PINGCount = 0;      // give the link one more cycle to answer
        this.triggerPing();
        return;
      }
      perifLog.error('[link] RESET did not recover the link -- reconnecting '
        + '(this resets the board and ends the run)');
      this._linkResyncTries = 0;
    } else if (this.PINGCount === 0) {
      this._linkResyncTries = 0;   // link is healthy again
      if ((links[this.id] || {}).state === 'SUSPECT' && !((links[this.id] || {}).linkHealth || {}).suspect)
        publish(this.id, { state: 'CONNECTED', suspectSrc: null });
    }

    super._sendPing();
  }

  // Opening the serial port toggles DTR, which hard-resets the ESP32 -- see
  // the full account in the original script.jsx comment. Retry the config
  // resync until it actually arrives instead of trusting the first attempt.
  static RESYNC_RETRY_MS = 1000;
  static RESYNC_MAX_TRIES = 10;

  machineSetupReSync() {
    this._resyncTries = 0;
    this._resyncKick();
  }

  _resyncKick() {
    if (Object.keys(this.cfg || {}).length > 0) return;   // got it, stop
    if (this.CONN_ID === undefined) return;             // a reconnect will restart this
    if (this._resyncTries >= uInspESP32_API.RESYNC_MAX_TRIES) {
      log.warn('[uInspESP32] config resync gave up after', this._resyncTries, 'tries');
      return;
    }
    this._resyncTries++;
    super.machineSetupReSync();
    setTimeout(() => this._resyncKick(), uInspESP32_API.RESYNC_RETRY_MS);
  }

  // Everything the firmware's set_setup handler actually consumes
  // (LegacyFirmware.cpp, the JSON_SETIF_ABLE block). Anything else in a
  // get_setup reply is read-only runtime state.
  static SETTABLE_KEYS = [
    'plate_freq', 'plate_accel', 'speed_band_pct', 'min_detect_sep_us',
    'pulse_min_width', 'pulse_max_width',
    'gate_debounce_rise', 'gate_debounce_fall',
    // The gate's distance rejection. It was mapped in uinspCfg but missing
    // here, so the panel could display it and never write it.
    'min_detect_dist_um',
    'stepper_en_active', 'stepper_dir',
    'unanswered_stop_after',
    'host_timeout_ms', 'pulses_per_rev', 'plate_diameter_mm',
    'stage_pulse_offset', 'io_on_level',
    // Per-station widths in MICROSECONDS -- the device converts to ticks
    // against its own plate_freq, so a recipe survives a speed change.
    'stage_pulse_width_us',
    // Window CENTRES in ticks. 0 = that station keeps the forward-only
    // shape, where the offset is the window's start.
    'stage_pulse_center',
    // Calibration trigger pulse width, us.
    'cal_pulse_us',
    // The camera clock: match window and the recal/drift settings.
    'cam_match_window_us', 'cam_recal_idle_ms', 'cam_drift_comp',
    // The match window expressed as what it actually is -- a position
    // tolerance. Settable on the device since 2026-08-11.
    'cam_match_tolerance_mm',
    'report_match_ts', 'report_match_pcnt',
    // skip_policy_mode replaced auto_rate + unanswered_policy (2026-08-08).
    'skip_policy_mode',
    'machine_id', 'CAM1_Tags', 'CAM2_Tags', 'persist',
  ];

  // This board owns its own configuration -- the sync is READ-ONLY: values
  // are stored for display and never echoed. Settable keys land in cfg,
  // runtime state in deviceState. Writing config is an explicit act
  // (machineSetupUpdateAndPersist / setMachineId), not a side effect of
  // connecting. See the original script.jsx comment for the full history.
  machineSetupUpdate(newMachineInfo, doReplace = false, push = false) {
    const settable = {}, readonly = {};
    Object.keys(newMachineInfo || {}).forEach((k) => {
      if (uInspESP32_API.SETTABLE_KEYS.indexOf(k) >= 0) settable[k] = newMachineInfo[k];
      else readonly[k] = newMachineInfo[k];
    });
    this.deviceState = doReplace ? readonly : { ...this.deviceState, ...readonly };
    this.cfg = doReplace ? settable : { ...this.cfg, ...settable };

    // Actively clear, not merely "leave unset": machineSetupReSync() assigns
    // `this.machineSetup = ret` BEFORE it calls this method, and the base's
    // connect() pushes machineSetup directly whenever the field is set, so
    // the only way to guarantee a reconnect stays read-only is to keep the
    // field empty. The UI reads the published copy, not the instance field.
    this.machineSetup = undefined;

    publish(this.id, { machineSetup: this.cfg, deviceState: this.deviceState });
    if (push !== true) return;
    this.send(uinspRegroup({ type: 'set_setup', ...settable }),
      (ret) => log.debug('[machine-setup] set_setup ack', ret),
      (e) => log.warn('[machine-setup] set_setup failed', e));
  }

  getMachineSetup() { return this.cfg; }
  getDeviceState() { return this.deviceState; }

  // ---- inspection mode -------------------------------------------------

  enterInspMode() { return this.sendP({ type: 'enter_insp_mode' }); }
  exitInspMode() { return this.sendP({ type: 'exit_insp_mode' }); }
  clearError() { return this.sendP({ type: 'clear_error' }); }
  clearErrorHistory() { return this.sendP({ type: 'clear_error_history' }); }

  // ---- sorting ---------------------------------------------------------

  // cat 1 -> SEL1, 2 -> SEL2. tid comes from the bT trigger message that
  // announced the object, so a late result cannot be applied to the wrong
  // part.
  report(tid, cat) { return this.sendP({ type: 'report', tid, cat }); }

  // count of -1 disables the countdown; reaching zero faults the machine.
  setSel1Countdown(count) { return this.sendP({ type: 'set_sel1_cd', count }); }
  getSel1Countdown() { return this.sendP({ type: 'get_sel1_cd' }); }

  // ---- stepper / diagnostics -------------------------------------------

  // Steady backlight for camera setup. Polarity-aware on the firmware side
  // -- do NOT use pin_on/pin_off, those are raw writes and this machine's
  // io_on_level makes ON active-low, so they invert.
  light(ch, on, timeout_ms) { return this.sendP({ type: 'light', ch, on, timeout_ms }); }

  // One camera trigger, no pipeline object behind it. This is the shutter,
  // not the sorter.
  trigCamPulse() { return this.sendP({ type: 'trig_cam_pulse' }); }

  // A single lit frame, for setting up on a stopped plate. Light first,
  // settle, shoot, then drop it; the timeout is a backstop.
  camSnapWithLight(ch = 'L1A', settle_ms = 120) {
    return this.light(ch, true, 4000)
      .then(() => new Promise((r) => setTimeout(r, settle_ms)))
      .then(() => this.trigCamPulse())
      .then((r) => new Promise((res) => setTimeout(() => res(r), settle_ms)))
      .then((r) => this.light(ch, false, 0).then(() => r),
        (e) => this.light(ch, false, 0).then(() => { throw e; }));
  }

  stepperEnable() { return this.sendP({ type: 'stepper_enable' }); }
  stepperDisable() { return this.sendP({ type: 'stepper_disable' }); }

  // Station placement. jogGoto is ABSOLUTE on purpose: the device computes
  // the relative move -- a browser cannot know where the plate stopped.
  jogArm(freq) { return this.sendP({ type: 'jog_arm', ...(freq ? { freq } : {}) }); }
  jogGoto(offset, freq) { return this.sendP({ type: 'jog', offset, ...(freq ? { freq } : {}) }); }
  jogEnd() { return this.sendP({ type: 'jog_end' }); }

  resetRunningStat() { return this.sendP({ type: 'reset_running_stat' }); }
  getRunningStat() {
    return this.sendP({ type: 'get_running_stat' }).then((ret) => {
      this.runningStat = ret;
      publish(this.id, { runningStat: ret });
      return ret;
    });
  }

  // The enum lives in the firmware, so its text should come from there too.
  getStateNames() { return this.sendP({ type: 'get_state_names' }); }

  // Promise flavour of get_setup, for callers that need to CONFIRM a write
  // landed rather than fire and hope. Flattened, like machineSetupReSync's
  // copy -- callers read flat names off this.
  getSetupP() { return this.sendP({ type: 'get_setup' }).then(uinspFlatten); }

  // ---- persistence -----------------------------------------------------

  // Persisting is deliberate, not automatic -- offset probing during setup
  // should not burn flash cycles.
  saveSetupToDevice() { return this.sendP({ type: 'save_setup' }); }
  clearSavedSetupOnDevice() { return this.sendP({ type: 'clear_saved_setup' }); }

  // push=true: this is the explicit write path. Everything else -- connect,
  // resync, file load -- is read-only, see machineSetupUpdate.
  machineSetupUpdateAndPersist(newMachineInfo) {
    this.machineSetupUpdate(newMachineInfo, false, true);
    return this.saveSetupToDevice();
  }

  setMachineId(machine_id) { return this.sendP({ type: 'set_setup', machine_id, persist: true }); }
  getMachineId() { return (this.machineSetup || {}).machine_id; }

  // True when the running config came from the board's NVS rather than the
  // compiled fallback.
  isConfigFromNVS() { return this.deviceState.cfg_from_nvs === true; }
}

// ============================================================================
// Generic serial peripheral with no device-specific status handling.
// ============================================================================

export class GenPerif_API extends Perif_API_Base {
  constructor(id, settingFilePath, pg_id_channel = 10025) {
    super(id, settingFilePath, pg_id_channel);
  }

  resyncRequiresAck() { return true; }

  // Deliberately does NOT publish machineStatus: the pre-refactor
  // GenPerif_API decoded the PING reply and dropped it on the floor, and
  // SLID_API inherits from here. Publishing it would be a behaviour change
  // to a machine in production, so it stays opt-in per device.
  onPingStatus(machineStatus) {}
}

// ============================================================================
// SLID -- slope-inspection device with the browser-side EM_STOP rule engine.
// ============================================================================

export class SLID_API extends GenPerif_API {
  constructor(id, settingFilePath, pg_id_channel = 10025) {
    super(id, settingFilePath, pg_id_channel);
    this.checkInfoInterval = undefined;
    this.checkInfoListenerDict = {};
    this.is_in_EM_STOP = false;
    this.EM_STOP_src_list = [];
    this.no_obj_detected_time_ms = -1;
    this.no_ava_detected_time_ms = -1;
    this.EM_STOP_Rule = this.readLocalstorage_SLID_EM_STOP_RULE({//set default value
      enable_EM_STOP: false,
      no_obj_detected_time_max_ms: 60 * 5 * 1000,
      no_ava_detected_time_max_ms: 60 * 5 * 1000,
      SNG_Max: 10,
      CNG_Max: 20,
      consecutive_SNG_Max: 3,
      consecutive_CNG_Max: 6,
      fuzzy_consecutive_SNG_Max: -1,
      fuzzy_consecutive_CNG_Max: -1,
    });
  }

  readLocalstorage_SLID_EM_STOP_RULE(defaultRule) {
    const readRule = window.localStorage.getItem('SLID_EM_STOP_RULE');
    if (readRule === null) {
      this.saveLocalstorage_SLID_EM_STOP_RULE(defaultRule);
      return defaultRule;
    }
    try { return { ...defaultRule, ...JSON.parse(readRule) }; }
    catch (e) { return defaultRule; }
  }

  saveLocalstorage_SLID_EM_STOP_RULE(rule = this.EM_STOP_Rule) {
    window.localStorage.setItem('SLID_EM_STOP_RULE', JSON.stringify(rule));
  }

  connect(connInfo) {
    if (this.checkInfoInterval === undefined) {//start scan loop
      this.checkInfoInterval = window.setInterval(() => this.checkInfoState(), 1000);
    }
    super.connect(connInfo);
  }

  reload_EM_STOP_RULE() {
    this.EM_STOP_Rule = this.readLocalstorage_SLID_EM_STOP_RULE();
  }

  update_EM_STOP_RULE(newRule) {
    this.EM_STOP_Rule = { ...this.EM_STOP_Rule, ...newRule };
    this.saveLocalstorage_SLID_EM_STOP_RULE(this.EM_STOP_Rule);

    const reportStatisticState = GetObjElement(deps.getState(), ['UIData', 'edit_info', 'reportStatisticState']);
    Object.keys(this.checkInfoListenerDict).forEach((key) => {
      this.checkInfoListenerDict[key](this, reportStatisticState);
    });
  }

  tmp_EM_STOP_RULE(newRule) {
    this.EM_STOP_Rule = { ...this.EM_STOP_Rule, ...newRule };
  }

  checkInfoListenerKeyUsed(key) { return this.checkInfoListenerDict[key] !== undefined; }
  checkInfoListenerAdd(key, cb) { this.checkInfoListenerDict[key] = cb; }
  checkInfoListenerRemove(key) { delete this.checkInfoListenerDict[key]; }

  checkInfoState() {
    const reportStatisticState = GetObjElement(deps.getState(), ['UIData', 'edit_info', 'reportStatisticState']);
    const c_state = GetObjElement(deps.getState(), ['UIData', 'c_state']);
    const m_state = xstate_GetCurrentMainState(c_state);

    if (this.p_state != m_state.state) {
      if (m_state.state == UIAct.UI_SM_STATES.INSP_MODE) this.clear_EM_STOP_state();
      this.p_state = m_state.state;
    }

    if (m_state.state == UIAct.UI_SM_STATES.INSP_MODE && this.EM_STOP_Rule.enable_EM_STOP == true) {
      this.no_obj_detected_time_ms = -1;
      if (this.reportCount == reportStatisticState.reportCount) {//no change
        if (this.noreport_timestamp === undefined) this.noreport_timestamp = Date.now();
        else this.no_obj_detected_time_ms = Date.now() - this.noreport_timestamp;
      } else {
        this.noreport_timestamp = undefined;
        this.reportCount = reportStatisticState.reportCount;
      }

      this.no_ava_detected_time_ms = -1;
      const ava_report_count = reportStatisticState.reportCount - reportStatisticState.emptyReportCount;
      if (this.ava_report_count == ava_report_count) {//no change
        if (this.latest_ava_report_timestamp === undefined) this.latest_ava_report_timestamp = Date.now();
        else this.no_ava_detected_time_ms = Date.now() - this.latest_ava_report_timestamp;
      } else {
        this.latest_ava_report_timestamp = undefined;
        this.ava_report_count = ava_report_count;
      }

      if (this.is_in_EM_STOP == false) {//check status to EM_STOP
        let needToTrigEM_STOP = false;
        const EM_STOP_src_list = [];
        if (this.EM_STOP_Rule.no_obj_detected_time_max_ms > 0 && this.no_obj_detected_time_ms >= this.EM_STOP_Rule.no_obj_detected_time_max_ms) {
          EM_STOP_src_list.push('no_obj_detected_time_ms');
          needToTrigEM_STOP = true;
        }
        if (this.EM_STOP_Rule.no_ava_detected_time_max_ms > 0 && this.no_ava_detected_time_ms >= this.EM_STOP_Rule.no_ava_detected_time_max_ms) {
          EM_STOP_src_list.push('no_ava_detected_time_ms');
          needToTrigEM_STOP = true;
        } else {
          reportStatisticState.statisticValue.measureList.forEach((msure) => {
            const stat_sp = msure.statistic.sp;//find every

            if (this.EM_STOP_Rule.SNG_Max > 0 && stat_sp.SNG_count >= this.EM_STOP_Rule.SNG_Max) {
              EM_STOP_src_list.push('SNG_count');
              needToTrigEM_STOP = true; return;
            }
            if (this.EM_STOP_Rule.CNG_Max > 0 && stat_sp.CNG_count >= this.EM_STOP_Rule.CNG_Max) {
              EM_STOP_src_list.push('CNG_count');
              needToTrigEM_STOP = true; return;
            }
            if (this.EM_STOP_Rule.consecutive_SNG_Max > 0 && stat_sp.consecutive_SNG_count >= this.EM_STOP_Rule.consecutive_SNG_Max) {
              EM_STOP_src_list.push('consecutive_SNG_count');
              needToTrigEM_STOP = true; return;
            }
            if (this.EM_STOP_Rule.consecutive_CNG_Max > 0 && stat_sp.consecutive_CNG_count >= this.EM_STOP_Rule.consecutive_CNG_Max) {
              EM_STOP_src_list.push('consecutive_CNG_count');
              needToTrigEM_STOP = true; return;
            }
            if (this.EM_STOP_Rule.fuzzy_consecutive_SNG_Max > 0 && stat_sp.fuzzy_consecutive_SNG_count >= this.EM_STOP_Rule.fuzzy_consecutive_SNG_Max) {
              EM_STOP_src_list.push('fuzzy_consecutive_SNG_count');
              needToTrigEM_STOP = true; return;
            }
            if (this.EM_STOP_Rule.fuzzy_consecutive_CNG_Max > 0 && stat_sp.fuzzy_consecutive_CNG_count >= this.EM_STOP_Rule.fuzzy_consecutive_CNG_Max) {
              EM_STOP_src_list.push('fuzzy_consecutive_CNG_count');
              needToTrigEM_STOP = true; return;
            }
          });
        }
        if (needToTrigEM_STOP) {
          this.EM_STOP_src_list = EM_STOP_src_list;
          this.trigger_EM_STOP();
        }
      }
    } else {
      this.noreport_timestamp = undefined;
    }

    Object.keys(this.checkInfoListenerDict).forEach((key) => {
      this.checkInfoListenerDict[key](this, reportStatisticState);
    });
  }

  clear_EM_STOP_state() {
    this.noreport_timestamp = undefined;
    this.latest_ava_report_timestamp = undefined;
    this.is_in_EM_STOP = false;
    this.EM_STOP_src_list = [];
  }

  trigger_EM_STOP(keep_ms) {
    this.is_in_EM_STOP = true;

    if (keep_ms === undefined) keep_ms = this.machineSetup.EM_STOP_keep_ms;//try to find the keep time from setup first
    if (keep_ms === undefined) keep_ms = 2000;//if still unset use 2s keep time
    this.send({ type: 'EM_STOP', keep_ms },
      (ret) => {}, (e) => console.log(e));
  }
}
