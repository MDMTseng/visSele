// title      : OpenJSCAD.org Logo
// author     : Rene K. Mueller
// license    : MIT License
// revision   : 0.003
// tags       : Logo,Intersection,Sphere,Cube
// file       : logo.jscad
const {RotateJoint_parts,GoProJoint_parts,PolyRod} = require('./joint.js')
const {FUYU_Linear_Module,default_FUYU_Linear_Module,default_FUYU_Linear_Module_Z_Holder} = require('./mods.js')
//const stlDeSerializer = require('@jscad/stl-deserializer')

let fn=10;



function main (p) {


  let mountBox=cube().translate([0,-0.5,0]).scale([50,50,6]);
  mountBox=difference(mountBox,
    union(default_FUYU_Linear_Module.screwSet.carriageScrewSet)
    .scale([1,1,-2]).translate([20,0,0])
    );
  let showObj=union(
    [
      mountBox,
      new CSG()//Empty, just for 
    ])

   
  console.log(">>.Done");
  return showObj
}
