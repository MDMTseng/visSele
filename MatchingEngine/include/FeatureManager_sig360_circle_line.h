#ifndef FeatureManagerSIG360_CIRCLE_LINE__HPP
#define FeatureManagerSIG360_CIRCLE_LINE__HPP

#include "FeatureManager.h"



class FeatureManager_sig360_circle_line:public FeatureManager_binary_processing {

  #define FeatureManager_NAME_LENGTH 32
  typedef struct featureDef_circle{
    char name[FeatureManager_NAME_LENGTH];
    acv_Circle circleTar;
    float initMatchingMargin;
  }featureDef_circle;
  typedef struct searchKeyPoint{
    acv_XY searchStart;
    acv_XY searchVec;
    float searchDist;
  }searchKeyPoint;
  typedef struct featureDef_line{
    char name[FeatureManager_NAME_LENGTH];
    acv_Line lineTar;
    acv_XY searchVec;//The vector to searching the contour edge
    acv_XY searchEstAnchor;//The vector to searching the contour edge
    float initMatchingMargin;
    float MatchingMarginX;
    float searchDist;
    vector<searchKeyPoint> skpsList;
  }featureDef_line;

  typedef struct judgeDef{

    char name[FeatureManager_NAME_LENGTH];

    enum{
      AREA,
      SIGMA,
      ANGLE,
      DISTANCE,
    } measure_type;
    enum{
      NONE,
      LINE,
      CIRCLE,
      AUX_LINE,
      AUX_CIRCLE,
      AUX_POINT
    } OBJ1_type,OBJ2_type;
    int priv_OBJ1_idx;
    int priv_OBJ2_idx;
    char OBJ1[FeatureManager_NAME_LENGTH];
    char OBJ2[FeatureManager_NAME_LENGTH];


    float targetVal;
    float targetVal_margin;
  }judgeDef;

  vector<featureDef_circle> featureCircleList;
  vector<featureDef_line> featureLineList;
  vector<judgeDef> judgeList;
  vector<acv_XY> feature_signature;
  vector<acv_XY> tmp_signature;
  ContourGrid inward_curve_grid;
  ContourGrid straight_line_grid;

  vector<FeatureReport_sig360_circle_line_single> reportDataPool;
  vector<FeatureReport_sig360_circle_line_single> reports;
public :
  FeatureManager_sig360_circle_line(const char *json_str);
  int reload(const char *json_str) override;
  int FeatureMatching(acvImage *img,acvImage *buff,vector<acv_LabeledData> &ldData,acvImage *dbg) override;
  static bool check(cJSON *root);
  virtual const FeatureReport* GetReport() override;
  static const char* GetFeatureTypeName(){return "sig360_circle_line";};
protected:

  int parse_search_key_points_Data(cJSON *kspArr_obj,vector<searchKeyPoint> &skpsList);
  float find_search_key_points_longest_distance(vector<searchKeyPoint> &skpsList);
  int parse_circleData(cJSON * circle_obj);
  int parse_lineData(cJSON * line_obj);
  int parse_signatureData(cJSON * signature_obj);
  int parse_judgeData(cJSON * judge_obj);
  int parse_jobj() override;


  int measure_process_L2L(struct judgeDef &judge);


  int FindFeatureDefIndex(vector<featureDef_line> &list, char* name);
  int FindFeatureDefIndex(vector<featureDef_circle> &list, char* name);
};



#endif
