// Drive the PACKAGED launcher against a throwaway userData so the three new
// screens can be seen: the update question, the password prompt, and a version
// row that failed. Nothing here touches the real config or the real app root.
import { _electron as electron } from 'playwright';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const SCRATCH = 'C:/Users/w2110/AppData/Local/Temp/claude/C--Users-w2110-Documents-workspace-visSele/8b8b78b7-1ef0-4c98-8245-47dba9add707/scratchpad';
const PACKED = 'C:/Users/w2110/Documents/workspace/visSele/UI/Launcher/release-builds/Xception INSP-win32-x64/Xception INSP.exe';
const userData = fs.mkdtempSync(path.join(os.tmpdir(), 'lnc-ui-'));
const appRoot = path.join(userData, 'apps');

// two installed versions, so the table has rows to mark
for (const v of ['2.0.0-rc2', '1.1.104']) {
  fs.mkdirSync(path.join(appRoot, v, 'scripts'), { recursive: true });
  fs.writeFileSync(path.join(appRoot, v, 'info.json'), JSON.stringify({ version: v }));
  fs.writeFileSync(path.join(appRoot, v, 'scripts', 'boot.js'), 'module.exports={};\n');
}
fs.writeFileSync(path.join(appRoot, 'current.json'), JSON.stringify({ version: '2.0.0-rc2' }));
// one of them failed its set-up, so a "!" appears
fs.writeFileSync(path.join(appRoot, 'update_state.json'), JSON.stringify({
  failures: { '1.1.104': { refused: false, reason: 'npm install 失敗:無法連線到套件伺服器', at: new Date().toISOString(), tries: 2 } },
  skipped: [],
}, null, 2));
fs.writeFileSync(path.join(userData, 'launcher.json'), JSON.stringify({
  appRoot, workingDir: path.join(userData, 'work'),
  updateSource: SCRATCH + '/updsrc',
}, null, 2));
fs.mkdirSync(path.join(userData, 'work'), { recursive: true });

const app = await electron.launch({ executablePath: PACKED, args: [`--user-data-dir=${userData}`] });
const win = await app.firstWindow();
await win.waitForLoadState('domcontentloaded');
await win.waitForFunction(() => document.getElementById('ident').textContent.length > 0, null, { timeout: 20000 });
await win.waitForTimeout(2500);

const shot = async (name) => { await win.screenshot({ path: `${SCRATCH}/ui_${name}.png` }); console.log('shot:', name); };

// 1) the update question, pushed at start-up
const hasModal = await win.evaluate(() => !!document.getElementById('modal'));
console.log('update modal raised at start:', hasModal);
if (hasModal) console.log('  title:', await win.textContent('.modalTitle'));
await shot('offer');

// 2) close it, then open a failed version's detail
await win.evaluate(() => { const m = document.getElementById('modal'); if (m) m.remove(); });
const bangs = await win.locator('#versions .tag.bad').all();
console.log('failure marks in the version table:', bangs.length);
for (const b of bangs) console.log('  mark:', (await b.textContent()).trim());
const bang = win.locator('#versions .tag.bad', { hasText: '!' }).first();
if (await bang.count()) { await bang.click(); await win.waitForTimeout(400);
  console.log('detail title:', await win.textContent('.modalTitle'));
  console.log('detail reason:', (await win.textContent('.modalReason')).trim()); }
await shot('failure');

// 3) the password prompt, from 設為現行
await win.evaluate(() => { const m = document.getElementById('modal'); if (m) m.remove(); });
const setCur = win.locator('#versions button', { hasText: '設為現行' }).first();
if (await setCur.count()) { await setCur.click(); await win.waitForTimeout(500);
  console.log('password modal:', await win.textContent('.modalTitle'));
  console.log('  body:', (await win.textContent('.modalBody')).replace(/\s+/g,' ').trim().slice(0,90)); }
await shot('password');

await app.close();
console.log('userData:', userData);
