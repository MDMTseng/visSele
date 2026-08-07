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
  // Inspection region -- the station. An object whose centre falls outside it is
  // not ours to judge, so the locator drops it before doing any work on it.
  //
  // FULL-SENSOR pixels, i.e. the same frame the lens model uses, NOT the def's
  // object frame. That is deliberate: this describes where the part sits when
  // the camera fires, which is a property of the machine and its mechanics, not
  // of the product. It rides on the BacPac for the same reason lensCalib and
  // fieldCal do -- this struct is the machine's context, the def is the
  // product's. It comes from data/machine_setting.json, set from InspectionUI.
  //
  // w <= 0 or h <= 0 = no region configured = every object is a candidate
  // (the behaviour before this existed).
  //
  // A locator works in inspection-image pixels at its own downsample level, so
  // the comparison is
  //     full_px = centre * dsampLevel + sampler->getOriginOffset()
  // where originOffset is the camera's hardware ROI. Nothing is cropped and no
  // coordinate frame moves -- this is a filter, not a crop, precisely so that
  // the imgOffset/cropOffset assumptions scattered through the measurement code
  // stay untouched.
  float insp_region_x = 0, insp_region_y = 0;
  float insp_region_w = 0, insp_region_h = 0;
  bool hasInspRegion() const { return insp_region_w > 0 && insp_region_h > 0; }
  bool inInspRegion(float full_px_x, float full_px_y) const {
    if (!hasInspRegion()) return true;
    return full_px_x >= insp_region_x && full_px_x < insp_region_x + insp_region_w &&
           full_px_y >= insp_region_y && full_px_y < insp_region_y + insp_region_h;
  }
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
