
#ifndef FeatureManager_BINARY_PROCESSING_HPP
#define FeatureManager_BINARY_PROCESSING_HPP

#include "FeatureManager.h"



class FeatureManager_binary_processing:public FeatureManager {

protected:
  virtual int parse_jobj()=0;
  acvImage *originalImage;
  vector<acv_LabeledData> *_ldData;
public :
  FeatureManager_binary_processing(const char *json_str):FeatureManager(json_str){
  ClearReport();};
  virtual int reload(const char *json_str)=0;
  void setOriginalImage(acvImage *oriImage){this->originalImage = oriImage;};
  void setLabeledData(vector<acv_LabeledData> *ldData){this->_ldData = ldData;};
  
  void ClearReport(){FeatureManager::ClearReport();};
};



class FeatureManager_sig360_extractor:public FeatureManager_binary_processing {

  vector<acv_XY> signature;
  vector<acv_XY> tmp_contour;
  vector<acv_CircleFit> detectedCircles;
  vector<acv_LineFit> detectedLines;
public :
  FeatureManager_sig360_extractor(const char *json_str);
  int reload(const char *json_str) override;
  int FeatureMatching(acvImage *img) override;
  cJSON *jobj;
  virtual const FeatureReport* GetReport() override;
  virtual void ClearReport() override;
  static const char* GetFeatureTypeName(){return "sig360_extractor";};
protected:
  int parse_jobj() override;
};

#endif
