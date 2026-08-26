// Xception INSP launcher -- main process.
//
// Responsibilities, and nothing else:
//   * own one window
//   * let the operator say WHERE things are: the app folder, and the machine's
//     working directory
//   * pick which installed version runs
//   * ask that version's scripts/boot.js HOW it starts, then start it, drain
//     its output, watch its health, and stop it properly
//   * install new versions from a local package
//
// WHAT IT DOES NOT KNOW. There is no executable name in this file, no argument,
// no port, no "Core/", no "WebUI/", no "data/". Every one of those belongs to a
// particular application and a particular machine, and every one of them was
// hard-coded in the launcher this replaces. The launcher's own vocabulary is
// two paths the operator chooses and one file the application supplies.
//
// It also runs no server. The launcher it replaces started express, an Apollo
// GraphQL endpoint on 8085 and a mongoose connection, and served a static
// InspMonitor build -- none of which this machine's settings point at any more.
// The single thing the WebUI actually used from all of that was one native
// folder-picker, reached over a WebSocket. It is an IPC call now.
'use strict';

const { app, BrowserWindow, ipcMain, dialog, shell } = require('electron');
const path = require('node:path');
const fs = require('node:fs');

const { Config } = require('./src/config');
const { AppStore } = require('./src/apps');
const { Updater } = require('./src/updater');
const { Supervisor } = require('./src/supervisor');
const boot = require('./src/boot');

// Before anything else. The old launcher asked for the lock AFTER building and
// loading its window, so a second copy flashed a window, loaded the whole UI,
// and only then quit -- and for those few seconds two processes were both
// willing to spawn a core.
if (!app.requestSingleInstanceLock()) {
  app.quit();
  process.exit(0);
}

let cfg = null;
let apps = null;
let updater = null;
let supervisor = null;
let win = null;

// 'shell' = the launcher's own pages (setup, update, error).
// 'app'   = the application's UI. Anything that would rewrite files under a
//           running core is refused in this state.
let uiState = 'shell';
let lastExit = null;
let lastPlanError = null;
let quitting = false;

const SHELL_INDEX = path.join(__dirname, 'shell', 'index.html');

function send(channel, payloadObj) {
  if (win && !win.isDestroyed()) win.webContents.send(channel, payloadObj);
}
function shellLog(message) {
  send('launcher:log', { at: Date.now(), message });
}
function isDir(p) {
  try { return fs.statSync(p).isDirectory(); } catch { return false; }
}

// --- window ----------------------------------------------------------------

function createWindow() {
  win = new BrowserWindow({
    width: 1100,
    height: 780,
    show: false,
    backgroundColor: '#101418',
    title: 'Xception INSP',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      // All three back at their safe defaults. The old shell ran with
      // contextIsolation off, nodeIntegration on and webSecurity disabled --
      // and used electron.remote, which is why it could never move past
      // Electron 12.
      contextIsolation: true,
      nodeIntegration: false,
      webSecurity: true,
      sandbox: true,
    },
  });
  win.once('ready-to-show', () => win.show());
  win.on('closed', () => { win = null; });

  // The renderer must not be able to navigate anywhere we did not send it, and
  // must never open a second window.
  win.webContents.setWindowOpenHandler(({ url }) => {
    shell.openExternal(url);
    return { action: 'deny' };
  });
}

async function showShell(reason) {
  uiState = 'shell';
  stopRendererGc();
  if (quitting) return;
  if (!win || win.isDestroyed()) createWindow();
  try {
    await win.loadFile(SHELL_INDEX);
  } catch (e) {
    // loadFile rejects with ERR_FAILED (-2) when the window is torn down
    // mid-navigation, which is what happens when the core exits BECAUSE the app
    // is closing: the supervisor's exit handler races the shutdown. Nothing is
    // wrong and there is nothing to show, but an unhandled rejection here would
    // print a stack trace on every clean exit and train everyone to ignore it.
    if (!quitting) console.error('[launcher] could not show the shell:', e.message);
    return;
  }
  if (reason) send('launcher:reason', reason);
}

// The application says where its UI is: a file inside its own version
// directory, or a loopback URL it serves itself. Both are shapes a real
// application has; neither is the launcher's business beyond loading it.
async function showApp(ui) {
  uiState = 'app';
  if (ui.kind === 'url') await win.loadURL(ui.target);
  else await win.loadFile(ui.target);
  win.maximize();
}

// --- renderer garbage collection ---------------------------------------------
//
// See the note on rendererGcIntervalMs in src/config.js for the measurements
// behind this. In short: the renderer accumulates off-heap garbage that V8
// cannot see and therefore never collects, and a single
// HeapProfiler.collectGarbage returns all of it.
//
// Attached only while the application UI is showing, and detached when it is
// not: a debugger session on the launcher's own shell buys nothing, and leaving
// one attached across a navigation is how it ends up attached twice.
let gcTimer = null;
let gcAttached = false;

function stopRendererGc() {
  if (gcTimer) { clearInterval(gcTimer); gcTimer = null; }
  if (gcAttached && win && !win.isDestroyed()) {
    try { win.webContents.debugger.detach(); } catch { /* already gone */ }
  }
  gcAttached = false;
}

function startRendererGc() {
  stopRendererGc();
  const every = Number(cfg.values.rendererGcIntervalMs) || 0;
  if (every <= 0) { shellLog('renderer GC disabled (rendererGcIntervalMs = 0)'); return; }
  try {
    win.webContents.debugger.attach('1.3');
    gcAttached = true;
  } catch (e) {
    // Never fatal: a machine that cannot collect is worse than one that can,
    // but it is still a working machine, and it now says so.
    shellLog(`could not attach the debugger for renderer GC: ${e.message}`);
    return;
  }
  gcTimer = setInterval(async () => {
    if (uiState !== 'app' || !win || win.isDestroyed()) return;
    try { await win.webContents.debugger.sendCommand('HeapProfiler.collectGarbage'); }
    catch (e) { /* a failed collection is not worth a log line every two minutes */ }
  }, every);
  gcTimer.unref?.();
}

// --- start / stop ------------------------------------------------------------

// Work out what would run, without running it. Used by the shell to explain the
// current state, and by startCore below, so the two can never disagree.
async function currentPlan() {
  const target = apps.resolve();
  if (!target) return { error: { kind: 'no-app' } };

  const workingDir = cfg.workingDir;
  if (!workingDir) return { target, error: { kind: 'no-working-dir' } };
  if (!isDir(workingDir)) return { target, error: { kind: 'working-dir-missing', workingDir } };

  let plan;
  try {
    plan = boot.load(target.dir, workingDir, shellLog);
  } catch (e) {
    return { target, error: { kind: 'boot-failed', message: e.message, appDir: target.dir } };
  }

  // What must exist before there is any point starting -- decided by the
  // APPLICATION, either by listing paths or by supplying a checkRequirements
  // hook. The launcher has no opinion about "data/" or about anything else; it
  // asks, and it repeats the answer to the operator verbatim, including the
  // reason, because the operator is the one who has to fix it.
  const services = boot.makeServices(plan, shellLog, () => []);
  const unmet = await boot.callHook(plan, 'checkRequirements', services,
                                    boot.defaults.checkRequirements, shellLog);
  if (Array.isArray(unmet) && unmet.length) {
    return { target, plan, error: { kind: 'unmet-requirements', unmet, workingDir } };
  }

  return { target, plan };
}

// The plan as the shell should see it: paths and numbers, no functions.
function planForDisplay(plan) {
  if (!plan) return null;
  return {
    name: plan.name,
    processes: plan.processes.map((p) => ({
      id: p.id, primary: p.primary, exe: p.exe, args: p.args, cwd: p.cwd,
      env: Object.keys(p.env),
      control: p.control, readyTimeoutMs: p.readyTimeoutMs,
    })),
    ui: plan.ui,
    requires: plan.requires,
    // Which parts of the behaviour this version overrode. Worth showing: if a
    // machine behaves oddly on shutdown, "this version supplies its own
    // requestShutdown" is the first thing to know.
    hooks: Object.keys(plan.hooks),
  };
}

// --- the setup gate ----------------------------------------------------------
//
// The bench machines are touch-only, and once the application's UI takes the
// window there is no way back to the launcher's settings: assertShell refuses
// every one of them. Until now the only ways in were killing the core from Task
// Manager or hand-editing launcher.json, neither of which an operator can be
// asked to do.
//
// So the launcher screen -- which is on the display anyway while the core boots
// -- counts taps. Three of them stop the start and keep the launcher up. It is
// deliberately not a button: a button on the application's UI is something an
// operator can press by accident mid-shift, and this is a thing you have to
// mean.
//
// The window costs no boot time. The core is spawned FIRST and the taps
// overlap with a startup that was happening regardless; splashHoldMs only
// guarantees a floor for the day the core answers quickly.
let setupGate = null;

// --- the good timer ----------------------------------------------------------
//
// A version becomes the last known good only after it has RUN for
// goodAfterMs -- see the note there. The record is written when this fires,
// never at shutdown: a machine that crashes must still be able to accumulate
// one, and a machine that is killed must not be able to claim a stretch it did
// not finish.
let goodTimer = null;

function armGoodTimer(version) {
  clearGoodTimer();
  const after = Number(cfg.values.goodAfterMs) || 0;
  if (!after || !version) return;
  goodTimer = setTimeout(() => {
    goodTimer = null;
    try {
      apps.markGood(version, after / 1000);
      shellLog(`${version} 已連續執行 ${(after / 3600000).toFixed(1)} 小時 -- 記錄為 last known good`);
    } catch (e) {
      shellLog(`could not record ${version} as good: ${e.message}`);
    }
  }, after);
  // Never keep the process alive just to write this.
  if (goodTimer.unref) goodTimer.unref();
}
function clearGoodTimer() {
  if (goodTimer) { clearTimeout(goodTimer); goodTimer = null; }
}

async function startCore() {
  const { target, plan, error } = await currentPlan();
  lastPlanError = error || null;
  if (error) { await showShell(error); return; }

  if (target.fellBackFrom) {
    shellLog(`current.json names "${target.fellBackFrom}", which is missing or incomplete -- `
           + `falling back to ${target.version}`);
  }
  shellLog(`starting ${plan.name || target.version}`);

  setupGate = { armed: true, requested: false, since: Date.now() };
  send('launcher:setupGate', { ms: Number(cfg.values.splashHoldMs) || 0 });

  try {
    supervisor.start(plan);
  } catch (e) {
    setupGate = null;
    await showShell({ kind: 'spawn-failed', error: e.message });
    return;
  }

  // Wait for the application to answer before handing the window over. A UI
  // whose first act is to connect to its backend shows a connection error on
  // every start otherwise, which the operator has to clear by hand.
  //
  // Not answering is NOT treated as a failure: this core retries camera init,
  // which is slow and normal. The health monitor keeps watching either way, so
  // the UI is shown and the truth arrives there.
  const pong = await supervisor.waitUntilReady((m) => shellLog(m));
  if (pong) shellLog(`answered: ${pong.version || ''} ${pong.git || ''}`.trim());
  if (!supervisor.running) return;         // the exit handler owns this case

  // Hold the launcher screen for the rest of the gate, then close it. Taps that
  // land during this are acted on the moment they arrive (see the IPC handler),
  // so nobody is left waiting on a countdown for something they already did.
  const hold = Number(cfg.values.splashHoldMs) || 0;
  const left = hold - (Date.now() - setupGate.since);
  if (left > 0) await new Promise((r) => setTimeout(r, left));
  if (setupGate.requested) return;          // the exit handler owns this case too
  setupGate.armed = false;
  send('launcher:setupGate', null);

  // target.version, not current.json: resolve() may have fallen back to the
  // newest valid version, and that is the one now executing. Passing it stops
  // prune from deleting the running directory out from under the process.
  armGoodTimer(target.version);

  const pruned = apps.prune(cfg.values.keepVersions, target.version);
  if (pruned.removed.length)
    shellLog(`removed old versions: ${pruned.removed.join(', ')} (kept ${pruned.kept})`);
  // Said out loud too: a rollback target surviving past keepVersions is the
  // whole point, and it should not look like prune failed to do its job.
  if (pruned.protected.length)
    shellLog(`kept as rollback targets despite keepVersions: ${pruned.protected.join(', ')}`);
  // Said out loud, every start. A folder in the application root that is not a
  // version means the root is probably pointing somewhere it should not -- at a
  // machine's working directory, most dangerously -- and prune deliberately
  // leaves it alone rather than deleting what it does not understand.
  if (pruned.foreign.length)
    shellLog(`NOT versions, left alone: ${pruned.foreign.join(', ')} `
           + `-- if these are machine data, the application folder is set wrong`);

  // An application without a UI is legitimate -- a headless soak build, say --
  // and the shell is then the right thing to keep showing.
  if (plan.ui) {
    await showApp(plan.ui);
    startRendererGc();
  } else {
    shellLog('this version declares no UI; staying on the launcher screen');
  }
}

function wireSupervisor() {
  supervisor.on('exit', async (info) => {
    lastExit = info;
    clearGoodTimer();
    // `quitting` is set by before-quit, which stops the core deliberately. The
    // exit that follows is the one we asked for, and the window is already
    // going away -- there is nothing to report it to.
    if (quitting || !win || win.isDestroyed()) return;
    // Deliberately no automatic restart. The core is the thing that decides
    // whether a part passes; a supervisor that silently brings it back after an
    // unexplained death can let bad parts through while the line keeps running.
    // An operator decides.
    if (setupGate && setupGate.requested) {
      // Not a crash. Somebody asked for the launcher and we stopped the core to
      // give it to them; saying "the core exited unexpectedly" would be a lie
      // that sends an operator looking for a fault that is not there.
      await showShell({ kind: 'setup-requested' });
      return;
    }
    await showShell({ kind: 'core-exited', ...info });
  });
  supervisor.on('health', (st) => send('launcher:health', st));
  supervisor.on('line', (line) => send('launcher:coreline', line));
}

// --- IPC ---------------------------------------------------------------------

function assertShell(action) {
  if (uiState !== 'shell') throw new Error(`${action} is only available from the launcher screen`);
}
function assertStopped(action) {
  if (supervisor.running) throw new Error(`stop the core before ${action}`);
}

async function pickDirectory(title) {
  const r = await dialog.showOpenDialog(win, { title, properties: ['openDirectory'] });
  return r.canceled ? null : r.filePaths[0];
}

function registerIpc() {
  ipcMain.handle('launcher:status', async () => {
    const { target, plan, error } = await currentPlan();
    return {
      uiState,
      launcherVersion: app.getVersion(),
      electron: process.versions.electron,
      configFile: cfg.file,
      configError: cfg.loadError,
      config: cfg.values,
      appRoot: cfg.appRoot,
      workingDir: cfg.workingDir,
      versions: apps.list(),
      current: apps.currentVersion(),
      // Which version has proved itself, and what the current pointer
      // displaced. Both are rollback targets, and an operator choosing one
      // under pressure should not have to guess which is which.
      lastGood: apps.lastGood(),
      previous: apps.previousVersion(),
      resolved: target || null,
      // The plan is shown in the UI on purpose. "What exactly is this launcher
      // about to run, out of which folder, and which of its behaviours has this
      // version overridden" should be answerable without reading any code --
      // because none of it is in the launcher any more, and moving it somewhere
      // less inspectable would be a poor trade.
      plan: planForDisplay(plan),
      planError: error || null,
      core: supervisor.status(),
      rendererGc: { everyMs: cfg.values.rendererGcIntervalMs, attached: gcAttached },
      lastExit,
      tail: supervisor.tail(200),
      bootRel: boot.BOOT_REL,
    };
  });

  // The one thing the application's UI needs from the launcher, and the entire
  // reason the old design ran a WebSocket server.
  ipcMain.handle('launcher:pickFolder', async (_e, options = {}) => {
    const r = await dialog.showOpenDialog(win, {
      title: options.title || 'Select Directory',
      defaultPath: options.defaultPath || undefined,
      properties: ['openDirectory', 'createDirectory'],
    });
    return r.canceled ? { canceled: true, filePaths: [] }
                      : { canceled: false, filePaths: r.filePaths };
  });

  ipcMain.handle('launcher:startCore', async () => {
    assertShell('starting the core');
    if (supervisor.running) return { ok: false, error: 'already running' };
    lastExit = null;
    await startCore();
    return { ok: true };
  });

  ipcMain.handle('launcher:stopCore', async () => ({ ok: true, ...(await supervisor.stop()) }));

  // Only from the launcher's own screen, and only while the gate is open. Once
  // the application's UI has the window there is no gate to open -- by then the
  // renderer is the application, and this must not be a door it can walk
  // through on its own.
  ipcMain.handle('launcher:requestSetup', async () => {
    if (uiState !== 'shell' || !setupGate || !setupGate.armed) {
      return { ok: false, error: 'the setup gate is not open' };
    }
    setupGate.armed = false;
    setupGate.requested = true;
    send('launcher:setupGate', null);
    shellLog('進入設定模式 -- 停止核心,留在啟動器畫面');
    await supervisor.stop();
    return { ok: true };
  });

  ipcMain.handle('launcher:selectVersion', (_e, version) => {
    assertShell('selecting a version');
    assertStopped('changing version');
    return { ok: true, version: updater.select(version) };
  });

  ipcMain.handle('launcher:chooseAndInstall', async () => {
    assertShell('installing an update');
    assertStopped('installing an update');
    const picked = await dialog.showOpenDialog(win, {
      title: 'Select update package',
      properties: ['openFile'],
      filters: [{ name: 'Update package', extensions: ['zip'] }],
    });
    if (picked.canceled) return { ok: false, canceled: true };
    try {
      const r = await updater.install(picked.filePaths[0], shellLog);
      shellLog(`installed ${r.version}; select it to make it current`);
      return { ok: true, ...r };
    } catch (e) {
      shellLog(`UPDATE FAILED: ${e.message}`);
      return { ok: false, error: e.message };
    }
  });

  ipcMain.handle('launcher:pickAppRoot', async () => {
    assertShell('changing the application folder');
    assertStopped('changing the application folder');
    const chosen = await pickDirectory('Select the folder that holds the installed versions');
    if (!chosen) return { ok: false, canceled: true };
    cfg.set('appRoot', chosen);
    apps = new AppStore(cfg);
    updater = new Updater(cfg, apps);
    return { ok: true, appRoot: cfg.appRoot };
  });

  ipcMain.handle('launcher:pickWorkingDir', async () => {
    assertShell('changing the working directory');
    assertStopped('changing the working directory');
    const chosen = await pickDirectory('Select the folder the application runs IN');
    if (!chosen) return { ok: false, canceled: true };
    cfg.set('workingDir', chosen);
    // Report what the app makes of it, rather than judging it here: whether
    // this directory is the right one is a question only boot.js can answer,
    // and answering it here would put the application's layout back into the
    // launcher through the side door.
    const { error } = await currentPlan();
    return { ok: true, workingDir: cfg.workingDir, planError: error || null };
  });

  ipcMain.handle('launcher:openFolder', (_e, which) => {
    const target = which === 'logs' ? cfg.logDir
                 : which === 'apps' ? cfg.appRoot
                 : which === 'working' ? cfg.workingDir
                 : null;
    if (!target) return { ok: false, error: 'nothing to open' };
    fs.mkdirSync(cfg.logDir, { recursive: true });   // only ever the log dir
    shell.openPath(target);
    return { ok: true, path: target };
  });
}

// --- lifecycle ---------------------------------------------------------------

app.on('second-instance', () => {
  if (win) { if (win.isMinimized()) win.restore(); win.focus(); }
});

app.whenReady().then(async () => {
  cfg = new Config(app.getPath('userData'));
  apps = new AppStore(cfg);
  updater = new Updater(cfg, apps);
  supervisor = new Supervisor(cfg);
  wireSupervisor();
  registerIpc();

  createWindow();
  await showShell(null);
  if (cfg.loadError) shellLog(cfg.loadError);
  await startCore();
});

// Closing the window stops the machine, so it goes through the same graceful
// path as an explicit stop: ask over the control channel the application
// declared, wait, and only force it if it will not go. The old launcher sent
// kill('SIGINT') -- which on Windows is TerminateProcess, not a signal -- and
// then taskkill /f immediately after, so the core's own teardown never ran.
app.on('before-quit', (e) => {
  if (quitting || !supervisor || !supervisor.running) return;
  e.preventDefault();
  quitting = true;
  supervisor.stop().finally(() => app.quit());
});

app.on('window-all-closed', () => { app.quit(); });
