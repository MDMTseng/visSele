#ifndef MATCHING_CORE_HPP
#define MATCHING_CORE_HPP

#include "acvImage_ToolBox.hpp"

#include "acvImage_BasicDrawTool.hpp"
#include <cstdlib>
#include <unistd.h>

void ContourFeatureDetect(std::vector<acv_XY> &signature,const std::vector<acv_XY> &tar_signature);

#endif
