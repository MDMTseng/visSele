// title      : OpenJSCAD.org Logo
// author     : Rene K. Mueller
// license    : MIT License
// revision   : 0.003
// tags       : Logo,Intersection,Sphere,Cube
// file       : logo.jscad
const {RotateJoint_parts,GoProJoint_parts,PolyRod} = require('./joint.js')
//const stlDeSerializer = require('@jscad/stl-deserializer')

let fn=10;
function camModule(){
  
  let lens=cylinder({size:1, center: true,fn}).scale([.3,0.3,0.5]).rotateY(90);
  let camBody=color("Red",cube({size:1, center: true}).scale([0.1,1,1]));
  let camHolder=cube({size:1, center: true});
  let camera = union( 
    color("blue",lens.translate([-0.3, 0,0]))
    ,color("Red",camBody)
    ,camHolder.translate([(0.1+1)/2, 0,0]))
  return union(
    camera.scale(10)
    ,GoProJoint_parts().map(part=>part.scale(0.3).translate([5, 0,10]))
    );
}


function GoProMount_Screw(mountLen=12,stickLen=18,stickThickness=7,stickWedgedRate=0.9)
{
  console.log(">>GoProMount_Screw call GoProJoint_parts");
  let GPJoint=GoProJoint_parts(mountLen);
  let GPJ2=(GPJoint[1]).rotateZ(180);
  let endPlate=cube({center:true}).scale([5,15,16]).translate([-mountLen-2,0,0.5]);
  let rod_len=stickLen;
  let rod=cylinder({r1: 1,r2:stickWedgedRate,center:true,fn:4})
  .rotateZ(-45).rotateY(-90).scale([1,1/Math.SQRT2,1/Math.SQRT2]).scale([rod_len,stickThickness,stickThickness]).translate([-mountLen-rod_len/2,0,0]);
  return union(GPJ2,endPlate,rod)
}



function CamHolder(legnth=10)
{
  let GPJoint=GoProJoint_parts(legnth);

  //let GPJ2=(GPJoint[1]).rotateZ(180);

  return union(
    union(GPJoint[0]).rotateX(90).rotateY(90).translate([0,0,legnth]),
    sheel1()
    )
}


function multiCamHolder()
{
  let cholder = CamHolder().rotateY(-45).translate([50, 0,0])
  let hArr=[]

  //PolyRod(5,-Math.PI*2/5/4).scale([30,30,30])
  for(let i=0;i<5;i++)
  {
      hArr.push((cholder).rotateZ(360/5*i)); 
  }
  return (hArr)
  
} 

 
function HolderMountStick()
{ 
  let screw=GoProMount_Screw(mountLen=12,stickLen=18,stickThickness=7,stickWedgedRate=0.9).translate([35,0,0]).rotateX(90).rotateY(0);
   
  // screw.polygons.forEach((vex)=>{
  //   vex.vertices.forEach((v)=>{
  //     if(v.pos.dd===undefined)
  //     {
  //       v.pos.x*=0.9;
  //       v.pos.y*=0.9;
  //       v.pos.z*=0.9;
  //       v.pos._x*=0.9;
  //       v.pos._y*=0.9;
  //       v.pos._z*=0.9;
  //     }
  //     v.pos.dd=5;
  //   })
  // });
  //console.log(screw.polygons)
  return screw;
} 

function HolderBase()
{
  let screw=HolderMountStick();
 
  let screwBase=GoProJoint_parts(20)[0].rotateY(90).translate([0,0,20]);
  screwBase=union(
    screwBase.translate([0,0,0])
  ).rotateZ(360/5/4);


  let mountBase = difference(
    union(
      PolyRod(5,-360/5/2).scale([25,25,20]),
      screwBase), 
    union([0,1,2,3,4].map(idx=>screw.rotateZ(360*idx/5))))

  return union(
    mountBase.rotateZ(-360/5/4)
    )
} 

function sheel1(){
  let plate=union(
      // cylinder({r: 1.5, h: 1}).translate([0, 0, 0]),
      cube({size:1, center: false}).scale([40,29,3])
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
  ).translate([-40/2,-29/2,-3/2])
}
function sShell(){
  let plate=union(
      // cylinder({r: 1.5, h: 1}).translate([0, 0, 0]),
      cube({size:1, center: false}).scale([20,29,3])
      // .setColor([12,12,0])
      )
  // let faceRight=cube({size:1, center: true}).scale([50,44,1]);
  // let faceBack=cube({size:1, center: true}).scale([1,13,1]);
  let drillHoleOffset=((40-36)/2);
  let hole=cylinder({r: 3/2, h: 100,fn});
  let holes=union(
    hole.translate([drillHoleOffset, 4.5, 0])
      ,hole.translate([drillHoleOffset,20+4.5, 0])
      
      );
  return difference(
      plate.rotateZ(0),
      holes
  ).translate([-20/2,-29/2,-3/2])
}

function main (p) {
  //let obj = union(sampleCube()).translate([5, 0,0])
  // const rawData = fs.readFileSync('/Users/mdm/visSele/Peripheral/misc/OpenJSCAD/simpleCube.stl')
  // const csgData = stlDeSerializer.deserialize(rawData, undefined, {output: 'csg'})

  let showObj=union(
    [
      HolderBase(),
      //GoProMount_Screw(),
      //HolderMountStick(),
      //multiCamHolder()
      //cylinder({fn:5}),
      new CSG()//Empty, just for 
    ])
  console.log(">>.Done");
  return showObj;
}
