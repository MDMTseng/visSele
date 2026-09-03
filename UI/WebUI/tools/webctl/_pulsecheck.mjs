// Did the board actually drive the camera line? cam_pcnt.dev_pulses is its own
// count of CAM1 edges it drove -- the one number here that does not depend on
// the camera having answered.
import { makeCtl, toMain, dismissCamModal, freshPage } from './lib_enter.mjs';
import { makeProbe, sleep } from './_rf_lib.mjs';
const ctl = makeCtl('http://127.0.0.1:8765'); const { ev } = ctl; const P = makeProbe(ev);
await freshPage(ctl, 'http://127.0.0.1:8081/');
await P.waitFor('app', async () => (await ev(`typeof window.__GP_STORE__`)) === 'object', { timeout: 40000 });
await toMain(ctl); await dismissCamModal(ctl); await sleep(2500);

const stat = () => ev(`(function(){
  var links = window.__GP_PERIF_LINKS__ ? window.__GP_PERIF_LINKS__() : {};
  var id = Object.keys(links).find(function(k){ return /ESP32/i.test(k); });
  var api = window.__GP_PERIF__ && window.__GP_PERIF__(id);
  if (!api) return Promise.resolve('no api');
  return api.getRunningStat().then(function(s){
    return JSON.stringify({ pulses: s && s.cam_pcnt && s.cam_pcnt.dev_pulses,
                            state: s && s.state,
                            frames: s && s.cam_sync && s.cam_sync.established });
  }, function(e){ return 'ERR ' + e; });
})()`);
console.log('before:', await stat());
await ev(`window.__GP_STORE__.dispatch({ type:'MW_API_CALL',
  id: window.__GP_STORE__.getState().ConnInfo.CORE_ID, method:'send',
  param:{ tl:'SC', prop:0, data:{ type:'cam_snap_lit' } } })`);
await sleep(2000);
console.log('after :', await stat());
process.exit(0);
