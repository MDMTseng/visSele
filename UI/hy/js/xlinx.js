function parseData(url, callBack) {
    Papa.parse(url, {
        download: true,
        dynamicTyping: true,
        complete: function(results) {
            callBack(results.data);
        }
    });
}
function lerpColor(a, b, amount) { 

    let ah = parseInt(a.replace(/#/g, ''), 16),
        ar = ah >> 16, ag = ah >> 8 & 0xff, ab = ah & 0xff,
        bh = parseInt(b.replace(/#/g, ''), 16),
        br = bh >> 16, bg = bh >> 8 & 0xff, bb = bh & 0xff,
        rr = ar + amount * (br - ar),
        rg = ag + amount * (bg - ag),
        rb = ab + amount * (bb - ab);

    return '#' + ((1 << 24) + (rr << 16) + (rg << 8) + rb | 0).toString(16).slice(1);
}
Number.prototype.numberFormat = function(c, d, t){
    var n = this,
        c = isNaN(c = Math.abs(c)) ? 2 : c,
        d = d == undefined ? "." : d,
        t = t == undefined ? "," : t,
        s = n < 0 ? "-" : "",
        i = String(parseInt(n = Math.abs(Number(n) || 0).toFixed(c))),
        j = (j = i.length) > 3 ? j % 3 : 0;
    return s + (j ? i.substr(0, j) + t : "") + i.substr(j).replace(/(\d{3})(?=\d)/g, "$1" + t) + (c ? d + Math.abs(n - i).toFixed(c).slice(2) : "");
};
function VA2(currt, newV, speed) {
    return speed * (newV - currt) + currt;

}

function VA(currt, newV, speed) {
    return speed * newV + (currt * (1 - speed));
}


function degreesToRadians (degrees) {
   return degrees * (Math.PI/180);     
}

function radiansToDegrees (radians) {
   return radians * (180/Math.PI);
}




//=============for later use
function drawZoomAreaX(C1, C2) {
    // let ctx1 = C1.getContext("2d");
    let context = C2.getContext("2d");
    context.clearRect(0, 0, C2.width, C2.height);
    context.save();
    context.translate(translatePos.x, translatePos.y);
    context.translate(-(C2.width / 2 * scale), -(C2.height / 2 * scale));
    context.scale(scale, scale);
    context.drawImage(C1, 0, 0);
    context.restore();
    
}

function drawRAWAreaX(C1) {

    let context = C1.getContext("2d");


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
    let grd = context.createLinearGradient(-59, -100, 81, 100);
    grd.addColorStop(0, "#8ED6FF"); // light blue
    grd.addColorStop(1, "#004CB3"); // dark blue
    context.fillStyle = grd;
    context.fill();

    context.lineWidth = 5;
    context.strokeStyle = "#0000ff";
    context.stroke();
    context.restore();
}