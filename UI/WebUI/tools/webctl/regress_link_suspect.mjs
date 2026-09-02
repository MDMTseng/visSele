// THE STATUS-BAR ICON MUST STAY RED WHILE THE DEVICE IS SILENT.
//
// Reported from the bench: uInspESP32 goes away and the right-hand icon never
// changes colour -- the disconnection is only visible after opening the modal,
// which reads a different signal entirely (the panel's own get_running_stat
// poll). The icon reads the perif link store, and that store was repainting
// itself CONNECTED once every seven seconds: the RESYNC escalation zeroes
// PINGCount to buy the link another cycle, and the recovery branch read that
// same zero as "a PING was answered".
//
// So this does not look at pixels. It makes the device silent WITHOUT touching
// the serial port -- replacing triggerPing with a no-op, so no reply is ever
// counted -- and samples the link state twice a second. Once SUSPECT appears
// it must not go back.
//
// The window stops short of LINK_RESYNC_MAX so the watchdog never reaches its
// reconnect, which would reopen the port and reset the board.
import { makeCtl, toMain, dismissCamModal, freshPage } from './lib_enter.mjs';
import { sleep } from './_rf_lib.mjs';

const { ev } = makeCtl('http://127.0.0.1:8765');
const ctl = makeCtl('http://127.0.0.1:8765');
const URL = process.argv[2] || 'http://127.0.0.1:8083/';
const WINDOW_MS = 14000;

// COLD, always. freshPage reuses a healthy open page, and this suite exists to
// test a change to the bundle -- reusing meant the first run of this file
// measured the build it was written to replace, and said FAIL about code that
// was already fixed.
process.env.WEBCTL_COLD = '1';
await freshPage(ctl, URL);
await toMain(ctl);
await dismissCamModal(ctl);

const start = await ev(`(function(){
  var links = window.__GP_PERIF_LINKS__ ? window.__GP_PERIF_LINKS__() : {};
  var id = Object.keys(links).find(function(k){ return /ESP32/i.test(k); });
  if (!id) return 'no ESP32 link (' + Object.keys(links).join(',') + ')';
  if ((links[id]||{}).state !== 'CONNECTED')
    return 'link is ' + (links[id]||{}).state + ', not CONNECTED -- nothing to make silent';
  var api = window.__GP_PERIF__(id);
  if (!api) return 'no api for ' + id;
  window.__LS__ = { id: id, samples: [], t0: Date.now() };
  api.__realTriggerPing = api.triggerPing;
  api.triggerPing = function(){};            // sent into the void; never answered
  window.__LS_TIMER__ = setInterval(function(){
    var l = window.__GP_PERIF_LINKS__()[window.__LS__.id] || {};
    window.__LS__.samples.push([Math.round((Date.now()-window.__LS__.t0)/100)/10, l.state]);
  }, 500);
  return 'silenced ' + id;
})()`);
console.log(start);
if (!String(start).startsWith('silenced')) process.exit(2);

await sleep(WINDOW_MS);

const out = JSON.parse(await ev(`(function(){
  clearInterval(window.__LS_TIMER__);
  var api = window.__GP_PERIF__(window.__LS__.id);
  if (api.__realTriggerPing) { api.triggerPing = api.__realTriggerPing; delete api.__realTriggerPing; }
  return JSON.stringify(window.__LS__.samples);
})()`));

const states = out.map((s) => s[1]);
const firstSuspect = states.indexOf('SUSPECT');
console.log(out.map((s) => s[0] + 's ' + s[1]).join('\n'));

let fail = null;
if (firstSuspect < 0) fail = 'never went SUSPECT in ' + (WINDOW_MS/1000) + 's of silence';
else {
  const back = states.slice(firstSuspect).findIndex((s) => s === 'CONNECTED');
  if (back >= 0)
    fail = 'went back to CONNECTED at ' + out[firstSuspect + back][0]
         + 's without a single PING being answered -- the icon turns green on a dead link';
}
if (fail) { console.log('FAIL: ' + fail); process.exit(1); }
console.log('PASS: SUSPECT from ' + out[firstSuspect][0] + 's and held for the rest of the window');
process.exit(0);
