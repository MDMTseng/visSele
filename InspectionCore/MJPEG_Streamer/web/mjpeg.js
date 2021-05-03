


function NewMPEGStream(url)
{
  let img = new Image();
  img.src = url;
  return img;
}


// namespace MJPEG { ...
var MJPEG = (function(module) {
  "use strict";

  // class Player { ...
  module.Player = function(canvas, url, options) {

    var self = this;
    if (typeof canvas === "string" || canvas instanceof String) {
      canvas = document.getElementById(canvas);
    }
    var context = canvas.getContext("2d");


    self.stream = NewMPEGStream(url);
    
    self.draw_stream=()=>{
      console.log(self.stream);
      context.drawImage(self.stream,0,0);
    }
    canvas.addEventListener("click", function() {
      self.draw_stream();
    }, false);


  };

  return module;

})(MJPEG || {});