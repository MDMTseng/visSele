function parseData(url, callBack) {
    Papa.parse(url, {
        download: true,
        dynamicTyping: true,
        complete: function(results) {
            callBack(results.data);
        }
    });
}

function VA2(currt, newV, speed) {
    return speed * (newV - currt) + currt;

}

function VA(currt, newV, speed) {
    return speed * newV + (currt * (1 - speed));
}

function timeInterval33() {
    drawX();

}
function degreesToRadians (degrees) {
   return degrees * (Math.PI/180);     
}

function radiansToDegrees (radians) {
   return radians * (180/Math.PI);
}
function timeInterval1000() {
    console.log("[INFO][HB][1000ms][IP]="+clientIP+"[WS]="+WS_URI);
    // doSendWS("test","mmmsssggg");
    // $('#checkAreaTitle').html(new Date());
    // var text=$("#textareaX").val();
    // console.log("[DEBUG] textareaX="+ text);
    // doSendWS("from_mobile",text);
    // $("#output").append(" [T]"+new Date().getMilliseconds());
    // window.ctx.update();
    // $("#sync1").css("animation-name") == "G2R_keyframe";
    // $("#sync1").toggle();
}



//=============for later use
function drawZoomAreaX(C1, C2) {
    // var ctx1 = C1.getContext("2d");
    var context = C2.getContext("2d");
    context.clearRect(0, 0, C2.width, C2.height);
    context.save();
    context.translate(translatePos.x, translatePos.y);
    context.translate(-(C2.width / 2 * scale), -(C2.height / 2 * scale));
    context.scale(scale, scale);
    context.drawImage(C1, 0, 0);
    context.restore();
    
}

function drawRAWAreaX(C1) {

    var context = C1.getContext("2d");


    // clear canvas
    context.clearRect(0, 0, C1.width, C1.height);

    context.save();
    context.translate(C1.width >> 1, C1.height >> 1);
    // context.scale(scale, scale);
    context.beginPath(); // begin custom shape
    context.moveTo(0, 0);
    context.lineTo(111, 111);

    context.moveTo(-119, -20);

    context.bezierCurveTo(-159, 0, -159, 50, -59, 50);
    context.bezierCurveTo(-39, 80, 31, 80, 51, 50);
    context.bezierCurveTo(131, 50, 131, 20, 101, 0);
    context.bezierCurveTo(141, -60, 81, -70, 51, -50);
    context.bezierCurveTo(31, -95, -39, -80, -39, -50);
    context.bezierCurveTo(-89, -95, -139, -80, -119, -20);
    context.closePath(); // complete custom shape
    var grd = context.createLinearGradient(-59, -100, 81, 100);
    grd.addColorStop(0, "#8ED6FF"); // light blue
    grd.addColorStop(1, "#004CB3"); // dark blue
    context.fillStyle = grd;
    context.fill();

    context.lineWidth = 5;
    context.strokeStyle = "#0000ff";
    context.stroke();
    context.restore();
}