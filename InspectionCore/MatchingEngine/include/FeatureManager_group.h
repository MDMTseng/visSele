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
  cv::Mat binary_img_storage;    // CV_8UC1 binary mask (Phase 1+)
  cv::Mat labeled_img_storage;   // CV_8UC3 BGR-packed labels for legacy walker

  FeatureReport_ERROR error;
public :
  FeatureManager_binary_processing_group(const char *json_str);
  int FeatureMatching(cv::Mat &img_cv) override;
  virtual const FeatureReport* GetReport() override;
  virtual void ClearReport() override;
  // Forward to the first sub-feature with a trained shape localizer (SF round-trip).
  cJSON *getShapeFeaturePointsJson() override;
  static const char* GetFeatureTypeName(){return "binary_processing_group";};


protected:
  double intrusionSizeLimitRatio=0;
  // Pre-binarization downsample (1 = off, 2/4 valid). Threshold + CCL + signature
  // build all run at this reduced resolution; per-label coords get scaled back
  // up via the existing dsampLevel plumbing so sub-features still address
  // measurement at original-image resolution. Set via def "inspection_downsample".
  int inspection_downsample = 1;
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
