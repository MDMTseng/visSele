// Usage: node migrate_dump.mjs <name>...  -- webctld + core running; for each data/<name>.hydef presses the
// real 升級 button and writes data/<name>_sbm.hydef (+png) with exactly what SAVE would write.
// Migrate each def through the real 升級 button and write what SAVE would write to data/<name>_sbm.hydef (+png).
import fs from 'node:fs';
import { makeCtl, toMain, dismissCamModal, loadRecipe, freshPage, sleep } from './lib_enter.mjs';
const ctl = makeCtl(); const { ev } = ctl;
const D = '../../../../InspectionCore/Core0_1/data/';
const clickText = (sel, txt) => ev(`(function(){var bs=[...document.querySelectorAll('${sel}')];var b=bs.find(x=>x.textContent.split(' ').join('').indexOf('${txt}')>=0);if(!b)return 'nobtn';b.click();return 'ok'})()`);
for (const name of process.argv.slice(2)) {
  await freshPage(ctl, 'http://127.0.0.1:8083/'); await toMain(ctl); await dismissCamModal(ctl);
  await loadRecipe(ctl, 'data/' + name);
  await ev(`window.__GP_STORE__.dispatch({ type: 'Edit_Mode' })`);
  for (let i=0;i<60;i++){ const s=await ev(`JSON.stringify(window.__GP_STORE__.getState().UIData.c_state.value)`); if(String(s).indexOf('DEFCONF')>=0) break; await sleep(400); }
  await ev(`window.__GP_STORE__.dispatch({ type: 'DefConf_Lock_Level_Update', data: 0 })`); await sleep(3000);
  await ev(`document.querySelector('[data-testid="upgrade-def"]').click()`); await sleep(700);
  await clickText('.ant-modal-confirm .ant-btn-primary', '升級');
  let summary='none'; for (let i=0;i<120;i++){ summary = await ev(`(function(){var ms=[...document.querySelectorAll('.ant-modal-confirm')];var m=ms.find(x=>x.textContent.indexOf('還沒存檔')>=0||x.textContent.indexOf('抽不到特徵')>=0);return m?m.textContent.split(' ').join(''):'none'})()`); if(summary!=='none') break; await sleep(500); }
  await clickText('.ant-modal-confirm .ant-btn', '知道了'); await sleep(300);
  const gen = await ev(`(function(){var ei=window.__GP_STORE__.getState().UIData.edit_info;return JSON.stringify(window.__GP_UTIL__.defFileGeneration(ei));})()`);
  fs.writeFileSync(D + name + '_sbm.hydef', gen); fs.copyFileSync(D + name + '.png', D + name + '_sbm.png');
  const d = JSON.parse(gen); const s = (d.featureSet[0].inherentfeatures||[]).find(e=>e&&e.name==='@__SBM_INFO__');
  console.log(name, '->', name+'_sbm', 'eng=', d.featureSet[0].locating_engine, 'roi=', !!(s&&s.shape_cache&&s.shape_cache.roi), '|', summary.replace(/已轉換.*$/,'').slice(0,90));
}
