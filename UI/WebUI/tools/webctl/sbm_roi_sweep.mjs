// node sbm_roi_sweep.mjs [spacings] [names...]   -- core with INSP_ALLOW_MULTI_CLIENT=1 on $CORE_PORT (default 4093),
//   cwd InspectionCore/Core0_1.  spacings default "-1" (auto = ROI half). e.g. node sbm_roi_sweep.mjs -1,12,20
//
// Decide, per recipe, whether ROI-point de-overlap (shape_roi_spacing) is safe to adopt. De-overlapping removes
// redundant/overlapping refine windows (fleet: 598 pairs < 30 px) -- faster and better-conditioned -- but it MOVES the
// refined pose and therefore the measured values on a real part (ok11 judge38 went 2.3032 FAIL -> 2.2937 PASS). On a
// metrology machine that is a per-recipe decision against ground truth, so the automatic rule here is deliberately
// strict: adopt ONLY where no judge verdict changes on the unperturbed part AND it is faster; any verdict change (either
// direction) is put on a REVIEW list for human eyes, never auto-adopted. Robustness margin under a small augmentation
// set is reported so a reviewer can see whether the conditioning actually improved.
import fs from 'node:fs'; import WebSocket from 'ws';
const PORT = process.env.CORE_PORT || '4093'; const D = '../../../../InspectionCore/Core0_1/data/'; const HDR = 9, enc = new TextEncoder();
function frame(t,pr,pg,o){const b=enc.encode(JSON.stringify(o));const u=new Uint8Array(HDR+b.length+1);u[0]=t.charCodeAt(0);u[1]=t.charCodeAt(1);u[2]=pr;new DataView(u.buffer).setUint16(3,pg,false);new DataView(u.buffer).setUint32(5,b.length+1,false);u.set(b,HDR);return u;}
const ws = new WebSocket('ws://127.0.0.1:' + PORT); ws.binaryType = 'arraybuffer'; let pg = 17000; const W = {};
ws.on('message',(d)=>{const b=new Uint8Array(d);const ty=String.fromCharCode(b[0],b[1]);const id=new DataView(b.buffer,b.byteOffset).getUint16(3,false);if(ty==='HR'){ws.send(frame('HR',0,1,{a:['d']}));return;}const txt=new TextDecoder().decode(b.subarray(HDR)).replace(/\0+$/,'');const w=W[id];if(!w)return;if(ty==='RP'){try{w.rp=JSON.parse(txt);}catch(e){}}if(ty==='SS'){try{if(JSON.parse(txt).cmd==='II'){delete W[id];w.res(w.rp);}}catch(e){}}});
const ii=(def,img,pert)=>new Promise(res=>{const id=pg++;W[id]={res};const body={definfo:def,imgsrc:img,img_property:{calibInfo:{type:'disable',mmpp:def.featureSet[0].mmpp}}};if(pert)body.img_property.perturb=pert;ws.send(frame('II',0,id,body));setTimeout(()=>{if(W[id]){delete W[id];res(null);}},30000);});
await new Promise(r=>ws.on('open',()=>setTimeout(r,400)));

const arg0 = process.argv[2] && /^[-0-9.,]+$/.test(process.argv[2]) ? process.argv[2] : null;
const spacings = (arg0 || '-1').split(',').map(Number);
const names0 = process.argv.slice(arg0 ? 3 : 2);
const names = names0.length ? names0 : fs.readFileSync('_ok_names.txt','utf8').split(/\r?\n/).map(s=>s.trim()).filter(Boolean);
const clone=(d)=>JSON.parse(JSON.stringify(d));
const setk=(d,k,v)=>{ d[k]=v; d.featureSet[0][k]=v; };
const objs=(rp)=>{const g=rp&&rp.reports&&rp.reports[0];return (g&&g.reports)||[];};
const judges=(o)=>(o.judgeReports||[]).map(j=>[j.id, +(+j.value).toFixed(5), j.status]).sort((a,b)=>a[0]-b[0]);
const nok=(o)=>(o.judgeReports||[]).filter(j=>j.status===0).length;
// small robustness set (same convention as sbm_sweep): rotation, shift, brightness
const AUG=[{rot_deg:1,seed:7},{rot_deg:-1,seed:7},{shift_x:0.5,shift_y:0.5,seed:7},{gain:0.85,seed:7}];
const mmppOf=(d)=>d.featureSet[0].mmpp;

const out={}; let nAdopt=0,nReview=0,nNoGain=0,nSkip=0;
for (const name of names) {
  const f = D + name + '_sbm.hydef'; if (!fs.existsSync(f)) continue;
  const def0 = JSON.parse(fs.readFileSync(f,'utf8')); const mmpp = mmppOf(def0); const img='data/'+name+'.png';
  const base = objs(await ii(def0, img)); if (!base.length) { console.log(name.padEnd(40)+' skip: no object'); nSkip++; continue; }
  const baseJ = base.map(judges); const baseNok = base.map(nok);
  // baseline time (median of a few unperturbed runs; the machine is noisy)
  const t=[]; for(let k=0;k<4;k++){const r=await ii(def0,img); if(r&&r.insp_wall_ms)t.push(r.insp_wall_ms);} t.sort((a,b)=>a-b); const baseMs=t[t.length>>1]||0;
  let best=null;
  for (const sp of spacings) {
    const d = clone(def0); setk(d, 'shape_roi_spacing', sp);
    const o = objs(await ii(d, img));
    if (o.length !== base.length) { (out[name]??={}); out[name][sp]={verdict:'objcount', n:o.length}; continue; }
    // verdict change on the unperturbed part?
    let flips=[]; for(let i=0;i<o.length;i++){ const nj=judges(o[i]); for(let k=0;k<baseJ[i].length;k++){ const a=baseJ[i][k], b=nj.find(x=>x[0]===a[0]); if(b && a[2]!==b[2]) flips.push(`obj${i}j${a[0]} ${a[2]}->${b[2]}${(a[2]!==0&&b[2]===0)?'(FAIL->PASS!)':''}`); } }
    // timing + robustness margin under augmentation (min passing-judge count over aug)
    const tt=[]; for(let k=0;k<4;k++){const r=await ii(d,img); if(r&&r.insp_wall_ms)tt.push(r.insp_wall_ms);} tt.sort((a,b)=>a-b); const ms=tt[tt.length>>1]||0;
    let augOk=true; for(const p of AUG){ const ro=objs(await ii(d,img,p)); if(ro.length!==base.length){augOk=false;break;} for(let i=0;i<ro.length;i++) if(nok(ro[i])<baseNok[i]){augOk=false;break;} if(!augOk)break; }
    const gain = baseMs>0 ? (1-ms/baseMs) : 0;
    (out[name]??={}); out[name][sp]={flips, ms:+ms.toFixed(1), gain:+gain.toFixed(3), augOk};
    if (flips.length===0 && augOk && gain>=0.05) { if(!best || sp<best.sp) best={sp,ms,gain}; }  // prefer the widest safe spacing (most negative / auto)
    if (flips.length) { /* recorded */ }
  }
  const anyFlip = Object.values(out[name]||{}).some(v=>v.flips&&v.flips.length);
  const decision = best ? `ADOPT sp=${best.sp} (${(best.gain*100).toFixed(0)}% faster)` : (anyFlip ? 'REVIEW (judge verdict changes)' : 'keep (no safe gain)');
  if (best) nAdopt++; else if (anyFlip) nReview++; else nNoGain++;
  out[name].decision = decision; out[name].baseMs = +baseMs.toFixed(1);
  const flipStr = anyFlip ? '  ' + Object.entries(out[name]).filter(([k,v])=>v.flips&&v.flips.length).map(([k,v])=>`sp${k}:${v.flips.join(';')}`).join(' ') : '';
  console.log(name.padEnd(40)+' '+decision+`  baseMs ${baseMs.toFixed(1)}`+flipStr);
}
console.log(`\n${nAdopt} adopt, ${nReview} REVIEW (verdict changes -- need ground truth), ${nNoGain} no safe gain, ${nSkip} skipped.`);
fs.writeFileSync(process.env.OUT || '_roi_sweep.json', JSON.stringify(out, null, 1));
ws.close(); process.exit(0);
