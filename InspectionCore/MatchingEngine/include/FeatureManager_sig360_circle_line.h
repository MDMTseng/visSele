#ifndef FeatureManagerSIG360_CIRCLE_LINE__HPP
#define FeatureManagerSIG360_CIRCLE_LINE__HPP

#include "FeatureManager.h"
#include "FeatureReport.h"

#include "logctrl.h"
#include "FeatureManager_binary_processing.h"
#include <ContourGrid.h>
#include <MatchingCore.h>



class ContourSignature
{
public :
  vector<acv_XY>signature_data;
  float mean;
  float sigma;
  float angleOffset;

  ContourSignature(cJSON*);
  ContourSignature(int Len=0);
  int CalcInfo();
  int RELOAD(cJSON*);

  int RESET(int Len);

  float match_min_error(ContourSignature &s,
    float searchAngleOffset,float searchAngleRange,int facing,
    bool *ret_isInv, float *ret_angle);

  void match_span(ContourSignature &s,
    float offset1,float offset2,int count,vector<acv_XY> &error,float stride,bool flip);

};


class IMGMoment//TODO
{
  typedef struct PQ{
    int P,Q,M;
  }PQ;
  vector<PQ> mSet;

  IMGMoment()
  {

  }

  void update(acvImage *img, acv_XY TL, acv_XY WH)
  {

  }


};
class ConstrainMap
{
  public:
  typedef struct anchorPair{
    acv_XY from;
    acv_XY to;
    acv_XY constrainVector;
  }anchorPair;
  
  acv_XY center;
  vector<anchorPair> anchorPairs;
  ConstrainMap()
  {

  }

  void add(acv_XY from,acv_XY to,acv_XY constrainVector)
  {
    anchorPairs.push_back((anchorPair){from:from,to:to,constrainVector:acvVecNormalize(constrainVector)});
  }

  int size()
  {
    return anchorPairs.size();
  }
  void clear()
  {
    anchorPairs.clear();
  }



  acv_XY convert_Ave(acv_XY from)
  {
    acv_XY wvecSum={0,0};
    acv_XY vecSum={0,0};

    for(int i=0;i<anchorPairs.size();i++)
    {
      anchorPair pair = anchorPairs[i];
      if(pair.to.X!=pair.to.X)//NAN
      {
        continue;
      }
      float distance=acvDistance(from,pair.from);
      if(distance<0.01)distance=0.01;
      float w=1/distance;


      wvecSum=acvVecAdd(wvecSum,(acv_XY){w,w});

      acv_XY vec=acvVecSub(pair.to,pair.from);
      vecSum=acvVecAdd(vecSum,acvVecMult(vec,w));
    }
    // LOGI("vecSum:%f %f wvecSum: %f %f",vecSum.X,vecSum.Y,wvecSum.X,wvecSum.Y);
    vecSum.X/=wvecSum.X;
    vecSum.Y/=wvecSum.Y;
    
    LOGI("vecAve:%f %f",vecSum.X,vecSum.Y); 
    return acvVecAdd(from,vecSum);
  }



  acv_XY convert_polar(acv_XY from);

  acv_XY convert_vec(acv_XY from);
  acv_XY convert(acv_XY from);
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
  ConstrainMap cm;
  vector<acv_XY>signature_data_buffer;
  ContourFetch edge_grid;

  float matching_angle_margin;
  float matching_angle_offset;
  int matching_face;
  bool matching_without_signature;
  float single_result_area_ratio;


  vector<FeatureReport_sig360_circle_line_single> reportDataPool;
  vector<FeatureReport_sig360_circle_line_single> reports;

  acvImage buff1;
  acvImage buff2;
  
  acvImage *p_cropImg;
  acvImage _cropImg;
  acv_XY cropOffset;


  vector<ContourFetch::ptInfo > tmp_points;
  vector<ContourFetch::contourMatchSec > m_sections;
  
public :
  FeatureManager_sig360_circle_line(const char *json_str);
  ~FeatureManager_sig360_circle_line();
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


  FeatureReport_searchPointReport searchPoint_process(
  FeatureReport_sig360_circle_line_single &report, acv_XY center,
  float sine,float cosine,float flip_f,float thres,
  featureDef_searchPoint &def,edgeTracking &eT);

  FeatureReport_lineReport LineMatching_ReportGen(
  featureDef_line *plineDef,edgeTracking &eT,
  acv_XY calibCen,float mmpp,float cached_cos,float cached_sin,float flip_f);



  FeatureReport_circleReport CircleMatching_ReportGen(
  featureDef_circle *plineDef,edgeTracking &eT,
  acv_XY calibCen,float mmpp,float cached_cos,float cached_sin,float flip_f);

  FeatureReport_searchPointReport SPointMatching_ReportGen(
  featureDef_searchPoint *def,
  FeatureReport_sig360_circle_line_single &singleReport,
  edgeTracking &eT,
  acv_XY calibCen,float mmpp,float cached_cos,float cached_sin,float flip_f);


  FeatureReport_auxPointReport APointMatching_ReportGen(
  featureDef_auxPoint *def,
  FeatureReport_sig360_circle_line_single &singleReport,
  float sine,float cosine,float flip_f
  );

  FeatureReport_judgeReport Judge_ReportGen(
    FeatureReport_judgeDef *def,
    FeatureReport_sig360_circle_line_single &singleReport,
    float sine,float cosine,float flip_f
  );
  
  int TreeExecution(int id,
    FeatureReport_sig360_circle_line_single &singleReport,
    edgeTracking &eT,
    acv_XY calibCen,float mmpp,float cached_cos,float cached_sin,float flip_f);

  int TreeExecution(
    FeatureReport_sig360_circle_line_single &singleReport,
    edgeTracking &eT,
    acv_XY calibCen,float mmpp,float cached_cos,float cached_sin,float flip_f);


  int FindFeatureDefIndex(int feature_id,FEATURETYPE *ret_type);
  int FindFeatureReportIndex(FeatureReport_sig360_circle_line_single &report,int feature_id,FEATURETYPE *ret_type);
  int ParseMainVector(float flip_f,FeatureReport_sig360_circle_line_single &report,int feature_id, acv_XY *vec);
  int ParseLocatePosition(FeatureReport_sig360_circle_line_single &report,int feature_id, acv_XY *pt);
  int lineCrossPosition(float flip_f,FeatureReport_sig360_circle_line_single &report,int line1_id,int line2_id, acv_XY *pt);

  acv_XY ParseMainVector(featureDef_searchPoint *def_sp);

  int SingleMatching(acvImage *originalImage,acvImage *labeledBuff,acvImage *binarizedBuff,acvImage* buffer_img,
  int lableIdx,acv_LabeledData *ldData,
  int grid_size, ContourFetch &edge_grid,int scanline_skip, FeatureManager_BacPac *bacpac,
  FeatureReport_sig360_circle_line_single &singleReport,
  vector<ContourFetch::ptInfo > &tmp_points,vector<ContourFetch::contourMatchSec >&m_sections);
};



#endif
