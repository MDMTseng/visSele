
// var wsUri22 = "ws://127.0.0.1:8000/solar_wss";
var WS_URI = "ws://127.0.0.1:4090/xlinx";
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
    console.log("onMessage:::");
    console.log(evt);
    $('#checkAreaTitle').html("[WS]OnMessage"+",Status="+websocket.readyState);
    if (evt.data instanceof ArrayBuffer) {
        let header = BPG_Protocol.raw2header(evt);
        console.log("onMessage:["+header.type+"]");
        if(header.type === "HR")
        {

            RXMSG.HR=BPG_Protocol.raw2obj(evt);
            // console.log(header);
            // console.log("Hello I am ready.");
            websocket.send(BPG_Protocol.obj2raw("HR",{a:["d"]}));

            // websocket.send(BPG_Protocol.obj2raw("TG",{}));
            setTimeout(()=>{

                websocket.send(BPG_Protocol.obj2raw("TG",{}));
            },0);
        }


        else if(header.type === "SS")
        {

            RXMSG.SS=BPG_Protocol.raw2obj(evt);
            // console.log(header);
            // console.log("Session start:");
        }

        else if(header.type === "IM")
        {
            let pkg = BPG_Protocol.raw2Obj_IM(evt);
            RXMSG.IM=pkg;
            RXMSG.RAWIMAGE= new ImageData(pkg.image, pkg.width);
            // RAW_I_DATA = new ImageData(pkg.image, pkg.width);

            // console.log(pkg);


        }else if(header.type === "IR")
        {

            RXMSG.IR=BPG_Protocol.raw2obj(evt).data;
            RXMSG.after_IR();
            // console.log(header);
            // console.log("Session start:");
        }


    }
RXMSG.listAll();
}

function onError(evt) {
    $('#checkAreaTitle').html("[WS]OnError"+",Status="+websocket.readyState);
    writeToScreen('<span style="color: red;">ERROR:</span> ' + evt.data);
    init_WSocket();
}

function doSendWS(t, m) {
    let T;
    if (t == "PING")
        T = "PING";
    else if (t == "PONG")
        T = "PONG";
    else if (t == "REQUIRE_RAW")
        T = "REQUIRE_RAW";

    let date = performance.now();
    let msg = {
        TYPE: T,
        DATA: date,
        TIMESTAMP: date,
        MSG: m
    };
    // writeToScreen(JSON.stringify(msg));
    // console.log(websocket);
    if (websocket.readyState == 1)
    {
      //    websocket.send(BPG_Protocol.obj2raw("TC",msg));
    }
    else if (websocket.readyState == 3)
        init_WSocket();
    else
        init_WSocket();
    $('#checkAreaTitle').html("[WS]doSend"+",Status="+websocket.readyState);
}

function writeToScreen(message) {
    let pre = document.createElement("p");
    pre.style.wordWrap = "break-word";
    pre.innerHTML = message;
    document.getElementById("output").appendChild(pre);
}


// window.addEventListener("load", init, false);
