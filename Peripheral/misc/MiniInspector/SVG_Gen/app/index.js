/**
 * Application entry point
 */

// Load application styles
import 'styles/index.scss';
import paper from 'paper';
import SVG from "svg.js";
// ================================
// START YOUR APP HERE
// ================================
var draw = SVG('drawing').size(300, 130)

var rect = draw.rect(100, 100).move(120, 20).attr({
  'fill-opacity': 0,
  stroke: '#000',
  'stroke-width': 1,
  "vector-effect":"non-scaling-stroke"
  })
var circle =draw.circle(100).move(20, 20);



console.log(paper.view);




{

    console.log('PaperApplication::init');

    let w = window.innerWidth;
    let h = window.innerHeight;

    const canvas = document.createElement('canvas');
    canvas.id = 'paper-canvas';
    document.body.appendChild(canvas);

    paper.setup(canvas);

    var point1 = new paper.Point(0, 0);
    var point2 = new paper.Point(w, 0);
    var point3 = new paper.Point(w, h);
    var point4 = new paper.Point(0, h);

    var path1 = new paper.Path(point1, point3);
    var path2 = new paper.Path(point2, point4);

    var originals = new paper.Group({ insert: false }); // Don't insert in DOM.
    path1.strokeColor = 'black';
    path2.strokeColor = 'black';
    console.log(path1);
    //console.log(paper.project.exportSVG({asString:true}));
}
/*
// Create a Paper.js Path to draw a line into it:
var path = new Path();
// Give the stroke a color
path.strokeColor = 'black';
// Create a starting point.
// The params are x , y coordinates respectively.
var start = new Point(100, 100);
// Move to start and draw a line from there
path.moveTo(start);
// Note the plus operator on Point objects.
// PaperScript does that for us, and much more!
path.lineTo(start + [ 100, -50 ]);*/