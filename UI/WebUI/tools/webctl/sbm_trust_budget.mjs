// node sbm_trust_budget.mjs <port> [names...]  -- derive each recipe's poor_fit threshold
// (shape_trust_res_max) from a DEFORMATION BUDGET, so wiring trust -> judges NA
// (shape_trust_na) cannot false-flag a part that is merely inside its own spec.
//
// The ROI refine fits a RIGID model; a part that is sheared / scaled within spec still
// locates (found-rate ~100%) but its mean normal residual rises monotonically with the
// deformation (SBM_TRUST_SCORE_DESIGN.md, deformation caveat: a global 1.0 px flags
// 179/239 recipes at 1% scale). So the threshold is per recipe:
//     res_max = max(FLOOR, MARGIN * max residual over the in-spec augmentation set,
//                   NOISE_K * unperturbed residual)
// with the in-spec set = rotation / shift / gain (sensor) + shear / scale (part). The
// budget levels are the operator's statement about the part (BUDGET_SHEAR, BUDGET_SCALE
// env); the defaults are what the fleet showed as a typical in-spec deformation.
//
// Output: trust_budget.json {name: {base, inspec_max, res_max, misses, flags_at_1px}} and a
// summary. It NEVER edits a def; adoption = migrate shape_trust_res_max + shape_trust_na.
import fs from 'node:fs'; import WebSocket from 'ws';
const PORT=process.argv[2]||'4093'; const D='../../../../InspectionCore/Core0_1/data/_test/'; const HDR=9,enc=new TextEncoder();
function frame(t,pr,pg,o){const b=enc.encode(JSON.stringify(o));const u=new Uint8Array(HDR+b.length+1);u[0]=t.charCodeAt(0);u[1]=t.charCodeAt(1);u[2]=pr;new DataView(u.buffer).setUint16(3,pg,false);new DataView(u.buffer).setUint32(5,b.length+1,false);u.set(b,HDR);return u;}
const ws=new WebSocket('ws://127.0.0.1:'+PORT);ws.binaryType='arraybuffer';let pg=25000;const W={};
ws.on('message',d=>{const b=new Uint8Array(d);const ty=String.fromCharCode(b[0],b[1]);const id=new DataView(b.buffer,b.byteOffset).getUint16(3,false);if(ty==='HR'){ws.send(frame('HR',0,1,{a:['d']}));return;}const tx=new TextDecoder().decode(b.subarray(HDR)).replace(/\0+$/,'');const w=W[id];if(!w)return;if(ty==='RP'){try{w.rp=JSON.parse(tx);}catch(e){}}if(ty==='SS'){try{if(JSON.parse(tx).cmd==='II'){delete W[id];w.res(w.rp);}}catch(e){}}});
const ii=(def,img,pert)=>new Promise(res=>{const id=pg++;W[id]={res};const body={definfo:def,imgsrc:img,img_property:{calibInfo:{type:'disable',mmpp:def.featureSet[0].mmpp}}};if(pert)body.img_property.perturb=pert;ws.send(frame('II',0,id,body));setTimeout(()=>{if(W[id]){delete W[id];res(null);}},30000);});
await new Promise(r=>ws.on('open',()=>setTimeout(r,300)));
const objs=(rp)=>{const g=rp&&rp.reports&&rp.reports[0];return (g&&g.reports)||[];};
const prim=(os)=>os.slice().sort((a,b)=>b.similarity-a.similarity)[0];
const names=process.argv.slice(3); const list=names.length?names:fs.readFileSync('_ok_names.txt','utf8').split(/\r?\n/).map(s=>s.trim()).filter(Boolean);

const SHEAR=+(process.env.BUDGET_SHEAR||0.02), SCALE=+(process.env.BUDGET_SCALE||0.01);
const BASE_MAX=+(process.env.BUDGET_BASE_MAX||1.0);
const FLOOR=+(process.env.BUDGET_FLOOR||0.3), MARGIN=+(process.env.BUDGET_MARGIN||1.5), NOISE_K=+(process.env.BUDGET_NOISE_K||3);
const AUG=[
  {rot_deg:1,seed:7},{rot_deg:-1,seed:7},{shift_x:0.5,shift_y:0.5,seed:7},{gain:0.85,seed:7},
  {skew:SHEAR,seed:7},{skew:-SHEAR,seed:7},{scale:1-SCALE,seed:7},{scale:1+SCALE,seed:7},
  {skew:SHEAR,rot_deg:1,seed:7},{scale:1+SCALE,rot_deg:-1,seed:7},
];
const out={}; let n=0, nMiss=0, nFlag1=0, nReview=0;
console.log(`budget: shear ${SHEAR} scale ${SCALE} | floor ${FLOOR} margin ${MARGIN} noise_k ${NOISE_K}`);
console.log('name                                     base   inspec_max  res_max  miss  flags@1px');
for(const name of list){const f=D+name+'_sbm.hydef'; if(!fs.existsSync(f))continue;
  const def=JSON.parse(fs.readFileSync(f,'utf8')); const img='data/_test/'+name+'.png';
  const bres=[]; for(let k=0;k<3;k++){const o=prim(objs(await ii(def,img))); if(o&&o.trust)bres.push(o.trust.residual);}
  if(!bres.length){console.log(name.padEnd(40),'no primary'); continue;}
  const base=bres.sort((a,b)=>a-b)[bres.length>>1];
  // A threshold is derived from a KNOWN-GOOD baseline only. A recipe whose own
  // reference image already fits worse than BASE_MAX is exactly what the gate exists
  // to catch (ok39 2.8 px, ok97 4.8 px were the validated true positives); deriving a
  // budget from it would just switch the gate off. Those go to review, not to a number.
  if(base>BASE_MAX){out[name]={base:+base.toFixed(3),review:'baseline residual already above BASE_MAX -- ground-truth this recipe before enabling trust NA'}; nReview++; console.log(`${name.padEnd(40)} ${base.toFixed(3)}  REVIEW: baseline already > ${BASE_MAX} px`); continue;}
  let inMax=0, miss=0, flag1=0;
  for(const a of AUG){const o=prim(objs(await ii(def,img,a))); if(!o||!o.trust){miss++;continue;} inMax=Math.max(inMax,o.trust.residual); if(o.trust.residual>1.0)flag1++;}
  const resMax=Math.ceil(Math.max(FLOOR, MARGIN*inMax, NOISE_K*base)*20)/20;
  out[name]={base:+base.toFixed(3),inspec_max:+inMax.toFixed(3),res_max:resMax,misses:miss,flags_at_1px:flag1};
  n++; if(miss)nMiss++; if(flag1)nFlag1++;
  console.log(`${name.padEnd(40)} ${base.toFixed(3)}  ${inMax.toFixed(3).padStart(9)}  ${String(resMax).padStart(7)}  ${String(miss).padStart(4)}  ${String(flag1).padStart(5)}`);
}
fs.writeFileSync('trust_budget.json',JSON.stringify(out,null,1));
const rm=Object.values(out).filter(o=>o.res_max!=null).map(o=>o.res_max).sort((a,b)=>a-b);
console.log(`\n${n} recipes budgeted, ${nReview} REVIEW (baseline > ${BASE_MAX} px -- ground-truth first); ${nFlag1} would false-flag at the global 1.0 px under in-spec deformation; ${nMiss} had a miss in the set.`);
if(rm.length)console.log(`derived res_max: min ${rm[0]} median ${rm[rm.length>>1]} p95 ${rm[Math.floor(0.95*(rm.length-1))]} max ${rm[rm.length-1]}  -> trust_budget.json`);
process.exit(0);
