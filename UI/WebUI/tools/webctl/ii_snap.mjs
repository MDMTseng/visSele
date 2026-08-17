import WebSocket from 'ws';
const enc=new TextEncoder();
function frame(t,p,g,o){const b=enc.encode(JSON.stringify(o));const u=new Uint8Array(10+b.length);u[0]=t.charCodeAt(0);u[1]=t.charCodeAt(1);u[2]=p;u[3]=g>>8;u[4]=g&255;const l=u.length-9;u[5]=l>>>24;u[6]=(l>>16)&255;u[7]=(l>>8)&255;u[8]=l&255;u.set(b,9);return u;}
const ws=new WebSocket('ws://127.0.0.1:4090');ws.binaryType='arraybuffer';let pg=1;
ws.on('message',d=>{const b=new Uint8Array(d);const t=String.fromCharCode(b[0],b[1]);
 if(t==='HR'){ws.send(frame('HR',0,pg++,{a:['d']}));setTimeout(()=>{console.log('sending II snap...');ws.send(frame('II',0,pg++,{deffile:'data/test1.hydef'}))},300);return;}
 const s=new TextDecoder().decode(b.subarray(9,Math.min(b.length,400))).replace(/\0.*$/,'');
 if(t==='RP'){const j=JSON.parse(new TextDecoder().decode(b.subarray(9)).replace(/\0$/,''));console.log('RP: reports=',j.reports?.length,'size_wh=',j.image_w||j.w,j.image_h||j.h);}
 if(t==='IM'){console.log('IM frame arrived, bytes:',b.length-9);}
 if(t==='SS'&&s.includes('"II"')){console.log('II done:',s);setTimeout(()=>process.exit(0),300);}
});
setTimeout(()=>{console.log('timeout (no frame in 40s)');process.exit(1);},40000);
