// This file is required by the index.html file and will
// be executed in the renderer process for that window.
// No Node.js APIs are available in this process because
// `nodeIntegration` is turned off. Use `preload.js` to
// selectively enable features needed in the rendering
// process.

const ipc = require('electron').ipcRenderer
try {
  require('electron-reloader')(module)
} catch (_) {}
let buffer = new Uint8Array(8);
buffer[0]=1;
ipc.send('r2m',buffer)
// buffer[0]=2;



ipc.on('m2r', function (event, arg) {
  console.log('m2r',arg)
})
