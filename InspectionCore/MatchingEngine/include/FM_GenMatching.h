#ifndef FeatureManager_GEN_MATCHING_HPP
#define FeatureManager_GEN_MATCHING_HPP





#include "FeatureManager.h"


typedef struct regionInfo_single{
  acv_XY pt1,pt2;
  acv_XY normalized_pt1,normalized_pt2;
  float margin;
  int id;
  float RGBA[4];
  float sgRGBA[4];

  struct {
    
    float RGBA[4];
    float diff;
    int count;
    float max_window2wire_width_ratio;

  }inspRes;

//======= inspection return

}regionInfo_single;

class FM_GenMatching:public FeatureManager {
  vector<regionInfo_single> regionInfo;

  acvImage buf1,buf2,ImgOutput;
  acvImage backGroundTemplate;
  bool backgroundFlag=false;


  

  struct{
    acv_XY pos;

  }insp02;

public :
  FM_GenMatching(const char *json_str);
  ~FM_GenMatching();
  int reload(const char *json_str) override;
  int FeatureMatching(acvImage *img) override;
  
  int FeatureMatching0(acvImage *img);
  int FeatureMatching1(acvImage *img);

  void ClearReport() override;
  cJSON * SetParam(cJSON *json_str) override;

  cJSON * SetParam0(cJSON *json_str);
  cJSON * SetParam1(cJSON *json_str);


  static const char* GetFeatureTypeName(){return "gen";};
protected:
  int parse_jobj() override;
  cJSON * root;

  int inspectionStage=-1;
  int inspectionType=0;

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
  
  float cableSeachingRatio=0.3;


  int cableCount=12;

  int cableTableCount=2;



};


#endif
