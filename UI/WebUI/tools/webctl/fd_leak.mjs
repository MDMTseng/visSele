// Hammer PD CONNECT at a dead port N times and MEASURE the core's handle and
// thread counts across it.
//
//   node fd_leak.mjs [N=30]
//
// It used to print nothing at all -- the header said "the caller checks fd
// count" and no caller ever did, so a green run and a leaking run looked
// identical. Measuring here is the difference between a script and a test.
//
// Baseline taken 2026-08-19 on Windows: 232 handles / 16 threads, three rounds
// of 30 failed CONNECTs -> 279, 280, 279 handles and 16, 15, 13 threads. The
// first round's +47 is one-time allocation; it plateaus, so there is NO leak.
// Threads falling is retiring websocket clients, not a problem.
//
// Threads matter here for a second reason: a channel that actually carries
// traffic starts a synth sender thread that is never joined and is left
// running on the freed channel (AUDIT_BACKLOG P1 / CONSOLE_ABUSE F6). This
// probe does NOT trigger it -- the port is dead, so no channel ever opens and
// no trigger arrives -- which is exactly why thread count stays flat here.
import WebSocket from 'ws';
import { execSync } from 'node:child_process';

function coreCounts() {
  try {
    const out = execSync(
      "powershell -NoProfile -Command \"$p=Get-Process visSele -ErrorAction SilentlyContinue; " +
      "if($p){ $p.HandleCount.ToString() + ',' + $p.Threads.Count }\"",
      { encoding: 'utf8' }).trim();
    const [h, t] = out.split(',').map(Number);
    return Number.isFinite(h) ? { handles: h, threads: t } : null;
  } catch { return null; }
}
const N = parseInt(process.argv[2]||'30',10);
const BPG_HDR=9; const enc=new TextEncoder();
function frame(t,p,g,o){const b=enc.encode(o==null?'':JSON.stringify(o));const u=new Uint8Array(BPG_HDR+b.length+1);
u[0]=t.charCodeAt(0);u[1]=t.charCodeAt(1);u[2]=p;u[3]=g>>8;u[4]=g&255;const l=u.length-BPG_HDR;
u[5]=l>>>24;u[6]=(l>>16)&255;u[7]=(l>>8)&255;u[8]=l&255;u.set(b,BPG_HDR);return u;}
const ws=new WebSocket('ws://127.0.0.1:4090'); ws.binaryType='arraybuffer'; let pg=1;
ws.on('message',d=>{ const b=new Uint8Array(d instanceof ArrayBuffer?d:d.buffer.slice(d.byteOffset,d.byteOffset+d.byteLength));
  if(String.fromCharCode(b[0],b[1])==='HR')ws.send(frame('HR',0,pg++,{a:['d']})); });
await new Promise(r=>ws.on('open',()=>setTimeout(r,400)));
const before = coreCounts();
if (before) console.log(`before: handles=${before.handles} threads=${before.threads}`);
for(let i=0;i<N;i++){
  ws.send(frame('PD',0,pg++,{type:'CONNECT',ip:'127.0.0.1',port:5998,machine_type:'uInspESP32',cat_ok:3,cat_ng:1}));
  await new Promise(r=>setTimeout(r,1300)); // connect_nonb timeout is 1s
}
await new Promise(r=>setTimeout(r,2000));
const after = coreCounts();
if (before && after) {
  console.log(`handles ${before.handles} -> ${after.handles}  (${after.handles - before.handles >= 0 ? '+' : ''}${after.handles - before.handles} over ${N} failed CONNECTs)`);
  console.log(`threads ${before.threads} -> ${after.threads}`);
  // One-time allocation is fine; sustained per-attempt growth is not. Run this
  // twice: the second round's delta is the one that means anything.
  const per = (after.handles - before.handles) / N;
  console.log(per > 1 ? `NOTE: ${per.toFixed(2)} handles/attempt this round -- run again; a leak keeps this rate, allocation does not`
                      : `OK: ${per.toFixed(2)} handles/attempt`);
} else {
  console.log('could not read the core process counts (is visSele running?)');
}
ws.close(); process.exit(0);
