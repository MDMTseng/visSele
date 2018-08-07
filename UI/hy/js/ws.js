// var wsUri22 = "ws://127.0.0.1:8000/solar_wss";
var WS_URI = "ws://scube.decade.tw:7777/tnua_class_decade_tw";
var output;
var clientIP = "x.x.x.x";
var TX_SERIAL=0;
$(document).ready(function() {
    console.log("[ws.js][init]");
    var APIurl = 'http://api.ipify.org?format=jsonp&callback=?';
    $.getJSON(APIurl).done(function(data) {
        clientIP = data.ip;
        // console.log("clientIP1=" + clientIP);
    });
    init_WSocket();
    init_CanvasX();
    init_drawCanvas();
});

function init_WSocket() {

    output = document.getElementById("output");
    websocket = new WebSocket(WS_URI);
    websocket.onopen = function(evt) {
        onOpen(evt)
    };
    websocket.onclose = function(evt) {
        onClose(evt)
    };
    websocket.onmessage = function(evt) {
        onMessage(evt)
    };
    websocket.onerror = function(evt) {
        onError(evt)
    };
}

function onOpen(evt) {
    writeToScreen("CONNECTED");
    doSendWS("info","ws_on_open");
}

function onClose(evt) {
    writeToScreen("[info]DISCONNECTED");
    init_WSocket();
    writeToScreen("[info]...RE-CONNECTED");
}

function onMessage(evt) {
    var data = $.parseJSON(evt.data);
    $("#TIMECODE").html(data.TIME_CODE);
}

function onError(evt) {
    writeToScreen('<span style="color: red;">ERROR:</span> ' + evt.data);
}

function doSendWS(t, m) {
    var date = new Date().toLocaleDateString("en-US", {
        "year": "numeric",
        "month": "numeric",
        "day": "numeric",
        "hour": "numeric",
        "minute": "numeric",
        "second": "numeric"
    });
    var msg = {
        SERIAL: TX_SERIAL++,
        IP: clientIP,
        TYPE: t,
        ALL_TYPE:"info,line,rect,angle,circle",
        MSG: m,
        TIMESTAMP: date
    };
    // writeToScreen(JSON.stringify(msg));
    websocket.send(JSON.stringify(msg));
}

function writeToScreen(message) {
    var pre = document.createElement("p");
    pre.style.wordWrap = "break-word";
    pre.innerHTML = message;
    document.getElementById("output").appendChild(pre);
}

function valueChanged() {
    var SliderValue = $('.sliders').val();
    // var msg = "/" + $(this).attr("id")+"/"+SliderValue;
    // doSendWS("ws_cue",msg);
    console.log("SliderValue--->" + SliderValue);
}
// window.addEventListener("load", init, false);