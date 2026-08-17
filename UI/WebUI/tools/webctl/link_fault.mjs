// 1.4 link-health test against a fake TCP "board":
//  1. spin up a local TCP listener (the board)
//  2. PD CONNECT ip:127.0.0.1 -> channel opens, link.connected=true
//  3. start CI so verdicts flow, watch link counters
//  4. kill the listener -> writes fail -> tx_fail moves, suspect flips
//  5. PD CONNECT same desc -> must REOPEN (not reuse); fails (no listener) -> connected=false
//  6. with frames still flowing -> dropped_no_channel moves
import WebSocket from 'ws';
import net from 'node:net';
import fs from 'node:fs';
const DEF='/Users/mdm/workspace/visSele/InspectionCore/Core0_1/data/test1.hydef';
const BPG_HDR=9; const enc=new TextEncoder();
function frame(t,p,g,o){const b=enc.encode(o==null?'':JSON.stringify(o));const u=new Uint8Array(BPG_HDR+b.length+1);
u[0]=t.charCodeAt(0);u[1]=t.charCodeAt(1);u[2]=p;u[3]=g>>8;u[4]=g&255;const l=u.length-BPG_HDR;
u[5]=l>>>24;u[6]=(l>>16)&255;u[7]=(l>>8)&255;u[8]=l&255;u.set(b,BPG_HDR);return u;}

// fake board: accepts, discards everything
let sock=null;
const srv=net.createServer(s=>{sock=s; s.on('data',()=>{}); s.on('error',()=>{});});
await new Promise(r=>srv.listen(5999,r));

const ws=new WebSocket('ws://127.0.0.1:4090'); ws.binaryType='arraybuffer';
let pg=1; const gsWaiters=[];
ws.on('message',d=>{
  if(!(d instanceof ArrayBuffer))d=d.buffer.slice(d.byteOffset,d.byteOffset+d.byteLength);
  const b=new Uint8Array(d); const t=String.fromCharCode(b[0],b[1]);
  if(t==='HR'){ws.send(frame('HR',0,pg++,{a:['d']}));return;}
  if(t==='GS'){ try{const j=JSON.parse(new TextDecoder().decode(b.subarray(BPG_HDR)));
    const w=gsWaiters.shift(); if(w)w(j);}catch{} }
});
function link(){return new Promise(r=>{gsWaiters.push(j=>r((j.perif_pairing||{}).link));
  ws.send(frame('GS',0,pg++,{items:['perif_pairing']}));});}
await new Promise(r=>ws.on('open',()=>setTimeout(r,400)));

// 2: connect the fake board
ws.send(frame('PD',0,pg++,{type:'CONNECT',ip:'127.0.0.1',port:5999,machine_type:'uInspESP32',cat_ok:3,cat_ng:1}));
await new Promise(r=>setTimeout(r,1500));
console.log('after CONNECT:', JSON.stringify(await link()));

// 3: start CI so verdicts flow
const def=JSON.parse(fs.readFileSync(DEF,'utf8'));
ws.send(frame('CI',0,pg++,{definfo:def,frame_count:-1,trigger_mode:0}));
await new Promise(r=>setTimeout(r,4000));
console.log('CI flowing  :', JSON.stringify(await link()));

// 4: board dies
if(sock)sock.destroy(); await new Promise(r=>srv.close(r));
await new Promise(r=>setTimeout(r,6000));
console.log('board dead  :', JSON.stringify(await link()));

// 5: reconnect same desc (no listener -> open fails -> channel gone)
ws.send(frame('PD',0,pg++,{type:'CONNECT',ip:'127.0.0.1',port:5999,machine_type:'uInspESP32',cat_ok:3,cat_ng:1}));
await new Promise(r=>setTimeout(r,2000));
console.log('reCONNECT   :', JSON.stringify(await link()));

// 6: frames still flowing with no channel
await new Promise(r=>setTimeout(r,5000));
console.log('no channel  :', JSON.stringify(await link()));
ws.close(); process.exit(0);
