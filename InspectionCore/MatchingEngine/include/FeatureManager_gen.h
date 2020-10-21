#ifndef FeatureManager_GEN_HPP
#define FeatureManager_GEN_HPP

#include "FeatureManager.h"
class FeatureManager_gen:public FeatureManager {

  acvImage buf1,buf2,ImgOutput;
public :
  FeatureManager_gen(const char *json_str);
  int reload(const char *json_str) override;
  int FeatureMatching(acvImage *img) override;
  static const char* GetFeatureTypeName(){return "gen";};
protected:
  int parse_jobj() override;
};


#endif
