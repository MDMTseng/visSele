import { app, BrowserWindow } from 'electron';
const WebSocket = require('ws');
const {dialog} = require('electron');

const wss = new WebSocket.Server({ port: 8714 });

wss.on('connection', function connection(ws) {
  ws.on('message', function incoming(message) {
    let p = JSON.parse(message);
    let retData={
      type:"NAK",
      req_id:p.req_id,
    }
    if(p.type=="showOpenDialog")
    {
      console.log('cmd: %s', p);
      dialog.showOpenDialog(p.option).then(function (response) {
        if (!response.canceled) {
          retData.type="ACK";
          retData.filePaths=response.filePaths;
        }
        ws.send(JSON.stringify(retData));
      }).catch((err)=>{
        ws.send(JSON.stringify(retData));
      })
    }
    else
    {
      // console.log('received: %s', message);
      ws.send(JSON.stringify(retData));
    }
  });
});
// Handle creating/removing shortcuts on Windows when installing/uninstalling.
if (require('electron-squirrel-startup')) { // eslint-disable-line global-require
  app.quit();
}

// Keep a global reference of the window object, if you don't, the window will
// be closed automatically when the JavaScript object is garbage collected.
let mainWindow;

const createWindow = () => {
  // Create the browser window.
  mainWindow = new BrowserWindow({
    width: 800,
    height: 600,
    webPreferences: { devTools: true }
    });

  // and load the index.html of the app.
  // mainWindow.loadURL(`file://${__dirname}/index.html`);
  mainWindow.loadURL("http://localhost:8080/");
  // Open the DevTools.
  mainWindow.webContents.openDevTools();

  // Emitted when the window is closed.
  mainWindow.on('closed', () => {
    // Dereference the window object, usually you would store windows
    // in an array if your app supports multi windows, this is the time
    // when you should delete the corresponding element.
    mainWindow = null;
  });
};

// This method will be called when Electron has finished
// initialization and is ready to create browser windows.
// Some APIs can only be used after this event occurs.
app.on('ready', createWindow);

// Quit when all windows are closed.
app.on('window-all-closed', () => {
  // On OS X it is common for applications and their menu bar
  // to stay active until the user quits explicitly with Cmd + Q
  if (true||process.platform !== 'darwin') {
    app.quit();
  }
});

app.on('activate', () => {
  // On OS X it's common to re-create a window in the app when the
  // dock icon is clicked and there are no other windows open.
  if (mainWindow === null) {
    createWindow();
  }
});

// In this file you can include the rest of your app's specific main process
// code. You can also put them in separate files and import them here.
