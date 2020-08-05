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
};


class FeatureManager_binary_processing_group:public FeatureManager_group_proto {
  vector<FeatureManager_binary_processing*> binaryFeatureBundle;

  vector<acv_LabeledData> ldData;
  char subFeatureDefSha1[128];
  acvImage binary_img;
  acvImage ds_binary_img;
  
  FeatureReport_ERROR error;
public :
  FeatureManager_binary_processing_group(const char *json_str);
  int FeatureMatching(acvImage *img) override;
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
  int FeatureMatching(acvImage *img) override;
  static const char* GetFeatureTypeName(){return "group";};
protected:
  int addSubFeature(cJSON * subFeature) override;
  int clearFeatureGroup() override;
  ~FeatureManager_group(){clearFeatureGroup();};
};

#endif
