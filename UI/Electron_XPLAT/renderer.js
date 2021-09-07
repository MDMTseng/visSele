// This file is required by the index.html file and will
// be executed in the renderer process for that window.
// No Node.js APIs are available in this process because
// `nodeIntegration` is turned off. Use `preload.js` to
// selectively enable features needed in the rendering
// process.
const electron = require('electron')
const {dialog} = electron.remote;

const fs = require('fs');
const ipc = electron.ipcRenderer
try {
  require('electron-reloader')(module)
} catch (_) {}




function setUIInfo(root_APP_INFO,sub_APP_INFO)
{

  let isNullSetup=root_APP_INFO===undefined||root_APP_INFO.APP_INFO_FILE_PATH===undefined;
  var RootInfoPath = document.getElementById('RootInfoPath');
  if(!isNullSetup)
    RootInfoPath.innerHTML=root_APP_INFO.APP_INFO_FILE_PATH;

  let APP_RouterTxt=undefined;
  let Data_RouterTxt=undefined;
  if(sub_APP_INFO!==undefined)
  {
    APP_RouterTxt = sub_APP_INFO.APPContentPath;
    Data_RouterTxt = sub_APP_INFO.APPDataDirPath;
  }
  var APP_Router = document.getElementById('APP_Router');
  APP_Router.disabled=isNullSetup;
  APP_Router.innerHTML=APP_RouterTxt;
  var Data_Router = document.getElementById('Data_Router');
  Data_Router.disabled=isNullSetup;
  Data_Router.innerHTML=Data_RouterTxt;


  
  let isComp=checkAPPInfo_isComplete(root_APP_INFO,sub_APP_INFO);
  var btn_RUN = document.getElementById('btn_RUN');
  btn_RUN.disabled=!isComp;

  if(isComp)
  {
    console.log(APP_RouterTxt)
    const tar_launcher = require(APP_RouterTxt+"/scripts/launcher.js")
    tar_launcher.set_core_require_function(require);
    // console.log(tar_launcher,APP_INFO_FILE_PATH.APPContentPath);
    console.log(tar_launcher.update.getAppInfo());


    tar_launcher.update.fetchRemoteVersionList()
    .then(list=>{

      console.log(list);

      let logText="";
      var update_log = document.getElementById('update_log');
      
      var update_select = document.getElementById('update_select');
      list.forEach(ele=>{
        
        var btn = document.createElement("button");

        btn.onclick = function () {
          console.log(ele.url);
          tar_launcher.update.updateWithRemoteInfo(ele,root_APP_INFO.APP_INFO_FILE_PATH,
            (log)=>{
              logText+=log+"\n";
              update_log.innerText=logText;
            })
          .then(result=>{


            console.log("OK:"+result)
          }).catch(e=>{
            console.log("ER:"+e)
          });


          //TODO: disable all other UI for update
        };


        btn.appendChild(document.createTextNode(ele.version));
        update_select.innerHTML="";
        update_select.appendChild(btn);
      });
    

    })
    .catch(e=>{
      console.log(e)
    })
    
    
  }

  if(true)
  {
    
    var system_info = document.getElementById('system_info');
    try {
      let rawdata = fs.readFileSync(APP_RouterTxt+"/scripts/info.json");
      info=JSON.parse(rawdata);
      system_info.innerText=JSON.stringify(info);
      return true;
    } catch (err) {

      system_info.innerText="NO Info...";
    }

    return false;


  }

  

}
function checkAPPINFO(rootAppInfo)
{
  let workspaceAppInfo=undefined;

  {//check app info

    try {
      let APP_INFO_FILE_PATH=rootAppInfo["APP_INFO_FILE_PATH"];
      rawdata = fs.readFileSync(APP_INFO_FILE_PATH);
      workspaceAppInfo=JSON.parse(rawdata);
    } catch (err) {

    }
  }
  return {rootAppInfo,workspaceAppInfo};
}

function checkAPP_PATH_INFO(rootInfoPath=electron.remote.app.getAppPath(),rootInfoName="app_setup_info.json")
{
  let rootAppInfo=undefined;
  let workspaceAppInfo=undefined;
  let isAPPInfoComplete=true;
  {//check app info

    try {
      let rawdata = fs.readFileSync(rootInfoPath+"/"+rootInfoName);
      rootAppInfo = JSON.parse(rawdata);
      return checkAPPINFO(rootAppInfo);
    } catch (err) {

    }


    // console.log(rootAppInfo,workspaceAppInfo);
    // isAPPInfoComplete=false;
  }
  return {rootAppInfo,workspaceAppInfo};
  // 

}

function checkAPPInfo_isComplete(rootAppInfo,workspaceAppInfo)
{
  try {

    if(rootAppInfo===undefined)return false;
    if(workspaceAppInfo===undefined)return false;
    if(workspaceAppInfo.APPDataDirPath===undefined)return false;
    if(workspaceAppInfo.APPContentPath===undefined)return false;

    let rawdata = fs.readFileSync(workspaceAppInfo.APPContentPath+"/scripts/info.json");

    let info=JSON.parse(rawdata);
    if(info.version===undefined)
    {
      return false;
    }
    return true;
    
  } catch (err) {

  }
  return false;

}






document.addEventListener('DOMContentLoaded', function() {
  let rootInfoPath=electron.remote.app.getAppPath();
  let rootInfoName="app_setup_info.json";

  let rootAppInfo=undefined;
  let workspaceAppInfo=undefined;

  let isDataComplete=false;
  
  function refreshInfo()
  {
    let newInfo= checkAPP_PATH_INFO(rootInfoPath,rootInfoName);
    rootAppInfo=newInfo.rootAppInfo;
    workspaceAppInfo=newInfo.workspaceAppInfo;
    setUIInfo(rootAppInfo,workspaceAppInfo);
    let isComp=checkAPPInfo_isComplete(rootAppInfo,workspaceAppInfo);
    isDataComplete=isComp;
    console.log("isComp:",isComp);
  }
  refreshInfo();
  function RUN_APP()
  {
    ipc.send('RUN_APP1', {
      // ...workspaceAppInfo,
      rootAppInfo:{...rootAppInfo},
      workspaceAppInfo:{...workspaceAppInfo},
      
    });
  }

  
  var Splash = document.getElementById('Splash');
  let splashTimeout=setTimeout(()=>{

    let Splash_text = document.getElementById('Splash_text');
    Splash_text.innerHTML="GO...";
    if(isDataComplete)
    {
      RUN_APP();
    }
    else
    {
      if(Splash!==undefined)
      {
        Splash.remove();
        Splash=undefined;
      }
    }
  },2000);
  Splash.addEventListener('click', function(){
    clearTimeout(splashTimeout);
    splashTimeout=undefined;
    if(Splash!==undefined)
    {
      Splash.remove();
      Splash=undefined;
    }
  });




  {//register callback
    



    var RootInfoPath = document.getElementById('RootInfoPath');
    RootInfoPath.addEventListener('click', function(){


      dialog.showOpenDialog({
        properties: ['openDirectory']
      }).then((result) => {
        let folderPath=result.filePaths[0];
        if (folderPath === undefined){
            // console.log("You didn't save the file");
            return;
        }
        console.log(rootInfoPath+"/"+rootInfoName,rootInfoName);
        fs.writeFileSync(rootInfoPath+"/"+rootInfoName, JSON.stringify({
          APP_INFO_FILE_PATH:folderPath+"/"+rootInfoName,
        }))
        
        refreshInfo();
      }).catch(err => {
      console.log(err)
      }); 


    });
    var APP_Router = document.getElementById('APP_Router');
    APP_Router.addEventListener('click', function(){

      dialog.showOpenDialog({
        properties: ['openDirectory']
      }).then((result) => {
        let folderPath=result.filePaths[0];
        if (folderPath === undefined){
            return;
        }

        fs.writeFileSync(rootAppInfo.APP_INFO_FILE_PATH, JSON.stringify({
          ...workspaceAppInfo,
          APPContentPath:folderPath
        }))
        
        refreshInfo();
      }).catch(err => {
      console.log(err)
      }); 


    });
    var Data_Router = document.getElementById('Data_Router');
    Data_Router.addEventListener('click', function(){

      dialog.showOpenDialog({
        properties: ['openDirectory']
      }).then((result) => {
        let folderPath=result.filePaths[0];
        if (folderPath === undefined){
            return;
        }

        fs.writeFileSync(rootAppInfo.APP_INFO_FILE_PATH, JSON.stringify({
          ...workspaceAppInfo,
          APPDataDirPath:folderPath
        }))
        
        refreshInfo();
      }).catch(err => {
      console.log(err)
      }); 

    });


    
    var btn_RUN = document.getElementById('btn_RUN');
    btn_RUN.addEventListener('click', function(){
      ipc.send('RUN_APP1', {
        // ...workspaceAppInfo,
        rootAppInfo:{...rootAppInfo},
        workspaceAppInfo:{...workspaceAppInfo},
        
        //rootInfoPath+"/"+rootInfoName
        // rootAppInfo;
        // workspaceAppInfo;

  
      })
    });

  }


}, false);





// console.log(electron,dialog,ipc,fs)
// var authButton = document.getElementById('XXC');
// authButton.addEventListener('click', function(){
  

//   let APP_INFO_FILE_NAME="app_setup_info.json";
  
//   let APP_INFO_FILE_PATH=electron.remote.app.getAppPath();
//   console.log(">>>",APP_INFO_FILE_PATH)
//   // You can obviously give a direct path without use the dialog (C:/Program Files/path/myfileexample.txt)
//   dialog.showSaveDialog((fileName) => {
//       if (fileName === undefined){
//           console.log("You didn't save the file");
//           return;
//       }

//   }); 
// });

// if(false){
//   let buffer = new Uint8Array(4*500*10000/(Math.pow(1,2)));
//   buffer[0]=1;
  
  
//   setTimeout(()=>{
  
//     for(let i=0;i<100;i++)
//     {
//       ipc.send('r2m',buffer)
//     }
//   },3000);

// }
// buffer[0]=2;


let preTime=Date.now();

ipc.on('m2r', function (event, arg) {


  let curTime=Date.now();

  // console.log('m2r',arg)
  // console.log('m2r',curTime)
  
  console.log("time_ms:",curTime-preTime,arg[0])
  preTime=curTime;
})
