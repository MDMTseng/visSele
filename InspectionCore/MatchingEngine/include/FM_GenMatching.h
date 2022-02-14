#ifndef FeatureManager_FM_GEN_HPP
#define FeatureManager_FM_GEN_HPP





#include "SBM_if.hpp"
#include "FeatureManager.h"



class FM_GenMatching:public FeatureManager {

  
  acvImage DefTemplate;

public :
  FM_GenMatching(const char *json_str);
  ~FM_GenMatching();
  int reload(const char *json_str) override;
  int FeatureMatching(acvImage *img) override;
  
  int FeatureMatching0(acvImage *img);
  int FeatureMatching1(acvImage *img);

  void ClearReport() override;
  cJSON * SetParam(cJSON *json_str) override;
  int SetInspInfo(cJSON *jsonParam);
  static const char* GetFeatureTypeName(){return "FM_GenMatching";};
  
  struct region{
    float x,y,w,h;
    float rel_x,rel_y;
  };
protected:
  int parse_jobj() override;


  void resetSBMIF();
  SBM_if* getSBMIF();
  cJSON * root;
  int inspectionType;
  int inspectionStage=-1;
  int thres;
  char inspType[30];

  int ROI[4]={-1};
  float downScale=1;
  std::vector<region> locatingBlocks;
  std::vector<cv::Mat> locatingBlockImg;



  cv::Point2f temp_tp_anchorPt;
  SBM_if *p_sbmif=NULL;
  string templateBaseName="AAA";
};


#endif
