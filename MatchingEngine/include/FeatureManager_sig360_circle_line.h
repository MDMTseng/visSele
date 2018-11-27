#ifndef FeatureManagerSIG360_CIRCLE_LINE__HPP
#define FeatureManagerSIG360_CIRCLE_LINE__HPP

#include "FeatureManager.h"
#include "FeatureReport.h"

#include "FeatureManager_binary_processing.h"
#include <ContourGrid.h>



class FeatureManager_sig360_circle_line:public FeatureManager_binary_processing {

  typedef enum FEATURETYPE {
    
    NA,
    LINE,
    ARC,
    AUX_POINT,
    SEARCH_POINT,
    MEASURE
    }; 
  vector<featureDef_circle> featureCircleList;
  vector<featureDef_line> featureLineList;
  vector<FeatureReport_judgeDef> judgeList;
  
  vector<featureDef_auxPoint> auxPointList;
  vector<featureDef_searchPoint> searchPointList;
  int signature_feature_id;
  vector<acv_XY> feature_signature;
  vector<acv_XY> tmp_signature;
  ContourGrid inward_curve_grid;
  ContourGrid straight_line_grid;

  vector<FeatureReport_sig360_circle_line_single> reportDataPool;
  vector<FeatureReport_sig360_circle_line_single> reports;


  
  vector<int> s_intersectIdxs;
  vector<acv_XY> s_points;
public :
  FeatureManager_sig360_circle_line(const char *json_str);
  int reload(const char *json_str) override;
  int FeatureMatching(acvImage *img,acvImage *buff,vector<acv_LabeledData> &ldData,acvImage *dbg) override;
  static bool check(cJSON *root);
  virtual const FeatureReport* GetReport() override;
  static const char* GetFeatureTypeName(){return "sig360_circle_line";};
protected:

  int parse_search_key_points_Data(cJSON *kspArr_obj,vector<featureDef_line::searchKeyPoint> &skpsList);
  float find_search_key_points_longest_distance(vector<featureDef_line::searchKeyPoint> &skpsList);
  int parse_arcData(cJSON * circle_obj);
  int parse_lineData(cJSON * line_obj);
  int parse_auxPointData(cJSON * auxPoint_obj);
  int parse_searchPointData(cJSON * searchPoint_obj);
  int parse_sign360(cJSON * signature_obj);
  int parse_judgeData(cJSON * judge_obj);
  int parse_jobj() override;


  FeatureReport_judgeReport measure_process(FeatureReport_sig360_circle_line_single &report, 
  float sine,float cosine,float flip_f,
  FeatureReport_judgeDef &judge);

  FeatureReport_auxPointReport auxPoint_process(FeatureReport_sig360_circle_line_single &report,
  float sine,float cosine,float flip_f,
  featureDef_auxPoint &def);

  FeatureReport_searchPointReport searchPoint_process(acvImage *labeledImg,int labelId,acv_LabeledData labeledData,
  FeatureReport_sig360_circle_line_single &report, 
  float sine,float cosine,float flip_f,
  featureDef_searchPoint &def);

  int FindFeatureDefIndex(int feature_id,FEATURETYPE *ret_type);
  int FindFeatureReportIndex(FeatureReport_sig360_circle_line_single &report,int feature_id,FEATURETYPE *ret_type);
  int ParseMainVector(FeatureReport_sig360_circle_line_single &report,int feature_id, acv_XY *vec);
  int ParseLocatePosition(FeatureReport_sig360_circle_line_single &report,int feature_id, acv_XY *pt);
  int lineCrossPosition(FeatureReport_sig360_circle_line_single &report,int line1_id,int line2_id, acv_XY *pt);
};



#endif
