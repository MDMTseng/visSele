////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//         P5.GUI  :    Slider-Range Example 1                                //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

// This example shows how to use magic variables to control the slider range
// and step for individual variables.
// Just append 'min', 'max' or 'step' to your variable names...


// seed color and alpha
let seedColor = '#ff00dd';
let bgColor = [0, 110, 0];

// inital number of seeds
let seeds = 500;

// angle (phi)
let angle = 360 * (Math.sqrt(5) - 1) / 2;

// radius of the seed
let radius = 3;

// scale
let zoom = 15;

// seed opacity
let opacity = 150;

////////////////////////////////////////////////////////////////////////////////

// This is where the magic happens ...

// set slider range with magic variables
let seedsMin = 1;
let seedsMax = 2000;

// set angle range and step with magic variables
let angleMax = 360;
let angleStep = 0.1;

// set radius range and step with magic variables
let radiusMin = 0.5;
let radiusMax = 5;
let radiusStep = 0.1;

// set step range with magic variables
let zoomMax = 50;
let zoomStep = 0.1;

// set opacity range with magic variables
let opacityMax = 255;

////////////////////////////////////////////////////////////////////////////////

// the gui object
let gui;

function setup() {

  // all angles in degrees (0 .. 360)
  // angleMode(DEGREES);
  // let myCanvas = createCanvas(500,500);
  let myCanvas = createCanvas(500,400);
  myCanvas.class('R');
    myCanvas.parent("idnameofdiv_RIGHT");
// let myCanvas2 = createCanvas(400,400);
//     myCanvas2.parent("idnameofdiv_RIGHT");
    noStroke();
  rectMode(CENTER);
    // myCanvas.parent("idnameofdiv2");
  // create a canvas that fills the window
  // createCanvas(windowWidth, windowHeight);

  // create the GUI
  // gui = createGui('slider-range-1');
  // gui.addGlobals('seeds', 'angle', 'zoom', 'radius', 'seedColor', 'opacity', 'bgColor');

  // only call draw when then gui is changed
  // noLoop();

}
let theta=0.01;
let dx=6.28/1000;
let lastFR=0;
function VA2(currt,newV,speed){
  return speed*(newV-currt)+currt ;

}
function VA(currt,newV,speed){
  return speed*newV+ (currt*(1-speed)) ;
}

function VA_FR(){
  lastFR=VA2(lastFR,frameRate(),0.01)
  return lastFR;
}

function draw() {
  let gg=127*(1+sin(dx*millis()));
  bgColor = [ 0,gg, 110];
  background(bgColor);
  draw2();
  
  text("FR="+nf(VA_FR(),2,2),10,10);
}
let angle1=0;
let angle2=0;
let scalar = 70;

let sliderV=0;
function draw2() {
  

  let ang1 = radians(angle1);
  let ang2 = radians(angle2);

  let x1 = width/2 + (scalar * cos(ang1));
  let x2 = width/2 + (scalar * cos(ang2));
  
  let y1 = height/2 + (scalar * sin(ang1));
  let y2 = height/2 + (scalar * sin(ang2));
  
  fill(255);
  rect(width*0.5, height*0.5, 140, 140);

  fill(0, 102, 153);
  ellipse(x1, height*0.5 - 120, scalar, scalar);
  ellipse(x2, height*0.5 + 120, scalar, scalar);
  
  fill(255, 204, 0);
  ellipse(width*0.5 - 120, y1, scalar, scalar);
  ellipse(width*0.5 + 120, y2, scalar, scalar);

  angle1 += (sliderV*0.01);
  angle2 += 3; 
}



function draw1() {
  // hello darkness my old friend
theta+=0.01;
  // background(127*sin(theta*period));

  // let the seeds be filleth
  let c = color(seedColor);
  fill(red(c), green(c), blue(c), opacity);
  stroke(0, opacity);

  // absolute radius
  let r = radius * zoom;

  push();

  // go to the center of the sunflower
  translate(width/2, height/2);

  // rotate around the center while going outwards
  for(let i = 0; i < seeds; i++) {
    push();
    rotate(i * angle);
    // distance to the center of the sunflower
    let d = sqrt(i + 0.5) * zoom;
    ellipse(d, 0, r, r);
    pop();
  }

  pop();

}

// dynamically adjust the canvas to the window
function windowResized() {
  resizeCanvas(windowWidth, windowHeight);
}
