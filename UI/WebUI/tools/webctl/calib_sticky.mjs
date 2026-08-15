// Verify the ignore_calib stickiness fix:
//  session 1: CI with IMG_ignore_calib:true  -> station.ignore_calib must be true
//  session 2: plain CI (new connection)      -> station.ignore_calib must be false
import WebSocket from 'ws';
import fs from 'node:fs';
const DEF = process.argv[2];
const BPG_HDR = 9; const enc = new TextEncoder();
function frame(type, prop, pgID, obj) {
  const body = enc.encode(obj == null ? '' : JSON.stringify(obj));
  const buf = new Uint8Array(BPG_HDR + body.length + 1);
  buf[0]=type.charCodeAt(0); buf[1]=type.charCodeAt(1); buf[2]=prop;
  buf[3]=pgID>>8; buf[4]=pgID&255;
  const len=buf.length-BPG_HDR;
  buf[5]=len>>>24; buf[6]=(len>>16)&255; buf[7]=(len>>8)&255; buf[8]=len&255;
  buf.set(body,BPG_HDR); return buf;
}
const def = JSON.parse(fs.readFileSync(DEF,'utf8'));
function session(extra, label) {
  return new Promise((res, rej) => {
    const ws = new WebSocket('ws://127.0.0.1:4090'); ws.binaryType='arraybuffer';
    let pg=1, done=false;
    const to=setTimeout(()=>{ if(!done){done=true;ws.close();rej(new Error(label+': no station report in 20s'));} },20000);
    ws.on('open',()=>setTimeout(()=>{
      ws.send(frame('CI',0,pg++,{definfo:def,frame_count:-1,trigger_mode:0,...extra}));
      ws.send(frame('SB',0,pg++,{stream:true}));
    },400));
    ws.on('message',(d)=>{
      if(!(d instanceof ArrayBuffer)) d=d.buffer.slice(d.byteOffset,d.byteOffset+d.byteLength);
      const b=new Uint8Array(d); const t=String.fromCharCode(b[0],b[1]);
      if(t==='HR'){ws.send(frame('HR',0,pg++,{a:['d']}));return;}
      if(t!=='RP') return;
      let j; try{ j=JSON.parse(new TextDecoder().decode(b.subarray(BPG_HDR))); }catch{return;}
      const st=j.station||((j.reports&&j.reports[0])||{}).station;
      if(st && !done){ done=true; clearTimeout(to); ws.close(); res(st.ignore_calib); }
    });
    ws.on('error',e=>{ if(!done){done=true;rej(e);} });
  });
}
const a = await session({IMG_ignore_calib:true},'S1');
console.log('S1 (IMG_ignore_calib:true) -> station.ignore_calib =', a);
await new Promise(r=>setTimeout(r,1500));
const b = await session({},'S2');
console.log('S2 (plain)                 -> station.ignore_calib =', b);
console.log((a===true && b===false) ? 'PASS: flag is session-scoped' : 'FAIL');
process.exit(a===true&&b===false ? 0 : 1);
