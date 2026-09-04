// Localisation + measurement stability under rotation and translation, sig360 vs the
// migrated SBM def of the same recipe. Usage (core with INSP_ALLOW_MULTI_CLIENT=1):
//   node stability_sweep.mjs mig_CTA mig_BSG ...      # expects data/<name>.hydef, <name>.png, <name>_sbm.hydef
// Writes $STAB_OUT (default _stability_out.json) with every run. See
// InspectionCore/docs/SBM_STABILITY_2026-09-04.md for what it found the first time.
// Localisation + measurement stability under rotation and translation, sig360 vs migrated SBM.
// Raw websocket to the core (needs INSP_ALLOW_MULTI_CLIENT=1 if a browser is connected).
import fs from 'node:fs'; import WebSocket from 'ws';
const D = '../../../../InspectionCore/Core0_1/data/';
const names = process.argv.slice(2);
const HDR=9, enc=new TextEncoder();
function frame(type,prop,pg,obj){const b=enc.encode(JSON.stringify(obj));const u=new Uint8Array(HDR+b.length+1);u[0]=type.charCodeAt(0);u[1]=type.charCodeAt(1);u[2]=prop;new DataView(u.buffer).setUint16(3,pg,false);new DataView(u.buffer).setUint32(5,b.length+1,false);u.set(b,HDR);return u;}
const ws=new WebSocket('ws://127.0.0.1:4090'); ws.binaryType='arraybuffer';
let pg=100; const waiters={};
ws.on('message',(d)=>{const b=new Uint8Array(d);const ty=String.fromCharCode(b[0],b[1]);const id=new DataView(b.buffer,b.byteOffset).getUint16(3,false);if(ty==='HR'){ws.send(frame('HR',0,1,{a:['d']}));return;}
 const txt=new TextDecoder().decode(b.subarray(HDR)).replace(/\0+$/,'');
 const w=waiters[id]; if(!w)return; if(ty==='RP'){try{w.rp=JSON.parse(txt);}catch(e){w.rp=null;}} if(ty==='SS'){try{const j=JSON.parse(txt); if(j.cmd==='II'){delete waiters[id]; w.res(w.rp);}}catch(e){}}});
const ii=(def,img,perturb)=>new Promise((res,rej)=>{const id=pg++;waiters[id]={res};const body={definfo:def,imgsrc:img,img_property:{calibInfo:{type:'disable',mmpp:def.featureSet[0].mmpp}}};if(perturb)body.img_property.perturb=perturb;ws.send(frame('II',0,id,body));setTimeout(()=>{if(waiters[id]){delete waiters[id];res(null);}},20000);});
await new Promise(r=>ws.on('open',()=>setTimeout(r,400)));
// STAB_QUICK=1: 4 rotations + 4 shifts (9 inspections per def) for fleet-wide runs.
const QUICK = !!process.env.STAB_QUICK;
const ROT = QUICK ? [-10,-5,5,10] : [-10,-8,-6,-4,-2,2,4,6,8,10];
const SHIFT = QUICK ? [[30,0],[-30,0],[0,30],[0,-30]] : [[20,0],[-20,0],[0,20],[0,-20],[50,0],[-50,0],[0,50],[0,-50],[35,35],[-35,-35]];
const tolOf=(n)=>{const m=n.match(/\+\s*([\d.]+)\s*\/\s*-?\s*([\d.]+)/);return m?(+m[1]+ +m[2]):null;};
const out={};
for (const name of names) {
  for (const variant of [name, name+'_sbm']) {
    if (!fs.existsSync(D+variant+'.hydef')) { console.log('skip', variant); continue; }
    const def=JSON.parse(fs.readFileSync(D+variant+'.hydef','utf8')); const mmpp=def.featureSet[0].mmpp; const img='data/'+name+'.png';
    const runs=[]; const one=async(label,p)=>{const rp=await ii(def,img,p);const g=rp&&rp.reports&&rp.reports[0];const o=g&&g.reports&&g.reports[0];runs.push({label,p,located:!!o,locator:g&&g.locator,note:g&&g.locate&&g.locate.code,ms:rp&&rp.insp_wall_ms,pose:o&&{cx:o.cx,cy:o.cy,rot:o.rotate,sim:o.similarity},judges:o?Object.fromEntries((o.judgeReports||[]).map(j=>[j.name||String(j.id),{st:j.status,v:j.value}])):{}});};
    await one('base',null);
    for(const r of ROT) await one('rot'+r,{rot_deg:r,seed:7});
    for(const [x,y] of SHIFT) await one(`shift${x},${y}`,{shift_x:x,shift_y:y,seed:7});
    const base=runs[0]; const rest=runs.slice(1);
    const located=rest.filter(r=>r.located).length;
    // pose residuals
    let rotRes=[],shiftRes=[];
    if(base.located){for(const r of rest){if(!r.located)continue;if(r.p.rot_deg!==undefined){const dr=(r.pose.rot-base.pose.rot)*180/Math.PI;const a=Math.abs(Math.abs(dr)-Math.abs(r.p.rot_deg));rotRes.push(a);}
      else{const dx=(r.pose.cx-base.pose.cx)/mmpp,dy=(r.pose.cy-base.pose.cy)/mmpp;const ex=r.p.shift_x,ey=r.p.shift_y;const cands=[[dx-ex,dy-ey],[dx+ex,dy+ey],[dx-ex,dy+ey],[dx+ex,dy-ey]].map(([a,b])=>Math.hypot(a,b));shiftRes.push(Math.min(...cands));}}}
    const jn=Object.keys(base.judges); const jrows=[];
    for(const j of jn){const b=base.judges[j];const vals=rest.filter(r=>r.located&&r.judges[j]&&r.judges[j].st===0).map(r=>r.judges[j].v);const nOK=vals.length;const nNA=rest.filter(r=>r.located&&r.judges[j]&&r.judges[j].st===-128).length;const nNG=rest.filter(r=>r.located&&r.judges[j]&&r.judges[j].st===-1).length;
      let range=null,sd=null;if(vals.length>1){const mn=Math.min(...vals),mx=Math.max(...vals);range=mx-mn;const m=vals.reduce((a,b)=>a+b,0)/vals.length;sd=Math.sqrt(vals.reduce((a,b)=>a+(b-m)*(b-m),0)/vals.length);}
      jrows.push({name:j,baseSt:b.st,baseV:b.v,tol:tolOf(j),nOK,nNG,nNA,range,sd});}
    out[variant]={locator:base.locator,note:base.note,baseLocated:base.located,baseSim:base.pose&&base.pose.sim,located:located+'/'+rest.length,rotResMaxDeg:rotRes.length?Math.max(...rotRes):null,shiftResMaxPx:shiftRes.length?Math.max(...shiftRes):null,msAvg:Math.round(runs.filter(r=>r.ms).reduce((a,r)=>a+r.ms,0)/Math.max(1,runs.filter(r=>r.ms).length)),judges:jrows,runs};
    const st=out[variant];
    console.log(`\n=== ${variant}  locator=${st.locator}${st.note?'('+st.note+')':''}  base ${st.baseLocated?'located sim='+(+st.baseSim).toFixed(3):'NOT LOCATED'}  perturbed located ${st.located}  rotRes≤${st.rotResMaxDeg==null?'-':st.rotResMaxDeg.toFixed(3)}°  shiftRes≤${st.shiftResMaxPx==null?'-':st.shiftResMaxPx.toFixed(2)}px  ${st.msAvg}ms`);
    for(const r of jrows){const flag=r.baseSt!==0?'  <== base '+(r.baseSt===-128?'NA':'NG'):(r.range!=null&&r.tol&&r.range>r.tol*0.25?'  <== range>25% tol':(r.nNA>0?'  <== NA under perturb':''));
      console.log(`  ${r.name.padEnd(26)} base=${r.baseSt===0?(+r.baseV).toFixed(4):(r.baseSt===-128?'NA':'NG')}  OK/NG/NA=${r.nOK}/${r.nNG}/${r.nNA}  range=${r.range==null?'-':r.range.toFixed(4)} sd=${r.sd==null?'-':r.sd.toFixed(4)} tol=${r.tol==null?'-':r.tol}${flag}`);}
  }
}
fs.writeFileSync(process.env.STAB_OUT||'_stability_out.json', JSON.stringify(out));
ws.close(); process.exit(0);
