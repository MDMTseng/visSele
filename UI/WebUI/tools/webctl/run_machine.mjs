// Cold WebUI -> recipe loaded -> Inspection UI -> PLATE RUNNING -> reports on
// the wire. The whole "make the machine actually run" sequence, as one command
// that passes or fails.
//
//   node run_machine.mjs [--url http://localhost:8081/] [--def <path>] [--mode 測試]
//                        [--min-rp 5] [--secs 10] [--shot out.png]
//
// Why this exists: every step of it looks fine while the machine does nothing.
// The recipe loads, the station ROI applies, the state machine reports
// INSP_MODE_NEUTRAL -- and the plate is stopped, so not one frame is captured.
// The only honest check is the wire: subscribe to the core's stream and count
// RP/IM. That is what --min-rp asserts.
//
// Requires webctld running against the same URL:
//   WEBCTL_URL=http://localhost:8081 node webctld.mjs
import path from 'node:path';
import WebSocket from 'ws';
import { makeCtl, toMain, dismissCamModal, enterInspection, loadRecipe, startMachine, sleep }
  from './lib_enter.mjs';

const arg = (name, dflt) => {
  const hit = process.argv.find((a) => a === `--${name}` || a.startsWith(`--${name}=`));
  if (!hit) return dflt;
  if (hit.includes('=')) return hit.split('=').slice(1).join('=');
  return process.argv[process.argv.indexOf(hit) + 1] ?? dflt;
};

const URL_  = arg('url', 'http://localhost:8081/');
// Forward slashes on purpose: Windows accepts them, and a backslash path inside
// a JS string quietly becomes escape sequences -- this default once resolved to
// "C:Usersw2110Documents...<TAB>est1" (\U \w \t), the recipe silently failed to
// load, and it surfaced three steps later as a disabled play button with
// data-reason="no-def". Bitten twice: the patch script that fixed it hit the
// identical trap in Python.
const DEF   = arg('def', 'C:/Users/w2110/Documents/workspace/visSele/InspectionCore/Core0_1/data/test1');
const MODE  = arg('mode', '測試');
const MINRP = Number(arg('min-rp', 5));
const SECS  = Number(arg('secs', 10));
const SHOT  = arg('shot', null);
const CORE  = arg('core', 'ws://127.0.0.1:4090');

const ctl = makeCtl();
const step = (n, s) => console.log(`[${n}] ${s}`);
const log  = (s) => console.log('    ' + s);

// Count what the core actually emits, for SECS seconds.
function measure(secs) {
  return new Promise((resolve) => {
    const BPG = 9, enc = new TextEncoder();
    const frame = (t, p, pg, o) => {
      const b = enc.encode(o == null ? '' : JSON.stringify(o));
      const u = new Uint8Array(BPG + b.length + 1);
      u[0] = t.charCodeAt(0); u[1] = t.charCodeAt(1); u[2] = p;
      u[3] = pg >> 8; u[4] = pg & 255;
      const l = u.length - BPG;
      u[5] = l >>> 24; u[6] = (l >> 16) & 255; u[7] = (l >> 8) & 255; u[8] = l & 255;
      u.set(b, BPG); return u;
    };
    const ws = new WebSocket(CORE); ws.binaryType = 'arraybuffer';
    let pg = 1, rp = 0, im = 0, objects = 0, empty = 0;
    ws.on('open', () => setTimeout(() => ws.send(frame('SB', 0, pg++, { stream: true })), 200));
    ws.on('error', () => resolve({ rp: 0, im: 0, objects: 0, empty: 0, error: 'ws' }));
    ws.on('message', (d) => {
      if (!(d instanceof ArrayBuffer)) d = d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength);
      const b = new Uint8Array(d), t = String.fromCharCode(b[0], b[1]);
      if (t === 'HR') { ws.send(frame('HR', 0, pg++, { a: ['d'] })); return; }
      if (t === 'IM') im++;
      if (t === 'RP') {
        rp++;
        try {
          const j = JSON.parse(new TextDecoder().decode(b.subarray(BPG)));
          const r = j.reports?.[0]?.reports || [];
          objects += r.length; if (!r.length) empty++;
        } catch {}
      }
    });
    setTimeout(() => { try { ws.close(); } catch {} resolve({ rp, im, objects, empty }); }, secs * 1000);
  });
}

try {
  step(1, 'goto ' + URL_);
  await ctl.api('/goto', { url: URL_ });
  step(2, 'settling the state machine at MAIN');
  await toMain(ctl);
  step(3, 'clearing the camera-reconnect modal if present');
  await dismissCamModal(ctl);
  step(4, 'loading recipe ' + DEF);
  log('loaded def: ' + await loadRecipe(ctl, DEF));
  step(5, `entering the Inspection UI (mode ${MODE})`);
  await enterInspection(ctl, { mode: MODE, log });
  step(6, 'starting the plate');
  await startMachine(ctl, { log });
  step(7, `measuring for ${SECS}s`);
  // Measure through the UI's OWN counters first.
  //
  // The core serves one client at a time now, so subscribing a second socket to
  // count packets -- what this used to do -- gets refused and reports zero,
  // which reads exactly like "the machine is not running". The UI is already
  // connected and already counts every report it processes, so ask it. This is
  // also the more honest check: it proves the packets reached the OPERATOR'S
  // screen, not merely that the core emitted them.
  //
  // Falls back to the wire when the store handle is absent (production bundle)
  // or when INSP_ALLOW_MULTI_CLIENT=1 makes a second socket legal again.
  const rc = async () => {
    try {
      // Explicit -1 for "no handle", NOT `|| -1`: a legitimate count of 0 --
      // which is exactly what step 1's reload leaves behind -- would fall
      // through to the wire, and the wire now reads 0 for a REFUSED second
      // client. That reported a perfectly healthy machine as stopped.
      const r = await ctl.ev('(function(){if(!window.__GP_STORE__)return -1;var rs=window.__GP_STORE__.getState().UIData.edit_info.reportStatisticState;return (rs&&typeof rs.reportCount==="number")?rs.reportCount:-1;})()');
      return Number(r);
    } catch { return -1; }
  };
  let m;
  const rc0 = await rc();
  if (rc0 >= 0) {
    await sleep(SECS * 1000);
    const rc1 = await rc();
    m = { rp: rc1 - rc0, im: -1, objects: -1, empty: -1, via: 'ui' };
  } else {
    m = { ...(await measure(SECS)), via: 'wire' };
  }
  const rps = (m.rp / SECS).toFixed(1);
  log(m.via === 'ui'
    ? `reports into the UI: ${m.rp} (${rps}/s)`
    : `RP=${m.rp} (${rps}/s)  IM=${m.im}  objects=${m.objects}  empty=${m.empty}`);
  if (SHOT) { await ctl.api('/shot', { path: path.resolve(SHOT) }); log('shot: ' + SHOT); }
  if (m.rp < MINRP) {
    console.log(`FAIL: expected at least ${MINRP} reports in ${SECS}s -- the UI is up but the machine is not running.`);
    console.log('      check the panel: "STOP · 盤停止" means the plate never started; a device plate_freq of 0 makes the run button a no-op.');
    process.exit(1);
  }
  console.log(`OK: machine running -- ${rps} reports/s with ${m.objects} objects seen.`);
  process.exit(0);
} catch (e) {
  console.log('FAILED: ' + (e && e.message ? e.message : String(e)));
  process.exit(1);
}
