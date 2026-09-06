// node _fleet_eq.mjs <portA> <portB> [names...]  -- inspect every recipe's own picture on two cores, compare objects
// (count, pose, similarity, judge values). Used to prove a matcher change is result-neutral across the fleet.
import fs from 'node:fs'; import WebSocket from 'ws';
const D = '../../../../InspectionCore/Core0_1/data/'; const HDR = 9, enc = new TextEncoder();
function frame(type,prop,pg,obj){const b=enc.encode(JSON.stringify(obj));const u=new Uint8Array(HDR+b.length+1);u[0]=type.charCodeAt(0);u[1]=type.charCodeAt(1);u[2]=prop;new DataView(u.buffer).setUint16(3,pg,false);new DataView(u.buffer).setUint32(5,b.length+1,false);u.set(b,HDR);return u;}
function client(port){const ws=new WebSocket('ws://127.0.0.1:'+port);ws.binaryType='arraybuffer';let pg=15000;const W={};
  ws.on('message',(d)=>{const b=new Uint8Array(d);const ty=String.fromCharCode(b[0],b[1]);const id=new DataView(b.buffer,b.byteOffset).getUint16(3,false);if(ty==='HR'){ws.send(frame('HR',0,1,{a:['d']}));return;}const txt=new TextDecoder().decode(b.subarray(HDR)).replace(/\0+$/,'');const w=W[id];if(!w)return;if(ty==='RP'){try{w.rp=JSON.parse(txt);}catch(e){}}if(ty==='SS'){try{if(JSON.parse(txt).cmd==='II'){delete W[id];w.res(w.rp);}}catch(e){}}});
  const ii=(def,img)=>new Promise(res=>{const id=pg++;W[id]={res};ws.send(frame('II',0,id,{definfo:def,imgsrc:img,img_property:{calibInfo:{type:'disable',mmpp:def.featureSet[0].mmpp}}}));setTimeout(()=>{if(W[id]){delete W[id];res(null);}},30000);});
  return { ready: new Promise(r=>ws.on('open',()=>setTimeout(r,300))), ii, close: () => ws.close() }; }
const [pa, pb, ...names0] = process.argv.slice(2);
const A = client(pa), B = client(pb); await A.ready; await B.ready;
const names = names0.length ? names0 : fs.readFileSync('_ok_names.txt','utf8').split(/\r?\n/).map(s=>s.trim()).filter(Boolean);
const objs=(rp)=>{const g=rp&&rp.reports&&rp.reports[0];return ((g&&g.reports)||[]).map(o=>({cx:o.cx,cy:o.cy,rot:o.rotate,sim:o.similarity,j:(o.judgeReports||[]).map(j=>[j.id,+(+j.value).toFixed(5),j.status])})).sort((p,q)=>p.cx-q.cx);};
let same=0, diff=0, none=0; const msA=[], msB=[];
for (const name of names) {
  const f = D + name + '_sbm.hydef'; if (!fs.existsSync(f)) continue;
  const def = JSON.parse(fs.readFileSync(f,'utf8')); const img = 'data/' + name + '.png';
  const [ra, rb] = await Promise.all([A.ii(def,img), B.ii(def,img)]);
  if (ra) msA.push(ra.insp_wall_ms); if (rb) msB.push(rb.insp_wall_ms);
  const oa = objs(ra), ob = objs(rb);
  if (!oa.length && !ob.length) { none++; continue; }
  const eq = oa.length===ob.length && oa.every((o,i)=>{const p=ob[i];return Math.abs(o.cx-p.cx)<1e-6&&Math.abs(o.cy-p.cy)<1e-6&&Math.abs(o.rot-p.rot)<1e-7&&Math.abs(o.sim-p.sim)<1e-6&&JSON.stringify(o.j)===JSON.stringify(p.j);});
  if (eq) same++; else { diff++; console.log(name.padEnd(40)+` DIFF  A ${oa.length} obj ${oa.map(o=>o.sim.toFixed(3)).join(',')}  B ${ob.length} obj ${ob.map(o=>o.sim.toFixed(3)).join(',')}` + (oa.length===ob.length ? '  dpos ' + oa.map((o,i)=>Math.hypot(o.cx-ob[i].cx,o.cy-ob[i].cy).toFixed(5)).join(',') + ' drot ' + oa.map((o,i)=>((o.rot-ob[i].rot)*180/Math.PI).toFixed(4)).join(',') : '')); }
}
const med=(a)=>{a=[...a].sort((x,y)=>x-y);return a[a.length>>1];};
console.log(`\n${same} identical, ${diff} differ, ${none} located nothing on either.  insp_wall_ms median A ${med(msA)?.toFixed(1)}  B ${med(msB)?.toFixed(1)}  (sum A ${msA.reduce((a,b)=>a+b,0).toFixed(0)}  B ${msB.reduce((a,b)=>a+b,0).toFixed(0)})`);
A.close(); B.close(); process.exit(0);
