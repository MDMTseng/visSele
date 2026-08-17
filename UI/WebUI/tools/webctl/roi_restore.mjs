// Put the runtime crop back after a page that opened the full sensor
// (calibration does, deliberately, and nothing restores it until InspectionUI
// reloads the file). Reads InspectionROI from the machine's own file.
import WebSocket from 'ws';import fs from 'node:fs';
const cfg=JSON.parse(fs.readFileSync('../../../../InspectionCore/Core0_1/data/default_camera_setting.json','utf8'));
const roi=cfg.InspectionROI; if(!roi){console.log('no InspectionROI');process.exit(0);}
const enc=new TextEncoder();
function frame(t,p,g,o){const b=enc.encode(JSON.stringify(o));const u=new Uint8Array(9+b.length+1);u[0]=t.charCodeAt(0);u[1]=t.charCodeAt(1);u[2]=p;u[3]=g>>8;u[4]=g&255;const l=u.length-9;u[5]=l>>>24;u[6]=(l>>16)&255;u[7]=(l>>8)&255;u[8]=l&255;u.set(b,9);return u;}
const ws=new WebSocket('ws://127.0.0.1:4090');ws.binaryType='arraybuffer';let pg=1;
ws.on('message',d=>{const u=new Uint8Array(d);const t=String.fromCharCode(u[0],u[1]);
 if(t==='HR'){ws.send(frame('HR',0,pg++,{a:['d']}));setTimeout(()=>{ws.send(frame('ST',0,pg++,{CameraSetting:{ROI:roi}}));console.log('ROI restored ->',roi.join(','));setTimeout(()=>process.exit(0),500);},300);}});
setTimeout(()=>process.exit(1),10000);
