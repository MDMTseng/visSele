import WebSocket from 'ws';
const enc = new TextEncoder();
const frame = (t, p, g, o) => { const b = enc.encode(JSON.stringify(o));
  const u = new Uint8Array(10 + b.length);
  u[0]=t.charCodeAt(0); u[1]=t.charCodeAt(1); u[2]=p; u[3]=g>>8; u[4]=g&255;
  const l=u.length-9; u[5]=l>>>24; u[6]=(l>>16)&255; u[7]=(l>>8)&255; u[8]=l&255;
  u.set(b,9); return u; };
const ws = new WebSocket('ws://127.0.0.1:4090'); ws.binaryType='arraybuffer';
let pg=1;
ws.on('open', () => { console.log('connected');
  // Ask straight away: the core does not always volunteer an HR to a bare client.
  setTimeout(() => ws.send(frame('SC',0,pg++,{type:'cam_trigger_probe'})), 200); });
ws.on('error', (e) => { console.log('ws error', e.message); process.exit(1); });
ws.on('message', (d) => {
  const b=new Uint8Array(d), t=String.fromCharCode(b[0],b[1]);
  if (t!=='IM') console.log('  <-', t);
  if (t==='HR') { ws.send(frame('HR',0,pg++,{a:['d']}));
    setTimeout(()=>ws.send(frame('SC',0,pg++,{type:'cam_trigger_probe'})),300); return; }
  const s=new TextDecoder().decode(b.subarray(9)).replace(/\0.*$/,'');
  if (t==='SC') console.log('SC reply:', s.slice(0,200));
  if (s.includes('cam_trigger_probe')) {
    const j=JSON.parse(s);
    const MODE={0:'Off (free-run)',1:'On (needs a trigger)'};
    const SRC={0:'LINE0 (board)',7:'SOFTWARE'};
    console.log('camera says:', JSON.stringify(j));
    const core = j.TriggerMode===0 ? 0 : (j.TriggerMode===1 ? (j.TriggerSource===0?2:1) : null);
    console.log(`  TriggerMode=${MODE[j.TriggerMode]??j.TriggerMode}  Source=${SRC[j.TriggerSource]??j.TriggerSource}`);
    console.log(`  -> core trigger_mode would restore to: ${core}`);
    process.exit(0);
  }
});
setTimeout(()=>{console.log('no reply in 12s');process.exit(1);},12000);
