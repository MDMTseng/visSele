#ifndef MATCHING_CORE_HPP
#define MATCHING_CORE_HPP

#include "acvImage_ToolBox.hpp"

#include "acvImage_BasicDrawTool.hpp"
#include <cstdlib>
#include <unistd.h>
void ContourFeatureDetect(acvImage *img,acvImage *buff,const std::vector<acv_XY> &tar_signature);


#endif
