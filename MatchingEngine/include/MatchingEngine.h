#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP
using namespace std;
#include <vector>
#include <cstdlib>
#include <ctime>
#include "acvImage_BasicTool.hpp"
#include "VisSeleFeatureManager.h"



class MatchingEngine {
  vector<VisSeleFeatureManager*> featureBundle;
public:
  int AddMatchingFeature(const char *json_str);
  int AddMatchingFeature(VisSeleFeatureManager *featureSet);
  ~MatchingEngine();
};

#endif
