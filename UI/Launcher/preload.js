// The entire surface the renderer gets. Everything else -- fs, child_process,
// net, the whole of Node -- stays in the main process.
//
// Both pages load with this preload: the launcher's own shell, and the
// application's UI. They do not get the same powers, but that is enforced in
// the MAIN process (assertShell / assertStopped in main.js), not here: a check
// in preload runs in the renderer's world and is only as trustworthy as the
// renderer. Here the list is flat and the main side decides what is allowed in
// the current state.
'use strict';

const { contextBridge, ipcRenderer } = require('electron');

const on = (channel, fn) => {
  const wrapped = (_event, data) => fn(data);
  ipcRenderer.on(channel, wrapped);
  return () => ipcRenderer.removeListener(channel, wrapped);
};

contextBridge.exposeInMainWorld('launcher', {
  // Available everywhere -----------------------------------------------
  //
  // The application's UI uses pickFolder for its own directory buttons. Guard
  // the call site with `window.launcher?.pickFolder` -- in a plain browser (a
  // dev server) there is no preload and the property is absent, which is
  // exactly the condition that should leave such a button disabled.
  pickFolder: (options) => ipcRenderer.invoke('launcher:pickFolder', options),
  status: () => ipcRenderer.invoke('launcher:status'),
  stopCore: () => ipcRenderer.invoke('launcher:stopCore'),
  // Update, from the application's UI. Installing writes only a new version
  // directory and selecting decides the NEXT start, so neither disturbs the run
  // in progress -- see the note on the handlers.
  updateCheck: () => ipcRenderer.invoke('launcher:updateCheck'),
  updateApply: (file) => ipcRenderer.invoke('launcher:updateApply', file),

  // Launcher shell only; refused by the main process while the app UI is up.
  startCore: () => ipcRenderer.invoke('launcher:startCore'),
  // The three-tap gate on the splash. Shell only, and only while the gate is
  // open -- the main process checks both.
  requestSetup: () => ipcRenderer.invoke('launcher:requestSetup'),
  selectVersion: (v) => ipcRenderer.invoke('launcher:selectVersion', v),
  chooseAndInstall: () => ipcRenderer.invoke('launcher:chooseAndInstall'),
  pickAppRoot: () => ipcRenderer.invoke('launcher:pickAppRoot'),
  pickWorkingDir: () => ipcRenderer.invoke('launcher:pickWorkingDir'),
  pickUpdateSource: () => ipcRenderer.invoke('launcher:pickUpdateSource'),
  installFromSource: (file) => ipcRenderer.invoke('launcher:installFromSource', file),
  openFolder: (which) => ipcRenderer.invoke('launcher:openFolder', which),

  // Events
  onLog: (fn) => on('launcher:log', fn),
  onCoreLine: (fn) => on('launcher:coreline', fn),
  onHealth: (fn) => on('launcher:health', fn),
  onReason: (fn) => on('launcher:reason', fn),
  onSetupGate: (fn) => on('launcher:setupGate', fn),
});
