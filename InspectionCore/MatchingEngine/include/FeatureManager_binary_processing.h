
#ifndef FeatureManager_BINARY_PROCESSING_HPP
#define FeatureManager_BINARY_PROCESSING_HPP

#include "FeatureManager.h"



class FeatureManager_binary_processing:public FeatureManager {

protected:
  virtual int parse_jobj()=0;
  acvImage *originalImage;
  // Auto-shim populated by setOriginalImage(cv::Mat&); originalImage points
  // into this so the still-acv subclasses (sig360_circle_line) keep working
  // without the caller needing to maintain an acvImage on the stack.
  acvImage _orig_shim;
  vector<acv_LabeledData> *_ldData;
  int dsampLevel=1;
public :
  FeatureManager_binary_processing(const char *json_str):FeatureManager(json_str){
  ClearReport();};
  virtual int reload(const char *json_str)=0;
  void setOriginalImage(acvImage *oriImage){this->originalImage = oriImage;};
  void setOriginalImage(cv::Mat &oriImageCv){
    _orig_shim.useExtBuffer(oriImageCv.data,
                            (int)(oriImageCv.total() * oriImageCv.elemSize()),
                            oriImageCv.cols, oriImageCv.rows);
    this->originalImage = &_orig_shim;
  };
  void setLabeledData(vector<acv_LabeledData> *ldData){this->_ldData = ldData;};
  void setLabelDownSampLevel(int dsampLevel){this->dsampLevel=dsampLevel;};
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
