#include <stdio.h>
#include <math.h>

float* vecAdd(float* vr,float* v1,float* v2,int dim)
{
  for(int i=0;i<dim;i++)
  {
    vr[i]=v1[i]+v2[i];
  }
  return vr;
}
float* vecSub(float* vr,float* v1,float* v2,int dim)
{
  for(int i=0;i<dim;i++)
  {
    vr[i]=v1[i]-v2[i];
  }
  return vr;
}



float* vecLerp(float* vr,float* v0,float* v1,int dim,float ratio)
{
    for(int i=0;i<dim;i++)
    {
        vr[i]=v0[i]*(1-ratio)+v1[i]*ratio;
    }
    return vr;
}



// Function to calculate the dot product of two vectors
float vecDotProduct(float* v1,float* v2,int dim) {
    float result = 0.0;
    for (int i = 0; i < dim; i++) {
        result += v1[i] * v2[i];
    }
    return result;
}

float EuclideanDistance(float *v1,float *v2,int vecL)
{
  float sum=0;
  for(int i=0;i<vecL;i++)
  {
    float dist = v1[i]-v2[i];
    sum+=dist*dist;
  }
  return sqrt(sum);
}

float EuclideanMagnitude(float *vec,int vecL)
{
  float sum=0;
  for(int i=0;i<vecL;i++)
  {
    float dist = vec[i];
    sum+=dist*dist;
  }
  return sqrt(sum);
}


float ManhattanDistance(float *v1,float *v2,int vecL,int *ret_idx)
{

  float maxDist=0;
  int idx=-1;
  for(int i=0;i<vecL;i++)
  {
    float dist = v1[i]-v2[i];
    if(dist<0)dist=-dist;
    if(maxDist<dist)
    {
      idx=i;
      maxDist=dist;
    }
  }
  if(ret_idx)*ret_idx=idx;
  return maxDist;
}
float ManhattanMagnitude(float *vec,int vecL,int *ret_idx)
{

  float maxDist=0;
  int idx=-1;
  for(int i=0;i<vecL;i++)
  {
    float dist = vec[i];
    if(dist<0)dist=-dist;
    if(maxDist<dist)
    {
      idx=i;
      maxDist=dist;
    }
  }
  if(ret_idx)*ret_idx=idx;
  return maxDist;
}




float calcAngleAndOthers(float* p0,float*p1,float* p2,int dim,float spDistRatio,float*ret_sp0,float*ret_sp2,float *ret_distance)
{
    //find dot product between S10 and S12
    float dotProd=0;
    float magS10=0;
    float magS12=0;
    for(int i=0;i<dim;i++)
    {
        float dist10 = p1[i]-p0[i];
        float dist12 = p1[i]-p2[i];
        dotProd+=dist10*dist12;
        magS10+=dist10*dist10;
        magS12+=dist12*dist12;
    }
    magS10=sqrt(magS10);
    magS12=sqrt(magS12);


    if(magS10<magS12)
    {
        if(ret_sp0)
            vecLerp(ret_sp0,p0,p1,dim,1-spDistRatio);
        if(ret_sp2)
            vecLerp(ret_sp2,p2,p1,dim,1-spDistRatio*magS10/magS12);
        if(ret_distance)*ret_distance=magS10*(spDistRatio);
    }
    else
    {
        if(ret_sp0)
            vecLerp(ret_sp0,p0,p1,dim,1-spDistRatio*magS12/magS10);
        if(ret_sp2)
            vecLerp(ret_sp2,p2,p1,dim,1-spDistRatio);


        if(ret_distance)*ret_distance=magS12*(spDistRatio);
    }

    // Calculate the cosine of the angle between the vectors
    float cosAngle = dotProd / (magS10 * magS12);

    // Calculate the angle in radians
    return acos(cosAngle);


}

void ArcBezierCtrlPoints_2D( float* p1, float* pc,float* p4, float *ret_p2, float *ret_p3) {
    // Point2D a=sub(p1,pc);
    // Point2D b=sub(p4,pc);
    // double q1=a.x*a.x+a.y*a.y;
    // double q2=q1+a.x*b.x+a.y*b.y;
    // double k2=4.0/3.0*(sqrt(2*q1*q2)-q2)/(a.x*b.y-a.y*b.x);

    // Point2D p2 = {pc.x + a.x - k2 * a.y, pc.y + a.y + k2 * a.x};
    // Point2D p3 = {pc.x + b.x + k2 * b.y, pc.y + b.y - k2 * b.x};

    // if(ret_p2)
    //     *ret_p2=p2;
    // if(ret_p3)
    //     *ret_p3=p3;

    float a[2],b[2];
    vecSub(a,p1,pc,2);
    vecSub(b,p4,pc,2);
    float q1=vecDotProduct(a,a,2);
    float q2=q1+vecDotProduct(a,b,2);
    float k2=4.0/3.0*(sqrt(2*q1*q2)-q2)/(a[0]*b[1]-a[1]*b[0]);
    
    float p2[2],p3[2];
    vecAdd(p2,pc,a,2);
    vecAdd(p2,p2,(float[]){-k2*a[1],k2*a[0]},2);
    vecAdd(p3,pc,b,2);
    vecAdd(p3,p3,(float[]){k2*b[1],-k2*b[0]},2);

    if(ret_p2)
        vecAdd(ret_p2,pc,a,2);
    if(ret_p3)
        vecAdd(ret_p3,pc,b,2);

    
    
}



/*

                                Pt=[0,0]                                                      
       ---------------------------\-----------------------------                        
                             ---/^ ---\                                                 
                         ---/    |     ---\                                             
                     ---/Theda/2 |Theda/2  ---\                                         
                 ---/            |             ---\                                     
             ---/                |                 ---\                                 
         ---/                    |                     ---\                             
      -<-----------------------------------------------------/                          
        \             [a]        |                         /-                           
         -\                      |                        /                             
           \                     |                      /-                              
            \                    |                     /                               -
             -\                  |                   /-                                 
               \                 |                  /                                   
                \                |                /-                                    
                 -\              |               /                                      
              [r]  \             | [b]         /-                                       
                    \            |            /                                         
                     \           |           /                                          
                      -\         |         /-                                           
                        \        |        /                                             
                         \       |      /-                                              
                          -\     |     /                                                
                            \    |   /-                                                 
                             \   |  /                                                   
                              -\ |/-                                                    
                                \|                                                      
                                 -PC                                                      
                                                                                        

//find the geomitry info first, then set the point P1 P4 and Pc loation accordingly
//Thenn find the control point location
//What we really want is to get the "Kappa" = |p1c-p1|/|p1-pt|
Kappa is the length ratio of the control point to the tangent point
once we get the Kappa table we can use it to calculate the control point location easily just using P1,p4 and Pt 
without additional finding the Pc first, especially if the point dimension is high>3
                                                                                                              
                                       Pt=[0,0]                                                                   
                                                                                                            
                                      +----+                                                                
                                      |   /|    ------------------------------------                        
                                      +----+                                         ^                      
                                     /-     \--                                      |                      
                                  /--          \-                                    |                      
                               /--               \--                                 |                      
                            /--                     \--                              |                      
              P2c         /-                           \--    P3c                    |                      
                 +----+/--                                \+----+                    |  1                   
                 |  /-|                                    | \- |                    |                      
                 +----+                                    +----+-                   |                      
               /--        ------  --------------                  \--                |                      
            /--  -- -----/                       ---------\          \--             |                      
 P1      /--  --/                                          ---------    \--          |                      
       /- ---/                                                      -\     \-    P4  |                      
+----+---/                                                            --\    \--     |                      
| -- -/                                                                  -\   +----+ |                      
+----+                                                                     -- |    | v                      
   \                                                                          +----+                        
    -\                                                                        /                             
      -\                                                                    /-                              
        -\                                                                /-                                
          \                                                              /                                  
           -\                                                          /-                                   
             -\                                                      /-                                     
               \                                                    /                                       
                -\                                                /-                                        
                  -\                                            /-                                          
                    -\                                         /                                            
                      \                                      /-                                             
                       -\                                  /-                                               
                         -\                               /                                                 
                           -\                           /-                                                  
                             \                        /-                                                    
                              -\                     /                                                      
                                -\                 /-                                                       
                                  \              /-                                                         
                                   -\           /                                                           
                                     -\       /-                                                            
                                       -\   /-                                                              
                                         - -                                                                
                                         Pc                          
*/

float Ang2SplineKappa(float angle_rad,float *ret_r) {
    // vecAngleTest();
    float theta=angle_rad;//50*M_PI/180;
    float tan_H_theda=tan(theta/2);
    float a=tan_H_theda;
    float b=a*a;
    float r=a/cos(theta/2);
    if(ret_r)*ret_r=r*cos(theta/2);

    float p1[]={a,-1};
    float p4[]={-a,-1};

    float pc[]={0,-1-b};

    //find the geomitry info first, then set the point loation accordingly
    
    //Thenn find the control point location
    float p1c[2],p4c[2];//Bezier control point
    ArcBezierCtrlPoints_2D(p1,p4,pc,p1c,p4c);

    //What we really want is to get the "Kappa" = |p1c-p1|/|p1-pt|
    return EuclideanDistance(p1,p1c,2)/EuclideanMagnitude(p1,2);

}

//use polynomial to approximate
float Ang2SplineKappa2(float angle_rad) {
    /*
 1.9791009054508446e-005
 6.6613418614336073e-001
-3.3001348108030254e-001
 1.3007110452860979e-001
-4.2987380431706773e-002
 1.0273903175029121e-002
-1.5220155130653461e-003
 1.0085596055321018e-004*/

    float x1=angle_rad;
    float x2=x1*x1;
    float x3=x2*x1;
    float x4=x3*x1;
    float x5=x4*x1;
    float x6=x5*x1;
    float x7=x6*x1;

    
    float a0=1.9791009054508446e-005;
    float a1=6.6613418614336073e-001;
    float a2=-3.3001348108030254e-001;
    float a3=1.3007110452860979e-001;
    float a4=-4.2987380431706773e-002;
    float a5=1.0273903175029121e-002;
    float a6=-1.5220155130653461e-003;
    float a7=1.0085596055321018e-004;
    
    return a0+
    a1*x1+
    a2*x2+
    a3*x3+
    a4*x4+
    a5*x5+
    a6*x6+
    a7*x7;

}



//use polynomial to approximate
float Ang2RDivDist(float angle_rad) {



    float x1=angle_rad;
    float x2=x1*x1;
    float x3=x2*x1;
    float x4=x3*x1;
    float x5=x4*x1;
    float x6=x5*x1;
    float x7=x6*x1;

    float a0,a1,a2,a3,a4,a5,a6,a7;
    if(angle_rad<2.4)
    {
        /*
-7.5236760790757529e-004
 5.2363389360678936e-001
-1.6456067288158310e-001
 5.0343118394247488e-001
-6.3733013324315912e-001
 4.6547559937891825e-001
-1.6903340487027277e-001
 2.5479845982048912e-002*/
        a0=-7.5236760790757529e-004;
        a1=5.2363389360678936e-001;
        a2=-1.6456067288158310e-001;
        a3=5.0343118394247488e-001;
        a4=-6.3733013324315912e-001;
        a5=4.6547559937891825e-001;
        a6=-1.6903340487027277e-001;
        a7=2.5479845982048912e-002;


        
        return (a0+
        a1*x1+
        a2*x2+
        a3*x3+
        a4*x4+
        a5*x5+
        a6*x6+
        a7*x7);
    }

    if(angle_rad<2.8)
    {
            /*
-2.4930891913857995e+003
 3.6941134402472235e+003
-1.7003475561678206e+003
 1.1111069493394098e+002
 4.7416625814635609e+001
 4.2597583124448832e+001
-2.3087564508282753e+001
 2.9103929159095747e+000*/
    a0=-2.4930891913857995e+003;
    a1=3.6941134402472235e+003;
    a2=-1.7003475561678206e+003;
    a3=1.1111069493394098e+002;
    a4=4.7416625814635609e+001;
    a5=4.2597583124448832e+001;
    a6=-2.3087564508282753e+001;
    a7=2.9103929159095747e+000;




    float r=(a0+
    a1*x1+
    a2*x2+
    a3*x3+
    a4*x4+
    a5*x5+
    a6*x6+
    a7*x7);

    return r;
    }

    /*
 2.3519401902049908e+003
-9.2091075372012540e+002
-6.0322192105472709e+001
 7.4733564882614530e+001
-2.4719347865370480e+001
 9.5898956352426126e+000
-2.5319162772309953e+000
 2.4059988919351327e-001
*/
    a0=2.3519401902049908e+003;
    a1=-9.2091075372012540e+002;
    a2=-6.0322192105472709e+001;
    a3=7.4733564882614530e+001;
    a4=-2.4719347865370480e+001;
    a5=9.5898956352426126e+000;
    a6=-2.5319162772309953e+000;
    a7=2.4059988919351327e-001;




    float ir=(a0+
    a1*x1+
    a2*x2+
    a3*x3+
    a4*x4+
    a5*x5+
    a6*x6+
    a7*x7);
    float r = 1024/ir;

    return r>15000?NAN:r;
}

int vecAngleTest();

int main() 
{
    vecAngleTest() ;
    return 1;
    // const int segs=180;
    // Point2D pairs[segs]={0};
    // for(int i=0;i<=segs;i++)
    // {
    //     float theta=i*M_PI/segs*0.999+0.003;
    //     float r_ratio=0;
    //     float ratio=Ang2SplineKappa(theta,&r_ratio);
    //     pairs[i].x=theta;
    //     pairs[i].y=ratio;

    //     float ratio2=Ang2SplineKappa2(theta);
    //     float r_ratio2=Ang2RDivDist(theta);//a=V^2/r  Vmax=sqrt(ar)
    //     printf("%f ratio=%f-%f=%f  r=%f-%f=%f  \n",theta,ratio,ratio2,ratio-ratio2,r_ratio,r_ratio2,(r_ratio-r_ratio2));

    //     // printf("%f %f\n",theta,1024/r_ratio);
    // }

    // return 0;
}



void cubicBezier_TCoeff(double t,float *coeff_4) {
    double one_minus_t = 1.0 - t;

    float t_sq=t*t;
    float one_minus_t_sq=one_minus_t*one_minus_t;

    coeff_4[0]=one_minus_t_sq * one_minus_t;
    coeff_4[1]=3 * one_minus_t_sq * t;
    coeff_4[2]=3 * one_minus_t * t_sq;
    coeff_4[3]=t_sq * t;


}
float cubicBezier_Ele(float p0, float p1, float p2, float p3,float *coeff_4) {
    return coeff_4[0]*p0+coeff_4[1]*p1+coeff_4[2]*p2+coeff_4[3]*p3;
}

int vecAngleTest() 
{
    printf("===========\n\n\n");
    const int DIM=10;
    float p0[DIM]={0,30,0,0,0,0,0,0,0,0};
    float p1[DIM]={10,0,10,0,0,0,0,0,0,0};
    float p2[DIM]={20,10,0,0,0,0,0,0,0,0};

    float sp0[DIM];
    float sp2[DIM];
    float distance;
    // Calculate the angle in radians
    float angleRadians = calcAngleAndOthers(p0,p1,p2,DIM,0.8,sp0,sp2,&distance);
    
    float arc_r_div_dist;
    float kappa;//=Ang2SplineKappa(angleRadians,&arc_r_div_dist);

    kappa=Ang2SplineKappa2(angleRadians);
    arc_r_div_dist=Ang2RDivDist(angleRadians);

    double arc_r=arc_r_div_dist*distance;
    printf("arc_r_div_dist=%f arc_r:%f\n",arc_r_div_dist,arc_r);
    printf("\n\n\n");


    
    float ctrlpt0[DIM];
    float ctrlpt2[DIM];
    vecLerp(ctrlpt0,sp0,p1,DIM,kappa);
    vecLerp(ctrlpt2,sp2,p1,DIM,kappa);


    printf("\n\n");
    {
        int segs=(int)(arc_r*(M_PI-angleRadians));//*1.055);

        printf("\n\nsegs:%d  angleRadians:%f\n\n",segs,angleRadians);

        printf("[");
        float coeff[4];
        for(int i=0;i<=segs;i++)
        {
            cubicBezier_TCoeff((float)i/segs,coeff);
            float pointOnCurve[DIM]; 
            for(int k=0;k<DIM;k++)
            {
                pointOnCurve[k]=(double)cubicBezier_Ele(
                    (float)sp0[k],
                    (float)ctrlpt0[k],
                    (float)ctrlpt2[k],
                    (float)sp2[k],
                    coeff);
            }

            printf("[%0.5f, %0.5f, %0.5f],", pointOnCurve[0], pointOnCurve[1], pointOnCurve[2]);
        }
        // printf("\n\n");

        // float distSum=0;
        // for(int i=1;i<segs;i++)
        // {   
        //     float advDist = magnitude_(subV(pointsOnCurve[i],pointsOnCurve[i-1]));
        //     distSum+=advDist;
        //     printf("%0.5f, %0.5f\n",(i-0.5)/segs,advDist*1024);
        // }

    }
    // printf("\n\n");
    char fmt[]="[%0.5f, %0.5f, %0.5f],";
    printf(fmt, p0[0], p0[1], p0[2]);
    printf(fmt, p1[0], p1[1], p1[2]);
    printf(fmt, p2[0], p2[1], p2[2]);

    printf(fmt, sp0[0], sp0[1], sp0[2]);
    printf(fmt, ctrlpt0[0], ctrlpt0[1],  ctrlpt0[2]);
    printf(fmt, ctrlpt2[0], ctrlpt2[1],  ctrlpt2[2]);
    printf(fmt, sp2[0], sp2[1],  sp2[2]);

    printf("]\n");
    printf("\n");
    // Convert the angle to degrees
    double angleDegrees = angleRadians * (180.0 / M_PI);

    printf("Angle between S01 and S12: %.2lf degrees\n", angleDegrees);

    printf("===========\n\n\n");
    return 0;
}

