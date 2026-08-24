import WebSocket from 'ws';
const BPG=9, enc=new TextEncoder();
const frame=(t,p,pg,o)=>{const b=enc.encode(o==null?'':JSON.stringify(o));const u=new Uint8Array(BPG+b.length+1);
u[0]=t.charCodeAt(0);u[1]=t.charCodeAt(1);u[2]=p;u[3]=pg>>8;u[4]=pg&255;const l=u.length-BPG;
u[5]=l>>>24;u[6]=(l>>16)&255;u[7]=(l>>8)&255;u[8]=l&255;u.set(b,BPG);return u;};
const ws=new WebSocket('ws://127.0.0.1:4090');ws.binaryType='arraybuffer';let pg=1;
ws.on('open',()=>setTimeout(()=>{
  ws.send(frame('ST',0,pg++,{CameraSetting:{trigger_mode:Number(process.argv[2]||0)}}));
  console.log('sent trigger_mode='+(process.argv[2]||0));
  setTimeout(()=>process.exit(0),2500);
},400));
ws.on('message',d=>{if(!(d instanceof ArrayBuffer))d=d.buffer.slice(d.byteOffset,d.byteOffset+d.byteLength);
const b=new Uint8Array(d),t=String.fromCharCode(b[0],b[1]);if(t==='HR')ws.send(frame('HR',0,pg++,{a:['d']}));});
ws.on('error',e=>{console.log('err',e.message);process.exit(1)});
