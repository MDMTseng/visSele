const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const url = require('url');

// Keep a global reference of the window object to prevent garbage collection
let mainWindow;

// Create the browser window
function createWindow() {
  mainWindow = new BrowserWindow({
    width: 800,
    height: 600,
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false,
      preload: path.join(__dirname, 'preload.js')
    }
  });

  // Load the index.html file
  mainWindow.loadURL(url.format({
    pathname: path.join(__dirname, 'index.html'),
    protocol: 'file:',
    slashes: true
  }));

  // Open DevTools in development mode
  if (process.env.NODE_ENV === 'development') {
    mainWindow.webContents.openDevTools();
  }

  // Handle window being closed
  mainWindow.on('closed', () => {
    mainWindow = null;
  });
}

// Create window when Electron has finished initialization
app.on('ready', () => {
  createWindow();
  
  // Initialize the shared memory receiver
  try {
    const sharedMemory = require('./build/Release/shared_memory');
    
    // Start the receiver in a separate thread
    sharedMemory.startReceiver();
    
    // Set up a callback to receive messages from the native addon
    sharedMemory.setMessageCallback((message) => {
      if (mainWindow && !mainWindow.isDestroyed()) {
        mainWindow.webContents.send('shared-memory-message', message);
      }
    });
    
    // Clean up when the app is about to quit
    app.on('will-quit', () => {
      sharedMemory.stopReceiver();
    });
    
  } catch (error) {
    console.error('Failed to initialize shared memory:', error);
  }
});

// Quit when all windows are closed (except on macOS)
app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

// Re-create window on macOS when dock icon is clicked
app.on('activate', () => {
  if (mainWindow === null) {
    createWindow();
  }
});

// Handle IPC messages from the renderer process
ipcMain.on('clear-messages', () => {
  if (mainWindow) {
    mainWindow.webContents.send('clear-messages');
  }
}); 