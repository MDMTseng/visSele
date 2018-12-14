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


cJSON* acv_LineFit2JSON(cJSON* Line_jobj, const acv_LineFit line, acv_XY center_offset )
{
  cJSON_AddNumberToObject(Line_jobj, "matching_pts", line.matching_pts);
  cJSON_AddNumberToObject(Line_jobj, "s", line.s);

  acv_XY point = acvClosestPointOnLine(line.end_pos, line.line);
  cJSON_AddNumberToObject(Line_jobj, "x0", point.X-center_offset.X);
  cJSON_AddNumberToObject(Line_jobj, "y0", point.Y-center_offset.Y);
  point = acvClosestPointOnLine(line.end_neg, line.line);
  cJSON_AddNumberToObject(Line_jobj, "x1", point.X-center_offset.X);
  cJSON_AddNumberToObject(Line_jobj, "y1", point.Y-center_offset.Y);
  cJSON_AddNumberToObject(Line_jobj, "cx", line.line.line_anchor.X-center_offset.X);
  cJSON_AddNumberToObject(Line_jobj, "cy", line.line.line_anchor.Y-center_offset.Y);
  cJSON_AddNumberToObject(Line_jobj, "vx", line.line.line_vec.X);
  cJSON_AddNumberToObject(Line_jobj, "vy", line.line.line_vec.Y);

  return Line_jobj;
}


cJSON* acv_CircleFit2JSON(cJSON* Circle_jobj,const acv_CircleFit cir , acv_XY center_offset)
{
  cJSON_AddNumberToObject(Circle_jobj, "matching_pts", cir.matching_pts);
  cJSON_AddNumberToObject(Circle_jobj, "s", cir.s);
  cJSON_AddNumberToObject(Circle_jobj, "x", cir.circle.circumcenter.X-center_offset.X);
  cJSON_AddNumberToObject(Circle_jobj, "y", cir.circle.circumcenter.Y-center_offset.Y);
  cJSON_AddNumberToObject(Circle_jobj, "r", cir.circle.radius);
  return Circle_jobj;
}

cJSON* JudgeReport2JSON(const FeatureReport_judgeReport judge , acv_XY center_offset )
{
  cJSON* judge_jobj = cJSON_CreateObject();
  cJSON_AddNumberToObject(judge_jobj, "status", judge.status);
  cJSON_AddNumberToObject(judge_jobj, "id", judge.def->id);
  cJSON_AddStringToObject(judge_jobj, "name", judge.def->name);
  cJSON_AddNumberToObject(judge_jobj, "value", judge.measured_val);

  switch(judge.def->measure_type)
  {
    case FeatureReport_judgeDef::ANGLE :
      cJSON_AddStringToObject(judge_jobj, "subtype", "angle");
    break;
    case FeatureReport_judgeDef::AREA :
      cJSON_AddStringToObject(judge_jobj, "subtype", "area");
    break;
    case FeatureReport_judgeDef::DISTANCE :
      cJSON_AddStringToObject(judge_jobj, "subtype", "distance");
    break;
    case FeatureReport_judgeDef::RADIUS :
      cJSON_AddStringToObject(judge_jobj, "subtype", "radius");
    break;
    case FeatureReport_judgeDef::SIGMA :
      cJSON_AddStringToObject(judge_jobj, "subtype", "sigma");
    break;
  }



  return judge_jobj;
}

cJSON* JudgeReportVector2JSON(const vector< FeatureReport_judgeReport> &judges , acv_XY center_offset)
{

  cJSON* judges_jarr = cJSON_CreateArray();

  for(int j=0;j<judges.size();j++)
  {
    cJSON_AddItemToArray(judges_jarr, JudgeReport2JSON(judges[j],center_offset) );
  }
  return judges_jarr;
}



cJSON* acv_CircleFitVector2JSON(const vector< FeatureReport_circleReport> &vec, acv_XY center_offset)
{

  cJSON* detectedCircles_jarr = cJSON_CreateArray();

  for(int j=0;j<vec.size();j++)
  {
    cJSON* cfj = cJSON_CreateObject();
    cJSON_AddNumberToObject(cfj, "status", vec[j].status);
    cJSON_AddNumberToObject(cfj, "id", vec[j].def->id);
    cJSON_AddStringToObject(cfj, "name", vec[j].def->name);
    
    if(vec[j].status!=FeatureReport_sig360_circle_line_single::STATUS_NA)
      acv_CircleFit2JSON(cfj,vec[j].circle,center_offset);
    cJSON_AddItemToArray(detectedCircles_jarr, cfj );

  }
  return detectedCircles_jarr;
}

cJSON* acv_CircleFitVector2JSON(const vector< acv_CircleFit> &vec, acv_XY center_offset)
{
  cJSON* detectedCircles_jarr = cJSON_CreateArray();
  for(int j=0;j<vec.size();j++)
  {
    cJSON* cfj = cJSON_CreateObject();
    cJSON_AddItemToArray(detectedCircles_jarr, acv_CircleFit2JSON(cfj,vec[j],center_offset) );
  }
  return detectedCircles_jarr;
}

cJSON* acv_AuxPointReport2JSON(const vector< FeatureReport_auxPointReport> &vec, acv_XY center_offset)
{
  
  cJSON* detectedAuxPoint_jarr = cJSON_CreateArray();

  for(int j=0;j<vec.size();j++)
  {
    cJSON* apj = cJSON_CreateObject();
    cJSON_AddNumberToObject(apj, "status", vec[j].status);
    cJSON_AddNumberToObject(apj, "id", vec[j].def->id);
    cJSON_AddStringToObject(apj, "name", vec[j].def->name);
    
    if(vec[j].status!=FeatureReport_sig360_circle_line_single::STATUS_NA)
    {
      cJSON_AddNumberToObject(apj, "x", vec[j].pt.X);
      cJSON_AddNumberToObject(apj, "y", vec[j].pt.Y);
    }
    cJSON_AddItemToArray(detectedAuxPoint_jarr, apj );

  }
  return detectedAuxPoint_jarr;
}
cJSON* acv_SearchPointReport2JSON(const vector< FeatureReport_searchPointReport> &vec, acv_XY center_offset)
{
  
  cJSON* detectedSearchPoint_jarr = cJSON_CreateArray();

  for(int j=0;j<vec.size();j++)
  {
    cJSON* spj = cJSON_CreateObject();
    cJSON_AddNumberToObject(spj, "status", vec[j].status);
    cJSON_AddNumberToObject(spj, "id", vec[j].def->id);
    cJSON_AddStringToObject(spj, "name", vec[j].def->name);
    
    if(vec[j].status!=FeatureReport_sig360_circle_line_single::STATUS_NA)
    {
      cJSON_AddNumberToObject(spj, "x", vec[j].pt.X);
      cJSON_AddNumberToObject(spj, "y", vec[j].pt.Y);
    }
    cJSON_AddItemToArray(detectedSearchPoint_jarr, spj );

  }
  return detectedSearchPoint_jarr;
}
cJSON* acv_LineFitVector2JSON(const vector< FeatureReport_lineReport> &vec, acv_XY center_offset)
{

  cJSON* detectedLines_jarr = cJSON_CreateArray();

  for(int j=0;j<vec.size();j++)
  {
    cJSON* lfj = cJSON_CreateObject();
    cJSON_AddNumberToObject(lfj, "status", vec[j].status);
    cJSON_AddNumberToObject(lfj, "id", vec[j].def->id);
    cJSON_AddStringToObject(lfj, "name", vec[j].def->name);
    if(vec[j].status!=FeatureReport_sig360_circle_line_single::STATUS_NA)
      acv_LineFit2JSON(lfj,vec[j].line,center_offset);
    cJSON_AddItemToArray(detectedLines_jarr, lfj );
  }
  return detectedLines_jarr;
}



cJSON* acv_LineFitVector2JSON(const vector< acv_LineFit> &vec, acv_XY center_offset)
{

  cJSON* detectedLines_jarr = cJSON_CreateArray();

  for(int j=0;j<vec.size();j++)
  {
    cJSON* lfj = cJSON_CreateObject();
    cJSON_AddItemToArray(detectedLines_jarr, acv_LineFit2JSON(lfj,vec[j],center_offset) );
  }
  return detectedLines_jarr;
}

cJSON* acv_Signature2JSON(const vector< acv_XY> &signature)
{

  cJSON* signature_jobj = cJSON_CreateObject();
  cJSON* magnitude_jarr = cJSON_CreateArray();
  cJSON* angle_jarr = cJSON_CreateArray();

  cJSON_AddItemToObject(signature_jobj,"magnitude",magnitude_jarr);
  cJSON_AddItemToObject(signature_jobj,"angle",angle_jarr);
  for(int j=0;j<signature.size();j++)
  {
    cJSON_AddItemToArray(magnitude_jarr, cJSON_CreateNumber(signature[j].X));
    cJSON_AddItemToArray(angle_jarr, cJSON_CreateNumber(signature[j].Y));
  }
  return signature_jobj;
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

  acv_XY offset = {0};
  const vector<FeatureReport_circleReport> &detectedCircle = *report.detectedCircles;
  cJSON_AddItemToObject(report_jobj,"detectedCircles",
    acv_CircleFitVector2JSON(detectedCircle,offset));

  const vector<FeatureReport_lineReport> &detectedLines =*report.detectedLines;
  cJSON_AddItemToObject(report_jobj,"detectedLines",
    acv_LineFitVector2JSON(detectedLines,offset));

  const vector<FeatureReport_auxPointReport> &detectedAuxPoints =*report.detectedAuxPoints;
  cJSON_AddItemToObject(report_jobj,"auxPoints",
    acv_AuxPointReport2JSON(detectedAuxPoints,offset));

  const vector<FeatureReport_searchPointReport> &detectedSearchPoints =*report.detectedSearchPoints;
  cJSON_AddItemToObject(report_jobj,"searchPoints",
    acv_SearchPointReport2JSON(detectedSearchPoints,offset));

  const vector< FeatureReport_judgeReport> &judgeReports=*report.judgeReports;
  cJSON_AddItemToObject(report_jobj,"judgeReports",
    JudgeReportVector2JSON(judgeReports,offset));



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

      cJSON_AddNumberToObject(report_jobj, "area", report->data.sig360_extractor.area);
      cJSON_AddNumberToObject(report_jobj, "scale", report->data.sig360_extractor.scale);
      cJSON_AddNumberToObject(report_jobj, "cx", report->data.sig360_extractor.Center.X);
      cJSON_AddNumberToObject(report_jobj, "cy", report->data.sig360_extractor.Center.Y);

      const vector<acv_CircleFit> *detectedCircle =
      report->data.sig360_extractor.detectedCircles;
      cJSON_AddStringToObject(report_jobj, "type", FeatureManager_sig360_extractor::GetFeatureTypeName());

      cJSON_AddItemToObject(report_jobj,"detectedCircles",
        acv_CircleFitVector2JSON(*detectedCircle,report->data.sig360_extractor.Center));

      const vector<acv_LineFit> *detectedLines =
      report->data.sig360_extractor.detectedLines;

      cJSON_AddItemToObject(report_jobj,"detectedLines",
        acv_LineFitVector2JSON(*detectedLines,report->data.sig360_extractor.Center));

      cJSON_AddItemToObject(report_jobj, "signature", acv_Signature2JSON(*(report->data.sig360_extractor.signature)));
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
