#ifndef MATCHING_CORE_HPP
#define MATCHING_CORE_HPP

#include "acvImage_ToolBox.hpp"
#include "ContourGrid.h"
#include "acvImage_BasicDrawTool.hpp"
#include <cstdlib>
#include <unistd.h>

void ContourFeatureDetect(std::vector<acv_XY> &signature,const std::vector<acv_XY> &tar_signature);
void MatchingCore_CircleLineExtraction(acvImage *img,acvImage *buff,std::vector<acv_LabeledData> &ldData,
  std::vector<acv_CircleFit> &detectedCircles,std::vector<acv_LineFit> &detectedLines);

void extractContourDataToContourGrid(acvImage *grayLevelImg,acvImage *labeledImg,int grid_size,ContourGrid &edge_curve_grid,int scanline_skip);

void extractLabeledContourDataToContourGrid(
  acvImage *grayLevelImg,acvImage *labeledImg,int label,acv_LabeledData ldat,
  int grid_size,ContourGrid &edge_curve_grid,int scanline_skip,acvRadialDistortionParam param);

acv_XY* findEndPoint(acv_Line line, int signees, std::vector<acv_XY> &points);

void circleRefine(std::vector<ContourGrid::ptInfo> &pointsInRange,acv_CircleFit *circleF);
#endif
