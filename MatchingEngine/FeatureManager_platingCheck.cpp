#include "FeatureManager_platingCheck.h"
#include "logctrl.h"
#include <stdexcept>
#include <common_lib.h>
#include <MatchingCore.h>

/*
  FeatureManager_platingCheck Section
*/
FeatureManager_platingCheck::FeatureManager_platingCheck(const char *json_str): FeatureManager(json_str)
{

  //LOGI(">>>>%s>>>>",json_str);
  root= NULL;
  int ret = reload(json_str);
  if(ret)
    throw std::invalid_argument( "Error:FeatureManager_platingCheck failed... " );
}

bool FeatureManager_platingCheck::check(cJSON *root)
{
  char *str;
  LOGI("FeatureManager_platingCheck>>>");
  if(!(getDataFromJsonObj(root,"type",(void**)&str)&cJSON_String))
  {
    return false;
  }
  if (strcmp("sig360_circle_line",str) == 0)
  {
    return true;
  }
  return false;
}
int FeatureManager_platingCheck::parse_jobj()
{


  return 0;
}


int FeatureManager_platingCheck::reload(const char *json_str)
{
  if(root)
  {
    cJSON_Delete(root);
  }

  root = cJSON_Parse(json_str);
  if(root==NULL)
  {
    LOGE("cJSON parse failed");
    return -1;
  }
  int ret_err = parse_jobj();
  if(ret_err!=0)
  {
    reload("");
    return -2;
  }
  return 0;
}

int FeatureManager_platingCheck::FeatureMatching(acvImage *img,acvImage *buff,acvImage *dbg)
{


  //LOGI(">>>>>>>>");
  return 0;
}
