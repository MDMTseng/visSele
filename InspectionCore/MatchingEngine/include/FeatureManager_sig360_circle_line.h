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
  // Ordered cartesian boundary points (ideal coord) the signature was sampled
  // from. v1 doesn't use this; v2 needs it to re-sample after centroid shift.
  // Populated lazily by convertContourGrid2Signature when v2 is requested.
  vector<acv_XY>cartesian_ideal;
  // Centroid the signature_data was sampled from (ideal coord).
  acv_XY sample_center = {0, 0};
  // |dy| < this threshold skip guard (mm units, same as cartesian_ideal).
  // Mirrors convertContourGrid2Signature's `|diffY| < 0.1` px guard expressed
  // in the cartesian's unit. Populated by v2 init path.
  float dy_skip_thres = 0.0f;
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

  // Re-sample signature_data from cartesian_ideal as seen from `center`.
  // Used by v2 centroid-iteration. Returns false if cartesian_ideal is empty.
  bool resampleFromCenter(acv_XY center);

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
      if(pair.to.x!=pair.to.x)//NAN
      {
        continue;
      }
      float distance=acvDistance(from,pair.from);
      if(distance<0.01)distance=0.01;
      float w=1/distance;


      wvecSum=acvVecAdd(wvecSum,acv_XY(w, w));

      acv_XY vec=acvVecSub(pair.to,pair.from);
      vecSum=acvVecAdd(vecSum,acvVecMult(vec,w));
    }
    // LOGI("vecSum:%f %f wvecSum: %f %f",vecSum.x,vecSum.y,wvecSum.x,wvecSum.y);
    vecSum.x/=wvecSum.x;
    vecSum.y/=wvecSum.y;
    
    LOGI("vecAve:%f %f",vecSum.x,vecSum.y); 
    return acvVecAdd(from,vecSum);
  }



  acv_XY convert_polar(acv_XY from);

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

  // Orientation-signature source: 0 = contour_sig (binary silhouette, default,
  // backward compatible) ; 1 = edge_sig (grayscale gradient edge per ray,
  // lighting/exposure/tint robust). Set via def "matching_method".
  int matching_method = 0;
  float edge_sig_min_strength = 8.0f;
  // Ray-cast sampling step (px) for the edge_sig build. Bigger = faster build
  // (the edge is still sub-pixel via the gradient-peak parabola). 1-2deg drift
  // budget tolerates ~1.5-2.0; default 1.5 (~4x faster than 0.5). Speed knob,
  // no signature-format change.
  float edge_sig_ray_step = 1.5f;

  // Orientation-matching algorithm: 1 = v1 (default, brute-force angle scan at
  // fixed centroid, untouched legacy path); 2 = v2 (xrefine wrapped in
  // centroid-iteration: re-samples signature from cos/sin-LSQ-corrected center
  // each iter, typically lifts the match score 0.05-0.25 when seed centroid
  // is biased by scratch/particle). Set via def "matching_version".
  int matching_version = 1;
  int matching_v2_max_iter = 4;
  float matching_v2_tol_mm = 0.002f;

  float sig_st1_matching_sim_thres;

  //it's the relativesimilar thres  <= 1- min_error/cur_angle_error
  float sigRelativeMatchSimThres=0.9;
  //it's the absolute similar thres <= 1-normalized error = 1-error/sig.mean
  float sigMatchSimThres=0.8;
  //

  vector<FeatureReport_sig360_circle_line_single> reportDataPool;
  vector<FeatureReport_sig360_circle_line_single> reports;

  cv::Mat p_cropImg_cv;       // currently labeled-image or original-image view
  acv_XY cropOffset;
  // labeled image + this object's label idx, kept so search_point_cv can mask out
  // background (dilated object label) and not lock onto background specks/dust.
  int m_objLabel = -1;


  vector<ContourFetch::ptInfo > tmp_points;
  vector<ContourFetch::contourMatchSec > m_sections;
  
public :
  FeatureManager_sig360_circle_line(const char *json_str);
  ~FeatureManager_sig360_circle_line();
  int reload(const char *json_str) override;
  int FeatureMatching(cv::Mat &img_cv) override;
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

  int SingleMatching(int lableIdx,acv_LabeledData *ldData,
  int grid_size, ContourFetch &edge_grid,int scanline_skip, FeatureManager_BacPac *bacpac,
  FeatureReport_sig360_circle_line_single &singleReport,
  vector<ContourFetch::ptInfo > &tmp_points,vector<ContourFetch::contourMatchSec >&m_sections);
};



#endif
