$(document).ready(function() {
    console.log("[ws.js][init]");
    var APIurl = 'http://api.ipify.org?format=jsonp&callback=?';
    $.getJSON(APIurl).done(function(data) {
        clientIP = data.ip;
        // console.log("clientIP1=" + clientIP);
    });
    init_WSocket();
    init_FETURE_JSON_TEMP();
    init_CanvasX();
    initCanvasEvents();
    initDataTables();
    // initTabulator();
    $( "#tabs" ).tabs();
    window.setInterval(timeInterval1000, 1000);
    window.setInterval(timeInterval33, 33);
});

function timeInterval33() {
    drawX();
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
