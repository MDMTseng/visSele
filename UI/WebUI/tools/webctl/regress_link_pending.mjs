// CONNECTED IS THE PORT, NOT THE BOARD.
//
// From page load the peripheral link goes CONNECTING -> CONNECTED the moment
// the core reports the serial port open -- before the device has answered a
// single PING and before its configuration has been read back. Painting that
// green says the machine is ready to sort when nobody has heard from it.
//
// So: reload cold and sample fast. There must be an amber (WS_PENDING) window
// before green, the operator strip must be blocked during it, and both must
// end -- a pending state that never resolves is a different bug, and one this
// check would otherwise call a pass.
import { makeCtl, freshPage } from './lib_enter.mjs';
import { sleep } from './_rf_lib.mjs';

const { ev } = makeCtl('http://127.0.0.1:8765');
const ctl = makeCtl('http://127.0.0.1:8765');
const URL = process.argv[2] || 'http://127.0.0.1:8083/';

process.env.WEBCTL_COLD = '1';          // the whole point is the startup window
await freshPage(ctl, URL);

// Sampling starts as early as the store exists, not after the UI has settled --
// toMain/dismissCamModal would spend the window this check is looking for.
await ev(`(function(){
  window.__LP__ = { samples: [], t0: Date.now() };
  window.__LP_T__ = setInterval(function(){
    if (!window.__GP_PERIF_LINKS__) return;
    var links = window.__GP_PERIF_LINKS__();
    var id = Object.keys(links).find(function(k){ return /ESP32/i.test(k); });
    if (!id) return;
    var l = links[id] || {};
    // Two separate facts. The operator strip lives on the inspection screen,
    // so on the landing page it is simply not mounted -- and "not mounted"
    // must not be reported as "not blocked", which is how an overlay check
    // passes forever without ever having run.
    var strip = document.querySelector('[data-testid="uinsp-mini"]');
    var blocked = !!document.querySelector('[data-testid="uinsp-mini-blocked"]');
    window.__LP__.samples.push([Math.round((Date.now()-window.__LP__.t0)/100)/10,
                                l.state, l.pingSeen, l.cfgSeen, !!strip, blocked]);
  }, 100);
  return 'sampling';
})()`);

await sleep(20000);

const rows = JSON.parse(await ev(`(function(){
  clearInterval(window.__LP_T__); return JSON.stringify(window.__LP__.samples);
})()`));

const pend = (r) => r[1] === 'CONNECTED' && (r[2] === false || r[3] === false);
const green = (r) => r[1] === 'CONNECTED' && r[2] !== false && r[3] !== false;
const iP = rows.findIndex(pend), iG = rows.findIndex(green);
const shown = rows.filter((r, i) => i === 0 || JSON.stringify(r.slice(1)) !== JSON.stringify(rows[i-1].slice(1)));
shown.forEach((r) => console.log(r[0] + 's ' + r[1] + ' ping=' + r[2]
  + ' cfg=' + r[3] + ' strip=' + r[4] + ' blocked=' + r[5]));

let fail = null;
if (rows.length === 0) fail = 'no samples -- the ESP32 link never appeared';
else if (iP < 0) fail = 'never PENDING: the icon went straight to green on a port open';
else if (iG < 0) fail = 'still PENDING after 20s -- it never resolved';
else if (iG < iP) fail = 'green at ' + rows[iG][0] + 's, before the pending window';
else {
  const unblocked = rows.slice(iP, iG).find((r) => r[4] === true && r[5] === false);
  if (unblocked) fail = 'the operator strip was mounted and pressable at '
                      + unblocked[0] + 's while the link was still pending';
}
if (fail) { console.log('FAIL: ' + fail); process.exit(1); }
const stripSeen = rows.slice(iP, iG).some((r) => r[4] === true);
console.log(`PASS: pending ${rows[iP][0]}s -> ready ${rows[iG][0]}s`
  + (stripSeen ? ', operator strip blocked throughout'
               : ' (operator strip NOT MOUNTED on this screen -- the overlay was not exercised;'
                 + ' run this from the inspection screen to cover it)'));
process.exit(0);
