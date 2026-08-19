// Measure the live datView stream: IM bytes/frame + fps + RP fps,
// raw (quality 0) vs JPEG (quality 85).
//   node dv_bench.mjs <seconds-per-mode>
import WebSocket from 'ws';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import fs from 'node:fs';
const SECS=parseInt(process.argv[2]||'12',10);
const BPG_HDR=9; const enc=new TextEncoder();
function frame(t,p,g,o){const b=enc.encode(o==null?'':JSON.stringify(o));const u=new Uint8Array(BPG_HDR+b.length+1);
u[0]=t.charCodeAt(0);u[1]=t.charCodeAt(1);u[2]=p;u[3]=g>>8;u[4]=g&255;const l=u.length-BPG_HDR;
u[5]=l>>>24;u[6]=(l>>16)&255;u[7]=(l>>8)&255;u[8]=l&255;u.set(b,BPG_HDR);return u;}
// Default to the test1 fixture IN THIS REPOSITORY -- the def these probes were
// /Users/mdm -- and at data/test1.hydef, which is gitignored -- so the probe
// died with ENOENT on every machine but one. WEBCTL_DEF overrides it.
const def=JSON.parse(fs.readFileSync((process.env.WEBCTL_DEF || path.join(path.dirname(fileURLToPath(import.meta.url)), 'fixtures', 'test1.hydef')),'utf8'));
const ws=new WebSocket('ws://127.0.0.1:4090'); ws.binaryType='arraybuffer'; let pg=1;
let im=0,imBytes=0,rp=0,rpBytes=0,other=0,otherBytes=0;
ws.on('message',d=>{
  if(!(d instanceof ArrayBuffer))d=d.buffer.slice(d.byteOffset,d.byteOffset+d.byteLength);
  const b=new Uint8Array(d); const t=String.fromCharCode(b[0],b[1]);
  if(t==='HR'){ws.send(frame('HR',0,pg++,{a:['d']}));return;}
  if(t==='IM'){im++;imBytes+=b.length;}
  else if(t==='RP'){rp++;rpBytes+=b.length;}
  else {other++;otherBytes+=b.length;}
});
function stats(label,dt){
  console.log(`${label}: IM ${im} msgs ${(imBytes/1e6).toFixed(1)}MB (${im?Math.round(imBytes/im/1024):0}KB/msg, ${(im/dt).toFixed(1)}/s)  RP ${rp} (${(rp/dt).toFixed(1)}/s, ${rp?Math.round(rpBytes/rp/1024):0}KB/msg)  other ${other} ${(otherBytes/1e6).toFixed(2)}MB`);
  im=0;imBytes=0;rp=0;rpBytes=0;other=0;otherBytes=0;
}
await new Promise(r=>ws.on('open',()=>setTimeout(r,400)));
ws.send(frame('CI',0,pg++,{definfo:def,frame_count:-1,trigger_mode:0}));
ws.send(frame('SB',0,pg++,{stream:true}));
// mode 1: raw
ws.send(frame('ST',0,pg++,{IMG_STREAMING_JPEG_QUALITY:0}));
await new Promise(r=>setTimeout(r,2000)); im=0;imBytes=0;rp=0;rpBytes=0;other=0;otherBytes=0;
await new Promise(r=>setTimeout(r,SECS*1000));
stats('RAW (q=0) ',SECS);
// mode 2: jpeg 85
ws.send(frame('ST',0,pg++,{IMG_STREAMING_JPEG_QUALITY:85}));
await new Promise(r=>setTimeout(r,2000)); im=0;imBytes=0;rp=0;rpBytes=0;other=0;otherBytes=0;
await new Promise(r=>setTimeout(r,SECS*1000));
stats('JPEG q=85 ',SECS);
ws.close();process.exit(0);
