//launcher is in mainjs thread
const { spawn, execFile, exec } = require('child_process');
const electron = require('electron')
// var electron;
const fs = require('fs');
const path = require('path')
const https = require('https');

let core_require=undefined;
let unzipper;
// const fetch = require('fetch')
let mongoose;



const download = (request,url, path, callback) => {
  request.head(url, (err, res, body) => {
    request(url)
      .pipe(fs.createWriteStream(path))
      .on('close', callback)
  })
}


exports.update={
  // currentVersion:()=>{"0.2.8"},
  getAppInfo:()=>({
    "version":"0.2.8"
  }),
  fetchRemoteVersionList:(url="https://api.github.com/repos/MDMTseng/visSele/releases/latest")=>{
    return fetch(url)
    .then(res =>res.json())
    .then(res =>{ 

      let updatePackAssets = res.assets;
      console.log(res.assets)
      switch(process.platform)
      {
        case "win_32":
        {
          updatePackAssets=updatePackAssets.filter(asse=>asse.name.includes("win"));
          break;
        }
        case "darwin":
        {
          updatePackAssets=updatePackAssets.filter(asse=>asse.name.includes("mac"));
          break;
        }
      }
      updatePackAssets=updatePackAssets.map(asse=>{
        let version=asse.browser_download_url;

        let rule = /download\/(.+)\//g;

        version= rule.exec(version);
        version=(version[1]!==undefined)?version[1]:version.input;
        return {...asse,version,url:asse.browser_download_url}
      })

      return updatePackAssets
      
    })
  },
  updateWithRemoteInfo:(remoteInfo,APP_INFO_FILE_PATH,updateInfo_cb=_=>_,TargetName_prefix="Xception-")=>{

    let unzipper=core_require("unzipper");
    let request=core_require("request");
    let TargetName=TargetName_prefix+remoteInfo.version
    var APP_INFO_FILE_Folder = path.dirname(APP_INFO_FILE_PATH);
    console.log(remoteInfo,APP_INFO_FILE_Folder,APP_INFO_FILE_PATH);
    
    let TargetName_zip=TargetName+".zip";


    let tmpFolder = APP_INFO_FILE_Folder+"/TMP";
    
    updateInfo_cb('Create TMP folder')
    if (!fs.existsSync(tmpFolder)) {
      fs.mkdirSync(tmpFolder)
    }


    return new Promise((resolve, reject)=>{ 

      updateInfo_cb('File downloading from:'+remoteInfo.url)
      download(request,remoteInfo.url, tmpFolder+"/"+TargetName_zip, () => {
        updateInfo_cb('File downloaded!')
        resolve({...remoteInfo,TargetZipPath:tmpFolder+"/"+TargetName_zip,TargetName_zip});
      })

    }).then(info=>new Promise((resolve, reject)=>{
      // 
      const file_r = fs.createReadStream(info.TargetZipPath)
      .pipe(unzipper.Extract({ path: tmpFolder+"/"+TargetName}))
      .on('entry', entry => entry.autodrain())
      .promise()
      .then( () =>{

        let postFix="";
        let counter=0;
        while(fs.existsSync(APP_INFO_FILE_Folder+"/"+TargetName+postFix))
        {
          postFix=`[${counter}]`;
          counter++;
        }
        
        updateInfo_cb(`Find Avalible name:${TargetName+postFix}`)
        let dstFolderName = APP_INFO_FILE_Folder+"/"+TargetName+postFix;

        let srcFolderName = tmpFolder+"/"+TargetName;
        {

          let folderStruct = fs.readdirSync(srcFolderName);
          console.log(folderStruct);
          if(folderStruct.length>0)
          {
            srcFolderName+="/"+folderStruct[0];
          }
          
        }
        
        updateInfo_cb(`Move file ${srcFolderName}  => ${dstFolderName}`)
        fs.renameSync(srcFolderName, dstFolderName);
        
        updateInfo_cb('DELETE TMP folder')
        fs.rmdirSync(tmpFolder, { recursive: true });
        updateInfo_cb("old ver update handling done...:",dstFolderName);
        updateInfo_cb("new ver update handling GO:");


        
        const new_launcher = require(dstFolderName+"/scripts/launcher.js")
        console.log(new_launcher);
        new_launcher.set_core_require_function(core_require);
        new_launcher.update.postUpdateWithRemoteInfo(remoteInfo,APP_INFO_FILE_PATH,updateInfo_cb)
        .then(resolve)
        .catch(reject)
      }, e => reject);


      //step2: unzip
      
    }))



  },
  postUpdateWithRemoteInfo:(remoteInfo,APP_INFO_FILE_PATH,updateInfo_cb=_=>_)=>{

    updateInfo_cb("In new ver. Do post update process....");
    const fs = core_require('fs');

    let rawdata = fs.readFileSync(APP_INFO_FILE_PATH);
    let APPINFO = JSON.parse(rawdata);

    return new Promise((resolve, reject)=>{
      updateInfo_cb("In new ver. post update process DONE");
      resolve("new software!! is here");
    });
  }
  


}



// = function (url)
// {
//   // fs.mkdirSync(dir);
//   console.log("<><><><");
// }
exports.set_core_require_function= function (core_require_fn) {
  core_require = core_require_fn;
}

exports.setup = function (pak) {
  // console.log(pak);
  
  console.log(core_require);
  let { WebSocket, mongoose, express } = pak;
  unzipper=pak.unzipper;
  
  console.log(pak);
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
    console.log("Launcher INIT!!!");
    let { APP_INFO_FILE_PATH,mainWindow } = pak;
    // {electron,APP_INFO_FILE_PATH,WebSocket}
    let APP_INFO={};
    try {
      rawdata = fs.readFileSync(APP_INFO_FILE_PATH);
      APP_INFO=JSON.parse(rawdata);
    } catch (err) {

    }

    initWSTunnel();

    mainWindow.loadFile(APP_INFO.APPContentPath + "/WebUI/index.html")
    // mainWindow.loadURL("http://localhost:8080/")
    // mainWindow.webContents.openDevTools()


    // Emitted when the window is closed.


    var spawnX=undefined;
    if (process.platform === "win32") {
      spawnX = spawn('./visSele', ["chdir=" + APP_INFO.APPDataDirPath], {
        cwd: APP_INFO.APPContentPath + "/Core",
        env: process.env,
        // stdio: 'inherit'
        stdio: [
          'inherit', // StdIn.
          'pipe',    // StdOut.
          'pipe',    // StdErr.
        ]
      }); 
    }
    else //if(process.platform === "darwin  ")
    {
      spawnX = spawn('./visSele', ["chdir=" + APP_INFO.APPDataDirPath], {
        cwd: APP_INFO.APPContentPath + "/Core",
        env: process.env,
        stdio: 'inherit',
        shell: true
      });
    }

    mainWindow.on('closed', () => {
      console.log("MAIN WINDOW LOSED");
      if(spawnX!==undefined)
      {
        spawnX.kill('SIGINT');
        spawn("taskkill", ["/pid", spawnX.pid, '/f', '/t']);
        // spawnX.kill('SIGUP');
        spawnX=undefined;
      }
      else
      {

      }
      electron.app.quit();
    });


    if(spawnX.stdout!=null)
    {
      spawnX.stdout.on('data', function (data) {
        console.log('stdout: ' + data.toString());
      });
    }
    // console.log(spawnX)
    // spawnX.stdout.pipe(process.stdout);
    // spawnX.stdout.setEncoding('utf8');


    // spawnX.stderr.on('data', function (data) {
    //   console.log('stderr: ' + data.toString());
    // });

    spawnX.on('exit', function (code) {
      console.log('child process exited with code ', code);
    });
  }


}

