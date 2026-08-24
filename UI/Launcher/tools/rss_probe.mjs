// Electron's own memory floor: the launcher shell with no payload, no core.
import { _electron as electron } from 'playwright';
import { execSync } from 'node:child_process';
import fs from 'node:fs'; import os from 'node:os'; import path from 'node:path';
const ud = fs.mkdtempSync(path.join(os.tmpdir(), 'rss-'));
const app = await electron.launch({ args: ['.', `--user-data-dir=${ud}`] });
const win = await app.firstWindow();
await win.waitForLoadState('domcontentloaded');
await win.waitForTimeout(6000);
const rows = execSync('powershell -NoProfile -Command "Get-Process electron -ErrorAction SilentlyContinue | Select-Object Id,@{n=\'MB\';e={[int]($_.WorkingSet64/1MB)}} | ConvertTo-Json -Compress"', {encoding:'utf8'});
const procs = JSON.parse(rows);
const list = Array.isArray(procs) ? procs : [procs];
console.log('electron processes:', list.length, ' total RSS MB:', list.reduce((a,b)=>a+b.MB,0));
console.log(list.map(p=>p.MB).sort((a,b)=>b-a).join(' + '));
await app.close(); fs.rmSync(ud,{recursive:true,force:true});
