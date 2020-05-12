#ifndef MATCHING_CORE_HPP
#define MATCHING_CORE_HPP

#include "acvImage_ToolBox.hpp"
#include "ContourGrid.h"
#include "acvImage_BasicDrawTool.hpp"
#include <cstdlib>
#include <unistd.h>
#include "FeatureManager.h"

void ContourFeatureDetect(std::vector<acv_XY> &signature,const std::vector<acv_XY> &tar_signature);
void MatchingCore_CircleLineExtraction(acvImage *img,acvImage *buff,std::vector<acv_LabeledData> &ldData,
  std::vector<acv_CircleFit> &detectedCircles,std::vector<acv_LineFit> &detectedLines);

void extractContourDataToContourGrid(acvImage *grayLevelImg,acvImage *labeledImg,int grid_size,ContourGrid &edge_curve_grid,int scanline_skip);

void extractLabeledContourDataToContourGrid(
  acvImage *grayLevelImg,acvImage *labeledImg,int label,acv_LabeledData ldat,int thres,
  int grid_size,ContourFetch &edge_curve_grid,int scanline_skip,FeatureManager_BacPac *bacpac);

ContourFetch::ptInfo* findEndPoint(acv_Line line, int signedness, std::vector<ContourFetch::ptInfo> &points);
void circleRefine(std::vector<ContourFetch::ptInfo> &pointsInRange,int len,acv_CircleFit *circleF);


void spline9_max(float *f,int fL,int div,float *ret_maxf,float *ret_maxf_x);
#endif
