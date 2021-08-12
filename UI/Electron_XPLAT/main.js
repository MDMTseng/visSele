// Modules to control application life and create native browser window
const electron = require('electron')

const { app } = electron

const ipc = require('electron').ipcMain
const path = require('path')
const child_process = require('child_process');

const fs = require('fs');


let preTime = Date.now();
ipc.on('r2m', function (event, arg) {
  // console.log("r2m>",arg)
  // event.sender.send('m2r',arg)
  let curTime = Date.now();

  console.log("time_ms:", curTime - preTime)
  // mainWindow.webContents.send('m2r',arg);

  preTime = curTime;
})



console.log("getAppPath:",electron.app.getAppPath());

// if(true){
//   let buffer = new Uint8Array(4*500*10000/(Math.pow(2,2)));
//   buffer[0]=1;


//   setTimeout(()=>{
//     for(let i=0;i<100;i++)
//     {
//       mainWindow.webContents.send('m2r',buffer);
//     }
//   },3000);

// }

function ipc_bridge_connection()
{
  const addon = require('bindings')('IPC_BRIDGE')
  const EventEmitter = require('events').EventEmitter
  const emitter = new EventEmitter()
  emitter.on('start', () => {
    console.log('### START ...')
  })

  let preTime = Date.now();
  let buffer_RECV = undefined;
  let buffer_SEND = undefined;


  emitter.on('buffer_RECV', (evt) => {
    buffer_RECV = evt;
  })

  emitter.on('buffer_SEND', (evt) => {
    buffer_SEND = evt;
  })


  emitter.on('data', (evt) => {
    let curTime = Date.now();
    mainWindow.webContents.send('m2r',buffer_RECV);
    // console.log(curTime - preTime, buffer_RECV);
    // let i=0;
    // for(i=0;i<1000;i++)
    // {
    //   console.log(buffer_RECV[i]);
    // }
    preTime = curTime;
  })

  emitter.on('end', () => {
    console.log('### END ###')
  })

  addon.callEmit(emitter.emit.bind(emitter))

}

const folderSelDialog = async (mainWindow) => {
  try {
   const chosenFolders = await electron.dialog.showOpenDialog(mainWindow, { properties: ['openDirectory'] });
   if (chosenFolders && chosenFolders.canceled === false) {

    return chosenFolders.filePaths;
   }
  } catch (err) {
    throw new Error(err);
  }

  return undefined;
 }



electron.app.whenReady().then(() => {
  setup()

  electron.app.on('activate', function () {
    // On macOS it's common to re-create a window in the app when the
    // dock icon is clicked and there are no other windows open.
    if (BrowserWindow.getAllWindows().length === 0) setup()
  })
})


function setup()
{


  let mainWindow = new electron.BrowserWindow({
    width: 800,
    height: 600,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      nodeIntegration: true,
      contextIsolation: false,
      enableRemoteModule: true,
      webSecurity:false
    }
  })

  ipc.on('RUN_APP1', (event, arg) => {
    // console.log(event,arg)

    // arg

    // APPDataDirPath: '/Users/mdm/visSele/export/scripts',
    // APPContentPath: '/Users/mdm/visSele/export/v01'
    
    let rootAppInfo=arg.rootAppInfo;
    let workspaceAppInfo=arg.workspaceAppInfo;
    const tar_launcher = require(workspaceAppInfo.APPContentPath+"/scripts/launcher.js")
    // console.log(tar_launcher,APP_INFO_FILE_PATH.APPContentPath);
    tar_launcher.setup({
      electron,
      APP_INFO_FILE_PATH:rootAppInfo.APP_INFO_FILE_PATH,
      mainWindow,
      WebSocket:require('ws'),
      express:require('express'),
      mongoose :require('mongoose')});


  })

  mainWindow.loadFile("index.html")
  mainWindow.webContents.openDevTools();
  const gotTheLock = electron.app.requestSingleInstanceLock();
  if (!gotTheLock) {
    app.quit()
    return;
  }


  let APP_INFO_FILE_NAME="app_setup_info.json";
  
  let APP_INFO_INFO ={//default
    "APPCorePath":"./res/Core/",
    "APPWebUIPath":"./res/WebUI/"
  };


}



// // Quit when all windows are closed, except on macOS. There, it's common
// // for applications and their menu bar to stay active until the user quits
// // explicitly with Cmd + Q.
// app.on('window-all-closed', function () {
//   if (process.platform !== 'darwin') app.quit()
// })

// // In this file you can include the rest of your app's specific main process
// // code. You can also put them in separate files and require them here.
