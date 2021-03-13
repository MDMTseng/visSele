#include <stdio.h>
#include <unistd.h>
#include <main.h>
#include "tmpCodes.hpp"
#include "polyfit.h"
#include "acvImage_BasicTool.hpp"



float randomGen(float from=0,float to=1)
{
  float r01=(rand()%1000000)/1000000.0;
  return r01*(to-from)+from;
}

int_fast32_t testPolyFit()
{
  srand(time(NULL));
  // These inputs should result in the following approximate coefficients:
  //         0.5           2.5           1.0        3.0
  //    y = (0.5 * x^3) + (2.5 * x^2) + (1.0 * x) + 3.0
  const int dataL=100;
  struct DATA_XY
  {
    float x;
    float y;
  }Data[dataL];


  float coeff[]={1.0001,2.5,-3,4,5};
  const int order = sizeof(coeff)/sizeof(coeff[0])-1;
  for(int i=0;i<dataL;i++)
  {
    Data[i].x=(i-dataL/2)*0.1;
    Data[i].y=polycalc(Data[i].x, coeff,order+1);//+randomGen(-1,1);
  }

  float resCoeff[order+1]={0}; // resulting array of coefs

  // Perform the polyfit
  int result = polyfit(&(Data[0].x),
                       &(Data[0].y),
                       NULL,
                       dataL,
                       order,
                       resCoeff,
                       sizeof(Data[0]),
                       sizeof(Data[0])
                       );




  printf("Original coeff\n");
  for(int i=0;i<order+1;i++)printf(",%.5f",coeff[order-i]);
  printf("\nNew coeff\n");
  for(int i=0;i<order+1;i++)printf(",%.5f",resCoeff[order-i]);
  printf("\n===========\n");

  
  for(int i=0;i<dataL;i++)
  {
    float pred_yData=polycalc(Data[i].x, resCoeff,order+1);

    LOGI("[%d]: x:%.5f   y:%.5f   _y:%.5f  diff:%.5f",i,Data[i].x,Data[i].y,pred_yData,pred_yData-Data[i].y);
  }

  return 0;
}

int testDistance()
{
  float errorSum=0;
  for(int i=0;i<100;i++)
  {
    
    acv_Line line={
      .line_vec=(acv_XY){randomGen(-100,100),randomGen(-100,100)},
      .line_anchor=(acv_XY){randomGen(-100,100),randomGen(-100,100)},
    };

    float tarDist=randomGen(-100,100);
    float tarSlide=randomGen(-100,100);

    acv_XY vec = acvVecNormalize(line.line_vec);
    acv_XY nvec =acvVecNormal(vec);

    acv_XY pt1=acvVecAdd(acvVecAdd(line.line_anchor,acvVecMult(vec,tarSlide)),acvVecMult(nvec,tarDist));

    float dist =acvDistance_Signed(line, pt1);
    errorSum+=abs(dist-tarDist);
    LOGI("Test[%d]: tarDist:%f dist:%f diff:%f",i,tarDist,dist,dist-tarDist);
  }
  
  LOGI("errorSum:%f",errorSum);
  return 0;
}

int main(int argc, char **argv)
{
  // return testDistance();
  // return testPolyFit();
  // tmpMain();
  // printf(">>>");
  return cp_main(argc, argv);
}

