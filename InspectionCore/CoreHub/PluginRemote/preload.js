// Preload script runs in the renderer process but has access to Node.js APIs
const { ipcRenderer } = require('electron');

// Expose protected methods that allow the renderer process to use
// the ipcRenderer without exposing the entire object
window.ipcRenderer = ipcRenderer; 