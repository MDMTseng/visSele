const CSG_api=require('@jscad/csg/api');
  
const {cylinder,cube} = CSG_api.primitives3d
const {color} = CSG_api.color
const {difference,union} =CSG_api.booleanOps
const {translate} =CSG_api.transformations
let fn=10;

const centercube = cube({center:true});
const centerCylinder = cylinder({d:1,center:true,fn});

function screw(headW=6,headD=5,threadW=4,threadD=12)
{
  return union(
    cylinder({d:1,fn}).scale([threadW,threadW,-threadD]),
    cylinder({d:1,fn}).scale([headW,headW,headD])
  );
}


function screw_section(headW=6,headD=5,threadW=4,threadD=12)
{
  return union(
    cylinder({d:1,fn:4}).scale([threadW,threadW,-threadD]),
    cylinder({d:1,fn:4}).scale([headW,headW,headD])
  );
}



const screw_4=screw(6,8,4,12);
const screw_3=screw(5,8,3,12);
const screw_3_blind=screw(3,8,3,12);

function FUYU_Linear_Module(modle_with_screw=false)
{
  let screwHole=screw_4;

  let two_screwHole=union(
    screwHole.translate([0,5,0]),
    screwHole.translate([0,25,0])
  );

  let union_diff=modle_with_screw?union:difference;
  let caseBtnThickness=7;


  let carriageScrewSet = [10+15-6,10+30-6].map(offset=>two_screwHole.translate([offset,0,0]));
  let carriage=union_diff(
    cube().scale([38,30,28]),
    union(carriageScrewSet).translate([0,0,30+3])
  ).translate([caseBtnThickness,0,6+6]);

  let carriage_offset=20;

  carriageScrewSet=carriageScrewSet.map(sc=>
    sc.translate([-25,-15,0]))//center it

  let baseScrewSet = [3.5,55,155,255].map(offset=>two_screwHole.translate([offset,0,5]));

  for(let i=0;i<7;i++)
  {
    baseScrewSet.push(screw_3_blind.translate([20+i*25,15,5]));
  }

  let baseSideScrew=screw_3_blind.rotateX(90).translate([13,0,7]);
  baseScrewSet.push(baseSideScrew);
  baseScrewSet.push(baseSideScrew.translate([0,30,0]));

  return {
    body:
      [
        union(
          color("gray",union_diff(
            union(
              cube().scale([375,30,12]),
              cube().scale([caseBtnThickness,30,42]),
            ),
            union(baseScrewSet).translate([0,0,0]))
          ),
          color("black",
            union(
              cube().scale([-32,28,28]).translate([0,1,42-28]),
              cube().scale([10,28,20]).translate([-32,1,-5])//for step motor's wires
            )
          ), 
        ),
        carriage.translate([carriage_offset,0,0])
      ],
    screwSet:{
      baseScrewSet,carriageScrewSet
    }
  }
}

let default_FUYU_Linear_Module=null;//FUYU_Linear_Module(true);

function FUYU_Linear_Module_Z_Holder()
{
  return difference(
    cube().scale([375,30,40]),
    union(default_FUYU_Linear_Module.body)
  );
}
let default_FUYU_Linear_Module_Z_Holder=null;//FUYU_Linear_Module_Z_Holder();


module.exports={
  FUYU_Linear_Module,
  FUYU_Linear_Module_Z_Holder,screw,screw_4,screw_3}