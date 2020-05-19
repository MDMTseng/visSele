#ifndef FeatureManager_HPP
#define FeatureManager_HPP
using namespace std;

#include "FeatureReport.h"
#include "CameraLayer.hpp"
#include "acvImage_ComponentLabelingTool.hpp"

#include "cJSON.h"


typedef struct FeatureManager_BacPac
{
  ImageSampler *sampler;
  CameraLayer *cam;
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
  virtual int FeatureMatching(acvImage *img)=0;
  virtual const FeatureReport* GetReport(){return NULL;};
  virtual void ClearReport(){
    bacpac=NULL;
    report.type=FeatureReport::NONE;
    report.bacpac=bacpac;
  };
  static const char* GetFeatureTypeName(){return NULL;};
  virtual ~FeatureManager(){};

};
#endif
