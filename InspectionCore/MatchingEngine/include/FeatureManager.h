#ifndef FeatureManager_HPP
#define FeatureManager_HPP
using namespace std;

#include "FeatureReport.h"
#include "CameraLayer.hpp"

#include "cJSON.h"
#include "LensCalib.h"
#include "FieldCalib.h"
#include <opencv2/core.hpp>


typedef struct FeatureManager_BacPac
{
  ImageSampler *sampler;
  CameraLayer *cam;
  // Lens calibration (telecentric/perspective). When applyLensCalib is true the
  // measurement path undistorts extracted feature points via lensCalib. Default
  // OFF -> no behaviour change until enabled + validated on the rig.
  bool applyLensCalib = false;
  LensCalibResult *lensCalib = 0;
  // Bright/Dark field calibration. Persisted as data/field_calib.json and
  // loaded at startup. applyFieldCal default OFF -- consumer (e.g.
  // FeatureManager_sig360_circle_line) decides when to gate on it.
  bool applyFieldCal = false;
  FieldCalibResult *fieldCal = 0;
  // Phase 2: sub-features that need the raw binary (CV_8UC1, bg=255/fg=0)
  // -- e.g. matching_version=2 sig360 doing morph-boundary signature build --
  // read this pointer. Group sets it before dispatching to sub-features.
  // nullptr when not available; consumers fall back to legacy paths.
  cv::Mat *binary_uc1_for_phase2 = nullptr;
}FeatureManager_BacPac;

class FeatureManager {
  protected:
  FeatureReport report;
  FeatureManager_BacPac *bacpac;
  cJSON *root;
  virtual int parse_jobj()=0;
public :
  FeatureManager(const char *json_str){
    ClearReport();
  };
  void setBacPac(FeatureManager_BacPac *bacpac){this->bacpac=bacpac;};
  virtual int reload(const char *json_str)=0;

  // Canonical engine entry. Subclasses must override.
  virtual int FeatureMatching(cv::Mat &img_cv) = 0;
  
  virtual cJSON * SetParam(cJSON *json_str){return cJSON_CreateNull();}
  // Shape-based localizer visualization round-trip ("生成特徵點"): return the trained
  // line2Dup feature points + ROI points as cJSON {"features":[{x,y}...],"roi":[...]}
  // in object-frame mm, or NULL if this manager has no trained shape localizer. Groups
  // forward to their sub-features. Caller owns the returned cJSON.
  virtual cJSON * getShapeFeaturePointsJson(){return NULL;}
  virtual const FeatureReport* GetReport(){return &report;};
  virtual void ClearReport(){
    bacpac=NULL;
    report.type=FeatureReport::NONE;
    report.bacpac=bacpac;
  };
  static const char* GetFeatureTypeName(){return NULL;};
  virtual ~FeatureManager(){};

};
#endif
