// The "unresponsive browser" scenario (backlog 2.8):
// A: healthy client running CI + stream, reports RP rate per 3s window.
// B: joins the stream, then PAUSES its socket for 15s (TCP backpressure).
// If the core holds locks across the blocking send, A's RP rate collapses.
import WebSocket from 'ws';
import fs from 'node:fs';
const BPG_HDR=9; const enc=new TextEncoder();
function frame(t,p,g,o){const b=enc.encode(o==null?'':JSON.stringify(o));const u=new Uint8Array(BPG_HDR+b.length+1);
u[0]=t.charCodeAt(0);u[1]=t.charCodeAt(1);u[2]=p;u[3]=g>>8;u[4]=g&255;const l=u.length-BPG_HDR;
u[5]=l>>>24;u[6]=(l>>16)&255;u[7]=(l>>8)&255;u[8]=l&255;u.set(b,BPG_HDR);return u;}
const def=JSON.parse(fs.readFileSync('/Users/mdm/workspace/visSele/InspectionCore/Core0_1/data/test1.hydef','utf8'));

const A=new WebSocket('ws://127.0.0.1:4090'); A.binaryType='arraybuffer'; let pgA=1;
let rp=0;
A.on('message',d=>{const b=new Uint8Array(d instanceof ArrayBuffer?d:d.buffer.slice(d.byteOffset,d.byteOffset+d.byteLength));
  const t=String.fromCharCode(b[0],b[1]);
  if(t==='HR'){A.send(frame('HR',0,pgA++,{a:['d']}));return;}
  if(t==='RP')rp++;});
await new Promise(r=>A.on('open',()=>setTimeout(r,400)));
A.send(frame('CI',0,pgA++,{definfo:def,frame_count:-1,trigger_mode:0}));
A.send(frame('SB',0,pgA++,{stream:true}));
A.send(frame('ST',0,pgA++,{IMG_STREAMING_JPEG_QUALITY:85}));

const B=new WebSocket('ws://127.0.0.1:4090'); B.binaryType='arraybuffer'; let pgB=1;
B.on('message',d=>{const b=new Uint8Array(d instanceof ArrayBuffer?d:d.buffer.slice(d.byteOffset,d.byteOffset+d.byteLength));
  if(String.fromCharCode(b[0],b[1])==='HR')B.send(frame('HR',0,pgB++,{a:['d']}));});
await new Promise(r=>B.on('open',()=>setTimeout(r,400)));
B.send(frame('SB',0,pgB++,{stream:true}));

const t0=Date.now();
let paused=false;
setInterval(()=>{
  const t=(Date.now()-t0)/1000;
  console.log(`t=${t.toFixed(0)}s  A.RP=${rp} (${(rp/3).toFixed(1)}/s)  B=${paused?'PAUSED':'reading'}`);
  rp=0;
  if(t>=6 && t<21 && !paused){ B._socket.pause(); paused=true; console.log('--- B stops reading ---'); }
  if(t>=21 && paused){ B._socket.resume(); paused=false; console.log('--- B resumes ---'); }
  if(t>=30){ process.exit(0); }
},3000);
