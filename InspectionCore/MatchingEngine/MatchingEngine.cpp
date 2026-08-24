#ifndef _WIN32
#include <sys/resource.h>
#endif
#include <ctime>
#include "MatchingEngine.h"
#include "include_priv/MatchingCore.h"
#include "FeatureManager_sig360_circle_line.h"
#include "FeatureManager_stage_light_report.h"
#include "FeatureManager_nop.h"
// #include "FM_GenMatching.h"
#include "FM_Blank.h"

#include "logctrl.h"
#include <common_lib.h>
#include "FeatureReport_UTIL.h"
#include "cJSON.h"

LOG_MODULE("match.engine");

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

// Owns a cJSON tree for the duration of a scope. The parsed def below has to
// survive several early returns and the std::invalid_argument that a failed
// FeatureManager ctor throws; a guard is the only way it gets released on all
// of them.
namespace {
struct _CJsonGuard {
  cJSON *j;
  explicit _CJsonGuard(cJSON *p):j(p){}
  ~_CJsonGuard(){ if(j) cJSON_Delete(j); }
  _CJsonGuard(const _CJsonGuard&)=delete;
  _CJsonGuard& operator=(const _CJsonGuard&)=delete;
};
}

int MatchingEngine::AddMatchingFeature(const char *json_str)
{

  FeatureManager *featureSet=NULL;

  cJSON *root = cJSON_Parse(json_str);
  if(root == NULL)
  {
    return -1;
  }
  _CJsonGuard rootGuard(root);

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
  else if(strcmp(FeatureManager_nop::GetFeatureTypeName(),str) == 0)
  {
    LOGI("FeatureManager_nop is the type...");
    featureSet = new FeatureManager_nop(json_str);
  }
  // else if(strcmp(FM_GenMatching::GetFeatureTypeName(),str) == 0)
  // {
  //   LOGI("FM_GenMatching is the type...");
  //   featureSet = new FM_GenMatching(json_str);
  // }
  // else if(strcmp(FM_Blank::GetFeatureTypeName(),str) == 0)
  // {
  //   LOGI("FM_Blank is the type...");
  //   featureSet = new FM_Blank(json_str);
  // }
  else
  {
    /*char * jstr  = cJSON_Print(root);
    LOGE("Cannot find a corresponding type:[%s]...",jstr);
    delete jstr;*/
    LOGE("Cannot find a corresponding type (  %s  )...",str);
  }
  return AddMatchingFeature(featureSet);   // rootGuard releases `root`
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

double MatchingEngine::lastStageMs[MatchingEngine::STAGE_MAX] = {0};
double MatchingEngine::lastStageCpuMs[MatchingEngine::STAGE_MAX] = {0};
int    MatchingEngine::lastStageN = 0;

static inline double me_now_ms()
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}
static inline double me_proc_cpu_ms()
{
#ifndef _WIN32
  struct rusage ru;
  if (getrusage(RUSAGE_SELF, &ru) == 0)
    return ru.ru_utime.tv_sec * 1000.0 + ru.ru_utime.tv_usec / 1000.0
         + ru.ru_stime.tv_sec * 1000.0 + ru.ru_stime.tv_usec / 1000.0;
#endif
  return 0.0;
}

void MatchingEngine::setNoCandidateFrame(bool v)
{
  for (size_t i = 0; i < featureBundle.size(); i++)
    featureBundle[i]->setNoCandidateFrame(v);
}

int MatchingEngine::FeatureMatching(cv::Mat &img_cv)
{
  const int n = (int)featureBundle.size();
  lastStageN = n < STAGE_MAX ? n : STAGE_MAX;
  for(int i=0;i<n;i++)
  {
    const bool rec = (i < STAGE_MAX);
    const double t0 = rec ? me_now_ms() : 0.0;
    const double c0 = rec ? me_proc_cpu_ms() : 0.0;
    featureBundle[i]->setBacPac(bacpac);
    featureBundle[i]->FeatureMatching(img_cv);
    if (rec)
    {
      lastStageMs[i]    = me_now_ms() - t0;
      lastStageCpuMs[i] = me_proc_cpu_ms() - c0;
    }
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

cJSON *MatchingEngine::GetShapeFeaturePoints()
{
  for (auto *fm : featureBundle)
  {
    fm->setBacPac(bacpac);
    cJSON *j = fm->getShapeFeaturePointsJson();
    if (j != NULL) return j;
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
