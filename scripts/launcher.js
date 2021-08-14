
const { spawn, execFile, exec } = require('child_process');
const electron = require('electron')
// var electron;
const fs = require('fs');
const path = require('path')
// const fetch = require('fetch')

let mongoose;

exports.update={
  // currentVersion:()=>{"0.2.8"},
  getAppInfo:()=>({
    "version":"0.2.8"
  }),
  getRemoteVersionList:(url="https://api.github.com/repos/MDMTseng/visSele/releases/latest")=>{
    fetch(url)
    .then(res =>res.json())
    .then(res =>{ 
        console.log(res)
      
      
    })
    .catch(err => console.log(err));

  }


}



// = function (url)
// {
//   // fs.mkdirSync(dir);
//   console.log("<><><><");
// }


exports.setup = function (pak) {
  console.log(pak);
  let { WebSocket, mongoose, express } = pak;
  // // This method will be called when Electron has finished
  // // initialization and is ready to create browser windows.
  // // Some APIs can only be used after this event occurs.
  init(pak);

  function LinkMongoDB(pak) {

  }



  function initWSTunnel() {

    WSTunnel = new WebSocket.Server({ port: 9714 });

    WSTunnel.on('connection', function connection(ws) {
      ws.on('message', function incoming(message) {
        let p = JSON.parse(message);
        let retData = {
          type: "NAK",
          req_id: p.req_id,
        }
        if (p.type == "showOpenDialog") {
          // console.log('cmd: %s', p);
          electron.dialog.showOpenDialog(p.option).then(function (response) {
            if (!response.canceled) {
              retData.type = "ACK";
              retData.filePaths = response.filePaths;
            }
            ws.send(JSON.stringify(retData));
          }).catch((err) => {
            ws.send(JSON.stringify(retData));
          })
        }
        else {
          // console.log('received: %s', message);
          ws.send(JSON.stringify(retData));
        }
      });
    });

  }

  function init(pak) {
    let { APP_INFO_FILE_PATH,mainWindow } = pak;
    // {electron,APP_INFO_FILE_PATH,WebSocket}

    let APP_INFO={};
    try {
      rawdata = fs.readFileSync(APP_INFO_FILE_PATH);
      APP_INFO=JSON.parse(rawdata);
    } catch (err) {

    }

    initWSTunnel();

    // mainWindow.loadFile(APP_INFO.APPContentPath + "/WebUI/index.html")
    mainWindow.loadURL("http://0.0.0.0:8080/")
    // mainWindow.webContents.openDevTools()


    // Emitted when the window is closed.


    var spawnX=undefined;
    if (process.platform === "win32") {
      // spawnX = spawn('./visSele.exe', [],{
      //   cwd: "./res/Core/win32/",
      //   env: process.env,
      //   stdio: 'inherit'});   
    }
    else //if(process.platform === "darwin  ")
    {
      spawnX = spawn('./visSele', ["chdir=" + APP_INFO.APPDataDirPath], {
        cwd: APP_INFO.APPContentPath + "/Core",
        env: process.env,
        stdio: 'inherit'
      });
    }

    mainWindow.on('closed', () => {
      if(spawnX!==undefined)
      {
        spawnX.kill('SIGINT');
        spawnX=undefined;
      }
      else
      {

      }
      electron.app.quit();
    });

    // spawnX.stdout.pipe(process.stdout);
    // spawnX.stdout.setEncoding('utf8');
    // spawnX.stdout.on('data', function (data) {
    //   console.log('stdout: ' + data.toString());
    // });

    // spawnX.stderr.on('data', function (data) {
    //   console.log('stderr: ' + data.toString());
    // });

    spawnX.on('exit', function (code) {
      console.log('child process exited with code ', code);
    });
  }


}

