// Dump caliper edge profiles + consensus labels for the edge-NN experiment.
// For every def/image pair: II with edge_profile on; per line/arc report, per caliper k:
//   raw[k] (intensity), g[k] (gradient), sel[k] (chosen sample or -1), hit st (0 miss/1 outlier/2 inlier),
//   hit r (px along search dir from the hit to the fit; target sample = sel - r/step), step, L, polarity, method, min_strength.
// Output: one JSON-lines file, one record per caliper. Held-out split is by recipe name (done at training time).
import fs from 'node:fs'; import WebSocket from 'ws';
const PORT=process.env.CORE_PORT||'4093';
const D='../../../../InspectionCore/Core0_1/data/'; const HDR=9, enc=new TextEncoder();
function frame(type,prop,pg,obj){const b=enc.encode(JSON.stringify(obj));const u=new Uint8Array(HDR+b.length+1);u[0]=type.charCodeAt(0);u[1]=type.charCodeAt(1);u[2]=prop;new DataView(u.buffer).setUint16(3,pg,false);new DataView(u.buffer).setUint32(5,b.length+1,false);u.set(b,HDR);return u;}
const ws=new WebSocket('ws://127.0.0.1:'+PORT); ws.binaryType='arraybuffer'; let pg=5000; const W={};
ws.on('message',(d)=>{const b=new Uint8Array(d);const ty=String.fromCharCode(b[0],b[1]);const id=new DataView(b.buffer,b.byteOffset).getUint16(3,false);if(ty==='HR'){ws.send(frame('HR',0,1,{a:['d']}));return;}const txt=new TextDecoder().decode(b.subarray(HDR)).replace(/\0+$/,'');const w=W[id];if(!w)return;if(ty==='RP'){try{w.rp=JSON.parse(txt);}catch(e){}}if(ty==='SS'){try{if(JSON.parse(txt).cmd==='II'){delete W[id];w.res(w.rp);}}catch(e){}}});
const ii=(def,img)=>new Promise(res=>{const id=pg++;W[id]={res};ws.send(frame('II',0,id,{definfo:def,imgsrc:img,img_property:{calibInfo:{type:'disable',mmpp:def.featureSet[0].mmpp}}}));setTimeout(()=>{if(W[id]){delete W[id];res(null);}},30000);});
await new Promise(r=>ws.on('open',()=>setTimeout(r,400)));
ws.send(frame('ST',0,pg++,{DEBUG_EMIT:{edge_profile:true}})); await new Promise(r=>setTimeout(r,300));
const out=fs.createWriteStream(process.env.OUT||'_nn_profiles.jsonl');
const pairs=[];
for (const n of fs.readFileSync('_ok_names.txt','utf8').split(String.fromCharCode(10)).map(s=>s.trim()).filter(Boolean)) pairs.push({recipe:n, def:n+'_sbm.hydef', img:n+'.png'});
for (const f of fs.readdirSync(D).filter(f=>/^test1_\d{8}_\d{6}\.png$/.test(f)).sort()) pairs.push({recipe:'test1', def:'test1.hydef', img:f});
pairs.push({recipe:'test1', def:'test1.hydef', img:'test1.png'});
let nRec=0, nCal=0, nObj=0;
for (const p of pairs) {
  const def=JSON.parse(fs.readFileSync(D+p.def,'utf8')); const mmpp=def.featureSet[0].mmpp;
  const byId={}; for (const s of def.featureSet[0].features) byId[s.id]=s;
  const rp=await ii(def,'data/'+p.img); const g=rp&&rp.reports&&rp.reports[0]; const objs=(g&&g.reports)||[];
  for (let oi=0; oi<objs.length; oi++) { const o=objs[oi]; nObj++;
    for (const k of ['detectedLines','detectedCircles']) for (const x of (o[k]||[])) {
      const prof=x.extra&&x.extra.edge_profile, hits=x.extra&&x.extra.cal_hits; if (!prof||!prof.g||!prof.raw||!hits) continue;
      const s=byId[x.id]||{}; const e=s.edge||{};
      for (let c=0;c<prof.g.length;c++) { const h=hits[c]||{};
        out.write(JSON.stringify({recipe:p.recipe,img:p.img,obj:oi,prim:x.id,type:k==='detectedLines'?'line':'arc',pst:x.status,cal:c,step:prof.step,L:prof.L,mmpp,
          pol:e.polarity||null,method:e.method||null,ms:e.min_strength??null,sel:prof.sel?prof.sel[c]:null,st:h.st??null,r:h.r??0,s:h.s??null,raw:prof.raw[c],g:prof.g[c]})+'\n'); nCal++; }
    }
  }
  nRec++; if (nRec%25===0) console.log(nRec,'pairs',nObj,'objects',nCal,'calipers');
}
out.end(); console.log('done',nRec,'pairs',nObj,'objects',nCal,'calipers');
ws.send(frame('ST',0,pg++,{DEBUG_EMIT:{edge_profile:false}})); await new Promise(r=>setTimeout(r,200)); ws.close(); process.exit(0);
