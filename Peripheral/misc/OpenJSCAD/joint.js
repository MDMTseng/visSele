const CSG_api=require('@jscad/csg/api');
  
const {cylinder,cube} = CSG_api.primitives3d
const {color} = CSG_api.color
const {difference,union} =CSG_api.booleanOps
const {translate} =CSG_api.transformations
let fn=10;
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
  let ballRod = cylinder({size:1, center: true,fn}).
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



function RotateJoint_parts(interval=0.3,d=1.5,plateshrinkRatio=0.99){

  let basePlate=cylinder({d: d, center: true,fn}).scale([1,1,interval*plateshrinkRatio])
  let plate0= basePlate.translate([0,0,-2*interval])
  let plate4= basePlate.translate([0,0,2*interval])
  let plateXX=union([plate0,basePlate,plate4].map(p=>p.rotateX(90)))
  
  let plate1= basePlate.translate([0,0,-1*interval])
  let plate3= basePlate.translate([0,0,1*interval])
  let plateTB=union([plate1,plate3].map(p=>p.rotateX(90)))

  return [
      plateTB,color("red",plateXX)
      ];
}


function HEX(radius=1,height=1){
  return PolyRod(polyCount=6,angleOffset=0).scale([radius,radius,height]);
}



function PolyRod(polyCount=5,angleOffset=0){
  return cylinder({center:true,fn:polyCount}).rotateZ(angleOffset).scale([1,1,1]);
}

function GoProScrew(length=5)
{
  let nutR=4.4*1.1;
  let nutH=4.8;
  let nutHole=HEX().scale([nutR,nutR,nutH]).translate([0,0,nutH/2]); 
  let screwRod=cylinder({d: 5, center: true,fn}).scale([1,1,length]).translate([0,0,-length/2]);

  return union(nutHole,screwRod)
}
function GoProJoint_plate(Leglenth=20)
{
  let thickness=2.93;
  let cylD=15;
  let fn=20
  let plate=cylinder({d: cylD, center: true,fn}).scale([1,1,thickness]);
  plate=union(plate,cube({size: [Leglenth,cylD,thickness], center:true}).translate([Leglenth/2,0,0])) 
  let screwRod=cylinder({d: 5, center: true,fn}).scale([1,1,20]);


  return difference(plate,screwRod)
}


function JointPlatOffset(plateModel,offsetXYZ=[1,0,0],idx=0,totC=3){
  let offset=offsetXYZ.map(ofs=>ofs*(idx-(totC-1)/2)    );
  return plateModel.translate(offset);
}


function GoProJoint_parts(legL=10){
  
  let gplat=GoProJoint_plate(legL);
  let plats = [0,1,2,3,4].map(idx=>JointPlatOffset(gplat,offsetXYZ=[0,0,3],idx,5));
  let objPlate=union([plats[1],plats[3]]);
  
  let lastPlate=union(plats[4],plats[4].translate([0,0,1]));
  
  lastPlate=difference(lastPlate, GoProScrew().translate([0,0,6.5]))
  let podPlate=union([plats[0],plats[2],lastPlate]);
  return [
    objPlate,podPlate
  ];
}




function joint_middle(angle)
{
  return union([
      cylinder({size: 1, center: true,fn}).scale([1,1,1/3]).translate([0,0,-1/3.0]),
      cylinder({size: 1, center: true,fn}).scale([1,1,1/3]).rotateZ(angle),
      cylinder({size: 1, center: true,fn}).scale([1,1,1/3]).translate([0,0,1/3.0])
      ]);
}


module.exports={ballJoint_parts,RotateJoint_parts,GoProJoint_parts,joint_middle,PolyRod}