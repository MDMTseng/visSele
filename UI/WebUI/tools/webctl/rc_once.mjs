import WebSocket from 'ws';
const enc=new TextEncoder();
function frame(t,p,g,o){const b=enc.encode(JSON.stringify(o));const u=new Uint8Array(10+b.length);u[0]=t.charCodeAt(0);u[1]=t.charCodeAt(1);u[2]=p;u[3]=g>>8;u[4]=g&255;const l=u.length-9;u[5]=l>>>24;u[6]=(l>>16)&255;u[7]=(l>>8)&255;u[8]=l&255;u.set(b,9);return u;}
const ws=new WebSocket('ws://127.0.0.1:4090');ws.binaryType='arraybuffer';let pg=1;
ws.on('message',d=>{const b=new Uint8Array(d);const t=String.fromCharCode(b[0],b[1]);
 if(t==='HR'){ws.send(frame('HR',0,pg++,{a:['d']}));setTimeout(()=>ws.send(frame('RC',0,pg++,{target:'camera_ez_reconnect'})),300);return;}
 if(t==='SS'){const s=new TextDecoder().decode(b.subarray(9)).replace(/\0$/,'');if(s.includes('RC')){console.log('RC done:',s);setTimeout(()=>process.exit(0),300);}}
});
setTimeout(()=>{console.log('timeout');process.exit(1);},20000);
