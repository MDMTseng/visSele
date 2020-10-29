#include "MatchingEngine.h"
#include "include_priv/MatchingCore.h"
#include "FeatureManager_sig360_circle_line.h"
#include "FeatureManager_stage_light_report.h"
#include "FeatureManager_nop.h"
#include "FeatureManager_gen.h"
#include "FeatureManager_platingCheck.h"
#include "logctrl.h"
#include <common_lib.h>
#include "FeatureReport_UTIL.h"
#include "cJSON.h"

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
  else if(strcmp(FeatureManager_stage_light_report::GetFeatureTypeName(),str) == 0)
  {

    LOGI("FeatureManager_stage_light_report is the type...");
    featureSet = new FeatureManager_stage_light_report(json_str);
  }
  else if(strcmp(FeatureManager_platingCheck::GetFeatureTypeName(),str) == 0)
  {

    LOGI("FeatureManager_platingCheck is the type...");
    featureSet = new FeatureManager_platingCheck(json_str);
  }
  else if(strcmp(FeatureManager_gen::GetFeatureTypeName(),str) == 0)
  {

    LOGI("FeatureManager_gen is the type...");
    featureSet = new FeatureManager_gen(json_str);
  }
  else if(strcmp(FeatureManager_nop::GetFeatureTypeName(),str) == 0)
  {
    LOGI("FeatureManager_nop is the type...");
    featureSet = new FeatureManager_nop(json_str);
  }
  else
  {
    /*char * jstr  = cJSON_Print(root);
    LOGE("Cannot find a corresponding type:[%s]...",jstr);
    delete jstr;*/
    LOGE("Cannot find a corresponding type (  %s  )...",str);
  }
  cJSON_Delete(root);
  return AddMatchingFeature(featureSet);
}




cJSON * MatchingEngine::SetParam(cJSON *json)
{
  if(!cJSON_IsArray(json))return NULL;
  cJSON * retJson=cJSON_CreateArray();
  for(int i=0;i<featureBundle.size();i++)
  {
    cJSON *json_param= cJSON_GetArrayItem(json,i);
    if(json_param==NULL)break;
    cJSON * retInfo = featureBundle[i]->SetParam(json_param);
    if(retInfo==NULL)
      retInfo = cJSON_CreateNull();
    cJSON_AddItemToArray(retJson, retInfo);
  }

  return retJson;
}

int MatchingEngine::FeatureMatching(acvImage *img)
{
  for(int i=0;i<featureBundle.size();i++)
  {
    featureBundle[i]->setBacPac(bacpac);
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
