#ifndef FeatureManager_GEN_HPP
#define FeatureManager_GEN_HPP

#include "FeatureManager.h"
class FeatureManager_gen:public FeatureManager {

  acvImage buf1,buf2,ImgOutput;
public :
  FeatureManager_gen(const char *json_str);
  ~FeatureManager_gen();
  int reload(const char *json_str) override;
  int FeatureMatching(acvImage *img) override;
  

  void ClearReport() override;
  cJSON * SetParam(cJSON *json_str) override;
  static const char* GetFeatureTypeName(){return "gen";};
protected:
  int parse_jobj() override;
  cJSON * root;

  int inspectionStage=-1;

  int HFrom=100;
  int HTo=0;
  int SMax=255;
  int SMin=80;
  int VMax=255;
  int VMin=100;

  int boxFilter1_Size=3;
  int boxFilter1_thres=30;
  int boxFilter2_Size=3;
  int boxFilter2_thres=30;


  float targetHeadWHRatio=4.0;
  float minHeadArea=45*45*targetHeadWHRatio;//90*350*0.9;
  float targetHeadWHRatioMargin=1.2;
  float FacingThreshold=1.2;
  
  float cableSeachingRatio=0.2;


  int cableCount=12;

  int cableTableCount=2;
};


#endif
