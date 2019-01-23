#include "MatchingEngine.h"
#include "include_priv/MatchingCore.h"
#include "FeatureManager_sig360_circle_line.h"
#include "FeatureManager_platingCheck.h"
#include "logctrl.h"
#include <common_lib.h>
#include "FeatureReport_UTIL.h"

int MatchingEngine::ResetFeature()
{
  for(int i=0;i<featureBundle.size() ;i++)
  {
    delete(featureBundle[i]);
  }
  featureBundle.resize(0);
  return 0;
}


int MatchingEngine::AddMatchingFeature(FeatureManager *featureSet)
{
  if(featureSet!=NULL)
  {
    featureBundle.push_back(featureSet);
    return 0;
  }
  return -1;
}

int MatchingEngine::AddMatchingFeature(const char *json_str)
{

  FeatureManager *featureSet=NULL;

  cJSON *root = cJSON_Parse(json_str);
  if(root == NULL)
  {
    return -1;
  }

  char *str=JFetch_STRING(root,"type");
  if(str==NULL)
  {
    return -1;
  }

  if(strcmp(FeatureManager_group::GetFeatureTypeName(),str) == 0)
  {

    LOGI("FeatureManager_group is the type...");
    featureSet = new FeatureManager_group(json_str);
  }
  else if(strcmp(FeatureManager_binary_processing_group::GetFeatureTypeName(),str) == 0)
  {

    LOGI("FeatureManager_binary_processing_group is the type...");
    featureSet = new FeatureManager_binary_processing_group(json_str);
  }
  else if(strcmp(FeatureManager_platingCheck::GetFeatureTypeName(),str) == 0)
  {

    LOGI("FeatureManager_platingCheck is the type...");
    featureSet = new FeatureManager_platingCheck(json_str);
  }
  else
  {
    /*char * jstr  = cJSON_Print(root);
    LOGE("Cannot find a corresponding type:[%s]...",jstr);
    delete jstr;*/
    LOGE("Cannot find a corresponding type...");
  }
  cJSON_Delete(root);
  return AddMatchingFeature(featureSet);
}


int MatchingEngine::FeatureMatching(acvImage *img)
{
  for(int i=0;i<featureBundle.size();i++)
  {
    featureBundle[i]->setRadialDistortionParam(param);
    featureBundle[i]->FeatureMatching(img);
  }

  return 0;
}


const FeatureReport * MatchingEngine::GetReport()
{
  //TODO: ONLY one report wil be generated...
  if(featureBundle.size()>0)
  {
    return featureBundle[0]->GetReport();
  }
  return NULL;
}

cJSON *MatchingEngine::FeatureReport2Json(const FeatureReport *report)
{
  return MatchingReport2JSON(report);
}

MatchingEngine::~MatchingEngine()
{
  ResetFeature();
}
