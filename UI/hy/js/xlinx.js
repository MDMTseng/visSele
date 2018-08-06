

function initConfig(configx) {

    for (var i = 0; i < 29; i++) {
        configx.data.labels.push("3/" + (i + 1));

    }
    for (var dsIndex = 0; dsIndex <= 2; dsIndex++) {
        for (var i = 0; i < 29; i++) {
            if (configx.data.datasets[dsIndex].data[i] == null) {
                configx.data.datasets[dsIndex].data.push(0);

            }
        }
    }
    console.log(configx);
    return configx;
}

function doStuff(data) {
    console.log(data);
}

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

function timeInterval1000() {
    console.log("[INFO][timeInterval1000XXX]");
    // $('#checkAreaTitle').html(new Date());
    // var text=$("#textareaX").val();
    // console.log("[DEBUG] textareaX="+ text);
    // doSendWS("from_mobile",text);
    // $("#output").append(" [T]"+new Date().getMilliseconds());
    // window.ctx.update();
    // $("#sync1").css("animation-name") == "G2R_keyframe";
    // $("#sync1").toggle();
}