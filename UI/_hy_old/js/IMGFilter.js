
Filters = {};
Filters.getPixels = function(img) {
  var c = this.getCanvas(img.width, img.height);
  var ctx = c.getContext('2d');
  ctx.drawImage(img);
  return ctx.getImageData(0,0,c.width,c.height);
};

Filters.getCanvas = function(w,h) {
  var c = document.createElement('canvas');
  c.width = w;
  c.height = h;
  return c;
};
Filters.filterImage = function(filter, image, var_args) {
  var args = [this.getPixels(image)];
  for (var i=2; i<arguments.length; i++) {
    args.push(arguments[i]);
  }
  return filter.apply(null, args);
};
Filters.grayscale = function(pixels, args) {
  var d = pixels.data;
  for (var i=0; i<d.length; i+=4) {
    var r = d[i];
    var g = d[i+1];
    var b = d[i+2];
      var v =   args*r +   args*g +   args*b;
    // var v = 0.2126*r + 0.7152*g + 0.0722*b;
    d[i] = d[i+1] = d[i+2] = v
  }
  return pixels;
};
Filters.brightness = function(pixels, adjustment) {
  var d = pixels.data;
  for (var i=0; i<d.length; i+=4) {
    d[i] += adjustment;
    d[i+1] += adjustment;
    d[i+2] += adjustment;
  }
  return pixels;
};
Filters.threshold = function(pixels, threshold) {
  var d = pixels.data;
  for (var i=0; i<d.length; i+=4) {
    var r = d[i];
    var g = d[i+1];
    var b = d[i+2];
    var v = (0.2126*r + 0.7152*g + 0.0722*b >= threshold) ? 255 : 0;
    d[i] = d[i+1] = d[i+2] = v
  }
  return pixels;
};

function adjustGlobaAlpha2(ctx){
    console.log("drawAlphaOnRAWImage");
    ctx.globalCompositeOperation = 'destination-out';
    ctx.fillStyle = "rgba(255, 1, 255, 0.5)";
    ctx.beginPath();
    ctx.fillRect(0, 0, ctx.width, ctx.height);
    ctx.fill();
    ctx.globalCompositeOperation = 'source-over';
}
function adjustGlobaAlpha(ctx){
    var aimg = new Image();
    ctx.save();
    ctx.globalAlpha = 0.4;
    ctx.drawImage(aimg, 0,0,500,500);
    ctx.restore();
}