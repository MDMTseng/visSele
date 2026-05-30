#ifndef FeatureManager_HPP
#define FeatureManager_HPP
using namespace std;

#include "FeatureReport.h"
#include "CameraLayer.hpp"
#include "acvImage_ComponentLabelingTool.hpp"

#include "cJSON.h"
#include "LensCalib.h"
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
}FeatureManager_BacPac;

class FeatureManager {
  protected:
  FeatureReport report;
  FeatureManager_BacPac *bacpac;
  cJSON *root;
  virtual int parse_jobj()=0;
  acvImage _buff;
public :
  //static bool check(cJSON *root);
  FeatureManager(const char *json_str){
    ClearReport();
  };
  void setBacPac(FeatureManager_BacPac *bacpac){this->bacpac=bacpac;};
  virtual int reload(const char *json_str)=0;

  // Both FeatureMatching overloads are now non-pure with mutual bridges.
  // Subclasses override either one (or both, when they care about avoiding
  // the shim cost). A per-instance re-entry guard aborts cleanly if neither
  // is overridden (would otherwise loop forever).
private:
  bool _fm_inflight = false;
public:
  virtual int FeatureMatching(acvImage *img) {
    if (_fm_inflight || img == NULL) return -1;
    _fm_inflight = true;
    cv::Mat view(img->GetHeight(), img->GetWidth(), CV_8UC3, img->CVector[0]);
    int rc = FeatureMatching(view);
    _fm_inflight = false;
    return rc;
  }
  virtual int FeatureMatching(cv::Mat &img_cv) {
    if (_fm_inflight || img_cv.empty()) return -1;
    if (!img_cv.isContinuous()) img_cv = img_cv.clone();
    _fm_inflight = true;
    acvImage _shim;
    _shim.useExtBuffer(img_cv.data,
                       (int)(img_cv.total() * img_cv.elemSize()),
                       img_cv.cols, img_cv.rows);
    int rc = FeatureMatching(&_shim);
    _fm_inflight = false;
    return rc;
  }
  
  virtual cJSON * SetParam(cJSON *json_str){return cJSON_CreateNull();}
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
