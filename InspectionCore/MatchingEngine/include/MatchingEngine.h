#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP
using namespace std;
#include <vector>
#include <cstdlib>
#include <ctime>
#include "acvImage_BasicTool.hpp"
#include "FeatureManager.h"
#include "FeatureManager_group.h"

#include "cJSON.h"


class MatchingEngine {
protected:
  vector<FeatureManager*> featureBundle;
  FeatureManager_BacPac *bacpac;
public:
  int ResetFeature();
  void setBacPac(FeatureManager_BacPac *bacpac){this->bacpac=bacpac;};
  int AddMatchingFeature(const char *json_str);
  cJSON * SetParam(cJSON *json_str);
  int AddMatchingFeature(FeatureManager *featureSet);
  int FeatureMatching(cv::Mat &img_cv);     // canonical cv::Mat entry
  // SF round-trip: first trained shape localizer's feature points (object-frame mm),
  // as cJSON {"features":[{x,y}...],"roi":[...]}, or NULL. Caller owns the cJSON.
  cJSON *GetShapeFeaturePoints();
  const FeatureReport *GetReport();
  cJSON*FeatureReport2Json(const FeatureReport *report);

  // Per-bundle-member timing of the last FeatureMatching call.
  //
  // A nearly EMPTY frame -- one part half in shot, some dust -- costs 26ms of
  // process CPU, against an expectation of ~10ms for the whole inspection. A
  // frame with five parts costs 159ms. The per-frame numbers say the cost
  // tracks object count, but not WHERE inside the bundle it goes, and the
  // bundle is a chain of managers (binary/labeling group, sig360 matcher,
  // stage light report) that each run over the whole image.
  //
  // Wall AND process cpu per member, because the two answer different
  // questions: wall says what the frame waited for, cpu says what it cost. The
  // matching runs 4-5 wide, so wall alone understates the work by that factor.
  static const int STAGE_MAX = 8;
  static double lastStageMs[STAGE_MAX];
  static double lastStageCpuMs[STAGE_MAX];
  static int    lastStageN;
  ~MatchingEngine();
};

#endif
