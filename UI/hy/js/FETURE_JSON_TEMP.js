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
    "CENTER_XY":{"x":150,"y":150},
    "RADIUS":20,
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
function init_drawCanvas() {
    console.log("[drawCanvas.js][init]");
    console.log(RXMSG_temp1);
    
    
  }