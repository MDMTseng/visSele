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

void extractContourDataToContourGrid(acvImage *labeledImg,int grid_size,ContourGrid &inward_curve_grid, ContourGrid &straight_line_grid,int scanline_skip);

acv_XY* findEndPoint(acv_Line line, int signees, std::vector<acv_XY> &points);

void circleRefine(std::vector<acv_XY> &pointsInRange,acv_CircleFit *circleF);
#endif
