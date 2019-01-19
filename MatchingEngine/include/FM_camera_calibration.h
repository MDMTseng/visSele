#ifndef FMCAMERACALIBRATION__HPP
#define FMCAMERACALIBRATION__HPP

#include "FeatureManager.h"
#include "FeatureReport.h"

#include "FeatureManager_binary_processing.h"
#include <ContourGrid.h>


class FM_camera_calibration:public FeatureManager_binary_processing {

public :
  FM_camera_calibration(const char *json_str);
  int reload(const char *json_str) override;
  int FeatureMatching(acvImage *img) override;
  cJSON *jobj;
  virtual const FeatureReport* GetReport() override;
  static const char* GetFeatureTypeName(){return "camera_calibration";};
protected:
  int parse_jobj() override;
};

#endif
