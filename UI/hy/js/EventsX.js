function initCanvasEvents() {
    canvas1.addEventListener("mousemove", mouseMove,1);
    canvas2.addEventListener("mousemove", mouseMove,2);
    canvas3.addEventListener("mousemove", mouseMove,3);


    document.getElementById("reset_ip").addEventListener("click", function () {
        setWSaddress(document.getElementById("coreIP").value);

    }, false);

    document.getElementById("zoomArea-spinAuto").addEventListener("click", function () {
        autoSpint = !autoSpint;
        // draw(scale, translatePos);
    }, false);

    document.getElementById("zoomArea-spinLeft").addEventListener("click", function () {
        startSpinPos += scaleMultiplier;
        // draw(scale, translatePos);
    }, false);

    document.getElementById("zoomArea-spinRight").addEventListener("click", function () {
        startSpinPos -= scaleMultiplier;
        // draw(scale, translatePos);
    }, false);

    document.getElementById("plus").addEventListener("click", function () {
        scale /= 0.9;
        // draw(scale, translatePos);
    }, false);

    document.getElementById("minus").addEventListener("click", function () {
        scale *= 0.9;
        // draw(scale, translatePos);
    }, false);
    document.getElementById("zoomArea-up").addEventListener("click", function () {

        let vec_move = {
            x: 0,
            y: -translateStep / scale
        };
        let vec_tmove = {};
        rotate2dtransform(vec_tmove, vec_move, -startSpinPos);

        translatePos.x += vec_tmove.x;
        translatePos.y += vec_tmove.y;
        // draw(scale, translatePos);
    }, false);

    document.getElementById("zoomArea-down").addEventListener("click", function () {

        let vec_move = {
            x: 0,
            y: translateStep / scale
        };
        let vec_tmove = {};
        rotate2dtransform(vec_tmove, vec_move, -startSpinPos);

        translatePos.x += vec_tmove.x;
        translatePos.y += vec_tmove.y;
        // draw(scale, translatePos);
    }, false);
    document.getElementById("zoomArea-left").addEventListener("click", function () {
        let vec_move = {
            x: -translateStep / scale,
            y: 0
        };
        let vec_tmove = {};
        rotate2dtransform(vec_tmove, vec_move, -startSpinPos);

        translatePos.x += vec_tmove.x;
        translatePos.y += vec_tmove.y;
        // draw(scale, translatePos);
    }, false);

    document.getElementById("zoomArea-right").addEventListener("click", function () {
        let vec_move = {
            x: translateStep / scale,
            y: 0
        };
        let vec_tmove = {};
        rotate2dtransform(vec_tmove, vec_move, -startSpinPos);

        translatePos.x += vec_tmove.x;
        translatePos.y += vec_tmove.y;
        // draw(scale, translatePos);
    }, false);
    canvas3.addEventListener("mousedown", function (evt) {
        mouseDown = true;
        startDragPos.x = evt.clientX;
        startDragPos.y = evt.clientY;

    });

    canvas3.addEventListener("mouseup", function (evt) {
        mouseDown = false;
        translatePos.x += translateDragOffset.x;
        translatePos.y += translateDragOffset.y;
        translateDragOffset.x = 0;
        translateDragOffset.y = 0;
    });

    canvas3.addEventListener("mouseover", function (evt) {
        mouseDown = false;
    });

    canvas3.addEventListener("mouseout", function (evt) {
        mouseDown = false;
    });

}
function mouseMove(evt) {

    if(evt.path[0].id==="canvas3"){
        MOUSE_VARs.updateMouseEvt(evt,2);
        $('#checkAreaTitle2').html('3mX=' + evt.offsetX + ',3mY=' + evt.offsetY);
    }else if(evt.path[0].id==="canvas1"){
        MOUSE_VARs.updateMouseEvt(evt,0);
        $('#checkAreaTitle').html('1mX=' + evt.offsetX + ',1mY=' + evt.offsetY);
    }else{
        MOUSE_VARs.updateMouseEvt(evt,1);
        $('#checkAreaTitle').html('2mX=' + evt.offsetX + ',2mY=' + evt.offsetY);
    }
}
function mouseMove1(evt) {
    console.log(evt);

}

function mouseMove2(evt) {
    console.log(evt);
    MOUSE_VARs.updateMouseEvt(evt,1);
    $('#checkAreaTitle2').html('2mX=' + evt.offsetX + ',2mY=' + evt.offsetY);
}

function mouseMove3(evt) {
    console.log(evt);


}

function RAWBriSlider(event, ui) {
    FV.updateGrayLevelVal(event.value);
    // console.log(FV.getGrayLevelVal());
    // let SliderValue = $('.sliders').val();
    // let msg = "/" + $(this).attr("id")+"/"+SliderValue;
    // doSendWS("ws_cue",msg);
    // console.log("SliderValue--->" + SliderValue);
}