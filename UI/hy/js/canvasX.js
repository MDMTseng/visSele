let centerCenter = new Path2D("M10 10 h 80 v 80 h -80 Z");
let theta = 0.01;
let canvas1;
let canvas2;
let canvas3;
let mouseDown = false;
let scale = 1.0;
let scaleMultiplier = 0.1;
let translateStep = 8;
let RXJS;
let allW = 500;

let startDragPos = {
	x: 0, 
	y: 0
};
let translateDragOffset = {
	x: 0,
	y: 0
};
let autoSpint = false;
let startSpinPos = 0;
let translatePos = {
	x: 0,
	y: 0
};

function rotate2dtransform(coord_dst, coord_src, theta) {
	let sin_v = Math.sin(theta);
	let cos_v = Math.cos(theta);

	coord_dst.x = coord_src.x * cos_v - coord_src.y * sin_v;
	coord_dst.y = coord_src.x * sin_v + coord_src.y * cos_v;
}

function init_CanvasX() {
	console.log("[cancasX.js][init]");
	
	canvas1 = document.getElementById("canvas1");
	canvas2 = document.getElementById("canvas2");
	canvas3 = document.getElementById("canvas3");
	
	canvas1.width = 1000;
	canvas1.height = 500;
	canvas2.width = allW;
	canvas2.height = allW;
	canvas3.width = allW;
	canvas3.height = allW;
	
	
	// console.log(RXJS);
	
}



let reDraw_C2 = true;
let reDraw_C1 = true;
let t0=0;
function drawX() {
	// console.log("[INFO][drawX()]");


	if (reDraw_C1&&canvas1.getContext) {
		// reDraw_C1=false;
		drawCheckArea(canvas1);
	}
	if (reDraw_C2 && canvas2.getContext) {
		// reDraw_C2 = false;
		drawRAWArea(canvas2);
	}
	if (canvas3.getContext){
        drawCrossMouse(canvas2);
		drawZoomArea(canvas2, canvas3);


	}
	let now = performance.now();
	GDA.FPS = now - t0;
	t0 = now;
}

function drawCheckArea(C) {

	let ctx = C.getContext("2d");
	ctx.fillStyle =  GDA.fadingDarkRed_1000ms ;
	ctx.fillRect(0, 0, C.width, C.height);
	ctx.fillStyle = "rgb(255,255,255)";
	ctx.fillRect(10,10, C.width - 20, C.height - 20);
}

function drawCrossMouse(C) {
    let context = C.getContext("2d");
    context.save();
    context.beginPath();
    context.strokeStyle = GDA.fadingWB_1000ms;

    context.moveTo(MOUSE_VARs.invX-50,MOUSE_VARs.invY);
    context.lineTo(MOUSE_VARs.invX+50,MOUSE_VARs.invY);
    context.closePath();
    context.moveTo(MOUSE_VARs.invX,MOUSE_VARs.invY-50);
    context.lineTo(MOUSE_VARs.invX,MOUSE_VARs.invY+50);
    context.closePath();

    context.rect(MOUSE_VARs.invX-15,MOUSE_VARs.invY-15,30,30);


    context.stroke();
    context.restore();
}

function drawJSON(C) {
	let context = C.getContext("2d");
	context.save();
	// context.translate(C1.width / 2, C1.height / 2);
	// RXJS = JSON.parse(RXMSG_temp1_json);
	if(RXMSG.IR===null)return;
	RXJS = RXMSG.IR;
    // adjustGlobaAlpha(context);

	if (RXJS.type != "binary_processing_group") return;
    for (let j = 0; j < RXJS.reports.length; j++)
	for (let i = 0; i < RXJS.reports[j].reports.length; i++) {
        drawJSONText(context,RXJS.reports[j].reports[i].area,RXJS.reports[j].reports[i].cx,RXJS.reports[j].reports[i].cy);
        context.lineWidth = 2;
        // context.strokeStyle="rgba(255,0,0,0.5)";
        context.strokeStyle = lerpColor('#ff0000', '#0fff00', i/RXJS.reports[j].reports.length);

				let offset = 0.5;
        for(let whichLine=0;whichLine<RXJS.reports[j].reports[i].detectedLines.length;whichLine++){
            let LINEs=RXJS.reports[j].reports[i].detectedLines;
            
            context.beginPath();
            context.moveTo(LINEs[whichLine].x0+offset,LINEs[whichLine].y0+offset);
            context.lineTo(LINEs[whichLine].x1+offset,LINEs[whichLine].y1+offset);
            context.closePath();
            context.stroke();
		}
        for(let whichLine=0;whichLine<RXJS.reports[j].reports[i].detectedCircles.length;whichLine++){
            let LINEs=RXJS.reports[j].reports[i].detectedCircles;
            context.beginPath();
            context.arc(LINEs[whichLine].x+offset,LINEs[whichLine].y+offset,LINEs[whichLine].r,0,Math.PI*2, false);
            context.closePath();
            context.stroke();
        }

	}
	context.restore();
}
function drawJSONText(context,labeledData,x,y){
    context.save();
    context.textAlign = "center";
    context.textBaseline = "middle";
    // context.strokeStyle= "red";
    // context.fillStyle = "white";
    context.font = "" + (18 / scale) + "px serif";
    context.lineWidth = 1 / scale;
    context.strokeStyle = 'gray';
    // context.strokeStyle='gradient';
    // context.translate(translatePos.x,translatePos.y);
    // context.rotate(-startSpinPos);
    context.strokeText(labeledData, x,y);
    // context.rotate(startSpinPos);
    context.restore();
}
function drawRAWArea(C1) {
	let context = C1.getContext("2d");
	context.clearRect(0, 0, C1.width, C1.height);

	if ( RXMSG.RAWIMAGE !== null) {
		if (context.canvas.width !== RXMSG.RAWIMAGE.width ||
			context.canvas.height !== RXMSG.RAWIMAGE.height) {
			context.canvas.width = RXMSG.RAWIMAGE.width;
			context.canvas.height = RXMSG.RAWIMAGE.height;
		}

		 context.putImageData(RXMSG.RAWIMAGE, 0, 0);


	}
}

function drawRotatedImage(C, image, x, y, angle) {
	let context = C.getContext("2d");
	context.save();
	context.translate(x, y);
	context.rotate(angle * Math.PI / 180);
	context.drawImage(image, -(image.width / 2), -(image.height / 2));
	context.restore();
}

function drawZoomArea(C1, C2) {
	// let ctx1 = C1.getContext("2d");
	// console.log(startSpinPos);
	//let imageData = ctx.getImageData(0, 0, canvasWidth, canvasHeight);


	let context = C2.getContext("2d");
	context.clearRect(0, 0, C2.width, C2.height);
	context.save();
	if (autoSpint)
		startSpinPos += 0.01;

	context.translate((C2.width / 2), (C2.height / 2));

	context.scale(scale, scale);

	context.rotate(startSpinPos);

	context.translate(translatePos.x + translateDragOffset.x, translatePos.y + translateDragOffset.y);

	context.translate(-(C1.width / 2), -(C1.height / 2));

	// context.translate(-(C2.width / 2), -(C2.height / 2));
	// context.putImageData(getRAWbackgroundImageData(C1), 0, 0);

    // RAW_I_DATA=Filters.grayscale(RAW_I_DATA,FV.getGrayLevelVal());
    context.filter ="invert("+FV.getGrayLevelVal()+")";
    // context.filter = "brightness("+FV.getGrayLevelVal()+")";
    // context.drawImage(image,0,0);
	context.drawImage(C1, 0, 0);
    // drawAlphaOnRAWImage(context);
    // adjustGlobaAlpha(context);
    context.filter = "brightness(100%)";
    // console.log(t);
    var mat = context.getTransform();
    var invMat = context.getTransform().invertSelf();
    // console.log(mat);
    // console.log(invmat);
    MOUSE_VARs.invX = MOUSE_VARs.getMouseXY(2)[0] * invMat.a + MOUSE_VARs.getMouseXY(2)[1] * invMat.c + invMat.e;
    MOUSE_VARs.invY = MOUSE_VARs.getMouseXY(2)[0] * invMat.b + MOUSE_VARs.getMouseXY(2)[1] * invMat.d + invMat.f;

    drawJSON(C2);
	context.restore();
}