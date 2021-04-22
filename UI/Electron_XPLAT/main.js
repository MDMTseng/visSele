// Modules to control application life and create native browser window
const {app, BrowserWindow,dialog} = require('electron')
const path = require('path')
const WebSocket = require('ws');


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




function createWindow () {
  // Create the browser window.
  const mainWindow = new BrowserWindow({
    width: 800,
    height: 600,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js')
    }
  })

  // and load the index.html of the app.
  // mainWindow.loadFile("http://localhost:8080/")
  mainWindow.loadURL("http://localhost:8080/")
  // Open the DevTools.
  mainWindow.webContents.openDevTools()

  
  // Emitted when the window is closed.
  mainWindow.on('closed', () => {
    // Dereference the window object, usually you would store windows
    // in an array if your app supports multi windows, this is the time
    // when you should delete the corresponding element.
    mainWindow = null;
  });
}

// This method will be called when Electron has finished
// initialization and is ready to create browser windows.
// Some APIs can only be used after this event occurs.
app.whenReady().then(() => {
  createWindow()
  
  app.on('activate', function () {
    // On macOS it's common to re-create a window in the app when the
    // dock icon is clicked and there are no other windows open.
    if (BrowserWindow.getAllWindows().length === 0) createWindow()
  })
})

// Quit when all windows are closed, except on macOS. There, it's common
// for applications and their menu bar to stay active until the user quits
// explicitly with Cmd + Q.
app.on('window-all-closed', function () {
  if (process.platform !== 'darwin') app.quit()
})

// In this file you can include the rest of your app's specific main process
// code. You can also put them in separate files and require them here.
