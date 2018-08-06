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
var scaleMultiplier = 0.8;
var startDragOffset = {};
var translatePos = {
		x: 0,
		y: 0
	};
$(function() {

	console.log("[cancasX.js][window.onload]1");
	canvas1 = document.getElementById("canvas1");
	canvas2 = document.getElementById("canvas2");
	canvas3 = document.getElementById("canvas3");
	canvas1.width  = 400;
	canvas1.height  = 400;
	canvas2.width  = 400;
	canvas2.height  = 400;
	canvas3.width  = 400;
	canvas3.height  = 400;
	// canvas1.style.width = "401px";
	// canvas1.style.height = "401px";
	// canvas2.style.width = "400px";
	// canvas2.style.height = "400px";
	// canvas3.style.width = "400px";
	// canvas3.style.height = "400px";

	canvas1.addEventListener("mousemove", mouseMove1);
	canvas2.addEventListener("mousemove", mouseMove2);
	canvas3.addEventListener("mousemove", mouseMove3);
	console.log("[cancasX.js][window.onload]2");



	document.getElementById("plus").addEventListener("click", function() {
		scale /= scaleMultiplier;
		// draw(scale, translatePos);
	}, false);

	document.getElementById("minus").addEventListener("click", function() {
		scale *= scaleMultiplier;
		// draw(scale, translatePos);
	}, false);
	document.getElementById("zoomArea-up").addEventListener("click", function() {
		translatePos.y-=1/scale;
		// draw(scale, translatePos);
	}, false);

	document.getElementById("zoomArea-down").addEventListener("click", function() {
		translatePos.y+=1/scale;
		// draw(scale, translatePos);
	}, false);
	document.getElementById("zoomArea-left").addEventListener("click", function() {
		translatePos.x-=1/scale;
		// draw(scale, translatePos);
	}, false);

	document.getElementById("zoomArea-right").addEventListener("click", function() {
		translatePos.x+=1/scale;
		// draw(scale, translatePos);
	}, false);
	canvas3.addEventListener("mousedown", function(evt) {
		mouseDown = true;
		startDragOffset.x = evt.clientX - translatePos.x;
		startDragOffset.y = evt.clientY - translatePos.y;
	});

	canvas3.addEventListener("mouseup", function(evt) {
		mouseDown = false;
	});

	canvas3.addEventListener("mouseover", function(evt) {
		mouseDown = false;
	});

	canvas3.addEventListener("mouseout", function(evt) {
		mouseDown = false;
	});


	console.log("[xlinx.js]4$(document).ready]");
	window.setInterval(timeInterval1000, 1000);
	window.setInterval(timeInterval33, 33);
});

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
		translatePos.x = evt.clientX - startDragOffset.x;
		translatePos.y = evt.clientY - startDragOffset.y;
		// draw(scale, translatePos);
	}
}

function drawX() {
	// console.log("[INFO][drawX()]");

	if (canvas1.getContext) {
		drawCheckArea(canvas1);
	}
	if (canvas2.getContext)
		drawRAWArea(canvas2);
	if (canvas3.getContext)
		drawZoomArea(canvas2, canvas3);

	var now = performance.now();
	FPS = now - t0;
	t0 = now;

	// console.log("FPS" + ((performance.now()) - t0) + " milliseconds.");

}
var recWH = 50;

function drawCheckArea(C) {

	var millisX = performance.now();
	// var d=new Date();
	// 	var millisX = d.getMilliseconds();
	var gg = 127 * (1 + Math.sin(dx * millisX));
	gg = Math.round(gg);
	var ctx = C.getContext("2d");
	ctx.fillStyle = 'rgb(' + gg + ',0,0)';

	var C_Hight=parseInt(C.height);
	var C_Width=C.width;

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


function drawRAWArea(C1) {

	var context = C1.getContext("2d");

	
	// clear canvas
	context.clearRect(0, 0, C1.width, C1.height);

	context.save();
	context.translate(C1.width>>1,C1.height>>1);
	// context.scale(scale, scale);
	context.beginPath(); // begin custom shape
	context.moveTo(0,0);
	context.lineTo(111,111);

	context.moveTo(-119, -20);

	context.bezierCurveTo(-159, 0, -159, 50, -59, 50);
	context.bezierCurveTo(-39, 80, 31, 80, 51, 50);
	context.bezierCurveTo(131, 50, 131, 20, 101, 0);
	context.bezierCurveTo(141, -60, 81, -70, 51, -50);
	context.bezierCurveTo(31, -95, -39, -80, -39, -50);
	context.bezierCurveTo(-89, -95, -139, -80, -119, -20);
	context.closePath(); // complete custom shape
	var grd = context.createLinearGradient(-59, -100, 81, 100);
	grd.addColorStop(0, "#8ED6FF"); // light blue
	grd.addColorStop(1, "#004CB3"); // dark blue
	context.fillStyle = grd;
	context.fill();

	context.lineWidth = 5;
	context.strokeStyle = "#0000ff";
	context.stroke();
	context.restore();
}
function drawZoomAreaX(C1, C2) {
	// var ctx1 = C1.getContext("2d");
	var context = C2.getContext("2d");
	context.clearRect(0, 0, C2.width, C2.height);
	context.save();
	context.translate(translatePos.x, translatePos.y);
	context.translate(-(C2.width/2*scale),-(C2.height/2*scale) );
	context.scale(scale, scale);
	context.drawImage(C1, 0, 0);
	context.restore();
}
function drawZoomArea(C1, C2) {
	// var ctx1 = C1.getContext("2d");
	var context = C2.getContext("2d");
	context.clearRect(0, 0, C2.width, C2.height);
	context.save(); 
	// context.translate(translatePos.x, translatePos.y);
	// context.translate(-(C2.width/2*scale),-(C2.height/2*scale) );
	context.translate(translatePos.x*scale,translatePos.y*scale);
	context.scale(scale, scale);
	context.drawImage(C1, 0, 0);

	context.restore();
	
}