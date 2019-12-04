const {cylinder,cube} = require('@jscad/csg/api').primitives3d
const {color} = require('@jscad/csg/api').color
const {difference,union} = require('@jscad/csg/api').booleanOps
const {translate} = require('@jscad/csg/api').transformations

function ballJoint_parts(){
  let translateXYZ=[0,0,0]
  let ball=union(sphere({r: 1, fn: 36}))
  
  let ballCover=
      cube({size:2, center: true}).translate([0.4,0,0])
      //ball.scale(1.1).translate([0.2*scale,0,0])
  
  ballCover=difference(
          ballCover
          ,cube({size:0.2, center: true}).scale([13,1,13]).translate([-0.3,0,0])
          ,cube({size:0.2, center: true}).scale([13,13,1]).translate([-0.3,0,0])
          ,ball
          )
  
  let ballCoverRodD= 0.4;
  let ballRod = cylinder({size:1, center: true}).
          translate([0,0,0.5]).scale([ballCoverRodD,ballCoverRodD,4]).rotateY(-90)
  
  
  ballCover=difference(ballCover
  ,ballRod.rotateY(-90)
  ,ballRod.rotateY(-80)
  ,ballRod.rotateY(-70)
  ,ballRod.rotateY(90)
  ,ballRod.rotateY(80)
  ,ballRod.rotateY(70))
  
  ball=union(ball)
  
  let scale=0.2;
  return [
      color("red",ballCover).translate(translateXYZ).scale(scale)
      ,ball.scale(0.98).translate(translateXYZ).scale(scale)
      ];
}

function RotateJoint_parts(){


  let basePlate=cylinder({size: 1, center: true}).scale([1,1,1/3.01])
  basePlate=union(basePlate)
  let plate0= basePlate.translate([0,0,-2/3.0])
  let plate4= basePlate.translate([0,0,2/3.0])
  let plateXX=union(plate0,plate4,basePlate).rotateX(90)
  
  let plate1= basePlate.translate([0,0,-1/3.0])
  let plate3= basePlate.translate([0,0,1/3.0])
  let plateTB=union(plate1,plate3).rotateX(90)

  return [
      plateTB,color("red",plateXX)
      ];
}


function joint_middle(angle)
{
  return union([
      cylinder({size: 1, center: true}).scale([1,1,1/3]).translate([0,0,-1/3.0]),
      cylinder({size: 1, center: true}).scale([1,1,1/3]).rotateZ(angle),
      cylinder({size: 1, center: true}).scale([1,1,1/3]).translate([0,0,1/3.0])
      ]);
}


module.exports={ballJoint_parts,RotateJoint_parts,joint_middle}