#ifndef FeatureManager_HPP
#define FeatureManager_HPP
using namespace std;
#include <vector>
#include <cstdlib>
#include <ctime>
#include "cJSON.h"
#include "acvImage_ComponentLabelingTool.hpp"
#include <ContourGrid.h>

class FeatureManager {
  protected:
  cJSON *root;
  virtual int parse_jobj()=0;
public :
  static bool check(cJSON *root){return false;};
  FeatureManager(const char *json_str){};
  virtual int reload(const char *json_str)=0;
  virtual int FeatureMatching(acvImage *img,acvImage *buff,acvImage *dbg)=0;
};

class FeatureManager_group_proto:public FeatureManager {
public :

  FeatureManager_group_proto(const char *json_str): FeatureManager(json_str){};
  int reload(const char *json_str) override;
protected:
  virtual int addSubFeature(cJSON * subFeature)=0;
  virtual int clearFeatureGroup()=0;
  int parse_jobj() override;
};

class FeatureManager_binary_processing:public FeatureManager {

protected:
  virtual int parse_jobj()=0;
public :
  FeatureManager_binary_processing(const char *json_str):FeatureManager(json_str){};
  virtual int reload(const char *json_str)=0;
  virtual int FeatureMatching(acvImage *img,acvImage *buff,acvImage *dbg){return -1;};
  virtual int FeatureMatching(acvImage *img,acvImage *buff,vector<acv_LabeledData> &ldData,acvImage *dbg)=0;
};


class FeatureManager_binary_processing_group:public FeatureManager_group_proto {
  vector<FeatureManager_binary_processing*> binaryFeatureBundle;

public :
  FeatureManager_binary_processing_group(const char *json_str);
  static bool check(cJSON *root);
  int FeatureMatching(acvImage *img,acvImage *buff,acvImage *dbg) override;
protected:
  int addSubFeature(cJSON * subFeature) override;
  int clearFeatureGroup() override;
  ~FeatureManager_binary_processing_group(){clearFeatureGroup();};
};

class FeatureManager_group:public FeatureManager_group_proto {
  vector<FeatureManager*> featureBundle;

public :
  FeatureManager_group(const char *json_str);
  static bool check(cJSON *root);
  int FeatureMatching(acvImage *img,acvImage *buff,acvImage *dbg) override;
protected:
  int addSubFeature(cJSON * subFeature) override;
  int clearFeatureGroup() override;
  ~FeatureManager_group(){clearFeatureGroup();};
};


class FeatureManager_sig360_circle_line:public FeatureManager_binary_processing {
  typedef struct featureDef_circle{
    acv_Circle circleTar;
    float initMatchingMargin;
  }featureDef_circle;
  typedef struct searchKeyPoint{
    acv_XY searchStart;
    acv_XY searchVec;
    float searchDist;
  }searchKeyPoint;
  typedef struct featureDef_line{
    acv_Line lineTar;
    acv_XY searchVec;//The vector to searching the contour edge
    acv_XY searchEstAnchor;//The vector to searching the contour edge
    float initMatchingMargin;
    float MatchingMarginX;
    float searchDist;
    vector<searchKeyPoint> skpsList;
  }featureDef_line;

  vector<featureDef_circle> featureCircleList;
  vector<featureDef_line> featureLineList;
  vector<acv_XY> feature_signature;
  vector<acv_XY> tmp_signature;
  ContourGrid inward_curve_grid;
  ContourGrid straight_line_grid;

public :
  FeatureManager_sig360_circle_line(const char *json_str);
  int reload(const char *json_str) override;
  int FeatureMatching(acvImage *img,acvImage *buff,vector<acv_LabeledData> &ldData,acvImage *dbg) override;
  static bool check(cJSON *root);
protected:

  int parse_search_key_points_Data(cJSON *kspArr_obj,vector<searchKeyPoint> &skpsList);
  float find_search_key_points_longest_distance(vector<searchKeyPoint> &skpsList);
  int parse_circleData(cJSON * circle_obj);
  int parse_lineData(cJSON * line_obj);
  int parse_signatureData(cJSON * signature_obj);
  int parse_jobj() override;
};

class FeatureManager_sig360_extractor:public FeatureManager_binary_processing {
public :
  FeatureManager_sig360_extractor(const char *json_str);
  int reload(const char *json_str) override;
  int FeatureMatching(acvImage *img,acvImage *buff,vector<acv_LabeledData> &ldData,acvImage *dbg) override;
  static bool check(cJSON *root);
  cJSON *jobj;
protected:
  int parse_jobj() override;
};
#endif
