
// Returns Infinity rather than throwing when either point is missing.
//
// Callers compare the result against a running minimum, so Infinity means "not
// this one" and the search carries on -- which is what every caller wanted. The
// unguarded version turned one unresolved aux reference into a TypeError on
// mouse move, and the error boundary then replaced the editor.
//
// A guard here is not a substitute for the caller checking: it makes a missing
// point unable to CRASH, not able to be correct. The callers still skip.
export function distance_point_point(pt1, pt2)
{
  if (!pt1 || !pt2) return Number.POSITIVE_INFINITY;
  return Math.hypot(pt1.x-pt2.x,pt1.y-pt2.y);
}
export function closestPointOnPoints(pt, pts)
{
  let dist;
  let cpt;
  pts.forEach(pt2 => {
    if(dist===undefined){
      dist=distance_point_point(pt, pt2);
      cpt=pt2;
      return;
    }
    let cur_dist=distance_point_point(pt, pt2);
    if(cur_dist<dist){
      dist=cur_dist;
      cpt=pt2;
    }
    
  });
  return cpt;
}


export function distance_arc_point(arc, point)
{
  //arc={x,y,r,angleFrom,angleTo}
  let arc2p_angle = Math.atan2(point.y-arc.y,point.x-arc.x);
  let arc2p_angle_BK = arc2p_angle;
  let angleFrom=arc.angleFrom;
  let angleTo=arc.angleTo;


  arc2p_angle-=angleFrom;
  angleTo-=angleFrom;
  arc2p_angle=arc2p_angle%(2*Math.PI);
  angleTo=angleTo%(2*Math.PI);
  if(arc2p_angle<0)arc2p_angle+=2*Math.PI;
  if(angleTo<0)angleTo+=2*Math.PI;
  angleFrom=0;

  if(arc2p_angle<angleTo)//Check is arc2p_angle within angleFrom to angleTo
  {
    arc2p_angle = arc2p_angle_BK;
    return {
      x: (arc.r*Math.cos(arc2p_angle))+arc.x,
      y: (arc.r*Math.sin(arc2p_angle))+arc.y,
      dist:Math.abs(Math.hypot(point.x-arc.x,point.y-arc.y)-arc.r)
    };
  }

  angleFrom=arc.angleFrom;
  angleTo=arc.angleTo;

  let point1={x:arc.r*Math.cos(angleTo)+arc.x,y:arc.r*Math.sin(angleTo)+arc.y};
  let point2={x:arc.r*Math.cos(angleFrom)+arc.x,y:arc.r*Math.sin(angleFrom)+arc.y};

  let dist1=Math.hypot(point.x-point1.x,point.y-point1.y);
  let dist2=Math.hypot(point.x-point2.x,point.y-point2.y);

  if(dist1<dist2)
  {
    point1.dist = dist1;
    return point1;
  }
  point2.dist = dist2;
  return point2;
}

export function closestPointOnLine(line, point)
{

  let line_ ;
  
  if(line.vx === undefined)
  {
    line_= {
      x:(line.x1+line.x2)/2,y:(line.y1+line.y2)/2,
      vx:line.x2-line.x1,vy:line.y2-line.y1
    };
  }
  else
  {
    line_= {
      x:line.cx,y:line.cy,
      vx:line.vx,vy:line.vy
    };
  }

  let normalizeFactor = Math.hypot(line_.vx,line_.vy);
  line_.vx/=normalizeFactor;
  line_.vy/=normalizeFactor;

  let point_={x:point.x,y:point.y};

  point_.x-=line_.x;
  point_.y-=line_.y;

  let dist = line_.vx * point_.x + line_.vy * point_.y;
  line_.x+=dist*line_.vx;
  line_.y+=dist*line_.vy;
  line_.dist =dist;

  return line_;
}



export function  intersectPoint( p1, p2, p3, p4)
{
  let intersec={x:0,y:0};
  let denominator;

  let V1 = (p1.x-p2.x);
  let V2 = (p3.x-p4.x);
  let V3 = (p1.y-p2.y);
  let V4 = (p3.y-p4.y);

  denominator = V1* V4 - V3* V2;

  let V12 = (p1.x*p2.y-p1.y*p2.x);
  let V34 = (p3.x*p4.y-p3.y*p4.x);
  intersec.x=( V12 * V2 - V1 * V34 )/denominator;
  intersec.y=( V12 * V4 - V3 * V34 )/denominator;

  return intersec;
}

export function vecXY_addin(v1,v2)
{
  v1.x+=v2.x;
  v1.y+=v2.y;
  return v1;
}
export function vecXY_mulin(v1,realNum)
{
  v1.x*=realNum;
  v1.y*=realNum;
  return v1;
}

export function distance_line_point(line, point)
{
  //arc={x1,y1,x2,y2}

  let closestPoint = closestPointOnLine(line, point);

  let ratio;

  if(Math.abs(line.x2-line.x1)>Math.abs(line.y2-line.y1))
  {
    ratio = (closestPoint.x-line.x1)/(line.x2-line.x1);
  }
  else
  {
    ratio = (closestPoint.y-line.y1)/(line.y2-line.y1);
  }

  if(ratio>1)
  {
    let dist2 = Math.hypot(point.x-line.x2,point.y-line.y2);
    return {x: line.x2,y: line.y2,dist:dist2};
  }

  if(ratio>0)
  {
    let dist3 = Math.hypot(point.x-closestPoint.x,point.y-closestPoint.y);
    closestPoint.dist = dist3;
    return closestPoint;
  }

  let dist1 = Math.hypot(point.x-line.x1,point.y-line.y1);
  return {x: line.x1,y: line.y1,dist:dist1};  



}


// The arc's actual sweep, pt2 included.
//
// Three points do not define an arc without pt2: pt1 and pt3 alone leave two
// arcs, the short one and its complement, and pt2 is the only thing that says
// which. Every seeder in the UI used to compute `span = a3 - a1` mod 2pi and
// never look at pt2, so an arc drawn "the other way round" got its COMPLEMENT
// -- a 30 degree arc measured as 330. Caliper width is seeded as arcLen/count,
// so those boxes came out 11x too wide, each one averaging across the whole
// feature instead of a slice of it.
//
// This is a port of the core's convert3Pts2ArcData
// (FeatureManager_sig360_circle_line.cpp:185): always sweep CCW from a0 to a1
// in the order that passes THROUGH pt2. The drawing code already did this
// correctly, which is exactly why the boxes disagreed with the width that
// placed them.
export function arcSweep(p1, p2, p3) {
  const a = threePointToArc(p1, p2, p3);
  if (!a || !(a.r > 0)) return { ...(a || { x: 0, y: 0, r: 0 }), a0: 0, a1: 0, span: 0, length: 0 };
  const TAU = 2 * Math.PI;
  const aP1 = Math.atan2(p1.y - a.y, p1.x - a.x);
  const aP2 = Math.atan2(p2.y - a.y, p2.x - a.x);
  const aP3 = Math.atan2(p3.y - a.y, p3.x - a.x);
  let angle21 = aP2 - aP1; if (angle21 < 0) angle21 += TAU;
  let angle31 = aP3 - aP1; if (angle31 < 0) angle31 += TAU;
  let a0, a1;
  if (angle31 > angle21) { a0 = aP1; a1 = aP3; }   // pt2 lies between: pt1 -> pt3
  else                   { a0 = aP3; a1 = aP1; }   // pt2 outside: the other way
  if (a1 < a0) a1 += TAU;
  const span = a1 - a0;
  return { x: a.x, y: a.y, r: a.r, a0, a1, span, length: a.r * span };
}

export function threePointToArc(p1, p2, p3)
{
  let offset =p2.x*p2.x +p2.y*p2.y;

  let bc =   (p1.x*p1.x +p1.y*p1.y - offset )/2.0;
  let cd =   (offset - p3.x*p3.x -p3.y*p3.y )/2.0;

  let det =  (p1.x - p2.x) * (p2.y - p3.y) - (p2.x - p3.x)* (p1.y - p2.y); 

  //if (Math.abs(det) < TOL) { throw new IllegalArgumentException("Yeah, lazy."); }

  //let idet = 1/det;
  //console.log(p2.y);
  let centerx =  (bc * (p2.y - p3.y) - cd * (p1.y - p2.y)) / det;
  let centery =  (cd * (p1.x - p2.x) - bc * (p2.x - p3.x)) / det;
  let radius = 
      Math.sqrt( Math.pow(p2.x - centerx,2) + Math.pow(p2.y-centery,2));

  let theta1 = Math.atan2(p1.y-centery,p1.x-centerx);
  let thetaM = Math.atan2(p2.y-centery,p2.x-centerx);
  let theta2 = Math.atan2(p3.y-centery,p3.x-centerx);
  let theta1M = (theta1 - thetaM+4*Math.PI)%(2*Math.PI);
  let theta12 = (theta1 - theta2+4*Math.PI)%(2*Math.PI);
  if(theta12>theta1M)
  {
    theta1M = theta1;
    theta1 = theta2;
    theta2 = theta1M;
  }

  return {x:centerx,y:centery,r:radius,thetaS:theta1,thetaE:theta2};
}


export function LineCentralNormal(line)
{
  let dx=(line.pt2.y-line.pt1.y);
  let dy=(line.pt2.x-line.pt1.x);
  let dist = Math.hypot(dx,dy);
  return {
    x:(line.pt1.x+line.pt2.x)/2,
    y:(line.pt1.y+line.pt2.y)/2,
    vx:dx/dist,
    vy:-dy/dist,
  };
}


export function PtRotate2d_sc(pt, sin_v,cos_v,flipF=1) {
  if(flipF>=0)flipF=1;
  else flipF=-1;
  return {
    x: pt.x * cos_v -flipF* pt.y * sin_v,
    y: pt.x * sin_v +flipF* pt.y * cos_v
  };
}

export function PtRotate2d(pt, theta,flipF=1) {
  let sin_v = Math.sin(theta);
  let cos_v = Math.cos(theta);
  return PtRotate2d_sc(pt,sin_v,cos_v,flipF);
}



