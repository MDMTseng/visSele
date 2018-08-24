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
var canvasF;
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
var recWH = 50;
var millisX = 0;
var gg;


function init_CanvasF() {
	console.log("[cancasXF.js][init]");
	window.setInterval(timeInterval1000, 1000);
	window.setInterval(timeInterval33, 33);
	canvas1 = document.getElementById("canvas1");
	canvas2 = document.getElementById("canvas2");
	canvas3 = document.getElementById("canvas3");
	canvas1.width = allW;
	canvas1.height = allW;
	canvas2.width = allW;
	canvas2.height = allW;
	// canvas3.width = allW;
	// canvas3.height = allW;

	initFabRic();
	initTable();
	initEvents();

}

function initFabRic() {
	canvasF = this.__canvas = new fabric.Canvas('canvas3');
	canvasF.setWidth(allW);
	canvasF.setHeight(allW);
	canvasF.backgroundColor = "#f00";
	var RAW_IMG = getRAWbackgroundImageData(canvas3);
	var f_img = new fabric.Image(RAW_IMG);

	// canvasF.add(f_img).renderAll().setActiveObject(f_img);
	// canvasF.setBackgroundColor({source: 'http://fabricjs.com/assets/escheresque_ste.png'}, canvasF.renderAll.bind(canvasF));
	// fabric.Object.prototype.transparentCorners = false;
	// fabric.Object.prototype.originX = 'center'; fabric.Object.prototype.originY = 'center';
	
	
	// canvasF.add(new fabric.Image(RAW_IMG));
	// canvasF.setBackgroundImage(f_img);

	// canvasF.add(f_img);

	// var imgF = fabric.Image.fromURL(f_img, function (i) {
 //    canvasF.clear();
 //    canvasF.add(i);
 //    canvasF.renderAll();
	// });

	// canvas.add(new fabric.Image(c, {left: canvas.width/2-c.width/2, top: canvas.height/2-c.height/2}));


	// canvasF.setBackgroundImage(imageUrl, canvasF.renderAll.bind(canvasF), {
	//    	backgroundImageOpacity: 0.5,
	//    	backgroundImageStretch: false
	// });
	// canvasF.renderAll();
	var text = 'this is\na multiline\ntext\naligned right!';
	var alignedRightText = new fabric.Text(text, {
		textAlign: 'right'
	});

	var textWithStroke = new fabric.Text("Text with a stroke", {
		stroke: '#ff1318',
		strokeWidth: 1
	});
	var loremIpsumDolor = new fabric.Text("Lorem ipsum dolor", {
		fontFamily: 'Impact',
		stroke: '#c3bfbf',
		strokeWidth: 3
	});
	var comicSansText = new fabric.Text("I'm in Comic Sans", {
		fontFamily: 'Comic Sans',
		fontSize: 40
	});
	var text = new fabric.Text('hello world', {
		left: 100,
		top: 100
	});
	canvasF.add(text);
	var rect = new fabric.Rect({
		left: 100,
		top: 50,
		width: 100,
		height: 100,
		fill: 'green',
		angle: 20,
		padding: 10
	});
	var circle1 = new fabric.Circle({
		radius: 65,
		fill: '#039BE5',
		left: 0
	});

	var circle2 = new fabric.Circle({
		radius: 65,
		fill: '#4FC3F7',
		left: 110,
		opacity: 0.7
	});

	var group = new fabric.Group([circle1, circle2, ], {
		left: 40,
		top: 250
	});

	canvasF.add(rect);
	canvasF.add(group);
	canvasF.on('after:render', function() {
		canvasF.contextContainer.strokeStyle = '#555';

		canvasF.forEachObject(function(obj) {
			var bound = obj.getBoundingRect();

			canvasF.contextContainer.strokeRect(
				bound.left + 0.5,
				bound.top + 0.5,
				bound.width,
				bound.height
			);
		})
	});

}

function drawFabRic() {



}

function drawX() {



	var now = performance.now();
	FPS = now - t0;
	t0 = now;

	// console.log("FPS" + ((performance.now()) - t0) + " milliseconds.");
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
	// console.log(imageData);
	imageData.data.set(buf8);
	return imageData;
}

function timeInterval33() {
	drawX();
}

function timeInterval1000() {
	console.log("[INFO][HB][1000ms][IP]=" + clientIP + "[WS]=" + WS_URI);
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