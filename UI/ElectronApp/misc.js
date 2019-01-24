var exec = require('child_process').execFile;
function runVOICE(what){
    console.log("fun() start");
    exec('/usr/bin/osascript' ,['-e','say "'+what+'"'],(err, stdout, stderr) => {
     // exec('ls',['-al'],(err, stdout, stderr) => {
     if (err) {
       console.error(err);
       return;
     }
     console.log(stdout);
   });
   
 }  
 
 const interval1000ms = setInterval(() => {
   var n = new Date().getMilliseconds();
   console.log('1000ms!'+n);
   
 }, 1000);
 
 const interval33ms = setInterval(() => {
    var n = new Date().getMilliseconds();
    // console.log('33ms!'+n);
  }, 33);
function avoidScreenSleep(){
    const powerSaveBlocker = require('electron').powerSaveBlocker;
    var id = powerSaveBlocker.start('prevent-display-sleep');
    console.log(powerSaveBlocker.isStarted(id));
    powerSaveBlocker.stop(id);
}

module.exports.runVOICE = runVOICE; 