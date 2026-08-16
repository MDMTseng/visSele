// RC camera_ez_reconnect + GS camera_info hammer.
import WebSocket from 'ws';
const URL = 'ws://127.0.0.1:4090';
const enc = new TextEncoder();
function frame(type, prop, pgID, obj) {
  const body = enc.encode(obj == null ? '' : JSON.stringify(obj));
  const buf = new Uint8Array(9 + body.length + 1);
  buf[0]=type.charCodeAt(0); buf[1]=type.charCodeAt(1); buf[2]=prop;
  buf[3]=pgID>>8; buf[4]=pgID&255;
  const len=buf.length-9;
  buf[5]=len>>>24; buf[6]=(len>>16)&255; buf[7]=(len>>8)&255; buf[8]=len&255;
  buf.set(body,9); return buf;
}
const ws = new WebSocket(URL); ws.binaryType='arraybuffer';
let pg=1, gsReplies=0;
ws.on('message',(d)=>{ const b=new Uint8Array(d); const t=String.fromCharCode(b[0],b[1]);
  if(t==='HR'){ ws.send(frame('HR',0,pg++,{a:['d']})); return; }
  if(t==='GS') gsReplies++;
});
ws.on('open', async ()=>{
  await new Promise(r=>setTimeout(r,500));
  for(let i=0;i<5;i++){
    ws.send(frame('RC',0,pg++,{target:'camera_ez_reconnect'}));
    for(let k=0;k<6;k++){ ws.send(frame('GS',0,pg++,{items:['camera_info']})); await new Promise(r=>setTimeout(r,120)); }
    console.log(`round ${i} done, gsReplies=${gsReplies}`);
    await new Promise(r=>setTimeout(r,3200));
  }
  console.log('final gsReplies:', gsReplies); process.exit(0);
});
ws.on('error',e=>{ console.error('ws error:',e.message); process.exit(1); });
ws.on('close',()=>{ console.error('WS CLOSED (core died?) gsReplies='+gsReplies); process.exit(2); });
