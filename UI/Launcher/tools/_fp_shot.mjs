// The whole first page, as it renders now. autoStart:false so the core never
// takes the window, and nothing is installed or selected -- the real appRoot is
// only READ (prune runs inside startCore, which does not run).
import { _electron as electron } from 'playwright';
import fs from 'node:fs'; import path from 'node:path'; import os from 'node:os';
const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname.slice(1)), '..');
const REPO = path.resolve(ROOT, '..', '..');
const ud = fs.mkdtempSync(path.join(os.tmpdir(), 'fp-'));
fs.writeFileSync(path.join(ud, 'launcher.json'), JSON.stringify({
  appRoot: path.join(REPO, 'export_v2', 'app'),
  workingDir: path.join(REPO, 'InspectionCore', 'Core0_1'),
  updateSource: path.join(REPO, 'InspectionCore', 'Core0_1', 'data', 'sync', 'DEV', 'X2Updates', 'INSP'),
  autoStart: false, checkUpdates: false,
}, null, 2));
const W = +(process.argv[3] || 0), H = +(process.argv[4] || 0);
const app = await electron.launch({ args: [ROOT, `--user-data-dir=${ud}`] });
const win = await app.firstWindow();
await win.waitForLoadState('domcontentloaded');
if (W && H) { await win.setViewportSize({ width: W, height: H }); }
await new Promise((r) => setTimeout(r, 4000));
const out = process.argv[2] || path.join(REPO, '_verify_shots', 'fp.png');
fs.mkdirSync(path.dirname(out), { recursive: true });
await win.screenshot({ path: out, fullPage: true });
// Widths of the three grid columns, per panel: a row that renders the wrong
// number of cells shows up here as a column that has no business being wide.
const cols = await win.evaluate(() => {
  const o = {};
  for (const id of ['plan', 'updateSrc', 'settings']) {
    const e = document.getElementById(id);
    if (!e) continue;
    o[id] = { cols: getComputedStyle(e).gridTemplateColumns, cells: e.children.length };
  }
  return o;
});
console.log(JSON.stringify(cols, null, 1));
console.log('shot ->', out);
await app.close();
