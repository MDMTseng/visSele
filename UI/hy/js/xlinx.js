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
    console.log("[INFO][TimerInterval][1000][IP]"+clientIP);
    doSendWS("test","mmmsssggg");
    // $('#checkAreaTitle').html(new Date());
    // var text=$("#textareaX").val();
    // console.log("[DEBUG] textareaX="+ text);
    // doSendWS("from_mobile",text);
    // $("#output").append(" [T]"+new Date().getMilliseconds());
    // window.ctx.update();
    // $("#sync1").css("animation-name") == "G2R_keyframe";
    // $("#sync1").toggle();
}