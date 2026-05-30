#ifndef FeatureManager_group_HPP
#define FeatureManager_group_HPP

#include "FeatureReport.h"
#include "FeatureManager.h"
#include <vector>
#include <cstdlib>
#include <ctime>
#include "cJSON.h"
#include <string>
#include "FeatureManager_binary_processing.h"
#include <opencv2/core.hpp>


class FeatureManager_group_proto:public FeatureManager {
public :
  FeatureManager_group_proto(const char *json_str): FeatureManager(json_str){
    ClearReport();
  };
  int reload(const char *json_str) override;
protected:
  vector<const FeatureReport*> sub_reports;
  virtual int addSubFeature(cJSON * subFeature)=0;
  virtual int clearFeatureGroup()=0;
  int parse_jobj() override;
  float briThres;

  // Per-region adaptive threshold for background-evenness correction (backlit).
  // bgThreshMap is a low-res (bgMapW x bgMapH) grid of T = D + ratio*(B-D),
  // bilinearly interpolated over the image at binarization time.
  bool useAdaptiveThres = false;
  float edgeRatio = 0.5f;
  int bgMapW = 0, bgMapH = 0;
  std::vector<float> bgThreshMap;
  // When true, bgThreshMap is built lazily from the per-camera background model
  // (sampler->stageLightInfo) at match time, instead of from the def arrays.
  bool useCalibBackground = false;
  float darkLevel = 0.0f; // dark-field level D (sensor offset); 0 until D-field exists

  // Calibration-free vignette/illumination-tolerant binarization:
  // estimate the smooth background by morphological close and flat-field divide.
  // def: "binarize":"bg_flatten", "bg_close_kernel", "bg_ratio", "bg_downscale".
  int binarize_method = 0;   // 0=threshold(global/adaptive), 1=bg_flatten
  int bg_close_kernel = 81;  // must exceed the largest object
  float bg_ratio = 0.5f;
  int bg_downscale = 4;
};


class FeatureManager_binary_processing_group:public FeatureManager_group_proto {
  vector<FeatureManager_binary_processing*> binaryFeatureBundle;

  vector<acv_LabeledData> ldData;
  char subFeatureDefSha1[128];
  // acv -> cv migration: the binary images are now stored in cv::Mat. The
  // acvImage objects are thin shims that share the cv::Mat memory via
  // useExtBuffer, so the not-yet-migrated acv* consumers (acvComponentLabeling_cv,
  // acvThresholdMap, binaryDownScale) keep working with no copy. Pair members
  // need to be re-bound whenever the storage Mat is reallocated.
  cv::Mat binary_img_storage;
  cv::Mat ds_binary_img_storage;
  acvImage binary_img;
  acvImage ds_binary_img;
  
  FeatureReport_ERROR error;
public :
  FeatureManager_binary_processing_group(const char *json_str);
  int FeatureMatching(cv::Mat &img_cv) override;
  virtual const FeatureReport* GetReport() override;
  virtual void ClearReport() override;
  static const char* GetFeatureTypeName(){return "binary_processing_group";};


protected:
  double intrusionSizeLimitRatio=0;
  int addSubFeature(cJSON * subFeature) override;
  int clearFeatureGroup() override;
  ~FeatureManager_binary_processing_group(){clearFeatureGroup();};
  int parse_jobj() override;

};

class FeatureManager_group:public FeatureManager_group_proto {
  vector<FeatureManager*> featureBundle;

public :
  FeatureManager_group(const char *json_str);
  int FeatureMatching(cv::Mat &img) override;
  static const char* GetFeatureTypeName(){return "group";};
protected:
  int addSubFeature(cJSON * subFeature) override;
  int clearFeatureGroup() override;
  ~FeatureManager_group(){clearFeatureGroup();};
};

#endif
