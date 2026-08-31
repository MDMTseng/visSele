import { makeCtl, toMain, dismissCamModal, freshPage } from './lib_enter.mjs';
import { sleep } from './_rf_lib.mjs';
const { ev } = makeCtl('http://127.0.0.1:8765'); const ctl = makeCtl('http://127.0.0.1:8765');
const on = process.argv[2] !== 'off';
await freshPage(ctl, 'http://127.0.0.1:8081/'); await toMain(ctl); await dismissCamModal(ctl);
console.log('perif ids:', await ev(`JSON.stringify((function(){var s=window.__GP_STORE__.getState();
  return { uInsp: s.UIData && s.UIData.System_Setting && Object.keys(s.UIData.System_Setting).filter(function(k){return /API_ID|uInsp/i.test(k);}),
           links: window.__GP_PERIF_LINKS__ ? Object.keys(window.__GP_PERIF_LINKS__()||{}) : 'none' };})())`));
console.log('light:', await ev(`(function(){
  var links = window.__GP_PERIF_LINKS__ ? window.__GP_PERIF_LINKS__() : {};
  // The backlight lives on the ESP32 board, not on the older uInsp link.
  var id = Object.keys(links).find(function(k){ return /ESP32/i.test(k); }) || Object.keys(links)[0];
  var api = window.__GP_PERIF__ && window.__GP_PERIF__(id);
  if (!api) return 'no perif api (id=' + id + ')';
  if (typeof api.light !== 'function')
    return 'id=' + id + ' has no light(); methods=' + Object.getOwnPropertyNames(Object.getPrototypeOf(api)).join(',');
  window.__LT__ = 'sent';
  ['L1A','L2A'].forEach(function(ch){
    api.light(ch, ${on}, ${on ? 300000 : 0}).then(function(r){ window.__LT__ = JSON.stringify(r); },
                                                  function(e){ window.__LT__ = 'ERR ' + e; });
  });
  return 'id=' + id; })()`));
await sleep(2500);
console.log('reply:', await ev(`window.__LT__`));
process.exit(0);
