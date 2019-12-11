function main () {
  let ALL= union(
      // texts().translate([100, 100, 0]),
    //   XYZ()
      U_Connector().translate([0, -3,0])
      ,U_Connector().translate([0, 29,0])
      ,sheel1()
      ,sheel1().translate([0, 3.15, 0]).rotateX(90)
      ,sheel1().translate([0, 3,-29-3.15]).rotateX(90)
    //   ,cameraMountHole()
      // sheel1().translate([0, 0, 3]).rotateX(90)
      );
  return union(
      ALL
  ).translate([0, 0, 0]).scale(1).rotateY(-90);
}
function U_Connector(){
    let plate=union(
        
        cube({size:1, center: false}).scale([20,3.1,3.1])
        
        )
    return union(
        plate.rotateZ(0)
    ).translate([0,0,0])
  }

function cameraMountHole(){
    let plate=union(
        // cylinder({r: 1.5, h: 1}).translate([0, 0, 0]),
        cube({size:1, center: false}).scale([29,29,3])
    );
    let holes2=union(
        cylinder({r: 29/2, h: 100}).translate([29/2,29/2, 0])
    );
    return difference(
        plate,holes2
    ).rotateY(-90).translate([0,0,3])
}
 
function csgFromSegments (segments) {
  let output = [];
  segments.forEach(segment => output.push(
    rectangular_extrude(segment, { w:0.1, h:1 })
  ));
  return union(output);
}
function XYZ(){
    let Xaxis=cube({size:0.1, center: false}).scale([1000,1,1]);
    let Yaxis=cube({size:0.1, center: false}).scale([1,1000,1]);
    let Zaxis=cube({size:0.1, center: false}).scale([1,1,1000]);
    
    return union(
        // Xaxis.rotateX(90),
        // Yaxis.rotateY(180),
        // Zaxis.rotateZ(0),
        Xaxis.setColor([255,0,0]),
        Yaxis.setColor([0,255,0]),
        Zaxis.setColor([0,0,255])
        
  ).translate([0, 0, 0]).scale(1);
}


function sheel1(){
    let plate=union(
        // cylinder({r: 1.5, h: 1}).translate([0, 0, 0]),
        cube({size:1, center: false}).scale([40,29,3])
        // .setColor([12,12,0])
        )
    // let faceRight=cube({size:1, center: true}).scale([50,44,1]);
    // let faceBack=cube({size:1, center: true}).scale([1,13,1]);
    let M3=3.2;
    let drillHoleOffset=((40-36)/2);
    let holes=union(
         cube({size:1, center: false}).scale([30,1,3]).translate([40/3, (29/2)-0.5, 0]),
         cylinder({r: M3/2, h: 100}).translate([drillHoleOffset, 4.5, 0])
        ,cylinder({r:M3/2, h: 100}).translate([drillHoleOffset,20+4.5, 0])
        
        ,cylinder({r:M3/2, h: 100}).translate([40-2,14.5, 0])
        ,cylinder({r:M3/2, h: 100}).translate([   2,14.5, 0])

        ,cylinder({r: M3/2, h: 100}).translate([40-drillHoleOffset,(29-22)/2, 0])
        ,cylinder({r: M3/2, h: 100}).translate([40-drillHoleOffset,29-(29-22)/2, 0])
        );
    return difference(
        plate.rotateZ(0),
        holes
    ).translate([0,0,0])
}