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


cJSON* MatchingReport2JSON(const FeatureReport *report )
{    

  if(report==NULL)
  {
    return NULL;
  }
  cJSON* report_jobj = cJSON_CreateObject();
  //for(int i=0;i<featureBundle.size();i++)
  

  switch (report->type)
  {
    case FeatureReport::binary_processing_group:
    {
      cJSON* label_jarr = cJSON_CreateArray();
      cJSON_AddItemToObject(report_jobj,"labeledData",label_jarr);
      cJSON* reports_jarr = cJSON_CreateArray();
      cJSON_AddItemToObject(report_jobj,"reports",reports_jarr);

      const vector<acv_LabeledData> *ldata = 
      report->data.binary_processing_group.labeledData;


      for(int j=2;j<ldata->size();j++)
      {
        cJSON* label_jobj = cJSON_CreateObject();
        cJSON_AddItemToArray(label_jarr, label_jobj );
        cJSON_AddNumberToObject(label_jobj, "area", (*ldata)[j].area);
      }



      const vector<const FeatureReport*> *sub_reports = 
      report->data.binary_processing_group.reports;

      for(int j=0;j<sub_reports->size();j++)
      {
        cJSON_AddItemToArray(reports_jarr, 
          MatchingReport2JSON((*sub_reports)[j]));
      }
    }
    break;

    case FeatureReport::sig360_extractor:
    {
      const vector<acv_CircleFit> *detectedCircle = 
      report->data.sig360_extractor.detectedCircles;

      cJSON* detectedCircles_jarr = cJSON_CreateArray();
      cJSON_AddItemToObject(report_jobj,"detectedCircles",detectedCircles_jarr);

      for(int j=0;j<detectedCircle->size();j++)
      {
        acv_CircleFit cir = (*detectedCircle)[j];
        cJSON* Circle_jobj = cJSON_CreateObject();
        cJSON_AddItemToArray(detectedCircles_jarr, Circle_jobj );
        cJSON_AddNumberToObject(Circle_jobj, "matching_pts", cir.matching_pts);
        cJSON_AddNumberToObject(Circle_jobj, "s", cir.s);
        cJSON_AddNumberToObject(Circle_jobj, "x", cir.circle.circumcenter.X);
        cJSON_AddNumberToObject(Circle_jobj, "y", cir.circle.circumcenter.Y);
        cJSON_AddNumberToObject(Circle_jobj, "r", cir.circle.radius);
      }


      const vector<acv_LineFit> *detectedLines = 
      report->data.sig360_extractor.detectedLines;
      cJSON* detectedLine_jarr = cJSON_CreateArray();
      cJSON_AddItemToObject(report_jobj,"detectedLines",detectedLine_jarr);

      for(int j=0;j<detectedLines->size();j++)
      {
        acv_LineFit line = (*detectedLines)[j];
        cJSON* Line_jobj = cJSON_CreateObject();
        cJSON_AddItemToArray(detectedLine_jarr, Line_jobj );
        cJSON_AddNumberToObject(Line_jobj, "matching_pts", line.matching_pts);
        cJSON_AddNumberToObject(Line_jobj, "s", line.s);
        cJSON_AddNumberToObject(Line_jobj, "x0", line.end_pos.X);
        cJSON_AddNumberToObject(Line_jobj, "y0", line.end_pos.Y);
        cJSON_AddNumberToObject(Line_jobj, "x1", line.end_neg.X);
        cJSON_AddNumberToObject(Line_jobj, "y1", line.end_neg.Y);
      }


    }
    break;
    default:

      LOGE("UNKNOWN type:%d",report->type);


  }

  return report_jobj;
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

    cJSON* jobj = MatchingReport2JSON(report);
    char * jstr  = cJSON_Print(jobj);

    LOGI("...\n%s\n...",jstr);
    cJSON_Delete(jobj);
    delete jstr;

    /*if(report->type == FeatureReport::binary_processing_group)
    {
      const vector<acv_LabeledData> *ldata = 
      report->data.binary_processing_group.labeledData;
      for(int j=0;j<ldata->size();j++)
      {
        LOGE("iTem[%d]: area:%d",j,(*ldata)[j].area);
      }
    }*/
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
