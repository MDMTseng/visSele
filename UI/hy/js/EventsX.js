function initCanvasEvents(){
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