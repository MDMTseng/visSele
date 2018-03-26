#include "MatchingEngine.h"

int MatchingEngine::AddMatchingFeature(VisSeleFeatureManager *featureSet)
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
  VisSeleFeatureManager *featureSet = new VisSeleFeatureManager(json_str);
  return AddMatchingFeature(featureSet);
}

MatchingEngine::~MatchingEngine()
{
  for(int i=0;i<featureBundle.size() ;i++)
  {
    free(featureBundle[i]);
  }
  featureBundle.resize(0);
}
