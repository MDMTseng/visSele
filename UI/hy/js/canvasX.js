var centerCenter = new Path2D("M10 10 h 80 v 80 h -80 Z");
var theta = 0.01;
var dx = 6.28 / 1000;
var lastFR = 0;
var t0 = performance.now();
var mouseMove1_evt;
var mouseMove2_evt;
var mouseMove3_evt;
var canvas1;
var canvas2;
var canvas3;
var mouseDown = false;
var scale = 1.0;
var scaleMultiplier = 0.1;
var translateStep = 8;
var RXJS;
var allW = 500;
var startDragPos = {
	x: 0,
	y: 0
};
var translateDragOffset = {
	x: 0,
	y: 0
};
var autoSpint = false;
var startSpinPos = 0;
var translatePos = {
	x: 0,
	y: 0
};

function rotate2dtransform(coord_dst, coord_src, theta) {
	var sin_v = Math.sin(theta);
	var cos_v = Math.cos(theta);

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
	
	
	console.log(RXJS);
	
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
		var vec_tmp = {
			x: (evt.clientX - startDragPos.x) / scale,
			y: (evt.clientY - startDragPos.y) / scale
		};
		rotate2dtransform(translateDragOffset, vec_tmp, -startSpinPos);
		// draw(scale, translatePos);
	}
}
var reDraw_C2 = true;
var reDraw_C1 = true;

function drawX() {
	// console.log("[INFO][drawX()]");
	millisX = performance.now();
	if (reDraw_C1&&canvas1.getContext) {
		// reDraw_C1=false;
		drawCheckArea(canvas1);
	}
	if (reDraw_C2 && canvas2.getContext) {
		// console.log("asf0");
		// reDraw_C2 = false;
		// console.log("asf1");
		drawRAWArea(canvas2);
		// console.log("asf2");

	}
	
	if (canvas3.getContext)
		drawZoomArea(canvas2, canvas3);

	var now = performance.now();
	FPS = now - t0;
	t0 = now;

	// console.log("FPS" + ((performance.now()) - t0) + " milliseconds.");


}
var recWH = 50;
var millisX = 0;
var gg;

function drawCheckArea(C) {
	gg = 55 * (1 + Math.sin(dx * millisX));
	gg = Math.round(gg);
	var ctx = C.getContext("2d");
	ctx.fillStyle = 'rgb(' + gg + ',0,0)';
	ctx.fillRect(0, 0, C.width, C.height);
	ctx.fillStyle = "rgb(255,255,255)";
	ctx.fillRect(10,10, C.width - 20, C.height - 20);
}

function drawLine(context, RXJS, i) {
	var scaleOri = scale;
	scale = 1;
	context.moveTo(scale * RXJS.SETS[i].XY[0].x, scale * RXJS.SETS[i].XY[0].y);
	for (var nextPointIndex = 1; nextPointIndex < RXJS.SETS[i].XY.length; nextPointIndex++) {
		context.lineTo(scale * RXJS.SETS[i].XY[nextPointIndex].x, scale * RXJS.SETS[i].XY[nextPointIndex].y);
	}
	if (RXJS.SETS[i].CLOSE_PATH)
		context.closePath();
	scale = scaleOri;

}
var indexColor = 0;
var lerpC = 0.001;

function drawJSON(C) {
	var context = C.getContext("2d");
	context.save();
	// context.translate(C1.width / 2, C1.height / 2);
	RXJS = JSON.parse(RXMSG_temp1_json);

	if (RXJS.TYPE != "FetureSets") return;
	for (var i = 0; i < RXJS.SETS.length; i++) {
		context.lineWidth = RXJS.SETS[i].STROKE_WIDTH;

		// context.strokeStyle = (indexColor++)%10==0?(RXJS.SETS[i].COLOR):("#FFFF00");



		context.beginPath();
		if (RXJS.SETS[i].type == 'line') {
			context.strokeStyle = lerpColor('#ff0000', '#00ff00', gg / 255);
			drawLine(context, RXJS, i);
		} else if (RXJS.SETS[i].type == 'shape') {
			context.strokeStyle = lerpColor('#00ff00', '#0000ff', gg / 255);
			drawLine(context, RXJS, i);
		} else if (RXJS.SETS[i].type == 'rect') {
			context.strokeStyle = lerpColor('#0000ff', '#ff0000', gg / 255);
			context.strokeRect(RXJS.SETS[i].XY[0].x, RXJS.SETS[i].XY[0].y, RXJS.SETS[i].XY[1].w, RXJS.SETS[i].XY[1].h);
		} else if (RXJS.SETS[i].type == 'arc') {
			context.strokeStyle = lerpColor('#0f0f0f', '#ff0000', gg / 255);
			context.arc(RXJS.SETS[i].CENTER_XY.x, RXJS.SETS[i].CENTER_XY.y, RXJS.SETS[i].RADIUS,
				degreesToRadians(RXJS.SETS[i].DEGREE_START_END.start), degreesToRadians(RXJS.SETS[i].DEGREE_START_END.end), false);
		}
		context.stroke();

		// Rect bounds = new Rect();
		//       paint.getTextBounds(text, 0, text.length(), bounds);
		// Bitmap bitmap = Bitmap.createBitmap(bounds.width(), bounds.height(), Bitmap.Config.ARGB_8888);
		//       Canvas canvas = new Canvas(bitmap);
		//       Paint.FontMetrics fontMetrics = paint.getFontMetrics();
		//       float yPos = -fontMetrics.top;
		//       canvas.drawText(text, 0, yPos, paint);

		context.textAlign = "center";
		context.textBaseline = "middle";
		// context.strokeStyle= "red";
		// context.fillStyle = "white";
		context.font = "" + (18 / scale) + "px serif";
		context.lineWidth = 1 / scale;
		context.strokeStyle = 'white';
		// context.strokeStyle='gradient';
		context.strokeText(RXJS.SETS[i].ID_NAME, RXJS.SETS[i].XY[0].x, RXJS.SETS[i].XY[0].y - (10 / scale));
		// context.fillText(RXJS.SETS[i].ID_NAME,RXJS.SETS[i].XY[0].x,RXJS.SETS[i].XY[0].y-(10/ scale));
	}
	context.restore();
}

function drawRAWArea(C1) {
	var context = C1.getContext("2d");
	context.clearRect(0, 0, C1.width, C1.height);
	if (typeof RAW_I_DATA != 'undefined') {
		if (context.canvas.width != RAW_I_DATA.width ||
			context.canvas.height != RAW_I_DATA.height) {
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
	var context = C.getContext("2d");
	context.save();
	context.translate(x, y);
	context.rotate(angle * Math.PI / 180);
	context.drawImage(image, -(image.width / 2), -(image.height / 2));
	context.restore();
}

function getRAWbackgroundImageData(C) {
	var ctx = C.getContext("2d");
	var imageData = ctx.getImageData(0, 0, C.width, C.height);

	var buf = new ArrayBuffer(imageData.data.length);
	var buf8 = new Uint8ClampedArray(buf);
	var data = new Uint32Array(buf);

	for (var y = 0; y < C.height; ++y) {
		for (var x = 0; x < C.width; ++x) {
			var value = 0.0001 * millisX * x * y & 0xff;

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
	// var ctx1 = C1.getContext("2d");
	// console.log(startSpinPos);
	//var imageData = ctx.getImageData(0, 0, canvasWidth, canvasHeight);
	var context = C2.getContext("2d");
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