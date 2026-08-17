// Poll the core's queue depths + snapshot-loss counters at 2Hz.
import WebSocket from 'ws';
const enc=new TextEncoder();
function frame(t,p,g,o){const b=enc.encode(JSON.stringify(o));const u=new Uint8Array(9+b.length+1);u[0]=t.charCodeAt(0);u[1]=t.charCodeAt(1);u[2]=p;u[3]=g>>8;u[4]=g&255;const l=u.length-9;u[5]=l>>>24;u[6]=(l>>16)&255;u[7]=(l>>8)&255;u[8]=l&255;u.set(b,9);return u;}
const ws=new WebSocket('ws://127.0.0.1:4090');ws.binaryType='arraybuffer';let pg=1;
const mx={insp:0,dview:0,snap:0}; let last={};
ws.on('message',d=>{const u=new Uint8Array(d);const t=String.fromCharCode(u[0],u[1]);
 if(t==='HR'){ws.send(frame('HR',0,pg++,{a:['d']}));setInterval(()=>ws.send(frame('GS',0,pg++,{items:['precess_queue_status','snap_queue_skip_count','save_snap_folder_full_delete_count','save_snap_disk_low_skip_count']})),500);return;}
 if(t==='GS'){const j=JSON.parse(new TextDecoder().decode(u.subarray(9)).replace(/\0+$/,''));
  const q=j.precess_queue_status||{};
  mx.insp=Math.max(mx.insp,q.inspQueue?.size||0);mx.dview=Math.max(mx.dview,q.datViewQueue?.size||0);mx.snap=Math.max(mx.snap,q.inspSnapQueue?.size||0);
  last={skip:j.snap_queue_skip_count,rot:j.save_snap_folder_full_delete_count,disk:j.save_snap_disk_low_skip_count};}});
setTimeout(()=>{console.log('queue HWM',JSON.stringify(mx),'counters',JSON.stringify(last));process.exit(0)},Number(process.argv[2]||70)*1000);
