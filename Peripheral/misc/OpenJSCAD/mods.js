const CSG_api=require('@jscad/csg/api');
  
const {cylinder,cube} = CSG_api.primitives3d
const {color} = CSG_api.color
const {difference,union} =CSG_api.booleanOps
const {translate} =CSG_api.transformations
let fn=10;

const centercube = cube({center:true});
const centerCylinder = cylinder({d:1,center:true,fn});
function FUYU_Linear_Module()
{
  let screwHole=union(
    cylinder({d:1,fn}).scale([4,4,-12]),
    cylinder({d:1,fn}).scale([6,6,5]).translate([0,0,0])
  );

  let two_screwHole=union(
    screwHole.translate([0,5,0]),
    screwHole.translate([0,25,0])
  );

  let caseBtnThickness=7;


  let carriageScrewSet = [10,10+15,10+30].map(offset=>two_screwHole.translate([offset,0,0]));
  let carriage=difference(
    cube().scale([50,30,28]),
    union(carriageScrewSet).translate([0,0,30])
  ).translate([caseBtnThickness,0,6+6]);

  let carriage_offset=20;

  carriageScrewSet=carriageScrewSet.map(sc=>
    sc.translate([-25,-15,0]))//center it

  let baseScrewSet = [3.5,55,155,255].map(offset=>two_screwHole.translate([offset,0,0]));


  return {
    body:
      [union(
        color("gray",difference(
          union(
            cube().scale([375,30,10]),
            cube().scale([caseBtnThickness,30,42]),
          ),union(baseScrewSet).translate([0,0,5]))),
        color("black",cube().scale([-32,28,28]).translate([0,1,42-28])),
      ),
      carriage.translate([carriage_offset,0,0])],
    screwSet:{
      baseScrewSet,carriageScrewSet
    }
  }
}

let default_FUYU_Linear_Module=FUYU_Linear_Module();

function FUYU_Linear_Module_Z_Holder()
{
  return difference(
    cube().scale([375,30,40]),
    union(default_FUYU_Linear_Module.body)
  );
}
let default_FUYU_Linear_Module_Z_Holder=FUYU_Linear_Module_Z_Holder();


module.exports={
  default_FUYU_Linear_Module,FUYU_Linear_Module,
  default_FUYU_Linear_Module_Z_Holder,FUYU_Linear_Module_Z_Holder}