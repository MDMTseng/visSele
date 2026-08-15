// Hammer PD CONNECT at a dead port N times; the caller checks fd count.
import WebSocket from 'ws';
const N = parseInt(process.argv[2]||'30',10);
const BPG_HDR=9; const enc=new TextEncoder();
function frame(t,p,g,o){const b=enc.encode(o==null?'':JSON.stringify(o));const u=new Uint8Array(BPG_HDR+b.length+1);
u[0]=t.charCodeAt(0);u[1]=t.charCodeAt(1);u[2]=p;u[3]=g>>8;u[4]=g&255;const l=u.length-BPG_HDR;
u[5]=l>>>24;u[6]=(l>>16)&255;u[7]=(l>>8)&255;u[8]=l&255;u.set(b,BPG_HDR);return u;}
const ws=new WebSocket('ws://127.0.0.1:4090'); ws.binaryType='arraybuffer'; let pg=1;
ws.on('message',d=>{ const b=new Uint8Array(d instanceof ArrayBuffer?d:d.buffer.slice(d.byteOffset,d.byteOffset+d.byteLength));
  if(String.fromCharCode(b[0],b[1])==='HR')ws.send(frame('HR',0,pg++,{a:['d']})); });
await new Promise(r=>ws.on('open',()=>setTimeout(r,400)));
for(let i=0;i<N;i++){
  ws.send(frame('PD',0,pg++,{type:'CONNECT',ip:'127.0.0.1',port:5998,machine_type:'uInspESP32',cat_ok:3,cat_ng:1}));
  await new Promise(r=>setTimeout(r,1300)); // connect_nonb timeout is 1s
}
ws.close(); process.exit(0);
