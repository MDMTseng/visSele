#include "MatchingEngine.h"
#include "include_priv/MatchingCore.h"
#include "FeatureManager_platingCheck.h"
#include "logctrl.h"

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

  if(FeatureManager_group::check(root))
  {

    LOGI("FeatureManager_group is the type...");
    featureSet = new FeatureManager_group(json_str);
  }
  else if(FeatureManager_binary_processing_group::check(root))
  {

    LOGI("FeatureManager_binary_processing_group is the type...");
    featureSet = new FeatureManager_binary_processing_group(json_str);
  }
  else if(FeatureManager_platingCheck::check(root))
  {

    LOGI("FeatureManager_platingCheck is the type...");
    featureSet = new FeatureManager_platingCheck(json_str);
  }
  else
  {
    LOGE("Cannot find a corresponding type...");
  }
  cJSON_Delete(root);
  return AddMatchingFeature(featureSet);
}


int MatchingEngine::FeatureMatching(acvImage *img,acvImage *buff,acvImage *dbg)
{
  for(int i=0;i<featureBundle.size();i++)
  {
    featureBundle[i]->FeatureMatching(img,buff,dbg);
    const FeatureReport *report = featureBundle[i]->GetReport();
    if(report==NULL)
    {
      continue;
    }


    if(report->type == FeatureReport::binary_processing_group)
    {
      const vector<acv_LabeledData> *ldata = 
      report->data.binary_processing_group.labeledData;
      for(int j=0;j<ldata->size();j++)
      {
        LOGE("iTem[%d]: area:%d",j,(*ldata)[j].area);
      }
    }
  }

  return 0;
}

MatchingEngine::~MatchingEngine()
{
  for(int i=0;i<featureBundle.size() ;i++)
  {
    delete(featureBundle[i]);
  }
  featureBundle.resize(0);
}
