// var wsUri22 = "ws://127.0.0.1:8000/solar_wss";
var WS_URI = "ws://192.168.168.249:40901/xlinx";
var output;
var clientIP = "x.x.x.x";
var TX_SERIAL = 0;
var reconnectTimes=3;
var RAW_I_DATA;
var requireNewRAW=true;

function setWSaddress(ip){
    WS_URI = "ws://"+ip+":4090/xlinx";
}
function init_WSocket() {
    console.log("Trying Re-Connect w/times="+reconnectTimes);
    reconnectTimes--;
    if(reconnectTimes<=0)return;

    try {
        websocket = new WebSocket(WS_URI);
        websocket.binaryType = "arraybuffer";
    } catch (err) {
        document.getElementById("output").innerHTML = err.message;

    }


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
    $('#checkAreaTitle').html("[WS]OnOpen"+",Status="+websocket.readyState);
    doSendWS("PING", "ON_CONNECTED_HIHI");

}

function onClose(evt) {
    writeToScreen("[info]DISCONNECTED");
    $('#checkAreaTitle').html("[WS]OnClose"+",Status="+websocket.readyState);
    writeToScreen("[info]...RE-CONNECTED");
}

// const arrayBuffer = new ArrayBuffer(32);
// console.log(arrayBuffer);
// const blob = new Blob([arrayBuffer]);
// console.log(blob);

function onMessage(evt) {
    // var Gray = (R*38 + G*75 + B*15) >> 7;
    // console.log(evt);
    $('#checkAreaTitle').html("[WS]OnMessage"+",Status="+websocket.readyState);
    if (evt.data instanceof ArrayBuffer) {
        // var aDataArray = new Float64Array(evt.data);
        // var aDataArray = new Uint8Array(evt.data);
        var headerArray = new Uint8ClampedArray(evt.data,0,5);
        
        

        var contentArray = new Uint8ClampedArray(evt.data,5,10);
        var RAW_WIDTH=contentArray[1]|(contentArray[2]<<8);
        var RAW_HEIGHT=contentArray[3]|(contentArray[4]<<8);
        // console.log(RAW_WIDTH);
        // console.log(RAW_HEIGHT);


        var dataArray = new Uint8ClampedArray(evt.data,10,4*RAW_WIDTH*RAW_HEIGHT);
        // for(var i=0;i<dataArray.length;i++){
        //     dataArray^= 0xff;
        // }
        RAW_I_DATA = new ImageData(dataArray, RAW_WIDTH);
        // doSendWS("PING","haha");
        // var dataArray = new Uint8ClampedArray(evt.data,11,600*150*4);
        // RAW_I_DATA = new ImageData(dataArray,150);

        
    }
    // if (evt.data instanceof Blob) {
    //     console.log("BBBB");
    //     console.log(event);
    //     console.log(event.data);

    //     var result_U8CA = new Uint8ClampedArray([event.data]);
    //     var result_U8A = new Uint8Array([event.data]);
    //     var result_I8A = new Int8Array([event.data]);

    //     console.log(result_U8CA[0]);
    //     console.log(result_U8A[8]);
    //     console.log(result_I8A[8]);
    //     // result.forEach(function(element) {
    //     //     console.log(element);
    //     // });
    //     // var gg=Array.from(result);


    // } else {
    //     console.log("!BBBB");
    //     try {
    //         var data = $.parseJSON(evt.data);
    //         console.log(data);

    //     } catch (err) {
    //         document.getElementById("output").innerHTML = err.message;
    //     }
    // }
}

function onError(evt) {
    $('#checkAreaTitle').html("[WS]OnError"+",Status="+websocket.readyState);
    writeToScreen('<span style="color: red;">ERROR:</span> ' + evt.data);
    init_WSocket();
}

function doSendWS(t, m) {
    var T;
    if (t == "PING")
        T = "PING";
    else if (t == "PONG")
        T = "PONG";
    else if (t == "REQUIRE_RAW")
        T = "REQUIRE_RAW";

    var date = performance.now();
    var msg = {
        TYPE: T,
        DATA: date,
        TIMESTAMP: date,
        MSG: m
    };
    // writeToScreen(JSON.stringify(msg));
    // console.log(websocket);
    if (websocket.readyState == 1)
        websocket.send(JSON.stringify(msg));
    else if (websocket.readyState == 3)
        init_WSocket();
    else
        init_WSocket();
    $('#checkAreaTitle').html("[WS]doSend"+",Status="+websocket.readyState);
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