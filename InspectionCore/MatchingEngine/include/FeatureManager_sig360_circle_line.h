#ifndef FeatureManagerSIG360_CIRCLE_LINE__HPP
#define FeatureManagerSIG360_CIRCLE_LINE__HPP

#include "FeatureManager.h"
#include "FeatureReport.h"

#include "FeatureManager_binary_processing.h"
#include <ContourGrid.h>



class ContourSignature
{
public :
  vector<acv_XY>signature_data;
  float mean;
  float sigma;

  ContourSignature(cJSON*);
  ContourSignature(int Len=0);
  int CalcInfo();
  int RELOAD(cJSON*);

  int RESET(int Len);

  float match_min_error(ContourSignature &s,
    float searchAngleOffset,float searchAngleRange,int facing,
    bool *ret_isInv, float *ret_angle);

};


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
  ContourSignature feature_signature;
  ContourSignature tmp_signature;
  ContourFetch edge_grid;

  float matching_angle_margin;
  float matching_angle_offset;
  int matching_face;
  bool matching_without_signature;


  vector<FeatureReport_sig360_circle_line_single> reportDataPool;
  vector<FeatureReport_sig360_circle_line_single> reports;

  acvImage buff1;
  acvImage buff2;
  
  vector<ContourFetch::ptInfo > tmp_points;
  vector<ContourFetch::contourMatchSec > m_sections;
  
public :
  FeatureManager_sig360_circle_line(const char *json_str);
  int reload(const char *json_str) override;
  int FeatureMatching(acvImage *img) override;
  virtual const FeatureReport* GetReport() override;
  virtual void ClearReport() override;
  static const char* GetFeatureTypeName(){return "sig360_circle_line";};
protected:

  //int parse_search_key_points_Data(cJSON *kspArr_obj,vector<featureDef_line::searchKeyPoint> &skpsList);
  //float find_search_key_points_longest_distance(vector<featureDef_line::searchKeyPoint> &skpsList);
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

  FeatureReport_searchPointReport searchPoint_process(acvImage *grayLevelImg,acvImage *labeledImg,int labelId,acv_LabeledData labeledData,
  FeatureReport_sig360_circle_line_single &report, 
  float sine,float cosine,float flip_f,float thres,
  featureDef_searchPoint &def);

  int FindFeatureDefIndex(int feature_id,FEATURETYPE *ret_type);
  int FindFeatureReportIndex(FeatureReport_sig360_circle_line_single &report,int feature_id,FEATURETYPE *ret_type);
  int ParseMainVector(float flip_f,FeatureReport_sig360_circle_line_single &report,int feature_id, acv_XY *vec);
  int ParseLocatePosition(FeatureReport_sig360_circle_line_single &report,int feature_id, acv_XY *pt);
  int lineCrossPosition(float flip_f,FeatureReport_sig360_circle_line_single &report,int line1_id,int line2_id, acv_XY *pt);


  int SingleMatching(acvImage *originalImage,acvImage *labeledBuff,acvImage *binarizedBuff,acvImage* buffer_img,
  int lableIdx,acv_LabeledData *ldData,
  int grid_size, ContourFetch &edge_grid,int scanline_skip, FeatureManager_BacPac *bacpac,
  FeatureReport_sig360_circle_line_single &singleReport,float angle,float flip_f,
  vector<ContourFetch::ptInfo > &tmp_points,vector<ContourFetch::contourMatchSec >&m_sections);
};



#endif
