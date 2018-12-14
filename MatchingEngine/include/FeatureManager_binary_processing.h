
#ifndef FeatureManager_BINARY_PROCESSING_HPP
#define FeatureManager_BINARY_PROCESSING_HPP

#include "FeatureManager.h"



class FeatureManager_binary_processing:public FeatureManager {

protected:
  virtual int parse_jobj()=0;
  acvImage *originalImage;
public :
  FeatureManager_binary_processing(const char *json_str):FeatureManager(json_str){};
  virtual int reload(const char *json_str)=0;
  virtual int FeatureMatching(acvImage *img,acvImage *buff,acvImage *dbg){return -1;};
  virtual int FeatureMatching(acvImage *img,acvImage *buff,vector<acv_LabeledData> &ldData,acvImage *dbg)=0;
  void setOriginalImage(acvImage *oriImage){this->originalImage = oriImage;};
};



class FeatureManager_sig360_extractor:public FeatureManager_binary_processing {

  vector<acv_XY> signature;
  vector<acv_CircleFit> detectedCircles;
  vector<acv_LineFit> detectedLines;
public :
  FeatureManager_sig360_extractor(const char *json_str);
  int reload(const char *json_str) override;
  int FeatureMatching(acvImage *img,acvImage *buff,vector<acv_LabeledData> &ldData,acvImage *dbg) override;
  static bool check(cJSON *root);
  cJSON *jobj;
  virtual const FeatureReport* GetReport() override;
  static const char* GetFeatureTypeName(){return "sig360_extractor";};
protected:
  int parse_jobj() override;
};
#endif
