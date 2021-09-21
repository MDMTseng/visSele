#ifndef MATCHING_CORE_HPP
#define MATCHING_CORE_HPP

#include "acvImage_ToolBox.hpp"
#include "ContourGrid.h"
#include "acvImage_BasicDrawTool.hpp"
#include <cstdlib>
#include <unistd.h>
#include "FeatureManager.h"


//return 0 => two real roots
//return 1 => single complex root => r0: real part r1: positive imaginary part
//aX^2+bx+c=0
int quadratic_roots(float a,float b,float c,float *r0,float*r1);
void ContourFeatureDetect(std::vector<acv_XY> &signature,const std::vector<acv_XY> &tar_signature);
void MatchingCore_CircleLineExtraction(acvImage *img,acvImage *buff,std::vector<acv_LabeledData> &ldData,
  std::vector<acv_CircleFit> &detectedCircles,std::vector<acv_LineFit> &detectedLines);

void extractContourDataToContourGrid(acvImage *grayLevelImg,acvImage *labeledImg,int grid_size,ContourGrid &edge_curve_grid,int scanline_skip);
int contourPixExtraction(acvImage *graylevelImg, acv_XY center_point,acv_XY sobel,float stepDist,int steps,float *pixels,FeatureManager_BacPac *bacpac=NULL);

float findMaxIdx_spline(float *grad,int gradL,float *ret_max);

float findGradMaxIdx_spline(float *f,int fL,float *ret_max);

int calc_pdf_mean_sigma(float *f,int fL,float *ret_mean,float *ret_sigma);
// void extractLabeledContourDataToContourGrid(
//   acvImage *grayLevelImg,acvImage *labeledImg,int label,acv_LabeledData ldat,int thres,
//   int grid_size,ContourFetch &edge_curve_grid,int scanline_skip,FeatureManager_BacPac *bacpac);

  
void contourGridGrayLevelRefine(acvImage *grayLevelImg,ContourFetch &edge_grid,FeatureManager_BacPac *bacpac);
void extractLabeledContourDataToContourGrid(acvImage *labeledImg,int label,acv_LabeledData ldat,ContourFetch &edge_grid,int scanline_skip);

ContourFetch::ptInfo* findEndPoint(acv_Line line, int signedness, std::vector<ContourFetch::ptInfo> &points);
void circleRefine(std::vector<ContourFetch::ptInfo> &pointsInRange,int len,acv_CircleFit *circleF);


void spline9_max(float *f,int fL,int div,float *ret_maxf,float *ret_maxf_x);



float CrossProduct(acv_XY p1,acv_XY p2,acv_XY p3);
void ComputeConvexHull2(const acv_XY *polygon,const int L);









class edgeTracking
{
//   "|" => the real edge
//   [.....]=> pixWidth=5 pixSideWidth=2


//   [   ] ^
//   [   ] | regionWidth=3 regionSideWidth=1
//   [   ] v
//

//dark|  bright
// [  |    ]
// [ |     ]
// [ |     ]
// [   |   ]
// [     | ]
//       |
//     |
//     |
//     |
//    |


  static const int pixSideWidth=10;
  static const int regionSideWidth=5;
  static const int pixWidth=1+2*pixSideWidth;
  static const int regionWidth=1+2*regionSideWidth;
  float pixRegion[regionWidth][pixWidth];
  float stepDist=1;
  
  float pixSum[pixWidth]={0};
  float grad[pixWidth]={0};
  acvImage *graylevelImg;
  acv_XY imgOffset;
  FeatureManager_BacPac *bacpac;
  public:
  edgeTracking (acvImage *graylevelImg,acv_XY imgOffset,FeatureManager_BacPac *bacpac=NULL);

  int fbIndex;
  int gradIndex;
  bool pixSumReset=false;
  void initTracking (ContourFetch::contourMatchSec &section,int new_regionSideWidth=regionSideWidth);

  protected:
  void runTracking (ContourFetch::contourMatchSec &section,int new_regionSideWidth);
  void PixSumReCalc(int start,int end);


  int contourPixExtraction(acvImage *graylevelImg, acv_XY center_point,acv_XY sobel,int stepJump,float stepDist,int steps,float *pixels,FeatureManager_BacPac *bacpac);



  float pixFetch(acvImage *graylevelImg, acv_XY pt,FeatureManager_BacPac *bacpac);


  void calc_info(float *mean_offset, float *sigma);

  
  void goSideShift (ContourFetch::contourMatchSec &section,bool goGradDir);

  void goAdv (ContourFetch::contourMatchSec &section,bool goForward,int new_regionSideWidth);
};
#endif
