import { execSync } from 'node:child_process';
const CTL='http://127.0.0.1:8765';
const MIN=Number(process.argv[2]||25), EVERY=60000;
const post=async(p,b)=>(await fetch(CTL+p,{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify(b)})).json();
const expr=`(()=>{const m=performance.memory||{};const st=(window.__GP_STORE__&&window.__GP_STORE__.getState())||{};
const rs=st.UIData&&st.UIData.edit_info&&st.UIData.edit_info.reportStatisticState;
return JSON.stringify({heap:+(m.usedJSHeapSize/1048576).toFixed(1),total:+(m.totalJSHeapSize/1048576).toFixed(1),
hist:rs?(rs.historyReport||[]).length:-1,track:rs?(rs.trackingWindow||[]).length:-1,add:rs?(rs.newAddedReport||[]).length:-1});})()`;
// tasklist, not PowerShell: spawning powershell every 60s under load kept
// timing out and the column filled with -1 -- the one number the operator
// actually watches. tasklist is a single fast exe and its CSV is trivial.
const chromeRSS=()=>{try{
  const o=execSync('tasklist /FI "IMAGENAME eq chrome.exe" /FO CSV /NH',{encoding:'utf8',timeout:15000});
  let kb=0;
  for(const line of o.split(/\r?\n/)){
    const m=line.match(/"([\d,]+) K"\s*$/);
    if(m) kb+=Number(m[1].replace(/,/g,''));
  }
  return Math.round(kb/1024);
}catch{return -1;}};
const t0=Date.now();
console.log('t_min,heapMB,totalMB,chromeRSS_MB,history,tracking,newAdded');
for(let i=0;i<=MIN;i++){
  try{const r=await post('/eval',{expr});const v=JSON.parse(r.result);
    console.log([((Date.now()-t0)/60000).toFixed(1),v.heap,v.total,chromeRSS(),v.hist,v.track,v.add].join(','));
  }catch(e){console.log('err,'+e.message);}
  await new Promise(r=>setTimeout(r,EVERY));
}
