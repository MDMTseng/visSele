
export type VEC2D={x:number,y:number}


export interface VEC2D_W_Distance extends VEC2D {
  dist:number
}

export interface SHAPE_ARC extends VEC2D {
  angleFrom:number,angleTo:number,r:number
}

export type SHAPE_LINE_seg={x1:number,y1:number,x2:number,y2:number};


export type SHAPE_LINE_ext=SHAPE_LINE_seg|{x:number,y:number,vx:number,vy:number};



export function distance_point_point(pt1:VEC2D, pt2:VEC2D):number
{
  return Math.hypot(pt1.x-pt2.x,pt1.y-pt2.y);
}
export function closestPointOnPoints(pt:VEC2D, pts:VEC2D[]):VEC2D|undefined
{
  let dist:number=Infinity;
  let cpt:VEC2D|undefined;
  pts.forEach(pt2 => {
    let cur_dist=distance_point_point(pt, pt2);
    if(cur_dist<dist){
      dist=cur_dist;
      cpt=pt2;
    }
  });
  return cpt;
}


export function distance_arc_point(arc:SHAPE_ARC, point:VEC2D):VEC2D_W_Distance
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

  let point1={x:arc.r*Math.cos(angleTo)+arc.x,y:arc.r*Math.sin(angleTo)+arc.y,dist:-1};
  let point2={x:arc.r*Math.cos(angleFrom)+arc.x,y:arc.r*Math.sin(angleFrom)+arc.y,dist:-1};

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

export function closestPointOnLine(line:SHAPE_LINE_ext|any, point:VEC2D):VEC2D_W_Distance
{

  let line_:{x:number,y:number,vx:number,vy:number} ;
  
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

  return {
    x:line_.x+dist*line_.vx,
    y:line_.y+dist*line_.vy,
    dist
  };
}



export function  intersectPoint( p1:VEC2D, p2:VEC2D, p3:VEC2D, p4:VEC2D):VEC2D
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

export function vecXY_addin(v1:VEC2D,v2:VEC2D)
{
  v1.x+=v2.x;
  v1.y+=v2.y;
  return v1;
}
export function vecXY_mulin(v1:VEC2D,realNum:number)
{
  v1.x*=realNum;
  v1.y*=realNum;
  return v1;
}

export function distance_line_point(line:SHAPE_LINE_ext|any, point:VEC2D):VEC2D_W_Distance
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


export function threePointToArc(p1:VEC2D, p2:VEC2D, p3:VEC2D)
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


export function LineCentralNormal(line:{pt1:VEC2D,pt2:VEC2D})
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


export function PtRotate2d_sc(pt:VEC2D, sin_v:number,cos_v:number,flipF=1) {
  if(flipF>=0)flipF=1;
  else flipF=-1;
  return {
    x: pt.x * cos_v -flipF* pt.y * sin_v,
    y: pt.x * sin_v +flipF* pt.y * cos_v
  };
}

export function PtRotate2d(pt:VEC2D, theta:number,flipF=1) {
  let sin_v = Math.sin(theta);
  let cos_v = Math.cos(theta);
  return PtRotate2d_sc(pt,sin_v,cos_v,flipF);
}




// Circle CircleFitByHyper (Data& data)
export function PointsFitCircle(pts:VEC2D[],w:number[])
{
    let i,iter,IterMAX=99;

    let Xi,Yi,Zi;
    let Mz,Mxy,Mxx,Myy,Mxz,Myz,Mzz,Cov_xy,Var_z;
    let A0,A1,A2,A22;
    let Dy,xnew,x,ynew,y;
    let DET,Xcenter,Ycenter;

    let ptsMean={x:0,y:0,w:0};
    pts.forEach((pt,idx)=>{
      ptsMean.x+=pt.x;
      ptsMean.y+=pt.y;
      ptsMean.w+=w[idx]
    })
    ptsMean.x/=ptsMean.w;
    ptsMean.y/=ptsMean.w;
    //let ptsMean=vecXY_mulin(,1.0/pts.length);


    
    // Circle circle;
//     computing moments

    Mxx=Myy=Mxy=Mxz=Myz=Mzz=0.;
    let sumW=0;
    for (i=0; i<pts.length; i++)
    {
        Xi = pts[i].x - ptsMean.x;   //  centered x-coordinates
        Yi = pts[i].y - ptsMean.y;   //  centered y-coordinates
        Zi = Xi*Xi + Yi*Yi;

        sumW+=w[i];
        Mxy += Xi*Yi*w[i];
        Mxx += Xi*Xi*w[i];
        Myy += Yi*Yi*w[i];
        Mxz += Xi*Zi*w[i];
        Myz += Yi*Zi*w[i];
        Mzz += Zi*Zi*w[i];
    }
    Mxx /= sumW;
    Myy /= sumW;
    Mxy /= sumW;
    Mxz /= sumW;
    Myz /= sumW;
    Mzz /= sumW;

//    computing the coefficients of the characteristic polynomial

    Mz = Mxx + Myy;
    Cov_xy = Mxx*Myy - Mxy*Mxy;
    Var_z = Mzz - Mz*Mz;

    A2 = 4*Cov_xy - 3*Mz*Mz - Mzz;
    A1 = Var_z*Mz + 4*Cov_xy*Mz - Mxz*Mxz - Myz*Myz;
    A0 = Mxz*(Mxz*Myy - Myz*Mxy) + Myz*(Myz*Mxx - Mxz*Mxy) - Var_z*Cov_xy;
    A22 = A2 + A2;

//    finding the root of the characteristic polynomial
//    using Newton's method starting at x=0
//     (it is guaranteed to converge to the right root)

	for (x=0.,y=A0,iter=0; iter<IterMAX; iter++)  // usually, 4-6 iterations are enough
    {
        Dy = A1 + x*(A22 + 16.*x*x);
        xnew = x - y/Dy;
        if ((xnew == x)||(!isFinite(xnew))) break;
        ynew = A0 + xnew*(A1 + xnew*(A2 + 4*xnew*xnew));
        if (Math.abs(ynew)>=Math.abs(y))  break;
        x = xnew;  y = ynew;
    }

//    computing paramters of the fitting circle

    DET = x*x - x*Mz + Cov_xy;
    Xcenter = (Mxz*(Myy - x) - Myz*Mxy)/DET/2;
    Ycenter = (Myz*(Mxx - x) - Mxz*Mxy)/DET/2;
//       assembling the output
    return {
      x:Xcenter + ptsMean.x,
      y:Ycenter + ptsMean.y,
      r:Math.sqrt(Xcenter*Xcenter + Ycenter*Ycenter + Mz - x - x)
    }
    
}
