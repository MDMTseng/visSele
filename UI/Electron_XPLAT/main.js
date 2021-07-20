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


{

  const gotTheLock = electron.app.requestSingleInstanceLock();
  if (!gotTheLock) {
    app.quit()
    return;
  }


  
  let APP_INFO_INFO ={//default
    "APPCorePath":"./res/Core/",
    "APPWebUIPath":"./res/WebUI/"
  };
  let APP_INFO_FILE_PATH;
  {



    var appFolderBack="/";
    if(process.platform === "win32")
    {

    }
    else //if(process.platform === "darwin  ")
    {
      // appFolderBack="/../../../../"
    }

    APP_INFO_FILE_PATH=app.getAppPath()+appFolderBack+'app_setup_info.json';
    
    
    while(true){
      
      let rawdata = fs.readFileSync(APP_INFO_FILE_PATH);
      console.log("APP_INFO_FILE_PATH",APP_INFO_FILE_PATH);
      APP_INFO_INFO = JSON.parse(rawdata);
      if(APP_INFO_INFO.APP_INFO_FILE_PATH!==undefined)
      {
        APP_INFO_FILE_PATH=APP_INFO_INFO.APP_INFO_FILE_PATH;
        console.log("APP_INFO_FILE_PATH",APP_INFO_FILE_PATH);
        continue;
      }
      APP_INFO_INFO=APP_INFO_INFO;
      break;
    }

  }
  console.log("APP_INFO_INFO",APP_INFO_INFO);
  
  const tar_launcher = require(APP_INFO_INFO.APPContentPath+"/scripts/launcher.js")

  console.log("tar_launcher",tar_launcher);

  tar_launcher.setup({electron,APP_INFO_FILE_PATH,
    WebSocket:require('ws'),
    express:require('express'),
    mongoose :require('mongoose')});
}



// // Quit when all windows are closed, except on macOS. There, it's common
// // for applications and their menu bar to stay active until the user quits
// // explicitly with Cmd + Q.
// app.on('window-all-closed', function () {
//   if (process.platform !== 'darwin') app.quit()
// })

// // In this file you can include the rest of your app's specific main process
// // code. You can also put them in separate files and require them here.
