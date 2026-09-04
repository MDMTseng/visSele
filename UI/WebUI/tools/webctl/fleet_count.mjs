// Usage: node fleet_count.mjs [-sig]   -- core with INSP_ALLOW_MULTI_CLIENT=1 on 4090.
// Runs every recipe listed in _ok_names.txt (data/<name>_sbm.hydef, or the sig360 original with -sig) on its own
// data/<name>.png once and prints located / judges-OK totals; per-recipe counts go to $OUT (default _fleet_count.json).
// The before/after number for any core or migration change; see InspectionCore/docs/SBM_STABILITY_2026-09-04.md.
import fs from 'node:fs'; import WebSocket from 'ws';
const D='../../../../InspectionCore/Core0_1/data/'; const HDR=9, enc=new TextEncoder();
function frame(type,prop,pg,obj){const b=enc.encode(JSON.stringify(obj));const u=new Uint8Array(HDR+b.length+1);u[0]=type.charCodeAt(0);u[1]=type.charCodeAt(1);u[2]=prop;new DataView(u.buffer).setUint16(3,pg,false);new DataView(u.buffer).setUint32(5,b.length+1,false);u.set(b,HDR);return u;}
const ws=new WebSocket('ws://127.0.0.1:4090'); ws.binaryType='arraybuffer'; let pg=1500; const W={};
ws.on('message',(d)=>{const b=new Uint8Array(d);const ty=String.fromCharCode(b[0],b[1]);const id=new DataView(b.buffer,b.byteOffset).getUint16(3,false);if(ty==='HR'){ws.send(frame('HR',0,1,{a:['d']}));return;}const txt=new TextDecoder().decode(b.subarray(HDR)).replace(/\0+$/,'');const w=W[id];if(!w)return;if(ty==='RP'){try{w.rp=JSON.parse(txt);}catch(e){}}if(ty==='SS'){try{if(JSON.parse(txt).cmd==='II'){delete W[id];w.res(w.rp);}}catch(e){}}});
const ii=(def,img)=>new Promise(res=>{const id=pg++;W[id]={res};ws.send(frame('II',0,id,{definfo:def,imgsrc:img,img_property:{calibInfo:{type:'disable',mmpp:def.featureSet[0].mmpp}}}));setTimeout(()=>{if(W[id]){delete W[id];res(null);}},20000);});
await new Promise(r=>ws.on('open',()=>setTimeout(r,400)));
const suffix = process.argv.includes('-sig') ? '' : '_sbm';
const names=fs.readFileSync('_ok_names.txt','utf8').split('\n').map(s=>s.trim()).filter(Boolean);
const out={}; let tot=0, loc=0;
for (const n of names) { const def=JSON.parse(fs.readFileSync(D+n+suffix+'.hydef','utf8')); const rp=await ii(def,'data/'+n+'.png'); const g=rp&&rp.reports&&rp.reports[0]; const o=g&&g.reports&&g.reports[0];
  const k=o?(o.judgeReports||[]).filter(j=>j.status===0).length:-1; out[n]=k; if(o){loc++;tot+=k;} }
console.log('located '+loc+'/'+names.length+'  judges OK '+tot);
fs.writeFileSync(process.env.OUT||'_fleet_count.json', JSON.stringify(out));
ws.close(); process.exit(0);
