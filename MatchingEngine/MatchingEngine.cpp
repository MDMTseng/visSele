#include "MatchingEngine.h"
#include "include_priv/MatchingCore.h"
#include "FeatureManager_sig360_circle_line.h"
#include "FeatureManager_platingCheck.h"
#include "logctrl.h"

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


cJSON* acv_LineFit2JSON(const acv_LineFit line )
{
  cJSON* Line_jobj = cJSON_CreateObject();
  cJSON_AddNumberToObject(Line_jobj, "matching_pts", line.matching_pts);
  cJSON_AddNumberToObject(Line_jobj, "s", line.s);


  acv_XY point = acvClosestPointOnLine(line.end_pos, line.line);
  cJSON_AddNumberToObject(Line_jobj, "x0", point.X);
  cJSON_AddNumberToObject(Line_jobj, "y0", point.Y);
  point = acvClosestPointOnLine(line.end_neg, line.line);
  cJSON_AddNumberToObject(Line_jobj, "x1", point.X);
  cJSON_AddNumberToObject(Line_jobj, "y1", point.Y);
  cJSON_AddNumberToObject(Line_jobj, "cx", line.line.line_anchor.X);
  cJSON_AddNumberToObject(Line_jobj, "cy", line.line.line_anchor.Y);
  cJSON_AddNumberToObject(Line_jobj, "vx", line.line.line_vec.X);
  cJSON_AddNumberToObject(Line_jobj, "vy", line.line.line_vec.Y);




  return Line_jobj;
}


cJSON* acv_LineFitVector2JSON(const vector< acv_LineFit> &vec)
{

  cJSON* detectedLines_jarr = cJSON_CreateArray();

  for(int j=0;j<vec.size();j++)
  {
    cJSON_AddItemToArray(detectedLines_jarr, acv_LineFit2JSON(vec[j]) );
  }
  return detectedLines_jarr;
}

cJSON* acv_CircleFit2JSON(const acv_CircleFit cir )
{
  cJSON* Circle_jobj = cJSON_CreateObject();
  cJSON_AddNumberToObject(Circle_jobj, "matching_pts", cir.matching_pts);
  cJSON_AddNumberToObject(Circle_jobj, "s", cir.s);
  cJSON_AddNumberToObject(Circle_jobj, "x", cir.circle.circumcenter.X);
  cJSON_AddNumberToObject(Circle_jobj, "y", cir.circle.circumcenter.Y);
  cJSON_AddNumberToObject(Circle_jobj, "r", cir.circle.radius);
  return Circle_jobj;
}



cJSON* acv_CircleFitVector2JSON(const vector< acv_CircleFit> &vec)
{

  cJSON* detectedCircles_jarr = cJSON_CreateArray();

  for(int j=0;j<vec.size();j++)
  {
    cJSON_AddItemToArray(detectedCircles_jarr, acv_CircleFit2JSON(vec[j]) );
  }
  return detectedCircles_jarr;
}


cJSON* acv_FeatureReport_sig360_circle_line_single2JSON(const FeatureReport_sig360_circle_line_single report )
{
  cJSON* report_jobj = cJSON_CreateObject();
  cJSON_AddNumberToObject(report_jobj, "area", report.area);
  cJSON_AddNumberToObject(report_jobj, "scale", report.scale);
  cJSON_AddStringToObject(report_jobj, "targetName", report.targetName);
  cJSON_AddNumberToObject(report_jobj, "cx", report.Center.X);
  cJSON_AddNumberToObject(report_jobj, "cy", report.Center.Y);
  cJSON_AddNumberToObject(report_jobj, "rotate", report.rotate);
  cJSON_AddBoolToObject(report_jobj, "isFlipped", report.isFlipped);


  const vector<acv_CircleFit> &detectedCircle = *report.detectedCircles;
  cJSON_AddItemToObject(report_jobj,"detectedCircles",
    acv_CircleFitVector2JSON(detectedCircle));

  const vector<acv_LineFit> &detectedLines =*report.detectedLines;
  cJSON_AddItemToObject(report_jobj,"detectedLines",
    acv_LineFitVector2JSON(detectedLines));

  return report_jobj;
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
      cJSON_AddStringToObject(report_jobj, "type", FeatureManager_binary_processing_group::GetFeatureTypeName());
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
      cJSON_AddStringToObject(report_jobj, "type", FeatureManager_sig360_extractor::GetFeatureTypeName());

      cJSON_AddItemToObject(report_jobj,"detectedCircles",
        acv_CircleFitVector2JSON(*detectedCircle));

      const vector<acv_LineFit> *detectedLines =
      report->data.sig360_extractor.detectedLines;

      cJSON_AddItemToObject(report_jobj,"detectedLines",
        acv_LineFitVector2JSON(*detectedLines));

    }
    break;

    case FeatureReport::sig360_circle_line:
    {
      cJSON_AddStringToObject(report_jobj, "type", FeatureManager_sig360_circle_line::GetFeatureTypeName());

      vector<FeatureReport_sig360_circle_line_single> &scl_reports =
        *report->data.sig360_circle_line.reports;

      cJSON* reports_jarr = cJSON_CreateArray();
      cJSON_AddItemToObject(report_jobj,"reports",reports_jarr);
      for(int i=0;i<scl_reports.size();i++)
      {

        cJSON_AddItemToArray(reports_jarr,
            acv_FeatureReport_sig360_circle_line_single2JSON(scl_reports[i]));

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
