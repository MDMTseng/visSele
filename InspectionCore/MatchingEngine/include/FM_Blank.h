#ifndef FeatureManager_FM_BLANK_HPP
#define FeatureManager_FM_BLANK_HPP





#include "FeatureManager.h"



class FM_Blank:public FeatureManager {

  
  acvImage backGroundTemplate;

public :
  FM_Blank(const char *json_str);
  ~FM_Blank();
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
  int inspectionType;
  int inspectionStage=-1;
};


#endif
