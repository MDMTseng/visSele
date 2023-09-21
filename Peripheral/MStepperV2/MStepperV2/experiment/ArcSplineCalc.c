#include <stdio.h>
#include <math.h>
typedef struct {
    double x, y;
} Point2D;




void cubicBezierCoeff(double t,float *coeff) {
    Point2D result;
    double one_minus_t = 1.0 - t;


    coeff[0]=one_minus_t * one_minus_t * one_minus_t;
    coeff[1]=3 * one_minus_t * one_minus_t * t;
    coeff[2]=3 * one_minus_t * t * t;
    coeff[3]=t * t * t;
}


Point2D cubicBezier(double t, Point2D p0, Point2D p1, Point2D p2, Point2D p3) {
    Point2D result;
    double one_minus_t = 1.0 - t;

    result.x = one_minus_t * one_minus_t * one_minus_t * p0.x 
             + 3 * one_minus_t * one_minus_t * t * p1.x 
             + 3 * one_minus_t * t * t * p2.x 
             + t * t * t * p3.x;

    result.y = one_minus_t * one_minus_t * one_minus_t * p0.y 
             + 3 * one_minus_t * one_minus_t * t * p1.y 
             + 3 * one_minus_t * t * t * p2.y 
             + t * t * t * p3.y;

    return result;
}

Point2D sub(Point2D p1, Point2D p2) {
    Point2D result;
    result.x = p1.x - p2.x;
    result.y = p1.y - p2.y;
    return result;
}
Point2D add(Point2D p1, Point2D p2) {
    Point2D result;
    result.x = p1.x + p2.x;
    result.y = p1.y + p2.y;
    return result;
}


void compBezierCtrlPoints( Point2D p1, Point2D pc,Point2D p4, Point2D *ret_p2, Point2D *ret_p3) {
    Point2D a=sub(p1,pc);
    Point2D b=sub(p4,pc);
    double q1=a.x*a.x+a.y*a.y;
    double q2=q1+a.x*b.x+a.y*b.y;
    double k2=4.0/3.0*(sqrt(2*q1*q2)-q2)/(a.x*b.y-a.y*b.x);

    Point2D p2 = {pc.x + a.x - k2 * a.y, pc.y + a.y + k2 * a.x};
    Point2D p3 = {pc.x + b.x + k2 * b.y, pc.y + b.y - k2 * b.x};

    if(ret_p2)
        *ret_p2=p2;
    if(ret_p3)
        *ret_p3=p3;
}



// Function to calculate the distance between two points
double distance(Point2D p1, Point2D p2) {
    return sqrt((p1.x - p2.x) * (p1.x - p2.x) + (p1.y - p2.y) * (p1.y - p2.y));
}


double magnitude(Point2D p1) {
    return sqrt((p1.x) * (p1.x) + (p1.y) * (p1.y));
}




int mains(Point2D p1,Point2D p4,Point2D pc,Point2D *ret_p2,Point2D *ret_p3) {
    
    Point2D p2,p3;
    
    compBezierCtrlPoints(p1, pc, p4,&p2, &p3);
    
    if(ret_p2)
        *ret_p2=p2;
    if(ret_p3)
        *ret_p3=p3;
    
    // int segs=100;

    // for (int i = 0; i <= segs; i++) {
    //     Point2D pointOnCurve = cubicBezier((double)i/segs, p1, p2, p3, p4);
    //     if(i!=0)
    //         printf(",");
    //     printf("(%0.5f, %0.5f)", pointOnCurve.x, pointOnCurve.y);
    // }
    // printf(",(%0.5f, %0.5f)", p2.x, p2.y);
    // printf(",(%0.5f, %0.5f)", p3.x, p3.y);

    // printf("\n");
    return 0;
}



int vecAngleTest();


double Ang2SplineKappa(double angle_rad,double *ret_r) {
    // vecAngleTest();
    double theta=angle_rad;//50*M_PI/180;
    double tan_H_theda=tan(theta/2);
    double a=tan_H_theda;
    double b=a*a;
    double r=a/cos(theta/2);
    if(ret_r)*ret_r=r*cos(theta/2);
    Point2D p1={a,-1};
    Point2D p4={-a,-1};

    Point2D pc={0,-1-b};
    

    // char *fstr="(%0.5f, %0.5f),";

    // printf(fstr, p1.x,p1.y);
    // printf(fstr, p4.x,p4.y);
    // printf(fstr, pc.x,pc.y);
    // printf("(0,0)");
    
    // printf("\n");



    Point2D p2,p3;
    mains(p1,p4,pc,&p2,&p3);
    double dratio=distance(p1,p2)/magnitude(p1);

    // printf("spline ratio:%f\n",dratio);


    if(0)
    {//in case the theta < 90 degree the approximation is not good, we could choose two segment approaximation
        Point2D pt=pc;
        pt.y+=r;

        Point2D p2,p3;
        mains(p1,pt,pc,&p2,&p3);
        mains(pt,p4,pc,&p2,&p3);
    }



    // printf("\n\n");

    return dratio;
}


float Ang2SplineKappa2(float angle_rad) {
    /*Mode: normal x,y analysis
Polynomial degree 7, 179 x,y data pairs.
Correlation coefficient = 0.9999999947411593
Standard error = 0.000013338233800319338

Output form: simple list (ordered x^0 to x^n):

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


int main() 
{
    vecAngleTest() ;
    return 1;
    const int segs=180;
    Point2D pairs[segs]={0};
    for(int i=0;i<=segs;i++)
    {
        double theta=i*M_PI/segs*0.999+0.003;
        double r_ratio=0;
        double ratio=Ang2SplineKappa(theta,&r_ratio);
        pairs[i].x=theta;
        pairs[i].y=ratio;

        double ratio2=Ang2SplineKappa2(theta);
        double r_ratio2=Ang2RDivDist(theta);//a=V^2/r  Vmax=sqrt(ar)
        printf("%f ratio=%f-%f=%f  r=%f-%f=%f  \n",theta,ratio,ratio2,ratio-ratio2,r_ratio,r_ratio2,(r_ratio-r_ratio2));

        // printf("%f %f\n",theta,1024/r_ratio);
    }

    return 0;
}


#define DIMENSION 10

// Define a structure to represent a 10D vector
struct Vector {
    double components[DIMENSION];
};


struct Vector subV(struct Vector v1, struct Vector v2) {

    struct Vector result;
    for (int i = 0; i < DIMENSION; i++) {
        result.components[i] = v1.components[i] - v2.components[i];
    }
    return result;
}


// Function to calculate the dot product of two vectors
double dotProduct(struct Vector v1, struct Vector v2) {
    double result = 0.0;
    for (int i = 0; i < DIMENSION; i++) {
        result += v1.components[i] * v2.components[i];
    }
    return result;
}


struct Vector lerp(struct Vector v1, struct Vector v2,float ratio) {
    struct Vector result;
    for (int i = 0; i < DIMENSION; i++) {
        result.components[i] = v1.components[i] * (1-ratio) + v2.components[i] * ratio;
    }
    return result;
}
// Function to calculate the magnitude of a vector
double magnitude_(struct Vector v) {
    double result = 0.0;
    for (int i = 0; i < DIMENSION; i++) {
        result += v.components[i] * v.components[i];
    }
    return sqrt(result);
}


double calcAngle(struct Vector p0,struct Vector p1,struct Vector p2,float spDistRatio,struct Vector *ret_sp0,struct Vector *ret_sp2,float *ret_distance)
{


    struct Vector S01=subV(p1,p0);
    struct Vector S12=subV(p1,p2);

    // Calculate the dot product of the vectors S01 and S12
    double dotProd = dotProduct(S01, S12);

    // Calculate the magnitudes of the vectors
    double magS01 = magnitude_(S01);
    double magS12 = magnitude_(S12);


    if(magS01<magS12)
    {
        if(ret_sp0)*ret_sp0=lerp(p0,p1,1-spDistRatio);
        if(ret_sp2)*ret_sp2=lerp(p2,p1,1-spDistRatio*magS01/magS12);
        if(ret_distance)*ret_distance=magS01*(spDistRatio);
    }
    else
    {
        // printf("magS  %f/%f=%f\n",magS01,magS12,magS01/magS12 );
        if(ret_sp0)*ret_sp0=lerp(p0,p1,1-spDistRatio*magS12/magS01);
        if(ret_sp2)*ret_sp2=lerp(p2,p1,1-spDistRatio);
        if(ret_distance)*ret_distance=magS12*(spDistRatio);
    }

    // Calculate the cosine of the angle between the vectors
    double cosAngle = dotProd / (magS01 * magS12);

    // Calculate the angle in radians
    return acos(cosAngle);

}


float cubicBezier_Ele(float p0, float p1, float p2, float p3,float *coeff) {
    return coeff[0]*p0+coeff[1]*p1+coeff[2]*p2+coeff[3]*p3;
}



int vecAngleTest() 
{
    printf("===========\n\n\n");

    struct Vector p0={0,30,0,0,0,0,0,0,0,0};
    struct Vector p1={10,0,10,0,0,0,0,0,0,0};
    struct Vector p2={20,10,0,0,0,0,0,0,0,0};



    struct Vector sp0;
    struct Vector sp2;
    float distance;
    // Calculate the angle in radians
    double angleRadians = calcAngle(p0,p1,p2,0.8,&sp0,&sp2,&distance);
    
    double arc_r_div_dist;
    double kappa=Ang2SplineKappa(angleRadians,&arc_r_div_dist);

    kappa=Ang2SplineKappa2(angleRadians);
    arc_r_div_dist=Ang2RDivDist(angleRadians);

    double arc_r=arc_r_div_dist*distance;
    printf("arc_r_div_dist=%f arc_r:%f\n",arc_r_div_dist,arc_r);
    printf("\n\n\n");


    
    struct Vector ctrlpt0=lerp(sp0,p1,kappa);
    struct Vector ctrlpt2=lerp(sp2,p1,kappa);


    printf("\n\n");
    {
        int segs=(int)(arc_r*(M_PI-angleRadians));//*1.055);

        printf("\n\nsegs:%d  angleRadians:%f\n\n",segs,angleRadians);

        printf("[");
        float coeff[4];
        struct Vector pointsOnCurve[1100];
        for(int i=0;i<=segs;i++)
        {
            cubicBezierCoeff((float)i/segs,coeff);
            struct Vector pointOnCurve; 
            for(int k=0;k<DIMENSION;k++)
            {
                pointOnCurve.components[k]=(double)cubicBezier_Ele(
                    (float)sp0.components[k],
                    (float)ctrlpt0.components[k],
                    (float)ctrlpt2.components[k],
                    (float)sp2.components[k],
                    coeff);
            }
            pointsOnCurve[i]=pointOnCurve;

            printf("[%0.5f, %0.5f, %0.5f],", pointOnCurve.components[0], pointOnCurve.components[1], pointOnCurve.components[2]);
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
    printf(fmt, p0.components[0], p0.components[1], p0.components[2]);
    printf(fmt, p1.components[0], p1.components[1], p1.components[2]);
    printf(fmt, p2.components[0], p2.components[1], p2.components[2]);

    printf(fmt, sp0.components[0], sp0.components[1], sp0.components[2]);
    printf(fmt, ctrlpt0.components[0], ctrlpt0.components[1],  ctrlpt0.components[2]);
    printf(fmt, ctrlpt2.components[0], ctrlpt2.components[1],  ctrlpt2.components[2]);
    printf(fmt, sp2.components[0], sp2.components[1],  sp2.components[2]);

    printf("]\n");
    printf("\n");
    // Convert the angle to degrees
    double angleDegrees = angleRadians * (180.0 / M_PI);

    printf("Angle between S01 and S12: %.2lf degrees\n", angleDegrees);

    printf("===========\n\n\n");
    return 0;
}

