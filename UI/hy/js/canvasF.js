let centerCenter = new Path2D("M10 10 h 80 v 80 h -80 Z");
let theta = 0.01;
let dx = 6.28 / 1000;
let lastFR = 0;
let t0 = performance.now();
let mouseMove1_evt;
let mouseMove2_evt;
let mouseMove3_evt;
let canvas1;
let canvas2;
let canvas3;
let canvasF;
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
let recWH = 50;
let millisX = 0;
let gg;


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
	let RAW_IMG = getRAWbackgroundImageData(canvas3);
	let f_img = new fabric.Image(RAW_IMG);

	// canvasF.add(f_img).renderAll().setActiveObject(f_img);
	// canvasF.setBackgroundColor({source: 'http://fabricjs.com/assets/escheresque_ste.png'}, canvasF.renderAll.bind(canvasF));
	// fabric.Object.prototype.transparentCorners = false;
	// fabric.Object.prototype.originX = 'center'; fabric.Object.prototype.originY = 'center';
	
	
	// canvasF.add(new fabric.Image(RAW_IMG));
	// canvasF.setBackgroundImage(f_img);

	// canvasF.add(f_img);

	// let imgF = fabric.Image.fromURL(f_img, function (i) {
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
	let text = 'this is\na multiline\ntext\naligned right!';
	let alignedRightText = new fabric.Text(text, {
		textAlign: 'right'
	});

	let textWithStroke = new fabric.Text("Text with a stroke", {
		stroke: '#ff1318',
		strokeWidth: 1
	});
	let loremIpsumDolor = new fabric.Text("Lorem ipsum dolor", {
		fontFamily: 'Impact',
		stroke: '#c3bfbf',
		strokeWidth: 3
	});
	let comicSansText = new fabric.Text("I'm in Comic Sans", {
		fontFamily: 'Comic Sans',
		fontSize: 40
	});
	let text = new fabric.Text('hello world', {
		left: 100,
		top: 100
	});
	canvasF.add(text);
	let rect = new fabric.Rect({
		left: 100,
		top: 50,
		width: 100,
		height: 100,
		fill: 'green',
		angle: 20,
		padding: 10
	});
	let circle1 = new fabric.Circle({
		radius: 65,
		fill: '#039BE5',
		left: 0
	});

	let circle2 = new fabric.Circle({
		radius: 65,
		fill: '#4FC3F7',
		left: 110,
		opacity: 0.7
	});

	let group = new fabric.Group([circle1, circle2, ], {
		left: 40,
		top: 250
	});

	canvasF.add(rect);
	canvasF.add(group);
	canvasF.on('after:render', function() {
		canvasF.contextContainer.strokeStyle = '#555';

		canvasF.forEachObject(function(obj) {
			let bound = obj.getBoundingRect();

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



	let now = performance.now();
	FPS = now - t0;
	t0 = now;

	// console.log("FPS" + ((performance.now()) - t0) + " milliseconds.");
}

function drawRAWArea(C1) {
	let context = C1.getContext("2d");
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
	// let text=$("#textareaX").val();
	// console.log("[DEBUG] textareaX="+ text);
	// doSendWS("from_mobile",text);
	// $("#output").append(" [T]"+new Date().getMilliseconds());
	// window.ctx.update();
	// $("#sync1").css("animation-name") == "G2R_keyframe";
	// $("#sync1").toggle();
}