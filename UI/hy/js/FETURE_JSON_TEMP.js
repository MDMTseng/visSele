var RXMSG_temp2=
{
 "labeledData": [{
   "area": 12198
  }],
 "reports": [{
   "detectedCircles": [{
     "matching_pts": 204,
     "s": 0.26877525448799133,
     "x": 400.5,
     "y": 289.5,
     "r": 36.477802276611328
    }, {
     "matching_pts": 716,
     "s": 0.2803465723991394,
     "x": 277.4781494140625,
     "y": 310.50238037109375,
     "r": 150.52250671386719
    }, {
     "matching_pts": 99,
     "s": 0.29968306422233582,
     "x": 298.531005859375,
     "y": 329.639892578125,
     "r": 43.3808708190918
    }],
   "detectedLines": [{
     "matching_pts": 106,
     "s": 0.37967479228973389,
     "x0": 284,
     "y0": 245,
     "x1": 421,
     "y1": 245
    }, {
     "matching_pts": 42,
     "s": 0.28663668036460876,
     "x0": 255,
     "y0": 271,
     "x1": 250,
     "y1": 312
    }, {
     "matching_pts": 66,
     "s": 0.4979780912399292,
     "x0": 290,
     "y0": 252,
     "x1": 424,
     "y1": 253
    }, {
     "matching_pts": 75,
     "s": 0.2722010612487793,
     "x0": 322,
     "y0": 375,
     "x1": 396,
     "y1": 354
    }, {
     "matching_pts": 85,
     "s": 0.31419235467910767,
     "x0": 321,
     "y0": 368,
     "x1": 405,
     "y1": 344
    }]
  }]
};
var RXMSG_temp1=
{
  "TYPE":"FetureSets",
  "SETS":[
  {
    "ID_NAME":"line-1","type":"line",
    "XY":[{"x":50,"y":50},{"x":100,"y":50}],
    "COLOR":"#0000ff",
    "STROKE_WIDTH":5
      
  },
  {
    "ID_NAME":"line-2","type":"line",
    "XY":[{"x":10,"y":100},{"x":100,"y":100}],
    "COLOR":"#0000ff",
    "STROKE_WIDTH":2
  },
  {
    "ID_NAME":"rect-1","type":"shape",
    "XY":[{"x":148,"y":120},{"x":200,"y":120},{"x":200,"y":200}],
    "COLOR":"#00ff00",
    "STROKE_WIDTH":2,
    "FILL":false,
    "CLOSE_PATH":true
  },
  {
    "ID_NAME":"rect-1","type":"shape",
    "XY":[{"x":58,"y":151},{"x":153,"y":52},{"x":265,"y":110},{"x":175,"y":285}],
    "COLOR":"#00ffff",
    "STROKE_WIDTH":3,
    "FILL":false,
    "CLOSE_PATH":false
  },
  {
    "ID_NAME":"rect-1","type":"rect",
    "XY":[{"x":150,"y":150},{"w":10,"h":10}],
    "COLOR":"#00aa00",
    "STROKE_WIDTH":2,
    "FILL":false,
    "CLOSE_PATH":true
  },
  {
    "ID_NAME":"rect-1","type":"arc",
    "XY":[{"x":150,"y":150}],
    "CENTER_XY":{"x":150,"y":150},
    "RADIUS":60,
    "DEGREE_START_END":{"start":0,"end":180},
    "CounterClockWise":false,
    "COLOR":"#ffff00",
    "STROKE_WIDTH":2,
    "FILL":false,
    "CLOSE_PATH":false
  }
  ]
}
;
var RXMSG_temp1_json = JSON.stringify(RXMSG_temp1);
var RXMSG_temp2_json = JSON.stringify(RXMSG_temp2);
function init_FETURE_JSON_TEMP() {
    console.log("[drawCanvas.js][init]");
    console.log(RXMSG_temp1);
    
    
  }