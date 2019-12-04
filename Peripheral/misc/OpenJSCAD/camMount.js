// title      : OpenJSCAD.org Logo
// author     : Rene K. Mueller
// license    : MIT License
// revision   : 0.003
// tags       : Logo,Intersection,Sphere,Cube
// file       : logo.jscad
const {RotateJoint_parts} = require('./joint.js')


function camModule(){
  
  let lens=cylinder({size:1, center: true}).scale([.3,0.3,0.5]).rotateY(90);
  let camBody=color("Red",cube({size:1, center: true}).scale([0.1,1,1]));
  let camHolder=cube({size:1, center: true});
  
  return union(
      color("blue",
          lens.translate([-0.3, 0,0])
      ),
      color("Red",camBody),
      
      camHolder.translate([(0.1+1)/2, 0,0]),
      RotateJoint_parts().map(part=>part.scale(0.3).translate([0.5, 0,1]))
      );
}


function multiCamHolder()
{
  let cholder=camModule().rotateY(-45).translate([5, 0,0]);
  let hArr=[]
  for(let i=0;i<5;i++)
  {
      hArr.push(cholder.rotateZ(360/5*i));
  }
  return hArr
  
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
  let holes=union(
      cylinder({r: 3/2, h: 100}).translate([drillHoleOffset, 4.5, 0])
      ,cylinder({r:3/2, h: 100}).translate([drillHoleOffset,20+4.5, 0])
      
      ,cylinder({r:3/2, h: 100}).translate([40-2,14.5, 0])
      
      ,cylinder({r: 3/2, h: 100}).translate([40-drillHoleOffset,(29-22)/2, 0])
      ,cylinder({r: 3/2, h: 100}).translate([40-drillHoleOffset,29-(29-22)/2, 0])
      );
  return difference(
      plate.rotateZ(0),
      holes
  ).translate([0,0,0])
}

function main () {
  //let obj = union(sampleCube()).translate([5, 0,0])
  return union(
      [camModule(),sheel1()]
      
  );
}
