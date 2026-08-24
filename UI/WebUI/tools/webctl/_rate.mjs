import WebSocket from 'ws';
const BPG=9, enc=new TextEncoder();
function frame(t,p,pg,o){const b=enc.encode(o==null?'':JSON.stringify(o));const u=new Uint8Array(BPG+b.length+1);
u[0]=t.charCodeAt(0);u[1]=t.charCodeAt(1);u[2]=p;u[3]=pg>>8;u[4]=pg&255;const l=u.length-BPG;
u[5]=l>>>24;u[6]=(l>>16)&255;u[7]=(l>>8)&255;u[8]=l&255;u.set(b,BPG);return u;}
const ws=new WebSocket('ws://127.0.0.1:4090');ws.binaryType='arraybuffer';let pg=1,rp=0,im=0,objs=0,na=0;
ws.on('open',()=>setTimeout(()=>ws.send(frame('SB',0,pg++,{stream:true})),200));
ws.on('message',d=>{if(!(d instanceof ArrayBuffer))d=d.buffer.slice(d.byteOffset,d.byteOffset+d.byteLength);
const b=new Uint8Array(d),t=String.fromCharCode(b[0],b[1]);
if(t==='HR'){ws.send(frame('HR',0,pg++,{a:['d']}));return;}
if(t==='IM')im++;
if(t==='RP'){rp++;try{const j=JSON.parse(new TextDecoder().decode(b.subarray(BPG)));const r=j.reports?.[0]?.reports||[];objs+=r.length;if(!r.length)na++;}catch{}}});
setTimeout(()=>{console.log(`over 10s: RP=${rp} IM=${im} objects=${objs} empty=${na}`);process.exit(0);},10000);
