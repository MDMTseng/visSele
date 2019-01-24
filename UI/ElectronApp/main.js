// import * as MISC from "./misc";
const {app, BrowserWindow} = require('electron');
let mainWindow;
let mainWindow2;
let MISC = require('./misc.js');
// var shell = require('shell');
// var appIcon = new Tray('/Users/xlinx/Pictures/20170923_132841.png');
// var window = new BrowserWindow({icon: '/Users/xlinx/Pictures/20170923_132841.png'});

function websocketConnect(url="ws://127.0.0.1")
{
  console.log("[init][WS]");
  this.state.websocketAir=new WebSocket(url);
  this.state.websocketAir.onmessage = this.onMessage;
  this.state.websocketAir.onerror = this.onError;
  console.log("[init][WS][OK]");
  console.log(this.state.websocketAir);
}
function onError(ev){
  this.websocketConnect();
}
function onMessage(ev){
  console.log(ev);
}


function createWindow () {
  console.log("ABC1");
  // MISC.runVOICE("Abcde, Pass, Check Ok, Okay, Okie, No, Yes, ");
  console.log("ABC2");
  // shell.openExternal('https://github.com');
  // shell.beep();
  console.log("ABC3");
  createWindow1();
}
function createWindow1() {
  var screenElectron = require('electron').screen;
  var mainScreen = screenElectron.getPrimaryDisplay();
  var allScreens = screenElectron.getAllDisplays();
  var dimensions = mainScreen.size;

  mainWindow = new BrowserWindow({
    width: dimensions.width,
    height: dimensions.height,
    x: 0,y: 0,
    icon: "/Users/xlinx/Pictures/20170923_132841.png",
    skipTaskbar: true,
    alwaysOnTop: false,
    // show: false,
    // frame: false,
    // autoHideMenuBar: true,
    // transparent: true,
    // resizable: false
  });
  mainWindow.loadFile('index.html')
  //mainWindow.loadURL('http://localhost:8080');
  mainWindow.on('closed', function () {
    mainWindow = null;
  });

}
function createWindow2() {
  var screenElectron = require('electron').screen;
  var mainScreen = screenElectron.getPrimaryDisplay();
  var allScreens = screenElectron.getAllDisplays();
  var dimensions = mainScreen.size;

  mainWindow = new BrowserWindow({
    width: dimensions.width/2,
    height: dimensions.height,
    x: 0,y: 0,
    icon: "/Users/xlinx/Pictures/20170923_132841.png",
    skipTaskbar: true,
    alwaysOnTop: false,
    // show: false,
    // frame: false,
    // autoHideMenuBar: true,
    // transparent: true,
    // resizable: false
  });
  // mainWindow = new BrowserWindow({width: 800, height: 600,transparent: true})
  mainWindow2 = new BrowserWindow({
    width: dimensions.width/2,
    height: dimensions.height,
    x: dimensions.width/2,
    y: 0,});
  // mainWindow.webContents.openDevTools();
  
  
  // mainWindow.loadFile('/Users/xlinx/Desktop/ECproduct/index.html')
  // mainWindow.loadURL('file:///Users/xlinx/Desktop/ECproduct/index.html')
  mainWindow2.loadURL('http://localhost:8080');
  mainWindow.loadURL('http://html5-demos.appspot.com/static/workers/transferables/index.html');
  // win.loadURL(`file://${__dirname}/app/index.html`);
  // Open the DevTools.
  // mainWindow.webContents.openDevTools()

  // Emitted when the window is closed.
  mainWindow.on('closed', function () {
    mainWindow = null;
  });
  mainWindow2.on('closed', function () {
    // Dereference the window object, usually you would store windows
    // in an array if your app supports multi windows, this is the time
    // when you should delete the corresponding element.
    mainWindow2 = null;
  });
}

// This method will be called when Electron has finished
// initialization and is ready to create browser windows.
// Some APIs can only be used after this event occurs.
app.on('ready', createWindow);

// Quit when all windows are closed.
app.on('window-all-closed', function () {
  // On macOS it is common for applications and their menu bar
  // to stay active until the user quits explicitly with Cmd + Q
  if (process.platform !== 'darwin') {
    app.quit();
  }
});
app.on('ready', function() {
  require('electron').powerMonitor.on('on-battery', function() {
    console.log('The system is going to sleep');
  });
});

app.on('activate', function () {
  // On macOS it's common to re-create a window in the app when the
  // dock icon is clicked and there are no other windows open.
  if (mainWindow === null) {
    
    createWindow();
    
  }


});

