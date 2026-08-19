// print the first station block a CI session produces
import WebSocket from 'ws';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import fs from 'node:fs';
const BPG_HDR=9; const enc=new TextEncoder();
function frame(t,p,g,o){const b=enc.encode(o==null?'':JSON.stringify(o));const u=new Uint8Array(BPG_HDR+b.length+1);
u[0]=t.charCodeAt(0);u[1]=t.charCodeAt(1);u[2]=p;u[3]=g>>8;u[4]=g&255;const l=u.length-BPG_HDR;
u[5]=l>>>24;u[6]=(l>>16)&255;u[7]=(l>>8)&255;u[8]=l&255;u.set(b,BPG_HDR);return u;}
// Default to the test1 fixture IN THIS REPOSITORY -- the def these probes were
// /Users/mdm -- and at data/test1.hydef, which is gitignored -- so the probe
// died with ENOENT on every machine but one. WEBCTL_DEF overrides it.
const def=JSON.parse(fs.readFileSync((process.env.WEBCTL_DEF || path.join(path.dirname(fileURLToPath(import.meta.url)), 'fixtures', 'test1.hydef')),'utf8'));
const ws=new WebSocket('ws://127.0.0.1:4090'); ws.binaryType='arraybuffer'; let pg=1;
ws.on('open',()=>setTimeout(()=>{ws.send(frame('CI',0,pg++,{definfo:def,frame_count:-1,trigger_mode:0}));ws.send(frame('SB',0,pg++,{stream:true}));},400));
ws.on('message',d=>{
  if(!(d instanceof ArrayBuffer))d=d.buffer.slice(d.byteOffset,d.byteOffset+d.byteLength);
  const b=new Uint8Array(d); const t=String.fromCharCode(b[0],b[1]);
  if(t==='HR'){ws.send(frame('HR',0,pg++,{a:['d']}));return;}
  if(t!=='RP')return;
  let j; try{j=JSON.parse(new TextDecoder().decode(b.subarray(BPG_HDR)));}catch{return;}
  const st=j.station||((j.reports&&j.reports[0])||{}).station;
  if(st){console.log(JSON.stringify(st));ws.close();process.exit(0);}
});
setTimeout(()=>{console.log('timeout');process.exit(2);},25000);
