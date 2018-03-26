#ifndef FeatureManager_HPP
#define FeatureManager_HPP
using namespace std;
#include <vector>
#include <cstdlib>
#include <ctime>
#include "cJSON.h"
#include "acvImage_BasicTool.hpp"



class FeatureManager {
  typedef struct featureDef_circle{
    acv_Circle circleTar;
    float initMatchingMargin;
  }featureDef_circle;
  typedef struct featureDef_line{
    acv_Line lineTar;
    acv_XY searchVec;//The vector to searching the contour edge
  }featureDef_line;

  vector<featureDef_circle> featureCircleList;
  vector<featureDef_line> featureLineList;
  vector<acv_XY> contour_signature;
  cJSON *root;
public :
  FeatureManager(const char *json_str);
  int reload(const char *json_str);
  int FeatureMatching(acvImage *img,acvImage *buff,acvImage *dbg);
protected:
  int parse_circleData(cJSON * circle_obj);
  int parse_lineData(cJSON * line_obj);
  int parse_signatureData(cJSON * signature_obj);
  int parse_jobj();
};

#endif
