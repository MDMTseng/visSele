let centerCenter = new Path2D("M10 10 h 80 v 80 h -80 Z");
let theta = 0.01;
let dx = 6.28 / 1000;
let t0 = performance.now();
let mouseMove1_evt;
let mouseMove2_evt;
let mouseMove3_evt;
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



function mouseMove1(evt) {
	// console.log(evt);
	mouseMove1_evt = evt;
	$('#checkAreaTitle').html('mX=' + evt.offsetX + ',mY=' + evt.offsetY);
}

function mouseMove2(evt) {
	// console.log(evt);
	mouseMove2_evt = evt;
	$('#checkAreaTitle2').html('mX=' + evt.offsetX + ',mY=' + evt.offsetY);
}

function mouseMove3(evt) {
	// console.log(evt);
	mouseMove3_evt = evt;
	$('#checkAreaTitle2').html('mX=' + evt.offsetX + ',mY=' + evt.offsetY);
	if (mouseDown) {
		let vec_tmp = {
			x: (evt.clientX - startDragPos.x) / scale,
			y: (evt.clientY - startDragPos.y) / scale
		};
		rotate2dtransform(translateDragOffset, vec_tmp, -startSpinPos);
		// draw(scale, translatePos);
	}
}
let reDraw_C2 = true;
let reDraw_C1 = true;

function drawX() {
	// console.log("[INFO][drawX()]");
	millisX = performance.now();
	if (reDraw_C1&&canvas1.getContext) {
		// reDraw_C1=false;
		drawCheckArea(canvas1);
	}
	if (reDraw_C2 && canvas2.getContext) {
		// reDraw_C2 = false;
		drawRAWArea(canvas2);
	}

	if (canvas3.getContext)
		drawZoomArea(canvas2, canvas3);
	let now = performance.now();
	FPS = now - t0;
	t0 = now;
}
let recWH = 50;
let millisX = 0;
let gg;

function drawCheckArea(C) {
	gg = 55 * (1 + Math.sin(dx * millisX));
	gg = Math.round(gg);
	let ctx = C.getContext("2d");
	ctx.fillStyle = 'rgb(' + gg + ',0,0)';
	ctx.fillRect(0, 0, C.width, C.height);
	ctx.fillStyle = "rgb(255,255,255)";
	ctx.fillRect(10,10, C.width - 20, C.height - 20);
}

function drawLine(context, RXJS, i) {
	let scaleOri = scale;
	scale = 1;
	context.moveTo(scale * RXJS.SETS[i].XY[0].x, scale * RXJS.SETS[i].XY[0].y);
	for (let nextPointIndex = 1; nextPointIndex < RXJS.SETS[i].XY.length; nextPointIndex++) {
		context.lineTo(scale * RXJS.SETS[i].XY[nextPointIndex].x, scale * RXJS.SETS[i].XY[nextPointIndex].y);
	}
	if (RXJS.SETS[i].CLOSE_PATH)
		context.closePath();
	scale = scaleOri;

}
let indexColor = 0;
let lerpC = 0.001;
function drawJSON(C) {
	let context = C.getContext("2d");
	context.save();
	// context.translate(C1.width / 2, C1.height / 2);
	// RXJS = JSON.parse(RXMSG_temp1_json);
	RXJS = RXMSG_temp3;
	if (RXJS.type != "binary_processing_group") return;
    for (let j = 0; j < RXJS.reports.length; j++)
	for (let i = 0; i < RXJS.reports[j].reports.length; i++) {
		// console.log("RXJS.reports[reports].length="+RXJS.reports[j].reports.length);
		// context.lineWidth = RXJS.SETS[i].STROKE_WIDTH;
        // context.strokeStyle="#ff0000";
		// context.strokeStyle = (i)%10==0?(RXJS.SETS[i].COLOR):("#FFFF00");
		context.beginPath();
        // for(let i=0;i<RXJS.reports.reports.detectedCircles;i++){
        //     context.arc(RXJS.SETS[i].CENTER_XY.x, RXJS.SETS[i].CENTER_XY.y, RXJS.SETS[i].RADIUS,0,6.28, false);
		// }
        context.lineWidth = 2;
        	context.strokeStyle = lerpColor('#ff0000', '#00ff00', 0.5);
        drawJSONText(context,RXJS.reports[j].reports[i].area,RXJS.reports[j].reports[i].cx,RXJS.reports[j].reports[i].cy);
        for(let whichLine=0;whichLine<RXJS.reports[j].reports[i].detectedLines.length;whichLine++){
            let LINEs=RXJS.reports[j].reports[i].detectedLines;
            context.moveTo(LINEs[whichLine].x0,LINEs[whichLine].y0);
            context.lineTo(LINEs[whichLine].x1,LINEs[whichLine].y1);
            context.closePath();
		}
        for(let whichLine=0;whichLine<RXJS.reports[j].reports[i].detectedCircles.length;whichLine++){
            let LINEs=RXJS.reports[j].reports[i].detectedCircles;
            context.arc(LINEs[whichLine].x,LINEs[whichLine].y,LINEs[whichLine].r,0,Math.PI*2, false);
            // context.closePath();
        }


		// if (RXJS.SETS[i].type === 'line') {
		// 	context.strokeStyle = lerpColor('#ff0000', '#00ff00', gg / 255);
		// 	drawLine(context, RXJS, i);
		// } else if (RXJS.SETS[i].type === 'shape') {
		// 	context.strokeStyle = lerpColor('#00ff00', '#0000ff', gg / 255);
		// 	drawLine(context, RXJS, i);
		// } else if (RXJS.SETS[i].type === 'rect') {
		// 	context.strokeStyle = lerpColor('#0000ff', '#ff0000', gg / 255);
		// 	context.strokeRect(RXJS.SETS[i].XY[0].x, RXJS.SETS[i].XY[0].y, RXJS.SETS[i].XY[1].w, RXJS.SETS[i].XY[1].h);
		// } else if (RXJS.SETS[i].type === 'arc') {
		// 	context.strokeStyle = lerpColor('#0f0f0f', '#ff0000', gg / 255);
		// 	context.arc(RXJS.SETS[i].CENTER_XY.x, RXJS.SETS[i].CENTER_XY.y, RXJS.SETS[i].RADIUS,
		// 		degreesToRadians(RXJS.SETS[i].DEGREE_START_END.start), degreesToRadians(RXJS.SETS[i].DEGREE_START_END.end), false);
		// }
		context.stroke();

		// Rect bounds = new Rect();
		//       paint.getTextBounds(text, 0, text.length(), bounds);
		// Bitmap bitmap = Bitmap.createBitmap(bounds.width(), bounds.height(), Bitmap.Config.ARGB_8888);
		//       Canvas canvas = new Canvas(bitmap);
		//       Paint.FontMetrics fontMetrics = paint.getFontMetrics();
		//       float yPos = -fontMetrics.top;
		//       canvas.drawText(text, 0, yPos, paint);


		// context.fillText(RXJS.SETS[i].ID_NAME,RXJS.SETS[i].XY[0].x,RXJS.SETS[i].XY[0].y-(10/ scale));
	}
	context.restore();
}
function drawJSONText(context,labeledData,x,y){
    context.textAlign = "center";
    context.textBaseline = "middle";
    // context.strokeStyle= "red";
    // context.fillStyle = "white";
    context.font = "" + (18 / scale) + "px serif";
    context.lineWidth = 1 / scale;
    context.strokeStyle = 'white';
    // context.strokeStyle='gradient';
    context.strokeText(labeledData, x,y);
}
function drawRAWArea(C1) {
	let context = C1.getContext("2d");
	context.clearRect(0, 0, C1.width, C1.height);
	if (typeof RAW_I_DATA != 'undefined') {
		if (context.canvas.width !== RAW_I_DATA.width ||
			context.canvas.height !== RAW_I_DATA.height) {
			context.canvas.width = RAW_I_DATA.width;
			context.canvas.height = RAW_I_DATA.height;
		}
		context.putImageData(RAW_I_DATA, 0, 0);
	}
	// console.log(RAW_I_DATA);
	// else
	// 	context.putImageData(getRAWbackgroundImageData(C1), 0, 0);

	// context.drawImage(C1, 0, 0);
}

function drawRotatedImage(C, image, x, y, angle) {
	let context = C.getContext("2d");
	context.save();
	context.translate(x, y);
	context.rotate(angle * Math.PI / 180);
	context.drawImage(image, -(image.width / 2), -(image.height / 2));
	context.restore();
}

function getRAWbackgroundImageData(C) {
	let ctx = C.getContext("2d");
	let imageData = ctx.getImageData(0, 0, C.width, C.height);

	let buf = new ArrayBuffer(imageData.data.length);
	let buf8 = new Uint8ClampedArray(buf);
	let data = new Uint32Array(buf);

	for (let y = 0; y < C.height; ++y) {
		for (let x = 0; x < C.width; ++x) {
			let value = 0.0001 * millisX * x * y & 0xff;

			data[y * C.width + x] =
				(value << 24) | // alpha
				(value << 16) | // blue
				(value << 8) | // green
				value; // red
		}
	}
	console.log();
	imageData.data.set(buf8);
	return imageData;


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
	context.drawImage(C1, 0, 0);
	drawJSON(C2);


	context.restore();


}