// Modules to control application life and create native browser window
const { app, BrowserWindow, dialog } = require('electron')
const ipc = require('electron').ipcMain
const path = require('path')
const {spawn, execFile,exec } = require('child_process');

const WebSocket = require('ws');
let mainWindow = undefined;
let preTime = Date.now();
ipc.on('r2m', function (event, arg) {
  // console.log("r2m>",arg)
  // event.sender.send('m2r',arg)
  let curTime = Date.now();

  console.log("time_ms:", curTime - preTime)
  // mainWindow.webContents.send('m2r',arg);

  preTime = curTime;
})



console.log("getAppPath:",app.getAppPath());

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


function createWindow() {
  const gotTheLock = app.requestSingleInstanceLock();
  if (!gotTheLock) {
    app.quit()
    return;
  }
  // Create the browser window.
  mainWindow = new BrowserWindow({
    width: 800,
    height: 600,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      nodeIntegration: true,
      contextIsolation: false,
    }
  })

  // and load the index.html of the app.
  // mainWindow.loadFile("index.html")
  mainWindow.loadFile("./res/WebUI/index.html")
  // mainWindow.loadURL("http://localhost:8080/")
  // Open the DevTools.
  mainWindow.webContents.openDevTools()


  // Emitted when the window is closed.
  mainWindow.on('closed', () => {
    app.quit();
  });
  // ipc_bridge_connection();

  // execFile('res/Core/visSele', [], (error, stdout, stderr) => {
  //   if (error) {
  //       console.error(`error: ${error}`);
  //       return;
  //   }
  //   console.log(`stdout: ${stdout}`);
  //   console.error(`stderr: ${stderr}`);
  // });
  // execFile('visSele', [], { cwd: 'res/Core/' });

  if(false)
  {
    const spawnX = exec('res/Core/visSele', function (error, stdout, stderr) {
      if (error) {
        console.log(error.stack);
        console.log('Error code: '+error.code);
        console.log('Signal received: '+error.signal);
      }
      console.log('Child Process STDOUT: '+stdout);
      console.log('Child Process STDERR: '+stderr);
    });
    spawnX.stdout.pipe(process.stdout);
    
    spawnX.on('exit', function (code) {
      console.log('Child process exited with exit code '+code);
    });

  }
  else
  {
    var spawnX;
    if(process.platform === "win32")
    {
      spawnX = spawn('./visSele.exe', [],{
        cwd: "./res/Core/win32/",
        env: process.env,
        stdio: 'inherit'});   
    }
    else //if(process.platform === "darwin  ")
    {
      spawnX = spawn('./visSele', ["chdir=/Users/mdm/visSele/InspectionCore/Core0_1"],{
        cwd: "/Users/mdm/visSele/InspectionCore/Core0_1",
        env: process.env,
        stdio: 'inherit'});   
    }


    // spawnX.stdout.pipe(process.stdout);
    // spawnX.stdout.setEncoding('utf8');
    // spawnX.stdout.on('data', function (data) {
    //   console.log('stdout: ' + data.toString());
    // });

    // spawnX.stderr.on('data', function (data) {
    //   console.log('stderr: ' + data.toString());
    // });

    spawnX.on('exit', function (code) {
      console.log('child process exited with code ' + code.toString());
    });
  }
   
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



const wss = new WebSocket.Server({ port: 9714 });

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
