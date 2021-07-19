
const {spawn, execFile,exec } = require('child_process');
var electron;
const fs = require('fs');
const path = require('path')
exports.SimpleMessage = 'Hello world';
let WebSocket;
let WSTunnel;

let mainWindow = undefined;

function initWSTunnel()
{

  WSTunnel = new WebSocket.Server({ port: 9714 });

  WSTunnel.on('connection', function connection(ws) {
    ws.on('message', function incoming(message) {
      let p = JSON.parse(message);
      let retData={
        type:"NAK",
        req_id:p.req_id,
      }
      if(p.type=="showOpenDialog")
      {
        console.log('cmd: %s', p);
        electron.dialog.showOpenDialog(p.option).then(function (response) {
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
  
}

exports.setup=function(pak)
{
  let {APP_INFO_FILE_PATH}=pak;
  // {electron,APP_INFO_FILE_PATH,WebSocket}
  WebSocket=pak.WebSocket;
  electron=pak.electron;


  initWSTunnel();
  let rawdata = fs.readFileSync(APP_INFO_FILE_PATH);
  console.log("APP_INFO_FILE_PATH",APP_INFO_FILE_PATH);
  let APP_INFO_INFO = JSON.parse(rawdata);
  

  mainWindow = new electron.BrowserWindow({
    width: 800,
    height: 600,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      nodeIntegration: true,
      contextIsolation: false,
    }
  })
  mainWindow.loadFile(APP_INFO_INFO.APPContentPath+"/WebUI/index.html")
  
  // mainWindow.webContents.openDevTools()


  // Emitted when the window is closed.
  mainWindow.on('closed', () => {
    electron.app.quit();
  });


  var spawnX;
  if(process.platform === "win32")
  {
    // spawnX = spawn('./visSele.exe', [],{
    //   cwd: "./res/Core/win32/",
    //   env: process.env,
    //   stdio: 'inherit'});   
  }
  else //if(process.platform === "darwin  ")
  {
    spawnX = spawn('./visSele', ["chdir="+APP_INFO_INFO.APPDataDirPath],{
      cwd: APP_INFO_INFO.APPContentPath+"/Core",
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