// var wsUri22 = "ws://127.0.0.1:8000/solar_wss";
var peopleA = 512;
var peopleB = 0;
var peopleC = 1522;
var wsUri = "ws://scube.decade.tw:7777/tnua_class_decade_tw";
var output;
var clientIP = "x.x.x.x";

$(document).ready(function() {
    console.log("ws.js doc ready");
    $.getJSON('http://api.ipify.org?format=jsonp&callback=?', function(data) {
        // clientIP=JSON.stringify(data, null, 2);

        console.log("clientIP1=" + clientIP);
    }).done(function(data) {
        console.log("second success");
        clientIP = data.ip;
    });
    console.log("clientIP2=" + clientIP);


});

function init() {

    output = document.getElementById("output");
    websocket = new WebSocket(wsUri);
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
    doSendInfo("WebSocket rocks");
}

function onClose(evt) {
    writeToScreen("DISCONNECTED");
    init();
    writeToScreen("...RE-CONNECTED");
}

function onMessage(evt) {

    var data = $.parseJSON(evt.data);
    // console.log("[O][onMsg]JSONDATA",data);
    $("#TIMECODE").html(data.TIME_CODE);


}

function onError(evt) {
    writeToScreen('<span style="color: red;">ERROR:</span> ' + evt.data);
}



// function doSendWS_chgFont(message) {

//     doSendWS("chgFont", message);
// }

function doSendInfo(message) {

    doSendWS("info", message);
}

function doSendWS(type, message) {

    var date = new Date().toLocaleDateString("en-US", {
        "year": "numeric",
        "month": "numeric",
        "day": "numeric",
        "hour": "numeric",
        "minute": "numeric",
        "second": "numeric"

    });
    var msg = {
        id: 7213,
        ip: clientIP,
        type: type,
        text: message,
        date: date
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
    // var SliderValue = $('.sliders').val();
    // // alert("Slider value = " + SliderValue);

    // var msg = "/" + $(this).attr("id")+"/"+SliderValue;
    // doSendWS("ws_cue",msg);
    // console.log("SliderValue--->" +SliderValue);
}
window.addEventListener("load", init, false);