// node sbm_adopt.mjs [--dry] [--trust] [--spacing] [names...]  -- write the verified per-recipe SBM
// fields into the LOCAL def copies (InspectionCore/Core0_1/data/<name>_sbm.hydef):
//   --trust    shape_trust_na=true + shape_trust_res_max from trust_budget.json (REVIEW entries skipped)
//   --spacing  shape_roi_spacing=-1 for the recipes in roi_spacing_adopt.json "adopt" (review skipped)
// Fields go on featureSet[0] (where the parser reads shape_*). A .bak_adopt_<ts> copy is written
// once per file. This edits LOCAL copies only: hy_sync is read-only and data/ never ships; the
// production step is the def migrate flow with these same fields.
import fs from 'node:fs';
const D='../../../../InspectionCore/Core0_1/data/';
const args=process.argv.slice(2); const dry=args.includes('--dry'); const doTrust=args.includes('--trust'); const doSp=args.includes('--spacing');
const names=args.filter(a=>!a.startsWith('--'));
if(!doTrust&&!doSp){console.error('nothing to do: pass --trust and/or --spacing');process.exit(2);}
const budget=doTrust?JSON.parse(fs.readFileSync('trust_budget.json','utf8')):{};
const spacing=doSp?JSON.parse(fs.readFileSync('roi_spacing_adopt.json','utf8')).adopt:{};
const all=new Set([...Object.keys(budget),...Object.keys(spacing)]); const list=names.length?names:[...all];
const ts=new Date().toISOString().replace(/[-:T]/g,'').slice(0,12); let nT=0,nS=0,nSkip=0,nW=0;
for(const name of list){const f=D+name+'_sbm.hydef'; if(!fs.existsSync(f)){nSkip++;continue;}
  const d=JSON.parse(fs.readFileSync(f,'utf8')); const fs0=d.featureSet[0]; const ch=[];
  if(doTrust&&budget[name]){const b=budget[name]; if(b.res_max!=null){fs0.shape_trust_na=true; fs0.shape_trust_res_max=b.res_max; ch.push(`trust_na res_max=${b.res_max}`); nT++;} else ch.push('trust: REVIEW, skipped');}
  if(doSp&&spacing[name]){fs0.shape_roi_spacing=spacing[name].shape_roi_spacing; ch.push(`roi_spacing=${spacing[name].shape_roi_spacing}`); nS++;}
  if(!ch.some(c=>!c.includes('skipped')))continue;
  console.log(name.padEnd(42),ch.join('  '));
  if(dry)continue;
  const bak=f+'.bak_adopt_'+ts; if(!fs.existsSync(bak))fs.copyFileSync(f,bak);
  fs.writeFileSync(f,JSON.stringify(d,null,1)); nW++;
}
console.log(`\n${dry?'DRY RUN: ':''}${nT} trust, ${nS} spacing, ${nW} files written, ${nSkip} names without a local _sbm def`);
