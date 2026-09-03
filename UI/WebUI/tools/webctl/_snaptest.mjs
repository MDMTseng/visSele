// Press the lit-snapshot path through the app's own store, and check the core
// is still alive afterwards -- the first version of cam_snap_lit crashed the
// core on the first press (a non-recursive mutex relocked on the same thread).
import { makeCtl, toMain, dismissCamModal, freshPage } from './lib_enter.mjs';
import { makeProbe, sleep } from './_rf_lib.mjs';
const ctl = makeCtl('http://127.0.0.1:8765'); const { ev } = ctl; const P = makeProbe(ev);
await freshPage(ctl, 'http://127.0.0.1:8081/');
await P.waitFor('app', async () => (await ev(`typeof window.__GP_STORE__`)) === 'object', { timeout: 40000 });
await toMain(ctl); await dismissCamModal(ctl); await sleep(2000);

const CMD = process.env.SNAP_CMD || 'cam_snap_lit';
const snap = () => ev(`(function(){
  window.__SNAPR__ = 'pending';
  var st = window.__GP_STORE__;
  var CORE_ID = st.getState().ConnInfo.CORE_ID;
  st.dispatch({ type:'MW_API_CALL', id: CORE_ID, method:'send',
    param: { tl:'SC', prop:0, data:{ type:'${CMD}' },
      promiseCBs: {
        resolve: function(pkts){
          var p = (pkts||[]).find(function(q){ return q && q.data && q.data.type==='${CMD}'; });
          window.__SNAPR__ = p ? JSON.stringify(p.data) : 'no cam_snap_lit packet';
        },
        reject: function(e){ window.__SNAPR__ = 'REJECT ' + String(e); } } } });
  return 'sent';})()`);

console.log('sending cam_snap_lit ->', await snap());
for (let i = 0; i < 12; i++) {
  await sleep(700);
  const r = await ev(`window.__SNAPR__`);
  if (r !== 'pending') { console.log('reply:', r); break; }
  if (i === 11) console.log('no reply in 8s');
}
await sleep(4500);   // past the 3s restore deadline
console.log('core still alive:', await ev(`(function(){
  var ws=(window.__GP_WS__&&window.__GP_WS__.inst&&window.__GP_WS__.inst.websocket)||null;
  return ws ? ws.readyState : 'no ws'; })()`), '(1 = open)');
process.exit(0);
