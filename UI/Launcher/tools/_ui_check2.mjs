import { _electron as electron } from 'playwright';
import fs from 'node:fs'; import os from 'node:os'; import path from 'node:path';
const SCRATCH='C:/Users/w2110/AppData/Local/Temp/claude/C--Users-w2110-Documents-workspace-visSele/8b8b78b7-1ef0-4c98-8245-47dba9add707/scratchpad';
const PACKED='C:/Users/w2110/Documents/workspace/visSele/UI/Launcher/release-builds/Xception INSP-win32-x64/Xception INSP.exe';
const userData=fs.mkdtempSync(path.join(os.tmpdir(),'lnc-ui2-'));
const appRoot=path.join(userData,'apps');
for(const v of ['2.0.0-rc2','1.1.104']){
  fs.mkdirSync(path.join(appRoot,v,'scripts'),{recursive:true});
  fs.writeFileSync(path.join(appRoot,v,'info.json'),JSON.stringify({version:v}));
  fs.writeFileSync(path.join(appRoot,v,'scripts','boot.js'),'module.exports={};\n');
}
fs.writeFileSync(path.join(appRoot,'current.json'),JSON.stringify({version:'2.0.0-rc2'}));
fs.writeFileSync(path.join(userData,'launcher.json'),JSON.stringify({
  appRoot, workingDir: path.join(userData,'work'), updateSource: SCRATCH+'/updsrc'},null,2));
fs.mkdirSync(path.join(userData,'work'),{recursive:true});
const app=await electron.launch({executablePath:PACKED,args:[`--user-data-dir=${userData}`]});
const win=await app.firstWindow();
await win.waitForLoadState('domcontentloaded');
await win.waitForFunction(()=>document.getElementById('ident').textContent.length>0,null,{timeout:20000});
await win.waitForTimeout(2000);
console.log('modal present at start :', await win.evaluate(()=>!!document.getElementById('modal')));
console.log('main says the offer is :', JSON.stringify(await win.evaluate(()=>window.launcher.updateOffer())));
console.log('status.checkUpdates    :', await win.evaluate(async()=>(await window.launcher.status()).checkUpdates));
console.log('status.update.error    :', await win.evaluate(async()=>(await window.launcher.status()).update.error));
console.log('status.update.release  :', JSON.stringify(await win.evaluate(async()=>(await window.launcher.status()).update.release)));
console.log('status.current         :', await win.evaluate(async()=>(await window.launcher.status()).current));
// now raise it by hand to prove the renderer half works
await win.evaluate(async()=>{ const o=await window.launcher.updateOffer(); if(o) showUpdateOffer(o); });
await win.waitForTimeout(500);
console.log('after manual raise     :', await win.evaluate(()=>!!document.getElementById('modal')),
            await win.evaluate(()=>{const t=document.querySelector('.modalTitle');return t?t.textContent:'';}));
await win.screenshot({path:`${SCRATCH}/ui_offer2.png`});
await app.close();
