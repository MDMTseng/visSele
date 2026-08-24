// Screenshot the shell with a payload installed, so the version table and the
// settings block are populated rather than showing their first-run state.
//
//   node tools/shot.mjs <update.zip> <out.png>
import { _electron as electron } from 'playwright';
import { createRequire } from 'node:module';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const require = createRequire(import.meta.url);
const { Config } = require('../src/config');
const { AppStore } = require('../src/apps');
const { Updater } = require('../src/updater');

const here = path.dirname(fileURLToPath(import.meta.url));
const appDir = path.resolve(here, '..');
const ZIP = process.argv[2];
const OUT = process.argv[3] || path.join(os.tmpdir(), 'launcher-shell.png');

const userData = fs.mkdtempSync(path.join(os.tmpdir(), 'launcher-shot-'));
const cfg = new Config(userData);
const apps = new AppStore(cfg);
// Point the working directory at something that does not exist, so the
// launcher stops at the shell instead of starting a real core and handing the
// window to the WebUI. Without this the screenshot is of whatever the WebUI
// happened to be showing -- which, with no camera and no machine, is nothing.
cfg.values.workingDir = path.join(userData, 'no-such-working-dir');
cfg.save();

const r = await new Updater(cfg, apps).install(ZIP, () => {});
apps.setCurrent(r.version);
// A second version so the table shows both the current one and a selectable
// one -- the screenshot should show the state an operator makes decisions in.
fs.cpSync(apps.versionDir(r.version), apps.versionDir('1.9.4'), { recursive: true });
fs.writeFileSync(path.join(apps.versionDir('1.9.4'), 'info.json'),
                 JSON.stringify({ version: '1.9.4' }));

const app = await electron.launch({
  args: [appDir, `--user-data-dir=${userData}`],
  env: { ...process.env },
});
const win = await app.firstWindow();
await win.waitForLoadState('domcontentloaded');
await win.waitForTimeout(4000);
await win.screenshot({ path: OUT });
console.log(OUT);
await app.close();
fs.rmSync(userData, { recursive: true, force: true });
