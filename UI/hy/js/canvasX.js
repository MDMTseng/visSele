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
	
	canvas1.width = allW;
	canvas1.height = allW;
	canvas2.width = allW;
	canvas2.height = allW;
	canvas3.width = allW;
	canvas3.height = allW;

	// canvas1.style.width = "401px";
	// canvas1.style.height = "401px";
	// canvas2.style.width = "400px";
	// canvas2.style.height = "400px";
	// canvas3.style.width = "400px";
	// canvas3.style.height = "400px";

	canvas1.addEventListener("mousemove", mouseMove1);
	canvas2.addEventListener("mousemove", mouseMove2);
	canvas3.addEventListener("mousemove", mouseMove3);


	document.getElementById("reset_ip").addEventListener("click", function() {
		setWSaddress(document.getElementById("coreIP").value);

	}, false);

	document.getElementById("zoomArea-spinAuto").addEventListener("click", function() {
		autoSpint = !autoSpint;
		// draw(scale, translatePos);
	}, false);

	document.getElementById("zoomArea-spinLeft").addEventListener("click", function() {
		startSpinPos += scaleMultiplier;
		// draw(scale, translatePos);
	}, false);

	document.getElementById("zoomArea-spinRight").addEventListener("click", function() {
		startSpinPos -= scaleMultiplier;
		// draw(scale, translatePos);
	}, false);

	document.getElementById("plus").addEventListener("click", function() {
		scale /= 0.9;
		// draw(scale, translatePos);
	}, false);

	document.getElementById("minus").addEventListener("click", function() {
		scale *= 0.9;
		// draw(scale, translatePos);
	}, false);
	document.getElementById("zoomArea-up").addEventListener("click", function() {

		var vec_move = {
			x: 0,
			y: -translateStep / scale
		};
		var vec_tmove = {};
		rotate2dtransform(vec_tmove, vec_move, -startSpinPos);

		translatePos.x += vec_tmove.x;
		translatePos.y += vec_tmove.y;
		// draw(scale, translatePos);
	}, false);

	document.getElementById("zoomArea-down").addEventListener("click", function() {

		var vec_move = {
			x: 0,
			y: translateStep / scale
		};
		var vec_tmove = {};
		rotate2dtransform(vec_tmove, vec_move, -startSpinPos);

		translatePos.x += vec_tmove.x;
		translatePos.y += vec_tmove.y;
		// draw(scale, translatePos);
	}, false);
	document.getElementById("zoomArea-left").addEventListener("click", function() {
		var vec_move = {
			x: -translateStep / scale,
			y: 0
		};
		var vec_tmove = {};
		rotate2dtransform(vec_tmove, vec_move, -startSpinPos);

		translatePos.x += vec_tmove.x;
		translatePos.y += vec_tmove.y;
		// draw(scale, translatePos);
	}, false);

	document.getElementById("zoomArea-right").addEventListener("click", function() {
		var vec_move = {
			x: translateStep / scale,
			y: 0
		};
		var vec_tmove = {};
		rotate2dtransform(vec_tmove, vec_move, -startSpinPos);

		translatePos.x += vec_tmove.x;
		translatePos.y += vec_tmove.y;
		// draw(scale, translatePos);
	}, false);
	// canvas3.addEventListener("mousedown", function(evt) {
	// 	mouseDown = true;
	// 	startDragPos.x = evt.clientX;
	// 	startDragPos.y = evt.clientY;
	// });

	// canvas3.addEventListener("mouseup", function(evt) {
	// 	mouseDown = false;
	// 	translatePos.x += translateDragOffset.x;
	// 	translatePos.y += translateDragOffset.y;
	// 	translateDragOffset.x = 0;
	// 	translateDragOffset.y = 0;
	// });

	// canvas3.addEventListener("mouseover", function(evt) {
	// 	mouseDown = false;
	// });

	// canvas3.addEventListener("mouseout", function(evt) {
	// 	mouseDown = false;
	// });


	window.setInterval(timeInterval1000, 1000);
	window.setInterval(timeInterval33, 33);
	console.log(RXJS);

	initTable();
}

function initTable() {
	var printIcon = function(cell, formatterParams) { //plain text value
		return "<i class='fa fa-print'></i>";
	};

	//Build Tabulator
	$("#example-table").tabulator({
		height: "311px",
		layout: "fitColumns",
		rowFormatter: function(row) {
			if (row.getData().col == "blue") {
				row.getElement().css({
					"background-color": "#A6A6DF"
				});
			}
		},
		columns: [{
			formatter: "rownum",
			align: "center",
			width: 40
		}, {
			formatter: printIcon,
			width: 40,
			align: "center",
			cellClick: function(e, cell) {
				alert("Printing row data for: " + cell.getRow().getData().name)
			}
		}, {
			title: "Name",
			field: "name",
			width: 150,
			formatter: function(cell, formatterParams) {
				var value = cell.getValue();
				if (value.indexOf("o") > 0) {
					return "<span style='color:red; font-weight:bold;'>" + value + "</span>";
				} else {
					return value;
				}
			}
		}, {
			title: "Progress",
			field: "progress",
			formatter: "progress",
			formatterParams: {
				color: ["#00dd00", "orange", "rgb(255,0,0)"]
			},
			sorter: "number",
			width: 100
		}, {
			title: "Rating",
			field: "rating",
			formatter: "star",
			formatterParams: {
				stars: 6
			},
			align: "center",
			width: 120
		}, {
			title: "Driver",
			field: "car",
			align: "center",
			formatter: "tickCross",
			width: 50
		}, {
			title: "Col",
			field: "col",
			formatter: "color",
			width: 50
		}, {
			title: "Line Wraping",
			field: "lorem",
			formatter: "textarea"
		}, {
			formatter: "buttonCross",
			width: 30,
			align: "center"
		}],
	});
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


function drawX() {
	// console.log("[INFO][drawX()]");

	if (canvas1.getContext) {
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

	millisX = performance.now();
	// var d=new Date();
	// 	var millisX = d.getMilliseconds();
	gg = 127 * (1 + Math.sin(dx * millisX));
	gg = Math.round(gg);
	var ctx = C.getContext("2d");
	ctx.fillStyle = 'rgb(' + gg + ',0,0)';

	var C_Hight = parseInt(C.height);
	var C_Width = C.width;

	// ctx.fillStyle = 'rgb(111,0,0)';
	ctx.fillRect(0, 0, C.width, C.height);

	// ctx.fillStyle = 'rgb(0,' +(255-gg)+ ',0)';
	ctx.fillStyle = 'rgb(0,222,0)';
	// ctx.fillStyle = "rgb(255,255,255)";
	ctx.fillRect(25, 25, C.width - 50, C.height - 50);
	// ctx.clearRect(45,45,60,60);
	ctx.strokeRect((C.width >> 1) - (recWH / 2), (C.height >> 1) - (recWH / 2), recWH, recWH);
	ctx.lineWidth = 1;
	ctx.lineCap = 'round';
	ctx.beginPath();
	ctx.moveTo(C.width >> 1, 0);
	ctx.lineTo(C.width >> 1, C.height);
	ctx.moveTo(0, C.height >> 1);
	ctx.lineTo(C.width, C.height >> 1);
	ctx.stroke();
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