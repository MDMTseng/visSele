// title      : OpenJSCAD.org Logo
// author     : Rene K. Mueller
// license    : MIT License
// revision   : 0.003
// tags       : Logo,Intersection,Sphere,Cube
// file       : logo.jscad
const {RotateJoint_parts,GoProJoint_parts,PolyRod} = require('./joint.js')
const {FUYU_Linear_Module,
  screw,
  screw_3,
  screw_4} = require('./mods.js')
//const stlDeSerializer = require('@jscad/stl-deserializer')

let fn=10;




function sShell(){
  let plate=union(
      // cylinder({r: 1.5, h: 1}).translate([0, 0, 0]),
      cube({size:1, center: false}).scale([44,29,3])
      // .setColor([12,12,0])
      )
  // let faceRight=cube({size:1, center: true}).scale([50,44,1]);
  // let faceBack=cube({size:1, center: true}).scale([1,13,1]);
  let drillHoleOffset=((40-36)/2);
  let hole=cylinder({r: 3/2, h: 100,fn});
  let holes=union(
       hole.translate([drillHoleOffset, 4.5, 0])
      ,hole.translate([drillHoleOffset,20+4.5, 0])
      
      ,hole.translate([40-2,14.5, 0])
      
      ,hole.translate([40-drillHoleOffset,(29-22)/2, 0])
      ,hole.translate([40-drillHoleOffset,29-(29-22)/2, 0])
      
      );
  return difference(
      plate.rotateZ(0),
      holes
  ).translate([-20/2,-29/2,-3/2])
}

function Isosceles_triangle()
{
  return linear_extrude({height: 1}, polygon([ [0,0],[1,0],[0,1] ]));
}

function linearModuleFixture(){

  let ground_fix_screw=union(
    screw_4.rotateY(90).translate([-25,-30,3]),
    screw_4.rotateY(90).translate([-25,-30,25]))
  ground_fix_screw=union(ground_fix_screw,ground_fix_screw.mirroredY());
 
  
  let linearModuleFixture=difference(
    union(
      cube().scale([93,40,20]).translate([-32,-20,-4]),
      cube().scale([23,40,30]).translate([-32,-20,-4]),
      cube().translate([0,-0.5,0]).scale([4,80,40]).translate([-32,0,-4]),
      //cube().translate([0,-0.5,0]).scale([20,80,4]).translate([-32,0,10]) 
      Isosceles_triangle().rotateZ(-45+180).scale([50,50,4]).translate([7,0,10])
    ), 
    //cube().scale([80,40,20]).translate([20,-20,14]), 
    cube().scale([90,30,30]).translate([0,-15,0]), 
    union(
      cube().scale([32,30,12]).translate([-36,-15,-4]), 

      screw_3.rotateX(-90).translate([-17,-14,7]), 
      screw_3.rotateX(90).translate([-17,14,7]), 

      screw_4.rotateX(-90).translate([-10,-18,3]), 
      screw_4.rotateX(90).translate([-10,18,3]), 
      screw_4.rotateX(-90).translate([-25,-18,3]), 
      screw_4.rotateX(90).translate([-25,18,3]),
      ground_fix_screw
    ),
 

    default_FUYU_Linear_Module.body[0].translate([0,-15,0]).scale(1)
    );
  return linearModuleFixture;
}


function joint_lock(scaleF=0.9,pullRatio=0.6,shapeXY)
{

  var spiral = 
  shapeXY.solidFromSlices({
  numslices: 2,
  callback: function(t, slice) {
    let sc=t==1?1:scaleF;
      return this.
      translate([0 , 0, t]).scale([sc,1,1]).
      translate([0 , (1-t)*pullRatio, 0]);
  }
  }).rotateZ(-90);
  
  return union(spiral,cube().translate([0,-0.5,0]).scale([1,2,1]));
}

function dove_lock(scaleF=0.9,slop_ratio=1,pullRatio=0.1)
{
  let slotwidth=4;
  var hex = CSG.Polygon.createFromPoints([
    // [0,0, 0],
    // [3,0, 0],
    // [2,1, 0],
    // [1,1, 0],
    [1,0, 0],
    [3,0, 0],
    [3+slop_ratio,1, 0],
    [1-slop_ratio,1, 0]
  ]).setColor(
  [0, 0.8, 0]
  ).translate([-2 , 0, 0]);

  return joint_lock(scaleF,pullRatio,hex);
}



function T_lock(scaleF=0.9)
{
  
  let slotwidth=4;
  var hex = CSG.Polygon.createFromPoints([
    // [0,0, 0],
    // [3,0, 0],
    // [2,1, 0],
    // [1,1, 0],
    [1,0, 0],
    [2,0, 0],

    [2,1, 0],
    [3,1, 0],
    [3,2, 0],
    [0,2, 0],
    [0,1, 0],
    [1,1, 0]
  ]).translate([-1.5 , 0, 0]).scale([1,1,1]);

  return joint_lock(1,hex);
}


function dove_lock_Plate(scaleF=0.9,slop_ratio=0.3,size=5)
{
  let dL = dove_lock(scaleF,slop_ratio).translate([0,0,-0.5]).scale([1,1,size]);

  let plate1 = union(
    cube().translate([-1,-0.50,-0.5]).scale([1,size,size]),
    dL
    );
  return plate1;
}



function dove_lock_Blocks(scaleF=0.9,slop_ratio=0.3,pullRatio=0.2,block_union,block_diff)
{ 
  let blockH=20;
  let dL = dove_lock(scaleF,slop_ratio,pullRatio).translate([0,0,-0.5]).scale([4,4,blockH]);
  
  let doveLockOffset=20;

  let pos_dL=dL;//.translate([0,doveLockOffset/2,0]);
  let neg_dL=pos_dL.rotateZ(180);
  let neg_dL_diff=neg_dL.scale(1.03);
  let pos_dL_diff=pos_dL.scale(1.03);
 
  let block1=cube().translate([-1,-0.5,-0.5]).scale([10,20,blockH]);
  block1=union(block1,pos_dL)

    
  let block2=cube().translate([0,-0.5,-0.5]).scale([10,20,blockH])
  block2=difference(block2,pos_dL_diff)
  return [block1, block2];
}



function main (p) {


  console.log("Start");
  // let mountBox=cube().translate([0,-0.5,0]).scale([50,50,6]);
  // mountBox=difference(mountBox,
  //   union(FUYU_Linear_Module().screwSet.carriageScrewSet)
  //   .scale([1,1,-2]).translate([20,0,0])
  //   );
  

    
       
  let dove_lock_X=dove_lock_Blocks(1,0.1);
  let showObj=union(
    [
      // mountBox,  
      // sShell().translate([0,0,-30]),
      //dove_lock(),  
      dove_lock_X[0],
      dove_lock_X[1].translate([-30,0,0]),
      //union(default_FUYU_Linear_Module.body),
      //linearModuleFixture(),
      //union(default_FUYU_Linear_Module.body),
      new CSG()//Empty, just for 
    ]) 

   
  console.log(">>.Done");
  return [
    showObj,
    ]
}
