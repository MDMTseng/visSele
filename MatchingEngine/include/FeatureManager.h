#ifndef FeatureManager_HPP
#define FeatureManager_HPP
using namespace std;
#include <vector>
#include <cstdlib>
#include <ctime>
#include "cJSON.h"
#include "acvImage_ComponentLabelingTool.hpp"

class FeatureManager {
  protected:
  cJSON *root;
  int parse_jobj();
public :
  static bool check(cJSON *root);
  FeatureManager(const char *json_str);
  int reload(const char *json_str);
  int FeatureMatching(acvImage *img,acvImage *buff,vector<acv_LabeledData> &ldData,acvImage *dbg);
};


class FeatureManager_sig360_circle_line:public FeatureManager {
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
public :
  FeatureManager_sig360_circle_line(const char *json_str);
  int reload(const char *json_str);
  int FeatureMatching(acvImage *img,acvImage *buff,vector<acv_LabeledData> &ldData,acvImage *dbg);
  static bool check(cJSON *root);
protected:
  int parse_circleData(cJSON * circle_obj);
  int parse_lineData(cJSON * line_obj);
  int parse_signatureData(cJSON * signature_obj);
  int parse_jobj();
};

#endif
