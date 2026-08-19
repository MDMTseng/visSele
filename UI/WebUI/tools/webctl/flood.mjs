// How long does the core stay unresponsive after a 1.2MB flood of empty
// 9-byte BPG packets? bpg_sweep's C11 only asks "did it answer within 3s".
import WebSocket from 'ws';
const HDR=9, enc=new TextEncoder();
function frame(tl,pg,obj,raw){
  const body = raw!==undefined?raw:enc.encode(obj==null?'':JSON.stringify(obj)+'\0');
  const b=new Uint8Array(HDR+body.length);
  b[0]=tl.charCodeAt(0); b[1]=tl.charCodeAt(1); b[2]=0; b[3]=pg>>8; b[4]=pg&255;
  const L=b.length-HDR; b[5]=L>>>24; b[6]=(L>>16)&255; b[7]=(L>>8)&255; b[8]=L&255;
  b.set(body,HDR); return b;
}
const ws=new WebSocket('ws://127.0.0.1:4090'); ws.binaryType='arraybuffer';
let pg=1, gotGS=false;
ws.on('message',(d)=>{ const b=new Uint8Array(d instanceof ArrayBuffer?d:d.buffer);
  if(String.fromCharCode(b[0],b[1])==='GS') gotGS=true; });
await new Promise(r=>ws.on('open',()=>setTimeout(r,600)));

// baseline latency
let t=Date.now(); gotGS=false; ws.send(frame('GS',pg++,{items:['perif_pairing']}));
while(!gotGS && Date.now()-t<5000) await new Promise(r=>setTimeout(r,20));
console.log('baseline GS latency:', gotGS?(Date.now()-t)+'ms':'NO REPLY');

// the flood
const one=frame('HR',pg++,undefined,new Uint8Array(0));
const N=Math.floor(1_200_000/one.length);
const u=new Uint8Array(one.length*N);
for(let i=0;i<N;i++) u.set(one,i*one.length);
console.log(`flooding ${N} packets (${u.length} bytes)...`);
const t0=Date.now(); ws.send(u);

// poll until a GS comes back
gotGS=false;
let sent=0;
while(!gotGS && Date.now()-t0<120000){
  if(Date.now()-t0 > sent*1000){ sent++; try{ws.send(frame('GS',pg++,{items:['perif_pairing']}));}catch(e){} }
  await new Promise(r=>setTimeout(r,50));
}
console.log(gotGS ? `recovered after ${Date.now()-t0}ms` : 'NEVER recovered within 120s');
ws.close();
