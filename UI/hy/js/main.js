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

initJsonPainter();
    initTreejs();
    $( "[data-role='header'], [data-role='footer']" ).toolbar();
});
function initJsonPainter(){
    $('#json').css('text-shadow','none');
    $('#json').jsonPainter();

    // Show painted JSON from a atring
    var str = '{ node: "value", list: [ 1, 3, 4 ] }';
    $('#json').jsonPainter(str);

    // Show painter JSON from an object
    // var obj = { node: "value", list: [ 1, 3, 4 ] };
    $('#json').jsonPainter(RXMSG_temp3_json);
}
function initTreejs(){
    console.log('initTreejs(1)');
    $("#jstreeX").jstree();  
    // 7 bind to events triggered on the tree
    $("#jstreeX").on("changed.jstree", function (e, data) {
      console.log(data.selected);
    });
    $('#jstreeX').jstree({ 'core' : {
    'data' : [
       { "id" : "ajson1", "parent" : "#", "text" : "Simple root node" },
       { "id" : "ajson2", "parent" : "#", "text" : "Root node 2" },
       { "id" : "ajson3", "parent" : "ajson2", "text" : "Child 1" },
       { "id" : "ajson4", "parent" : "ajson2", "text" : "Child 2" },
    ]
} });
    console.log('initTreejs(2)');
    console.log(RXMSG_temp3_json);
    
}
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
