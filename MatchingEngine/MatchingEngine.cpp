#include "MatchingEngine.h"
#include "include_priv/MatchingCore.h"

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

  if(FeatureManager::check(root))
  {
  }
  else if(FeatureManager_sig360_circle_line::check(root))
  {
    featureSet = new FeatureManager_sig360_circle_line(json_str);
  }
  cJSON_Delete(root);
  return AddMatchingFeature(featureSet);
}


int MatchingEngine::FeatureMatching(acvImage *img,acvImage *buff,acvImage *dbg)
{

  std::vector<acv_LabeledData> ldData;



  acvThreshold(img, 250, 0);
  acvDrawBlock(img, 1, 1, img->GetWidth() - 2, img->GetHeight() - 2);

  acvComponentLabeling(img);
  acvLabeledRegionInfo(img, &ldData);
  if(ldData.size()-1<1)
  {
    return 0;
  }
  ldData[1].area = 0;

  //Delete the object that has less than certain amount of area on ldData
  //acvRemoveRegionLessThan(img, &ldData, 120);


  acvCloneImage( img,buff, -1);


  for(int i=0;i<featureBundle.size();i++)
  {
    featureBundle[i]->FeatureMatching(img,buff,ldData,dbg);
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
