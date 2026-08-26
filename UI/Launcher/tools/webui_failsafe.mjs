// The inspection UI's update notice must vanish silently when the host cannot
// answer -- and the inspection UI must keep working.
//
//   node tools/webui_failsafe.mjs        (needs UI/WebUI/dist built)
//
// The WebUI is not only ever hosted by this launcher: it runs in a plain
// browser against the Vite dev server, and it will be run by launchers older
// than whatever added a given IPC. So the notice is checked against five hosts
// -- four broken in a different way, one working -- and in the four broken ones
// the requirement is that NOTHING happens: no throw, no notification, and the
// rest of the UI still renders.
import { _electron as electron } from 'playwright';
import http from 'node:http'; import fs from 'node:fs'; import path from 'node:path';
import os from 'node:os';
import { fileURLToPath } from 'node:url';

const dist = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..', 'WebUI', 'dist');
if (!fs.existsSync(path.join(dist, 'index.html'))) {
  console.error(`no built WebUI at ${dist} -- run "npx vite build" in UI/WebUI first`);
  process.exit(2);
}

// A minimal Electron host, so each case differs ONLY in what the preload
// exposes. Using the real launcher would not let us withhold its own API.
const HOST = fs.mkdtempSync(path.join(os.tmpdir(), 'failsafe-host-'));
fs.writeFileSync(path.join(HOST, 'package.json'), JSON.stringify({ name: 'failsafe-host', version: '1.0.0', main: 'main.js' }));
fs.writeFileSync(path.join(HOST, 'main.js'), `
const { app, BrowserWindow } = require('electron');
app.whenReady().then(() => {
  const pre = process.env.FS_PRELOAD;
  const w = new BrowserWindow({ width: 1200, height: 900, show: false,
    webPreferences: { contextIsolation: true, nodeIntegration: false, sandbox: true,
                      ...(pre ? { preload: pre } : {}) } });
  w.loadURL(process.env.FS_URL);
});
app.on('window-all-closed', () => app.quit());
`);
const PRELOADS = {
  old: `contextBridge.exposeInMainWorld('launcher', { pickFolder: () => {}, status: () => Promise.resolve({}) });`,
  throwing: `contextBridge.exposeInMainWorld('launcher', {
    updateCheck: () => Promise.reject(new Error("No handler registered for 'launcher:updateCheck'")), updateApply: () => {} });`,
  garbage: `contextBridge.exposeInMainWorld('launcher', {
    updateCheck: () => Promise.resolve({ hello: 'world' }), updateApply: () => {} });`,
  working: `contextBridge.exposeInMainWorld('launcher', {
    updateCheck: () => Promise.resolve({ ok: true, source: 'D:/sync', error: null,
      release: { version: '1.1.104' }, packages: [], current: '1.1.103', running: '1.1.103',
      pending: null, available: { version: '1.1.104', file: '1.1.104.zip' } }),
    updateApply: () => Promise.resolve({ ok: true, version: '1.1.104' }) });`,
};
for (const [k, body] of Object.entries(PRELOADS)) {
  fs.writeFileSync(path.join(HOST, `pre_${k}.js`), `const { contextBridge } = require('electron');
${body}
`);
}

const TYPES = { '.html':'text/html', '.js':'text/javascript', '.css':'text/css', '.ttf':'font/ttf' };
const srv = http.createServer((req,res)=>{
  const rel = decodeURIComponent(req.url.split('?')[0]);
  const p = path.join(dist, rel === '/' ? 'index.html' : rel);
  fs.readFile(p,(e,d)=>{ if(e){res.writeHead(404);res.end();return;}
    res.writeHead(200,{'content-type':TYPES[path.extname(p)]||'application/octet-stream'});res.end(d); });
});
await new Promise(r=>srv.listen(0,r));
const url = `http://127.0.0.1:${srv.address().port}/`;
let fail=0; const ok=(c,m,d)=>{console.log((c?'PASS  ':'FAIL  ')+m+(d?'  -- '+d:'')); if(!c)fail++;};

async function run(kind) {
  const env = { ...process.env, FS_URL: url };
  if (kind !== 'none') env.FS_PRELOAD = path.join(HOST, `pre_${kind}.js`);
  const app = await electron.launch({ args: [HOST], env });
  const win = await app.firstWindow();
  const errs = [];
  win.on('pageerror', e => errs.push(String(e && e.message)));
  await win.waitForTimeout(6000);
  const notices = await win.locator('.ant-notification-notice').count().catch(()=>-1);
  const alive = await win.evaluate(() => { const c = document.getElementById('container'); return !!c && c.children.length > 0; }).catch(()=>false);
  await app.close();
  const updateErrs = errs.filter(e => /update|launcher/i.test(e));
  console.log(`  [${kind}] pageerrors=${errs.length} update-related=${updateErrs.length} notices=${notices} rendered=${alive}`);
  if (updateErrs.length) console.log('   ', updateErrs.slice(0,2).join(' | '));
  return { errs, updateErrs, notices, alive };
}

for (const [kind, label] of [['none','no window.launcher (dev server / plain browser)'],
                             ['old','an OLDER launcher with no updateCheck'],
                             ['throwing','updateCheck rejects (handler absent)'],
                             ['garbage','updateCheck answers with the wrong shape']]) {
  const r = await run(kind);
  ok(r.updateErrs.length === 0, `${label}: nothing in the update path throws`);
  ok(r.notices === 0, `${label}: nothing is shown`);
  ok(r.alive, `${label}: the inspection UI still renders`);
}
{
  const r = await run('working');
  ok(r.notices >= 1, 'a WORKING host does show the notice', String(r.notices));
  ok(r.alive, 'and the inspection UI still renders');
}
srv.close();
console.log(fail?`${fail} FAILURES`:'--- all pass ---'); process.exit(fail?1:0);
