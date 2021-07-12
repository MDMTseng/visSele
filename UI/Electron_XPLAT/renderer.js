// This file is required by the index.html file and will
// be executed in the renderer process for that window.
// No Node.js APIs are available in this process because
// `nodeIntegration` is turned off. Use `preload.js` to
// selectively enable features needed in the rendering
// process.

const ipc = require('electron').ipcRenderer
try {
  require('electron-reloader')(module)
} catch (_) {}




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
