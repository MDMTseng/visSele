#include "FeatureManager_sig360_circle_line.h"
#include "logctrl.h"
#include <stdexcept>
#include <common_lib.h>
#include <algorithm>
#include <limits>
#include <MatchingCore.h>
#include <stdio.h>
#include <acvImage_SpDomainTool.hpp>
#include "polyfit.h"

static int searchP(acvImage *img, acv_XY *pos, acv_XY searchVec, float maxSearchDist);

void acvXY_pts_smooth(vector<ContourFetch::ptInfo> &from, vector<ContourFetch::ptInfo> &to, int sideW = 2)
{
  acv_XY CSum = {0};
  to.resize(0);
  for (int i = 0; i < sideW - 1; i++)
  {
    CSum = acvVecAdd(CSum, from[i].pt);
  }
  int width = 2 * sideW + 1;

  // for(int i=0;i<from.size();i++)
  // {
  //   int divC=
  //   CSum=acvVecSub(acvVecAdd(CSum,),);
  //   to.push_back(acvVecMult(CSum,1.0/width));
  // }
}

void acvXY_pts_smooth(vector<acv_XY> &from, vector<acv_XY> &to)
{
  to.resize(from.size());
  to[0] = from[0];
  to[from.size() - 1] = from[from.size() - 1];
  for (int i = 1; i < from.size() - 1; i++)
  {
    to[i].X = (from[i - 1].X + from[i].X + from[i + 1].X) / 3;
    to[i].Y = (from[i - 1].Y + from[i].Y + from[i + 1].Y) / 3;
  }
}
int roughness(vector<ContourFetch::ptInfo> &ptInfo, int smoothT, float *ret_MIN, float *ret_MAX, float *ret_RMSE)
{
  if (ptInfo.size() < 3)
    return -1;
  vector<acv_XY> buffPt1, buffPt2;
  buffPt1.resize(ptInfo.size());
  buffPt2.resize(ptInfo.size());
  for (int i = 0; i < ptInfo.size(); i++)
  {
    buffPt1[i] = ptInfo[i].pt;
  }

  vector<acv_XY> *smoothPt = &buffPt1;
  vector<acv_XY> *buffPt = &buffPt2;
  for (int i = 0; i < smoothT; i++)
  {
    if ((i & 1) == 0)
    {
      acvXY_pts_smooth(buffPt1, buffPt2);
      smoothPt = &buffPt2;
      buffPt = &buffPt1;
    }
    else
    {
      acvXY_pts_smooth(buffPt2, buffPt1);
      smoothPt = &buffPt1;
      buffPt = &buffPt2;
    }
  }
  float max = -999999, min = 999999;
  float RMSE = 0;

  for (int i = 0; i < ptInfo.size(); i++)
  {
    acv_Line line = {
      line_vec : (ptInfo[i].contourDir),
      line_anchor : ptInfo[i].pt,
    };

    // if(i==0)
    // {
    //   LOGI("pt:%f,%f  dir:%f,%f",ptInfo[i].pt_img.X,ptInfo[i].pt_img.Y,ptInfo[i].contourDir.X,ptInfo[i].contourDir.Y);
    // }
    float distance = acvDistance_Signed(line, (*smoothPt)[i]);
    //acvDistance(ptInfo[i].pt,(*smoothPt)[i]);
    //acvDistance_Signed(line,(*smoothPt)[i]);
    if (max < distance)
    {
      max = distance;
    }
    if (min > distance)
    {
      min = distance;
    }
    RMSE += distance * distance;
  }
  RMSE = sqrt(RMSE / ptInfo.size());
  if (ret_MIN)
    *ret_MIN = min;
  if (ret_MAX)
    *ret_MAX = max;
  if (ret_RMSE)
    *ret_RMSE = RMSE;
  return 0;
}

/*
  FeatureManager_sig360_circle_line Section
*/
FeatureManager_sig360_circle_line::FeatureManager_sig360_circle_line(const char *json_str) : FeatureManager_binary_processing(json_str)
{

  //LOGI(">>>>%s>>>>",json_str);
  root = NULL;
  int ret = reload(json_str);
  if (ret)
    throw std::invalid_argument("Error:FeatureManager_sig360_circle_line failed... ");

  ClearReport();
}

double *json_get_num(cJSON *root, char *path, char *dbg_str)
{
  double *pnum;
  if ((pnum = (double *)JFetch(root, path, cJSON_Number)) == NULL)
  {
    if (dbg_str)
    {
      LOGE("%s: Cannot get Number In path: %s", dbg_str, path);
    }
    else
    {
      LOGE("Cannot get Number In path: %s", path);
    }
    return NULL;
  }
  return pnum;
}

double *json_get_num(cJSON *root, char *path)
{
  return json_get_num(root, path, NULL);
}

#define JSON_GET_NUM(JROOT, PATH) json_get_num(JROOT, PATH, (char *)__func__)

char *json_find_str(cJSON *root, const char *key)
{
  char *str;
  if (!(getDataFromJsonObj(root, key, (void **)&str) & cJSON_String))
  {
    return NULL;
  }
  return str;
}

char *json_find_name(cJSON *root)
{
  return json_find_str(root, "name");
}

typedef struct arc_data
{
  acv_XY pt1, pt2, pt3; //three points arc, the root of all info
  acv_Circle circleTar;
  float sAngle;
  float eAngle;
} arc_data;

arc_data convert3Pts2ArcData(acv_XY pt1, acv_XY pt2, acv_XY pt3)
{
  arc_data retD = {pt1 : pt1, pt2 : pt2, pt3 : pt3};

  acv_XY circumcenter = acvCircumcenter(pt1, pt2, pt3);
  retD.circleTar.circumcenter = circumcenter;
  retD.circleTar.radius = hypot(circumcenter.X - pt2.X, circumcenter.Y - pt2.Y);

  pt1.X -= circumcenter.X;
  pt1.Y -= circumcenter.Y;
  pt2.X -= circumcenter.X;
  pt2.Y -= circumcenter.Y;
  pt3.X -= circumcenter.X;
  pt3.Y -= circumcenter.Y;
  float angle1 = atan2(pt1.Y, pt1.X);
  float angle2 = atan2(pt2.Y, pt2.X);
  float angle3 = atan2(pt3.Y, pt3.X);

  float angle21 = angle2 - angle1;
  float angle31 = angle3 - angle1;
  if (angle21 < 0)
    angle21 += 2 * M_PI;
  if (angle31 < 0)
    angle31 += 2 * M_PI;

  if (angle31 > angle21)
  {
    retD.sAngle = angle1;
    retD.eAngle = angle3;
  }
  else
  {
    retD.sAngle = angle3;
    retD.eAngle = angle1;
  }

  return retD;
}

int FeatureManager_sig360_circle_line::parse_arcData(cJSON *circle_obj)
{
  featureDef_circle cir;

  {
    char *tmpstr;
    cir.name[0] = '\0';
    if (tmpstr = json_find_name(circle_obj))
    {
      strcpy(cir.name, tmpstr);
    }
  }

  double *pnum;

  cir.id = (int)*JFetEx_NUMBER(circle_obj, "id");
  LOGV("feature is an arc:%s %d", cir.name, cir.id);

  cir.initMatchingMargin = *JFetEx_NUMBER(circle_obj, "margin");

  acv_XY pt1, pt2, pt3;

  pt1.X = *JFetEx_NUMBER(circle_obj, "pt1.x");
  pt1.Y = *JFetEx_NUMBER(circle_obj, "pt1.y");

  pt2.X = *JFetEx_NUMBER(circle_obj, "pt2.x");
  pt2.Y = *JFetEx_NUMBER(circle_obj, "pt2.y");

  pt3.X = *JFetEx_NUMBER(circle_obj, "pt3.x");
  pt3.Y = *JFetEx_NUMBER(circle_obj, "pt3.y");
  double direction = *JFetEx_NUMBER(circle_obj, "direction");

  cir.pt1 = pt1;
  cir.pt2 = pt2;
  cir.pt3 = pt3;

  cir.outter_inner = direction;

  LOGV("x:%f y:%f r:%f margin:%f",
       cir.circleTar.circumcenter.X,
       cir.circleTar.circumcenter.Y,
       cir.circleTar.radius,
       cir.initMatchingMargin);
  if (cir.name[0] == '\0')
  {
    sprintf(cir.name, "@CIRCLE_%d", featureCircleList.size());
  }
  featureCircleList.push_back(cir);
  return 0;
}

/*
float FeatureManager_sig360_circle_line::find_search_key_points_longest_distance(vector<featureDef_line::searchKeyPoint> &skpsList)
{
  float maxDist=0;
  for(int i=0;i<skpsList.size()-1;i++)
  {
    for(int j=i+1;j<skpsList.size();j++)
    {
      float dist=acvDistance(skpsList[i].searchStart,skpsList[j].searchStart);
      if(maxDist<dist)
        maxDist=dist;
    }
  }
  return maxDist;
}*/

int FeatureManager_sig360_circle_line::FindFeatureDefIndex(int feature_id, FEATURETYPE *ret_type)
{
  if (ret_type == NULL)
    return -1;
  *ret_type = FEATURETYPE::NA;
  for (int i = 0; i < featureCircleList.size(); i++)
  {
    if (featureCircleList[i].id == feature_id)
    {
      *ret_type = FEATURETYPE::ARC;
      return i;
    }
  }

  for (int i = 0; i < featureLineList.size(); i++)
  {
    if (featureLineList[i].id == feature_id)
    {
      *ret_type = FEATURETYPE::LINE;
      return i;
    }
  }

  for (int i = 0; i < judgeList.size(); i++)
  {
    if (judgeList[i].id == feature_id)
    {
      *ret_type = FEATURETYPE::MEASURE;
      return i;
    }
  }

  for (int i = 0; i < auxPointList.size(); i++)
  {
    if (auxPointList[i].id == feature_id)
    {
      *ret_type = FEATURETYPE::AUX_POINT;
      return i;
    }
  }

  for (int i = 0; i < searchPointList.size(); i++)
  {
    if (searchPointList[i].id == feature_id)
    {
      *ret_type = FEATURETYPE::SEARCH_POINT;
      return i;
    }
  }
  return -1;
}

int FeatureManager_sig360_circle_line::FindFeatureReportIndex(FeatureReport_sig360_circle_line_single &report, int feature_id, FEATURETYPE *ret_type)
{
  if (ret_type == NULL)
    return -1;
  *ret_type = FEATURETYPE::NA;
  for (int i = 0; i < report.detectedCircles->size(); i++)
  {
    featureDef_circle *def = (*report.detectedCircles)[i].def;
    if (def == NULL)
      continue;
    if (def->id == feature_id)
    {
      *ret_type = FEATURETYPE::ARC;
      return i;
    }
  }

  for (int i = 0; i < report.detectedLines->size(); i++)
  {
    featureDef_line *def = (*report.detectedLines)[i].def;
    if (def == NULL)
      continue;
    if (def->id == feature_id)
    {
      *ret_type = FEATURETYPE::LINE;
      return i;
    }
  }

  for (int i = 0; i < report.detectedAuxPoints->size(); i++)
  {
    featureDef_auxPoint *def = (*report.detectedAuxPoints)[i].def;
    if (def == NULL)
      continue;
    if (def->id == feature_id)
    {
      *ret_type = FEATURETYPE::AUX_POINT;
      return i;
    }
  }

  for (int i = 0; i < report.detectedSearchPoints->size(); i++)
  {
    featureDef_searchPoint *def = (*report.detectedSearchPoints)[i].def;
    if (def == NULL)
      continue;
    if (def->id == feature_id)
    {
      *ret_type = FEATURETYPE::SEARCH_POINT;
      return i;
    }
  }

  for (int i = 0; i < report.judgeReports->size(); i++)
  {
    FeatureReport_judgeDef *def = (*report.judgeReports)[i].def;
    if (def == NULL)
      continue;
    if (def->id == feature_id)
    {
      *ret_type = FEATURETYPE::MEASURE;
      return i;
    }
  }

  return -1;
}

int FeatureManager_sig360_circle_line::ParseMainVector(float flip_f, FeatureReport_sig360_circle_line_single &report, int feature_id, acv_XY *vec)
{
  if (vec == NULL)
    return -1;
  FEATURETYPE type;
  int idx = FindFeatureReportIndex(report, feature_id, &type);
  if (idx < 0)
    return -1;
  switch (type)
  {

  case MEASURE:
  case ARC:
  case AUX_POINT:
    return -1;
  case LINE:
  {
    if ((*report.detectedLines)[idx].status ==
        FeatureReport_sig360_circle_line_single::STATUS_NA)
    {
      return -2;
    }
    acv_LineFit line = (*report.detectedLines)[idx].line;
    *vec = line.line.line_vec;
    return 0;
  }
  case SEARCH_POINT:
  {
    FeatureReport_searchPointReport sPoint = (*report.detectedSearchPoints)[idx];

    // if (sPoint.status == FeatureReport_sig360_circle_line_single::STATUS_NA)
    // {
    //   return -2;
    // }
    if (sPoint.def->subtype != featureDef_searchPoint::anglefollow)
      return -1;
    acv_XY line_vec;
    int ret_val = ParseMainVector(flip_f, report, sPoint.def->data.anglefollow.target_id, &line_vec);
    if (ret_val < 0)
      return -1;
    float angle = sPoint.def->data.anglefollow.angleDeg * M_PI / 180;
    if (flip_f < 0)
      angle *= -1;
    acv_XY ret_vec = acvRotation(sin(angle), cos(angle), 1, line_vec);
    *vec = ret_vec;
    return 0;
  }
  break;
  }
  return -1;
}

acv_XY FeatureManager_sig360_circle_line::ParseMainVector(featureDef_searchPoint *def_sp)
{

  if (def_sp->subtype != featureDef_searchPoint::anglefollow)
  {
    return (acv_XY){NAN, NAN};
  }
  int linedef_id = def_sp->data.anglefollow.target_id;

  featureDef_line *defLine = NULL;
  for (int i = 0; i < featureLineList.size(); i++)
  {
    if (linedef_id == featureLineList[i].id)
    {
      defLine = &(featureLineList[i]);
      break;
    }
  }
  if (defLine == NULL)
  {
    return (acv_XY){NAN, NAN};
  }

  acv_XY line_vec = acvVecSub(defLine->p1, defLine->p0);

  float angle = def_sp->data.anglefollow.angleDeg * M_PI / 180;

  LOGI("line_p1:%f %f  p0:%f %f", defLine->p1.X, defLine->p1.Y, defLine->p0.X, defLine->p0.Y);
  LOGI("line_vec:%f %f  angle:%f", line_vec.X, line_vec.Y, angle);
  return acvRotation(sin(angle), cos(angle), 1, line_vec);
}

int FeatureManager_sig360_circle_line::lineCrossPosition(float flip_f, FeatureReport_sig360_circle_line_single &report, int obj1_id, int obj2_id, acv_XY *pt)
{
  if (pt == NULL)
    return -1;
  FEATURETYPE type1 = FEATURETYPE::NA, type2 = FEATURETYPE::NA;

  acv_XY vec1, vec2;

  if (ParseMainVector(flip_f, report, obj1_id, &vec1) != 0 ||
      ParseMainVector(flip_f, report, obj2_id, &vec2) != 0)
  {
    return -1;
  }

  acv_XY pt11, pt21;
  if (ParseLocatePosition(report, obj1_id, &pt11) != 0 ||
      ParseLocatePosition(report, obj2_id, &pt21) != 0)
  {
    return -1;
  }

  acv_XY pt12 = acvVecAdd(vec1, pt11);
  acv_XY pt22 = acvVecAdd(vec2, pt21);

  acv_XY cross = acvIntersectPoint(pt11, pt12, pt21, pt22);

  *pt = cross;
  return 0;
}

int FeatureManager_sig360_circle_line::ParseLocatePosition(FeatureReport_sig360_circle_line_single &report, int feature_id, acv_XY *pt)
{
  if (pt == NULL)
    return -1;
  FEATURETYPE type;
  int idx = FindFeatureReportIndex(report, feature_id, &type);
  if (idx < 0)
    return -1;
  switch (type)
  {

  case MEASURE:
    return -1;
  case ARC:
  {
    FeatureReport_circleReport cir = (*report.detectedCircles)[idx];

    if (cir.status == FeatureReport_sig360_circle_line_single::STATUS_NA)
    {
      return -2;
    }
    *pt = cir.circle.circle.circumcenter;
    return 0;
  }

  case AUX_POINT:
  {
    FeatureReport_auxPointReport aPoint = (*report.detectedAuxPoints)[idx];
    if (aPoint.status == FeatureReport_sig360_circle_line_single::STATUS_NA)
    {
      return -2;
    }
    *pt = aPoint.pt;
    return 0;
  }
  case LINE:
  {
    FeatureReport_lineReport line_report = (*report.detectedLines)[idx];
    if (line_report.status == FeatureReport_sig360_circle_line_single::STATUS_NA)
    {
      return -2;
    }
    acv_LineFit line = line_report.line;
    *pt = line.line.line_anchor;
    return 0;
  }
  case SEARCH_POINT:
  {
    FeatureReport_searchPointReport sPoint = (*report.detectedSearchPoints)[idx];
    if (sPoint.status == FeatureReport_sig360_circle_line_single::STATUS_NA)
    {
      return -2;
    }

    *pt = sPoint.pt;
    return 0;
  }
  break;
  }
  return -1;
}

int string_find_count(const char *str, char ch)
{
  int count = 0;
  for (int i = 0; str[i]; i++)
  {
    if (str[i] == ch)
    {
      count++;
    }
  }
  return count;
}

int parse_CALC_Id(const char *post_exp)
{
  if (post_exp[0] != '[')
    return -1;
  int idx = 0;
  for (int i = 1; post_exp[i]; i++)
  {
    char cc = post_exp[i];
    if ((cc < '0') || (cc > '9'))
      break;
    idx = idx * 10 + cc - '0';
  }
  return idx;
}

bool isParamsCache(const char *exp)
{
  if (*exp != '$')
    return false;
  for (int i = 1; exp[i]; i += 2)
  {
    if (exp[i] != ',')
      return false;
    if (exp[i + 1] != '$')
      return false;
  }

  return true;
}

bool strMatchExact(const char *src, const char *pat)
{
  for (int i = 0;; i++)
  {
    if (src[i] != pat[i])
      return false;
    if (src[i] == '\0')
      break;
  }
  return true;
}
double JfetchStrNUM(cJSON *obj, char *path)
{
  double *num = JFetch_NUMBER(obj, path);
  if (num)
    return *num;

  char *str_num = JFetch_STRING(obj, path);
  if (str_num == NULL)
    return NAN;
  try
  {
    double dou = std::stod(str_num);

    return dou;
  }
  catch (...)
  {
  }
  LOGI("EXP.....");
  return NAN;
}
int functionExec_(const char *exp, float *params, int paramL, float *ret_result)
{
  for (int i = 0; i < paramL; i++)
  {
    printf("[%d]:%f   ", i, params[i]);
  }
  printf("\n");
  if (ret_result)
    *ret_result = 0;
  if (strMatchExact(exp, "$+$"))
  {
    if (paramL != 2)
      return -1;
    *ret_result = params[0] + params[1];
    return 0;
  }
  else if (strMatchExact(exp, "$-$"))
  {

    if (paramL != 2)
      return -1;
    *ret_result = params[0] - params[1];
    return 0;
  }
  else if (strMatchExact(exp, "$*$"))
  {

    if (paramL != 2)
      return -1;
    *ret_result = params[0] * params[1];
    return 0;
  }
  else if (strMatchExact(exp, "$/$"))
  {

    if (paramL != 2)
      return -1;
    *ret_result = params[0] / params[1];
    return 0;
  }
  else if (strMatchExact(exp, "max$"))
  {
    float max = params[0];
    for (int i = 1; i < paramL; i++)
    {
      if (max < params[i])
        max = params[i];
    }
    *ret_result = max;
    return 0;
  }
  else if (strMatchExact(exp, "min$"))
  {
    float min = params[0];
    for (int i = 1; i < paramL; i++)
    {
      if (min > params[i])
        min = params[i];
    }
    *ret_result = min;
    return 0;
  }

  return -2;
}

int judge_CALC(FeatureReport_sig360_circle_line_single &reports, FeatureReport_judgeDef &judge, float *ret_result)
{
  vector<float> calcStack;

  //funcParamHeadIdx indicates the function params starts from
  //exp: [5,64,11]
  int funcParamCount = 0;

  //"exp": "max(sin([3]*3),0)",
  //"post_exp": ["[3]","3","$*$","sin$","0","$,$","max$"]
  for (int i = 0; i < judge.data.CALC.post_exp.size(); i++)
  {
    const string post_exp = judge.data.CALC.post_exp[i];
    //LOGI("post_exp[%d]:%s",i,post_exp.c_str());

    int id_exp = parse_CALC_Id(post_exp.c_str());
    if (id_exp >= 0)
    { //it's an id refence(  [4] means the result of judge report with id=4)
      int found_idx = -1;
      for (int j = 0; j < reports.judgeReports->size(); j++)
      {
        int id = (*reports.judgeReports)[j].def->id;
        if (id == id_exp)
        {
          found_idx = j;
        }
        else
          continue;
      }

      float val;
      if (found_idx >= 0)
      {
        int found_status = (*reports.judgeReports)[found_idx].status;
        if (found_status == FeatureReport_sig360_circle_line_single::STATUS_NA ||
            found_status == FeatureReport_sig360_circle_line_single::STATUS_UNSET)
        {
          if (ret_result)
            *ret_result = NAN;
          return -2;
        }
        else
          val = (*reports.judgeReports)[found_idx].measured_val;
      }
      else
      { //If there is no measure is found
        //val=NAN;
        if (ret_result)
          *ret_result = NAN;
        return -2;
      }
      calcStack.push_back(val);
      //(*reports.judgeReports)[]
    }
    else
    { //if it's not an id refence
      int paramSymbolCount = string_find_count(post_exp.c_str(), '$');
      if (paramSymbolCount > 0)
      { //if it's a function (sin$, cos$, max$)

        if (isParamsCache(post_exp.c_str()))
        { //If it's  $,$,.... just save the funcParamCount

          //LOGI("isParamsCache:%s>>>%d",post_exp.c_str(),paramSymbolCount);
          funcParamCount = paramSymbolCount;
        }
        else
        {
          if (funcParamCount > 1)
          {
            paramSymbolCount = funcParamCount;
          }
          funcParamCount = 1;

          //LOGI("isParamsCache:%s>>>%d",post_exp.c_str(),paramSymbolCount);
          float res;
          int err_code =
              functionExec_(
                  post_exp.c_str(),
                  &(calcStack[calcStack.size() - paramSymbolCount]),
                  paramSymbolCount,
                  &res);

          //LOGI("%f, %d",res,err_code);
          if (err_code != 0)
          {
            if (ret_result)
              *ret_result = NAN;
            return -50;
          }
          for (int k = 0; k < paramSymbolCount; k++)
          {
            calcStack.pop_back();
          }
          calcStack.push_back(res);
        }
      }
      else
      { //then it might be a number, try

        float val;
        try
        {
          val = std::stof(post_exp);
        }
        catch (...)
        {
          //val = NAN;
          if (ret_result)
            *ret_result = NAN;
          return -3;
        }
        calcStack.push_back(val);
      }
    }

    // for(int k=0;k<calcStack.size();k++)
    // {

    //   printf("%0.5f,",calcStack[k]);
    // }
    // printf("\n");
  }

  if (calcStack.size() != 1)
  {
    if (ret_result)
      *ret_result = NAN;
    return -50;
  }
  if (ret_result)
    *ret_result = calcStack[0];
  return 0;
}

FeatureReport_judgeReport FeatureManager_sig360_circle_line::measure_process(FeatureReport_sig360_circle_line_single &report,
                                                                             float sine, float cosine, float flip_f,
                                                                             FeatureReport_judgeDef &judge)
{
  FeatureReport_judgeReport judgeReport = {0};
  judgeReport.def = &judge;
  judgeReport.status = FeatureReport_sig360_circle_line_single::STATUS_NA;
  LOGV("judge:%s  OBJ1:%d, OBJ2:%d subtype:%d", judge.name, judge.OBJ1_id, judge.OBJ2_id, judge.measure_type);
  LOGV("val:%f  USL:%f,LSL:%f", judge.targetVal, judge.USL, judge.LSL);

  FEATURETYPE type1 = FEATURETYPE::NA, type2 = FEATURETYPE::NA;
  int idx1 = FindFeatureReportIndex(report, judge.OBJ1_id, &type1);
  int idx2 = FindFeatureReportIndex(report, judge.OBJ2_id, &type2);
  if (idx1 < 0)
    return judgeReport;

  judgeReport.measured_val = NAN;
  bool notNA = false;
  switch (judge.measure_type)
  {
  case FeatureReport_judgeDef::ANGLE:
    if (((type1 == FEATURETYPE::LINE) || (type1 == FEATURETYPE::SEARCH_POINT)) &&
        ((type2 == FEATURETYPE::LINE) || (type2 == FEATURETYPE::SEARCH_POINT)))
    {

      FEATURETYPE type1 = FEATURETYPE::NA, type2 = FEATURETYPE::NA;

      acv_XY vec1, vec2;

      if (ParseMainVector(flip_f, report, judge.OBJ1_id, &vec1) != 0 ||
          ParseMainVector(flip_f, report, judge.OBJ2_id, &vec2) != 0)
      {
        break;
      }

      acv_XY pt11, pt21;
      if (ParseLocatePosition(report, judge.OBJ1_id, &pt11) != 0 ||
          ParseLocatePosition(report, judge.OBJ2_id, &pt21) != 0)
      {
        break;
      }

      acv_XY pt12 = acvVecAdd(vec1, pt11);
      acv_XY pt22 = acvVecAdd(vec2, pt21);

      acv_XY interSectPt = acvIntersectPoint(pt11, pt12, pt21, pt22);
      acv_XY pt_mid = acvRotation(sine, cosine, flip_f, judge.data.ANGLE.pt);
      pt_mid = acvVecAdd(pt_mid, report.Center);

      acv_XY vec_mid = acvVecSub(pt_mid, interSectPt);

      if (acv2DDotProduct(vec1, vec_mid) < 0)
      {
        vec_mid = acvVecMult(vec_mid, -1);
      }

      if ((acv2DCrossProduct(vec1, vec2) < 0))
      {
        vec2 = acvVecMult(vec2, -1);
      }
      float sAngle = atan2(vec1.Y, vec1.X);
      float eAngle = atan2(vec2.Y, vec2.X);

      float midwayAngle = atan2(vec_mid.Y, vec_mid.X);
      float angleDiff = (eAngle - sAngle);
      float angleDiff_mid = (midwayAngle - sAngle);
      if (angleDiff_mid > M_PI)
      {
        angleDiff_mid -= 2 * M_PI;
      }
      if (angleDiff_mid < -M_PI)
      {
        angleDiff_mid += 2 * M_PI;
      }

      if (angleDiff < -M_PI)
      {
        angleDiff += 2 * M_PI;
      }
      if (angleDiff > M_PI)
      {
        angleDiff -= M_PI;
      }

      LOGV("angleDiff:%f ", angleDiff * 180 / M_PI);
      LOGV("angleDiff_mid:%f", angleDiff_mid * 180 / M_PI);

      if (angleDiff_mid < 0)
      {
        angleDiff = M_PI - angleDiff;
      }
      if (angleDiff < abs(angleDiff_mid))
      {
        angleDiff = M_PI - angleDiff;
      }

      // LOGI("pt_: %f %f   %f %f",pt11.X,pt11.Y,pt21.X,pt21.Y);
      // LOGI("pt_mid:%f %f   interSectPt:%f %f",pt_mid.X,pt_mid.Y,interSectPt.X,interSectPt.Y);
      // LOGI("sAngle:%f eAngle:%f midwayAngle:%f",sAngle*180/M_PI,eAngle*180/M_PI,midwayAngle*180/M_PI);
      // LOGI("angleDiff:%f ",angleDiff*180/M_PI);
      // LOGI("angleDiff_mid:%f",angleDiff_mid*180/M_PI);

      // LOGI("vec_mid:%f %f",vec_mid.X,vec_mid.Y);
      // LOGI("vec1:%f %f",vec1.X,vec1.Y);
      // LOGI("vec2:%f %f",vec2.X,vec2.Y);
      // LOGI("sAngle:%f eAngle:%f",sAngle*180/M_PI,eAngle*180/M_PI);
      // float angleDiff = (eAngle - sAngle)+20 * M_PI;
      // angleDiff=fmod(angleDiff, 2 * M_PI);

      judgeReport.measured_val = 180 * angleDiff / M_PI; //Convert to degree
      notNA = true;
    }

    break;
  case FeatureReport_judgeDef::DISTANCE:
  {
    int ret;
    acv_XY vec1, pt1, pt2;

    if (ParseLocatePosition(report, judge.OBJ1_id, &pt1) != 0 ||
        ParseLocatePosition(report, judge.OBJ2_id, &pt2) != 0)
    {

      judgeReport.measured_val = NAN;
      break;
    }
    int baseLine_id = judge.ref_baseLine_id;
    if (baseLine_id < 0)
      baseLine_id = judge.OBJ1_id;
    ret = ParseMainVector(flip_f, report, baseLine_id, &vec1);
    if (ret != 0)
    { //OBJ1 have no direction
      judgeReport.measured_val = acvDistance(pt1, pt2);
    }
    else
    {
      acv_Line line;
      line.line_vec = vec1;
      line.line_anchor = pt1;
      pt1 = acvClosestPointOnLine(pt2, line);
      judgeReport.measured_val = acvDistance(pt1, pt2);
    }

    notNA = true;
  }
  break;
  case FeatureReport_judgeDef::RADIUS:
  {
    if (type1 != FEATURETYPE::ARC)
      break;
    FeatureReport_circleReport cir = (*report.detectedCircles)[idx1];
    if (cir.status == FeatureReport_sig360_circle_line_single::STATUS_NA)
    {
      judgeReport.measured_val = NAN;
      notNA = false;
    }
    else
    {
      judgeReport.measured_val = cir.circle.circle.radius;
      notNA = true;
    }
  }
  break;

  case FeatureReport_judgeDef::CIRCLE_INFO:
  {
    if (type1 != FEATURETYPE::ARC)
      break;

    FeatureReport_circleReport cir = (*report.detectedCircles)[idx1];

    if (cir.status == FeatureReport_sig360_circle_line_single::STATUS_NA)
    {
      judgeReport.measured_val = NAN;
      notNA = false;
    }
    else
    {

      notNA = true;
      switch (judge.data.CIRCLE_INFO.info_type)
      {
      case FeatureReport_judgeDef::data::CIRCLE_INFO::MAX_DIAMETER:
        judgeReport.measured_val = cir.maxD;
        break;
      case FeatureReport_judgeDef::data::CIRCLE_INFO::MIN_DIAMETER:
        judgeReport.measured_val = cir.minD;
        break;
      case FeatureReport_judgeDef::data::CIRCLE_INFO::ROUGHNESS_MAX:
        judgeReport.measured_val = cir.roughness_MAX;
        break;
      case FeatureReport_judgeDef::data::CIRCLE_INFO::ROUGHNESS_MIN:
        judgeReport.measured_val = cir.roughness_MIN;
        break;
      default:
        judgeReport.measured_val = NAN;
        notNA = false;
        break;
      }

      if (judgeReport.measured_val != judgeReport.measured_val)
      {
        notNA = false;
      }
    }
  }
  break;
  case FeatureReport_judgeDef::SIGMA:
  {
    judgeReport.measured_val = 0;
  }
  break;
  case FeatureReport_judgeDef::CALC:
  {
    // for(int i=0;i<judge.data.CALC.post_exp.size();i++)
    // {
    //   (*report.judgeReports)[0].measured_val;
    //   LOGI("post_exp[%d]:%s",i,judge.data.CALC.post_exp[i].c_str());
    // }

    judgeReport.status = FeatureReport_sig360_circle_line_single::STATUS_UNSET;
    float res;
    int errCode = judge_CALC(report, judge, &res);
    judgeReport.measured_val = res;
    if (errCode == 0)
    {
      if (res == res)
      {
        notNA = true;
      }
      else
      {
        judgeReport.status = FeatureReport_sig360_circle_line_single::STATUS_NA;
        judgeReport.measured_val = NAN;
      }
    }
  }
  break;
  }

  { //Add adjust value
    float va = NAN;
    if (flip_f >= 0)
    {
      va = judgeReport.def->value_adjust;
    }
    else
    {
      va = judgeReport.def->value_adjust_b;
    }
    if (va = !va)
      va = 0;
    judgeReport.measured_val += va;
  }

  if (notNA)
  {
    float USL = flip_f < 0 ? judgeReport.def->USL_b : judgeReport.def->USL;
    float LSL = flip_f < 0 ? judgeReport.def->LSL_b : judgeReport.def->LSL;
    if (judgeReport.measured_val > USL ||
        judgeReport.measured_val < LSL)
    {
      judgeReport.status = FeatureReport_sig360_circle_line_single::STATUS_FAILURE;
    }
    else
    {
      judgeReport.status = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
    }
  }

  LOGV("===================");

  return judgeReport;
}



FeatureReport_judgeReport FeatureManager_sig360_circle_line::Judge_ReportGen(
  FeatureReport_judgeDef *def,
  FeatureReport_sig360_circle_line_single &singleReport,
  float cached_sin,float cached_cos,float flip_f)
{

  FeatureReport_judgeDef judge = *def;
  FeatureReport_judgeReport report = measure_process(singleReport, cached_sin, cached_cos, flip_f, judge);
  report.def = def;
  return report;
}

FeatureReport_auxPointReport FeatureManager_sig360_circle_line::APointMatching_ReportGen(  
  featureDef_auxPoint *def,
  FeatureReport_sig360_circle_line_single &singleReport,
  float sine,float cosine,float flip_f
  )
{



      // featureDef_auxPoint &apoint = *detectedAuxPoints[j].def;
      // if (apoint.subtype == featureDef_auxPoint::lineCross)
      // {
      //   //No value to convert, id only
      // }
      // else if (apoint.subtype == featureDef_auxPoint::centre)
      // {
      //   //No value to convert, id only
      // }
  FeatureReport_auxPointReport rep;
  rep.status = FeatureReport_sig360_circle_line_single::STATUS_NA;
  rep.def = def;
  switch (def->subtype)
  {
  case featureDef_auxPoint::lineCross:
  {
    acv_XY cross;
    int ret = lineCrossPosition(flip_f, singleReport,
                                def->data.lineCross.line1_id,
                                def->data.lineCross.line2_id, &cross);
    if (ret < 0)
    {
      return rep;
    }

    rep.status = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
    rep.pt = cross;
    return rep;
  }

  case featureDef_auxPoint::centre:
  {
    if (this->signature_feature_id == def->data.centre.obj1_id)
    {
      rep.status = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
      rep.pt = singleReport.Center;
      return rep;
    }
    acv_XY retXY;
    int ret_val = ParseLocatePosition(singleReport, def->data.centre.obj1_id, &retXY);
    if (ret_val != 0)
    {
      return rep;
    }
    rep.status = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
    rep.pt = retXY;
    return rep;
  }
  }
  return rep;
}
float checkPtNoise(ContourFetch::ptInfo pt)
{
  //(abs(pti.curvature) < 0.05)
  //if(abs(pt.curvature)<M_PI/10)return 0;

  return pt.contourDir.Y * pt.sobel.Y + pt.contourDir.X * pt.sobel.X;
}

/*
    <--epsilonX|epsilonX-->

                 Y
                 ^
     ____________|_____________
     |           |            |          ^epsilonY
  ---|-----------|------------|--->X     | 
     |___________|____________|          vepsilonY
  
*/
float distance_from_cage(acv_Line line, float epsilonX, float epsilonY, acv_XY point)
{
  acv_XY lvec = acvVecNormalize(line.line_vec);

  acv_XY rPt = acvRotation(-lvec.Y, lvec.X, 1, point);
  if (rPt.X < 0)
    rPt.X = -rPt.X;
  if (rPt.Y < 0)
    rPt.Y = -rPt.Y;
  rPt.X -= epsilonX;
  rPt.Y -= epsilonY;
  // if(rPt.X < 0 && rPt.Y < 0)//inside the cage
  // {
  //   return rPt.X >rPt.Y?rPt.X :rPt.Y;
  // }
  // else
  // {
  // }

  return rPt.X > rPt.Y ? rPt.X : rPt.Y;
}

FeatureReport_searchPointReport FeatureManager_sig360_circle_line::searchPoint_process(FeatureReport_sig360_circle_line_single &report,
                                                                                       acv_XY center,
                                                                                       float sine, float cosine, float flip_f, float thres,
                                                                                       featureDef_searchPoint &def, edgeTracking &eT)
{
  FeatureReport_searchPointReport rep;
  rep.status = FeatureReport_sig360_circle_line_single::STATUS_NA;
  rep.def = &def;
  def.tmp_pt.size();
  switch (def.subtype)
  {
  case featureDef_searchPoint::anglefollow:
  {
    acv_XY vec;
    int ret_val;

    {

      ret_val = ParseMainVector(flip_f, report, def.data.anglefollow.target_id, &vec);
      if (ret_val < 0)
      {
        break;
      }
      float angle = def.data.anglefollow.angleDeg * M_PI / 180;
      if (flip_f < 0)
      {
        angle = -angle; //depends on flip or not invert the angle
      }

      LOGV("line vec:%f %f", vec.X, vec.Y);
      vec = acvRotation(sin(angle), cos(angle), 1, vec); //No need to do the flip rotation
      LOGV("Angle:%f", angle * 180 / M_PI);
      LOGV("line vec:%f %f", vec.X, vec.Y);
    }

    // acv_XY pt = acvRotation(sine, cosine, flip_f, def.data.anglefollow.position); //Rotate the default point
    // pt = acvVecAdd(pt, center);
    acv_XY pt = def.data.anglefollow.position;

    if (flip_f > 0)
    {
      vec = acvVecMult(vec, -1);
    }
    float width = def.width;
    float margin = def.margin;

    vec = acvVecNormalize(vec);

    int searchDir = 1;
    int searchDir_NA_FLAG = -9999;
    if (def.data.anglefollow.search_far == true)
    {
      vec.X *= -1;
      vec.Y *= -1;
      searchDir = -1;
    }
    else
    {
    }
    // acvDrawCrossX(originalImage,
    //       (int)(pt_info.pt.X),(int)(pt_info.pt.Y),
    //       2,255,100,255,2);

    acv_XY searchVec_nor = vec;
    acv_XY searchVec = acvVecNormal(vec);

    LOGV("pt:%f %f", pt.X, pt.Y);
    LOGV("searchVec_nor:%f %f", searchVec_nor.X, searchVec_nor.Y);
    LOGV("searchVec:%f %f", searchVec.X, searchVec.Y);
    if (0)
    {

      acvDrawLine(originalImage,
                  pt.X,
                  pt.Y,
                  pt.X + searchVec.X * margin * 2,
                  pt.Y + searchVec.Y * margin * 2,
                  20, 255, 128);

      // acvDrawLine(originalImage,
      //             pt.X + searchVec_nor.X * width / 2,
      //             pt.Y + searchVec_nor.Y * width / 2,
      //             pt.X - searchVec_nor.X * width / 2,
      //             pt.Y - searchVec_nor.Y * width / 2,
      //             20, 255, 128);
    }

    m_sections.resize(0);

    acv_Line line = {line_vec : searchVec_nor, line_anchor : pt};
    acv_Line start_line = line;

    start_line.line_anchor = acvVecMult(searchVec, -999);                         //back margin vector
    start_line.line_anchor = acvVecAdd(start_line.line_anchor, line.line_anchor); //add line_anchor

    // LOGI("line vec %f %f, search_far:%d",line.line_vec.X,line.line_vec.Y,def.data.anglefollow.search_far);
    // LOGI("line_anchor %f %f",line.line_anchor.X,line.line_anchor.Y);

    if (searchDir != searchDir_NA_FLAG)
      edge_grid.getContourPointsWithInLineContour(line,
                                                  width / 2,
                                                  margin,
                                                  searchDir, m_sections, 999999, -0.5);

    float nearestDist = 99999999;
    acv_XY nearestPt;

    // for( auto &section_info:m_sections)
    // {
    //   for( auto &pt_info:section_info.section)
    //   {

    //     acv_XY pt=pt_info.pt_img;
    //     // LOGI("[%d]:%.2f %.2f",j,pt.X,pt.Y);

    //     // originalImage->CVector[(int)pt.Y][(int)pt.X*3]=255;
    //     originalImage->CVector[(int)pt.Y][(int)pt.X*3+1]=255;
    //     // originalImage->CVector[(int)pt.Y][(int)pt.X*3+2]=255;
    //   }
    // }

    ContourFetch::contourMatchSec *best_section_info = NULL;
    int nearestPt_idx = -1;
    for (auto &section_info : m_sections)
    {
      eT.initTracking(section_info, 0);
      for (int i = 0; i < section_info.section.size(); i++)
      {
        auto &pt_info = section_info.section[i];
        float dist = acvDistance(start_line, pt_info.pt);
        if (nearestDist > dist)
        {
          nearestPt = pt;
          nearestPt_idx = i;
          nearestDist = dist;
          best_section_info = &section_info;
        }
      }
    }

    if (nearestDist < 9999999 && best_section_info != NULL)
    {

      {
        // nearestPt

        int distance = 20;
        int startIdx = nearestPt_idx - distance;
        if (startIdx < 0)
          startIdx = 0;
        int endIdx = nearestPt_idx + distance;
        if (endIdx >= best_section_info->section.size())
          endIdx = best_section_info->section.size() - 1;
        def.tmp_pt.resize(0);
        for (int i = startIdx; i <= endIdx; i++)
        {
          def.tmp_pt.push_back(best_section_info->section[i]);
        }

        float rMIN, rMAX, rRMSE;
        roughness(def.tmp_pt, 5, &rMIN, &rMAX, &rRMSE);
        // LOGI("roughness: rMIN:%f,rMAX:%f,rRMSE:%f", rMIN,rMAX,rRMSE);
      }

      float accC = 0;
      nearestPt = acvVecMult(nearestPt, 0);
      for (auto &pt_info : best_section_info->section)
      {
        float dist = acvDistance(start_line, pt_info.pt);
        float reng = 2;
        if (nearestDist + reng > dist)
        {

          // float n = checkPtNoise(pt_info);
          // float abs_sobel=hypot(pt_info.sobel.X,pt_info.sobel.Y);
          // LOGI(">>>noise:%f  cur:%f |sobel|:%f",n,pt_info.curvature,n/abs_sobel);
          // if(abs(n/abs_sobel)>0.1)continue;

          float W = 1 - (dist - nearestDist) / reng;

          nearestPt = acvVecAdd(nearestPt, acvVecMult(pt_info.pt, W));
          accC += W;

          //acv_XY tmpPt = acvVecMult(nearestPt, 1.0 / accC);

          // LOGI("X:%f Y:%f D:%f W:%f",pt_info.pt.X,pt_info.pt.Y,dist,W);
          // LOGI(">>>X:%f Y:%f noise:%f",tmpPt.X,tmpPt.Y,n);
        }
      }

      if (accC == 0)
      {
        rep.pt.X = NAN;
        rep.pt.Y = NAN;
        rep.status = FeatureReport_sig360_circle_line_single::STATUS_NA;
      }
      else
      {
        nearestPt = acvVecMult(nearestPt, 1.0 / accC);

        float ret_rsp;
        // LOGI("nearestPt:%f %f",nearestPt.X,nearestPt.Y);
        //rep.pt = acvVecRadialDistortionRemove(nearestPt,param);
        rep.pt = nearestPt;
        rep.status = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
      }
    }
    else
    {
      rep.status = FeatureReport_sig360_circle_line_single::STATUS_NA;
    }

    LOGV("found:%d,rep.pt:%f %f, status:%d", 0, rep.pt.X, rep.pt.Y, rep.status);
  }
  break;
  }

  return rep;
}

int FeatureManager_sig360_circle_line::parse_searchPointData(cJSON *jobj)
{

  featureDef_searchPoint searchPoint;

  {
    char *tmpstr;
    searchPoint.name[0] = '\0';
    if (tmpstr = json_find_name(jobj))
    {
      strcpy(searchPoint.name, tmpstr);
    }
  }

  double *pnum;

  if ((pnum = JSON_GET_NUM(jobj, "id")) == NULL)
  {
    LOGE("No id");
    return -1;
  }
  searchPoint.id = (int)*pnum;
  LOGV("feature is an searchPoint:%s %d", searchPoint.name, searchPoint.id);

  //The subtype might be determined by definition file
  searchPoint.subtype = featureDef_searchPoint::anglefollow;

  searchPoint.margin =
      *JFetEx_NUMBER(jobj, "margin");

  searchPoint.width =
      *JFetEx_NUMBER(jobj, "width");

  switch (searchPoint.subtype)
  {
  case featureDef_searchPoint::anglefollow:
  {
    searchPoint.data.anglefollow.angleDeg =
        *JFetEx_NUMBER(jobj, "angleDeg");
    searchPoint.data.anglefollow.target_id = (int)*JFetEx_NUMBER(jobj, "ref[0].id");
    searchPoint.data.anglefollow.position.X =
        *JFetEx_NUMBER(jobj, "pt1.x");
    searchPoint.data.anglefollow.position.Y =
        *JFetEx_NUMBER(jobj, "pt1.y");

    void *target;
    searchPoint.data.anglefollow.search_far = false;
    int type = getDataFromJson(jobj, "search_far", &target);
    if (type == cJSON_False)
    {
      searchPoint.data.anglefollow.search_far = false;
    }
    else if (type == cJSON_True)
    {
      searchPoint.data.anglefollow.search_far = true;
    }

    searchPoint.data.anglefollow.locating_anchor = false;
    type = getDataFromJson(jobj, "locating_anchor", &target);
    if (type == cJSON_False)
    {
      searchPoint.data.anglefollow.locating_anchor = false;
    }
    else if (type == cJSON_True)
    {
      searchPoint.data.anglefollow.locating_anchor = true;
    }

    LOGV("searchPoint.X:%f Y:%f angleDeg:%f tar_id:%d",
         searchPoint.data.anglefollow.position.X,
         searchPoint.data.anglefollow.position.Y,
         searchPoint.data.anglefollow.angleDeg,
         searchPoint.data.anglefollow.target_id);
  }
  break;
  default:
    return -1;
  }

  searchPointList.push_back(searchPoint);

  return 0;
}

int FeatureManager_sig360_circle_line::parse_auxPointData(cJSON *jobj)
{
  featureDef_auxPoint auxPoint;

  strcpy(auxPoint.name, JFetEx_STRING(jobj, "name"));
  double *pnum;

  auxPoint.id = (int)*JFetEx_NUMBER(jobj, "id");
  LOGV("feature is an auxPoint:%s %d", auxPoint.name, auxPoint.id);

  if (JFetch_OBJECT(jobj, "ref[1]") != NULL)
  {
    //The subtype might be determined by definition file
    auxPoint.subtype = featureDef_auxPoint::lineCross;
    auxPoint.data.lineCross.line1_id = (int)*JFetEx_NUMBER(jobj, "ref[0].id");
    auxPoint.data.lineCross.line2_id = (int)*JFetEx_NUMBER(jobj, "ref[1].id");

    LOGV("id_1:%d id_2:%d",
         auxPoint.data.lineCross.line1_id, auxPoint.data.lineCross.line2_id);
  }
  else
  {
    auxPoint.subtype = featureDef_auxPoint::centre;
    auxPoint.data.centre.obj1_id = (int)*JFetEx_NUMBER(jobj, "ref[0].id");
    LOGV("the auxPoint is centre for the obj1_id:%d ",
         auxPoint.data.centre.obj1_id);
  }
  auxPointList.push_back(auxPoint);
  return 0;
}

featureDef_line lineDefDataPrep(featureDef_line pre) //,int id, float margin, acv_XY p0, acv_XY p1)
{
  featureDef_line &line = pre;
  // line.initMatchingMargin=margin;

  // line.p0=p0;
  // line.p1=p1;

  line.lineTar.line_anchor.X = (line.p0.X + line.p1.X) / 2;
  line.lineTar.line_anchor.Y = (line.p0.Y + line.p1.Y) / 2;
  line.lineTar.line_vec.X = (line.p1.X - line.p0.X);
  line.lineTar.line_vec.Y = (line.p1.Y - line.p0.Y);
  line.MatchingMarginX = hypot(line.lineTar.line_vec.X, line.lineTar.line_vec.Y) / 2;
  line.lineTar.line_vec = acvVecNormalize(line.lineTar.line_vec);

  // if (direction < 0)
  // {
  //   line.lineTar.line_vec = acvVecMult(line.lineTar.line_vec, -1);
  // }

  acv_XY normal = acvVecNormal(line.lineTar.line_vec);

  line.searchVec = normal;

  line.searchEstAnchor = line.lineTar.line_anchor;
  // line.searchEstAnchor.X -= normal.X * line.initMatchingMargin;
  // line.searchEstAnchor.Y -= normal.Y * line.initMatchingMargin;

  LOGV("anchor.X:%f anchor.Y:%f vec.X:%f vec.Y:%f ,MatchingMargin:%f",
       line.lineTar.line_anchor.X,
       line.lineTar.line_anchor.Y,
       line.lineTar.line_vec.X,
       line.lineTar.line_vec.Y,
       line.initMatchingMargin);
  LOGV("searchVec X:%f Y:%f vX:%f vY:%f,searchDist:%f",
       line.searchEstAnchor.X,
       line.searchEstAnchor.Y,
       line.searchVec.X,
       line.searchVec.Y,
       line.searchDist);

  return line;
}

int FeatureManager_sig360_circle_line::parse_lineData(cJSON *line_obj)
{
  featureDef_line line;
  line.MatchingMarginX = 0;
  double *pnum;

  {
    char *tmpstr;
    line.name[0] = '\0';
    if (tmpstr = json_find_name(line_obj))
    {
      strcpy(line.name, tmpstr);
    }
  }

  line.id = (int)*JFetEx_NUMBER(line_obj, "id");

  // LOGV("feature is a line:%s %d", line.name, line.id);

  line.initMatchingMargin = (float)*JFetEx_NUMBER(line_obj, "margin");

  acv_XY p0, p1;
  p0.X = *JFetEx_NUMBER(line_obj, "pt1.x");
  p0.Y = *JFetEx_NUMBER(line_obj, "pt1.y");
  p1.X = *JFetEx_NUMBER(line_obj, "pt2.x");
  p1.Y = *JFetEx_NUMBER(line_obj, "pt2.y");
  line.p0 = p0;
  line.p1 = p1;

  if (line.name[0] == '\0')
  {
    sprintf(line.name, "@LINE_%d", featureLineList.size());
  }
  line = lineDefDataPrep(line);
  featureLineList.push_back(line);
  return 0;
}

void sign360_process(vector<acv_XY> &target, vector<acv_XY> &buffer)
{
#define SIG_SOFT_SIZE 5
  SignatureSoften(target, SIG_SOFT_SIZE);
  // SignatureSoften(target,buffer,10);
  // for(int i=0;i<target.size();i++)
  // {
  //   float diff =1*buffer[i].X;
  //   LOGI("DIFF:%f-%f=%f ",target[i].X,diff,target[i].X-diff);
  //   target[i].X-=diff;
  // }
}

int FeatureManager_sig360_circle_line::parse_sign360(cJSON *signature_obj)
{

  this->signature_feature_id = (int)*JFetEx_NUMBER(signature_obj, "id");
  cJSON *signature;
  if (!(getDataFromJsonObj(signature_obj, "signature", (void **)&signature) & cJSON_Object))
  {
    LOGE("The signature is not an cJSON_Object");
    return -1;
  }

  feature_signature.RELOAD(signature);

  sign360_process(feature_signature.signature_data, signature_data_buffer);
  // signature_data_buffer

  return 0;
}

int FeatureManager_sig360_circle_line::parse_judgeData(cJSON *judge_obj)
{

  FeatureReport_judgeDef judge = {0};
  double *pnum;

  char *tmpstr = JFetEx_STRING(judge_obj, "name");
  strcpy(judge.name, tmpstr);

  judge.id = (int)*JFetEx_NUMBER(judge_obj, "id");

  judge.orientation_essential = false; //false by default
  {

    int vType = getDataFromJson(judge_obj, "orientation_essential", NULL);
    if (vType == cJSON_True)
    {
      judge.orientation_essential = true;
    }
    else if (vType == cJSON_False)
    {
      judge.orientation_essential = false;
    }
  }

  judge.quality_essential = true; //true by default
  {

    int vType = getDataFromJson(judge_obj, "quality_essential", NULL);
    if (vType == cJSON_True)
    {
      judge.quality_essential = true;
    }
    else if (vType == cJSON_False)
    {
      judge.quality_essential = false;
    }
  }

  char *subtype = JFetEx_STRING(judge_obj, "subtype");

  judge.measure_type = FeatureReport_judgeDef::NA;

  if (strcmp(subtype, "sigma") == 0)
  {
    judge.measure_type = FeatureReport_judgeDef::SIGMA;
  }
  else if (strcmp(subtype, "radius") == 0)
  {
    judge.measure_type = FeatureReport_judgeDef::RADIUS;
  }
  else if (strcmp(subtype, "circle_info") == 0)
  {
    judge.measure_type = FeatureReport_judgeDef::CIRCLE_INFO;

    char *infotype = JFetch_STRING(judge_obj, "info_type");
    if (strcmp(infotype, "max_diameter") == 0)
    {
      judge.data.CIRCLE_INFO.info_type = judge.data.CIRCLE_INFO.MAX_DIAMETER;
    }
    else if (strcmp(infotype, "min_diameter") == 0)
    {
      judge.data.CIRCLE_INFO.info_type = judge.data.CIRCLE_INFO.MIN_DIAMETER;
    }
    else if (strcmp(infotype, "roughness_max") == 0)
    {
      judge.data.CIRCLE_INFO.info_type = judge.data.CIRCLE_INFO.ROUGHNESS_MAX;
    }
    else if (strcmp(infotype, "roughness_min") == 0)
    {
      judge.data.CIRCLE_INFO.info_type = judge.data.CIRCLE_INFO.ROUGHNESS_MIN;
    }
    else if (strcmp(infotype, "roughness_rmse") == 0)
    {
      judge.data.CIRCLE_INFO.info_type = judge.data.CIRCLE_INFO.ROUGHNESS_RMSE;
    }
  }
  else if (strcmp(subtype, "distance") == 0)
  {
    judge.measure_type = FeatureReport_judgeDef::DISTANCE;
  }
  else if (strcmp(subtype, "calc") == 0)
  {
    judge.measure_type = FeatureReport_judgeDef::CALC;

    judge.data.CALC.exp.assign("");
    judge.data.CALC.post_exp.clear();

    //BLOCK exp
    {
      char *exp = JFetch_STRING(judge_obj, "calc_f.exp");
      if (exp != NULL)
      {
        judge.data.CALC.exp.assign(exp);
      }
    }

    for (int k = 0; k < 1000; k++)
    {
      char tmpStr[50];
      sprintf(tmpStr, "calc_f.post_exp[%d]", k);
      char *pexp = JFetch_STRING(judge_obj, tmpStr);
      if (pexp == NULL)
        break;
      judge.data.CALC.post_exp.push_back(pexp);
      //LOGI("[%d]:%s",k,pexp);
    }
  }
  else if (strcmp(subtype, "angle") == 0)
  {
    judge.measure_type = FeatureReport_judgeDef::ANGLE;
    pnum = JFetch_NUMBER(judge_obj, "quadrant");
    if (pnum == NULL)
    {
      judge.data.ANGLE.quadrant = 0;
    }
    else
    {
      judge.data.ANGLE.quadrant = (int)*pnum;
    }

    judge.data.ANGLE.pt.X = *JFetEx_NUMBER(judge_obj, "pt1.x");
    judge.data.ANGLE.pt.Y = *JFetEx_NUMBER(judge_obj, "pt1.y");

    LOGV("quadrant:%d", judge.data.ANGLE.quadrant);
  }
  else if (strcmp(subtype, "area") == 0)
  {
    judge.measure_type = FeatureReport_judgeDef::AREA;
  }

  if (judge.measure_type == FeatureReport_judgeDef::NA)
  {

    LOGV("the subtype:%s does not belong to what we support", subtype);
    LOGV("sigma,distance,angle,radius");
    return -1;
  }
  LOGV("feature is a measure/judge:%s id:%d subtype:%s", judge.name, judge.id, subtype);

  judge.targetVal_b = judge.targetVal = JfetchStrNUM(judge_obj, "value");
  judge.value_adjust_b = judge.value_adjust = JfetchStrNUM(judge_obj, "value_adjust");

  judge.USL_b = judge.USL = JfetchStrNUM(judge_obj, "USL");
  judge.LSL_b = judge.LSL = JfetchStrNUM(judge_obj, "LSL");

  void *target;
  int type = getDataFromJson(judge_obj, "back_value_setup", &target);
  if (type == cJSON_True)
  {
    judge.value_adjust_b = JfetchStrNUM(judge_obj, "value_adjust_b");
    judge.targetVal_b = JfetchStrNUM(judge_obj, "value_b");
    judge.USL_b = JfetchStrNUM(judge_obj, "USL_b");
    judge.LSL_b = JfetchStrNUM(judge_obj, "LSL_b");
  }

  pnum = JFetch_NUMBER(judge_obj, "ref[0].id");
  if (pnum == NULL)
    judge.OBJ1_id = -1;
  else
  {
    judge.OBJ1_id = *pnum;
  }

  pnum = JFetch_NUMBER(judge_obj, "ref[1].id"); //It's fine if we don't have OBJ2(ref[1])
  if (pnum == NULL)
    judge.OBJ2_id = -1;
  else
  {
    judge.OBJ2_id = *pnum;
  }

  pnum = JFetch_NUMBER(judge_obj, "ref_baseLine.id"); //It's fine if we don't have ref_baseLine
  if (pnum == NULL)
    judge.ref_baseLine_id = -1;
  else
  {
    judge.ref_baseLine_id = *pnum;
  }

  LOGV("value:%f USL:%f LSL:%f id1:%d id2:%d", judge.targetVal, judge.USL, judge.LSL, judge.OBJ1_id, judge.OBJ2_id);
  judgeList.push_back(judge);
  return 0;
}

int FeatureManager_sig360_circle_line::parse_jobj()
{
  const char *type_str = JFetch_STRING(root, "type");
  const char *ver_str = JFetch_STRING(root, "ver");
  const char *unit_str = JFetch_STRING(root, "unit");
  if (type_str == NULL || ver_str == NULL || unit_str == NULL)
  {
    LOGE("ptr: type:<%p>  ver:<%p>  unit:<%p>", type_str, ver_str, unit_str);
    return -1;
  }

  cm.clear();
  LOGI("type:<%s>  ver:<%s>  unit:<%s>", type_str, ver_str, unit_str);

  this->matching_angle_margin = M_PI; //+-PI => 2PI matching margin
  this->matching_angle_offset = 0;    //original signature init matching angle
  this->matching_face = 0;            //front and back facing
  this->matching_without_signature = true;
  {
    double *margin_deg = JFetch_NUMBER(root, "matching_angle_margin_deg");
    if (margin_deg != NULL)
    {
      this->matching_angle_margin = *margin_deg * M_PI / 180;
      double *angle_offset_deg = JFetch_NUMBER(root, "matching_angle_offset_deg");
      if (angle_offset_deg != NULL)
      {
        this->matching_angle_offset = *angle_offset_deg * M_PI / 180;
      }
    }
    double *face = JFetch_NUMBER(root, "matching_face");
    if (face != NULL)
    {
      this->matching_face = (int)*face;
    }

    void *target;
    int type = getDataFromJson(root, "matching_with_signature", &target);
    if (type == cJSON_False)
    {
      this->matching_without_signature = false;
    }

    single_result_area_ratio = -1;

    double *ratio = JFetch_NUMBER(root, "single_result_area_ratio");
    if (ratio != NULL)
    {
      this->single_result_area_ratio = (float)*ratio;
      LOGI("single_result_area_ratio:%f", this->single_result_area_ratio);
    }
  }

  cJSON *featureList = cJSON_GetObjectItem(root, "features");
  cJSON *inherentfeatureList = cJSON_GetObjectItem(root, "inherentfeatures");

  if (featureList == NULL)
  {
    LOGE("features array does not exists");
    return -1;
  }

  if (!cJSON_IsArray(featureList))
  {
    LOGE("features is not an array");
    return -1;
  }

  for (int i = 0; i < cJSON_GetArraySize(featureList); i++)
  {
    cJSON *feature = cJSON_GetArrayItem(featureList, i);
    const char *feature_type = JFetch_STRING(feature, "type");
    if (feature_type == NULL)
    {
      LOGE("feature[%d] has no type...", i);
      return -1;
    }

    if (strcmp(feature_type, "arc") == 0)
    {
      if (parse_arcData(feature) != 0)
      {
        LOGE("feature[%d] has error %s format", i, feature_type);
        return -1;
      }
    }
    else if (strcmp(feature_type, "line") == 0)
    {
      if (parse_lineData(feature) != 0)
      {
        LOGE("feature[%d] has error %s format", i, feature_type);
        return -1;
      }
    }
    else if (strcmp(feature_type, "aux_point") == 0)
    {

      if (parse_auxPointData(feature) != 0)
      {
        LOGE("feature[%d] has error %s format", i, feature_type);
        return -1;
      }
    }
    else if (strcmp(feature_type, "search_point") == 0)
    {

      if (parse_searchPointData(feature) != 0)
      {
        LOGE("feature[%d] has error %s format", i, feature_type);
        return -1;
      }
    }
    else if (strcmp(feature_type, "measure") == 0)
    {
      if (parse_judgeData(feature) != 0)
      {
        LOGE("feature[%d] has error %s format", i, feature_type);
        return -1;
      }
    }
    else if (strcmp(feature_type, "aux_line") == 0)
    {
      LOGE("TODO: feature[%d] feature[%d] ", i, feature_type);
      return -1;
    }
    else if (strcmp(feature_type, "measure_calc") == 0)
    {
      LOGE("TODO: feature[%d] feature[%d] ", i, feature_type);
      return -1;
    }
    else
    {
      LOGE("feature[%d] has unknown type:[%s]", i, feature_type);
      return -1;
    }
  }

  for (int j = 0; j < searchPointList.size(); j++)
  {
    acv_XY pos = searchPointList[j].data.anglefollow.position;
    acv_XY vec = ParseMainVector(&searchPointList[j]);
    LOGI("XY:%f %f", vec.X, vec.Y);
    vec = acvVecNormal(vec);
    LOGI("XY:%f %f", vec.X, vec.Y);
    cm.add(pos, pos, vec);
  }


  if (inherentfeatureList == NULL)
  {
    LOGE("inherentfeatures does not exists");
    return -1;
  }

  for (int i = 0; i < cJSON_GetArraySize(inherentfeatureList); i++)
  {
    cJSON *feature = cJSON_GetArrayItem(inherentfeatureList, i);
    const char *feature_type = JFetch_STRING(feature, "type");
    if (feature_type == NULL)
    {
      LOGE("feature[%d] has no type...", i);
      return -1;
    }
    if (strcmp(feature_type, "sign360") == 0)
    {
      if (parse_sign360(feature) != 0)
      {
        LOGE("feature[%d] has error %s format", i, feature_type);
        return -1;
      }
    }
    else if (strcmp(feature_type, "aux_point") == 0)
    {
      if (parse_auxPointData(feature) != 0)
      {
        LOGE("feature[%d] has error %s format", i, feature_type);
        return -1;
      }
    }
    else if (strcmp(feature_type, "aux_line") == 0)
    {
      //return -1;
    }
    else
    {
      LOGE("feature[%d] has unknown type:[%s]", i, feature_type);
      return -1;
    }
  }

  {
    for (int i = 0; i < featureLineList.size(); i++)
    {
      LOGV("featureLineList[%d]:%s", i, featureLineList[i].name);
    }
    for (int i = 0; i < featureCircleList.size(); i++)
    {
      LOGV("featureCircleList[%d]:%s", i, featureCircleList[i].name);
    }
    /*for(int i=0;i<judgeList.size();i++)
    {
      LOGV("judgeList[%d]:%s  OBJ1:%s, OBJ2:%s type:%d",i,judgeList[i].name,judgeList[i].OBJ1,judgeList[i].OBJ2,judgeList[i].measure_type);
      LOGV("-val:%f  margin:%f",judgeList[i].targetVal,judgeList[i].targetVal_margin);
    }*/
  }

  if (this->matching_without_signature && feature_signature.signature_data.size() == 0)
  {
    LOGE("No signature data");
    return -1;
  }

  return 0;
}

int FeatureManager_sig360_circle_line::reload(const char *json_str)
{
  if (root)
  {
    cJSON_Delete(root);
  }
  featureCircleList.resize(0);
  featureLineList.resize(0);
  judgeList.resize(0);

  auxPointList.resize(0);
  searchPointList.resize(0);

  feature_signature.RESET(0);

  root = cJSON_Parse(json_str);
  if (root == NULL)
  {
    LOGE("cJSON parse failed");
    return -1;
  }
  int ret_err = parse_jobj();
  if (ret_err != 0)
  {
    featureCircleList.resize(0);
    featureLineList.resize(0);
    feature_signature.RESET(0);
    reload("");
    return -2;
  }
  return 0;
}

int searchP(acvImage *img, acv_XY *pos, acv_XY searchVec, float maxSearchDist)
{
  if (img == NULL || pos == NULL)
    return -1;
  int X = (int)round(pos->X);
  int Y = (int)round(pos->Y);
  if (X < 0 || Y < 0 || X >= img->GetWidth() || Y >= img->GetHeight())
  {
    return -1;
  }

  searchVec = acvVecNormalize(searchVec);

  int tarX = 0;
  if (img->CVector[Y][3 * X] == 255)
  {
    tarX = 0; //Looking for non-255
  }
  else
  {
    /*tarX=255;//Looking for 255
    searchVec.X*=-1;//reverse search vector
    searchVec.Y*=-1;*/
    return -1;
  }

  for (int i = 0; i < maxSearchDist; i++)
  {
    X = (int)round(pos->X + searchVec.X * i);
    Y = (int)round(pos->Y + searchVec.Y * i);
    if (X < 0 || Y < 0 || X >= img->GetWidth() || Y >= img->GetHeight())
    {
      return -1;
    }

    if (img->CVector[Y][3 * X] == 255)
    {
      if (tarX == 255)
      {
        pos->X = pos->X + searchVec.X * (i - 1); //Get previous non-255
        pos->Y = pos->Y + searchVec.Y * (i - 1);
        return 0;
      }
    }
    else
    {
      if (tarX != 255)
      {
        pos->X = pos->X + searchVec.X * (i);
        pos->Y = pos->Y + searchVec.Y * (i);
        return 0;
      }
    }
  }

  return -1;
}

const FeatureReport *FeatureManager_sig360_circle_line::GetReport()
{
  report.type = FeatureReport::sig360_circle_line;
  //report.error = FeatureReport_sig360_circle_line::NONE;
  report.data.sig360_circle_line.reports = &reports;
  return &report;
}

void FeatureManager_sig360_circle_line::ClearReport()
{

  report.data.sig360_circle_line.error = FeatureReport_ERROR::NONE;
  reports.resize(0);
  report.data.sig360_circle_line.reports = &reports;
  FeatureManager_binary_processing::ClearReport();

  LOGI("bacpac:<%p>  report.type:%p", bacpac, report.type);
}

float OTSU_Threshold(acvImage &graylevelImg, acv_LabeledData *ldata, int skip = 5)
/* binarization by Otsu's method 
	based on maximization of inter-class variance */

{
  const int GRAYLEVEL = 256;
  int hist[GRAYLEVEL];
  float prob[GRAYLEVEL], omega[GRAYLEVEL]; /* prob of graylevels */
  float myu[GRAYLEVEL];                    /* mean value for separation */
  float max_sigma, sigma[GRAYLEVEL];       /* inter-class variance */
  int i, x, y;                             /* Loop variable */
  int threshold;                           /* threshold for binarization */

  /* Histogram generation */
  for (i = 0; i < GRAYLEVEL; i++)
    hist[i] = 0;
  int count = 0;
  for (y = ldata->LTBound.Y; y < ldata->RBBound.Y; y += skip)
    for (x = ldata->LTBound.X; x < ldata->RBBound.X; x += skip)
    {
      hist[graylevelImg.CVector[y][3 * x]]++;
      count++;
    }
  /* calculation of probability density */
  int totalPix = count;
  for (i = 0; i < GRAYLEVEL; i++)
  {
    prob[i] = (double)hist[i] / totalPix;
  }

  /* omega & myu generation */
  omega[0] = prob[0];
  myu[0] = 0.0; /* 0.0 times prob[0] equals zero */
  for (i = 1; i < GRAYLEVEL; i++)
  {
    omega[i] = omega[i - 1] + prob[i];
    myu[i] = myu[i - 1] + i * prob[i];
  }

  /* sigma maximization
     sigma stands for inter-class variance 
     and determines optimal threshold value */
  threshold = 0;
  max_sigma = 0.0;
  for (i = 0; i < GRAYLEVEL - 1; i++)
  {
    if (omega[i] != 0.0 && omega[i] != 1.0)
      sigma[i] = pow(myu[GRAYLEVEL - 1] * omega[i] - myu[i], 2) /
                 (omega[i] * (1.0 - omega[i]));
    else
      sigma[i] = 0.0;
    if (sigma[i] > max_sigma)
    {
      max_sigma = sigma[i];
      threshold = i;
    }

    {
      //printf("ELSE Update %d %f\n",i,sigma[i]);
    }
  }

  int searchRange = 7;
  float sigmaBaseIdx = threshold - (searchRange - 1) / 2;
  // float retMaxF = 0;
  // float retMaxF_X = 0;
  // spline9_max(&(sigma[(int)sigmaBaseIdx]), searchRange, 10, &retMaxF, &retMaxF_X);
  // sigmaBaseIdx += retMaxF_X;
  //printf("\nthreshold value =s:%f i:%d  %f %f %f\n",max_sigma, threshold,retMaxF,sigmaBaseIdx,retMaxF_X);

  return sigmaBaseIdx;
}

int EdgeGradientAdd(acvImage *graylevelImg, acv_XY gradVec, acv_XY point, vector<ContourFetch::ptInfo> ptList, int width)
{
  const int GradTableL = 7;
  float gradTable[GradTableL] = {0};

  //curpoint = point -(GradTableL-1)*gVec/2
  gradVec = acvVecNormalize(gradVec);
  //gradVec= acvVecMult(gradVec,1);

  acv_XY curpoint = acvVecMult(gradVec, -(float)(GradTableL - 1) / 2);
  curpoint = acvVecAdd(curpoint, point);
  ContourFetch::ptInfo tmp_pt;
  for (int i = 0; i < GradTableL; i++)
  {
    float ptn = acvUnsignedMap1Sampling(graylevelImg, curpoint, 0);
    //LOGV("%f<<%f,%f",ptn,curpoint.X,curpoint.Y);
    gradTable[i] = ptn;
    tmp_pt.pt = curpoint;
    curpoint = acvVecAdd(curpoint, gradVec);
  }

  curpoint = acvVecMult(gradVec, -(float)(GradTableL - 1) / 2);
  curpoint = acvVecAdd(curpoint, point);
  for (int i = 0; i < GradTableL - 1; i++)
  {
    float diff = gradTable[i] - gradTable[i + 1];
    if (diff < 0)
      diff = -diff;

    tmp_pt.pt = curpoint;
    tmp_pt.edgeRsp = diff * diff * diff * diff * diff;
    ptList.push_back(tmp_pt);
    curpoint = acvVecAdd(curpoint, gradVec);
  }

  return 0;
}
bool ptInfo_tmp_comp(const ContourFetch::ptInfo &a, const ContourFetch::ptInfo &b)
{
  return a.tmp < b.tmp;
}

const acv_LineFit lf_zero = {0};
const FeatureReport_lineReport lr_NA = {line : lf_zero, status : FeatureReport_sig360_circle_line_single::STATUS_NA};

bool drawDraw = false;

FeatureReport_lineReport SingleMatching_line(featureDef_line *line, edgeTracking &eT, float ppmm,
                                             acv_Line line_cand, ContourFetch &edge_grid, float flip_f,
                                             vector<ContourFetch::ptInfo> &tmp_points, vector<ContourFetch::contourMatchSec> &m_sections)
{
  line->tmp_pt.clear();
  float sigma;
  int mult = 100;

  acv_XY target_vec = {0};
  tmp_points.resize(0);

  // Apply distortion remove on init line candidate anchor(line 2335 Sig360_cir_lin..)
  // The init line candidate based on labeled image point scanning(5 init dots) and the labeled img is pre-distortion removal
  // Also, the point info in Contour grid are post-distortion removal. So the region will be misaligned.
  // So the solution is to apply acvVecRadialDistortionRemove for the init line_cand, that will compensate the problem.
  // Ideally, the line vector and region width should be compensated as well, but.... if the lens is really that bad, you better find a really good reason to use that.

  // bacpac->sampler->img2ideal(&line_cand.line_anchor);

  float MatchingMarginX = line->MatchingMarginX;
  float initMatchingMargin = line->initMatchingMargin;
  // {
  //   mult=MatchingMarginX;
  //   acvDrawLine(originalImage,
  //     line_cand.line_anchor.X-mult*line_cand.line_vec.X,
  //     line_cand.line_anchor.Y-mult*line_cand.line_vec.Y,
  //     line_cand.line_anchor.X+mult*line_cand.line_vec.X,
  //     line_cand.line_anchor.Y+mult*line_cand.line_vec.Y,
  //     20,255,128);
  // }
  edge_grid.getContourPointsWithInLineContour(line_cand,
                                              MatchingMarginX,

                                              //HACK:Kinda hack... the initial Margin is for initial keypoints search,
                                              //But since we get the cadidate line already, no need for huge Margin
                                              initMatchingMargin,
                                              flip_f, m_sections);

  if (m_sections.size() == 0)
  { //No match... FAIL
    LOGI("Not able to find matching contour");
    FeatureReport_lineReport lr = lr_NA;
    lr.def = line;
    return lr;
  }

  float section_sigma_thres = 9999; //m_sections[0].sigma + 3;
  vector<ContourFetch::ptInfo> &s_points = tmp_points;
  {
    s_points.resize(0);
    //for (int k = 0; k < m_sections.size(); k++)

    float minFactor = 9999999;
    for (int tk = 0; tk < m_sections.size(); tk++)
    {
      float factor = m_sections[tk].dist;
      // if(flip_f<0)factor*=-1;
      // factor+=(m_sections[tk].sigma/100);

      // LOGI("Sec,%f   f:%f",m_sections[tk].dist,factor);

      if (minFactor > factor)
      {
        minFactor = factor;
      }
    }

    // LOGI("minFactor:%f",minFactor);
    for (int tk = 0; tk < m_sections.size(); tk++)
    {

      float factor = m_sections[tk].dist;
      // if(flip_f<0)factor*=-1;

      if (minFactor < factor) //only use the min one
      {
        m_sections[tk].section.clear();
      }
    }

    // LOGI(">>>>>");

    for (int k = 0; k < m_sections.size(); k++)
    {
      eT.initTracking(m_sections[k], 0);
      // for(int m=0;m<m_sections[k].section.size();m++)
      // {
      //   LOGI("a:%d:%f,%f>",m,m_sections[k].section[m].pt.X,m_sections[k].section[m].pt.Y);
      // }
      if (m_sections[k].sigma > section_sigma_thres)
        continue;

      float dEnd = (float)MatchingMarginX * 0.8;
      float fEdge = dEnd * 3 / 5;
      float fEdgeMargin = dEnd - fEdge;
      float closestDist = 9999;
      for (int i = 0; i < m_sections[k].section.size(); i++) //edge off
      {

        acv_XY ptOnCandLine = acvClosestPointOnLine(m_sections[k].section[i].pt, line_cand);
        float distance = acvDistance(ptOnCandLine, line_cand.line_anchor);

        float fadeOutIdx = 1; //(float)i/downCount;
        float ratio;
        if (distance < fEdge)
        {
          fadeOutIdx = 0;
          ratio = 1;
        }
        else
        {
          fadeOutIdx = (distance - fEdge) / fEdgeMargin;
          if (fadeOutIdx > 1)
            fadeOutIdx = 1;
        }
        ratio = (1 + cos(fadeOutIdx * M_PI)) / 2;
        // printf("%4.2f ",ratio);
        // float ratio = 1-fadeOutIdx;
        m_sections[k].section[i].edgeRsp *= ratio;
      }

      for (auto ptInfo : m_sections[k].section)
      {
        s_points.push_back(ptInfo);
      }
    }
  }

  // if (drawDraw)
  //   for (int i = 0; i < m_sections.size(); i++)
  //   {

  //     LOGI("[%d]=================", i);
  //     for (int j = 0; j < m_sections[i].section.size(); j++)
  //     {
  //       acv_XY pt = m_sections[i].section[j].pt;
  //       // LOGI("[%d]:%.2f %.2f",j,pt.X,pt.Y);

  //       originalImage->CVector[(int)pt.Y][(int)pt.X * 3 + 1] = 255;
  //     }
  //   }

  if (s_points.size() > 10 && false)
  {
    acv_XY lineNormal = {X : -line_cand.line_vec.Y, Y : line_cand.line_vec.X};

    int sptL = s_points.size();

    float minS_pts = 0;
    float minSigma = 99999;

    int sampleL = s_points.size() / 10 + 3;
    if (sampleL > s_points.size())
    {
      sampleL = s_points.size() / 10;
    }
    if (sampleL > 20)
    {
      sampleL = 20;
    }
    for (int m = 0; m < 17; m++) //RANSAC find a good starting line
    {
      for (int k = 0; k < sampleL; k++) //Shuffle in
      {
        int idx2Swap = (rand() % (s_points.size() - k)) + k;

        ContourFetch::ptInfo tmp_pt = s_points[k];
        s_points[k] = s_points[idx2Swap];
        s_points[idx2Swap] = tmp_pt;
        //s_points[j].edgeRsp=1;
      }

      acv_Line tmp_line;
      acvFitLine(
          &(s_points[0].pt), sizeof(ContourFetch::ptInfo),
          &(s_points[0].edgeRsp), sizeof(ContourFetch::ptInfo), sampleL, &tmp_line, &sigma);

      int sigma_count = 0;
      float sigma_sum = 0;
      for (int i = 0; i < s_points.size(); i++)
      {
        float diff = acvDistance_Signed(tmp_line, s_points[i].pt);
        float abs_diff = (diff < 0) ? -diff : diff;
        // if (abs_diff > 5)
        // {
        //   //s_points[j].edgeRsp=0;
        //   continue;
        // }
        sigma_count++;
        sigma_sum += diff * diff;
      }
      sigma_sum = sqrt(sigma_sum / sigma_count);
      if (minSigma > sigma_sum)
      {
        minS_pts = sigma_count;
        minSigma = sigma_sum;
        line_cand = tmp_line;
        //LOGI("minSigma:%f",minSigma);
      }
    }

    int usable_L = 0;
    for (int k = 0; k < 2; k++)
    {
      usable_L = 0;
      if (s_points.size() == 0)
      {
        usable_L = 0;
        FeatureReport_lineReport lr = lr_NA;
        lr.def = line;
        return lr;
      }

      for (int n = 0; n < s_points.size(); n++)
      {
        float dist = acvDistance_Signed(line_cand, s_points[n].pt);
        if (dist < 0)
          dist = -dist;
        s_points[n].tmp = dist;
        //s_points[i].edgeRsp=1;
      }

      std::sort(s_points.begin(), s_points.end(), ptInfo_tmp_comp); //
      // LOGI("distThres:%f",s_points[s_points.size()*2 / 3].tmp);
      float distThres = s_points[s_points.size() * 2 / 3].tmp + 3;

      // float distThres = s_points[s_points.size() / 3].tmp*1.1;
      LOGV("sort finish size:%d, distThres:%f", s_points.size(), distThres);

      usable_L = s_points.size();

      //usable_L=usable_L*10/11;//back off
      LOGV("usable_L:%d/%d  minSigma:%f=>%f",
           usable_L, s_points.size(),
           s_points[usable_L - 1].tmp, distThres);
      float sigma;
      acvFitLine(
          &(s_points[0].pt), sizeof(ContourFetch::ptInfo),
          &(s_points[0].edgeRsp), sizeof(ContourFetch::ptInfo),
          usable_L, &line_cand, &sigma);
      if (1)
      {

        s_points.resize(0);
        for (int k = 0; k < m_sections.size(); k++)
        {
          float newSigma = 0;
          for (auto ptInfo : m_sections[k].section)
          {
            float dist = acvDistance(line_cand, ptInfo.pt);
            newSigma += dist * dist;
          }
          m_sections[k].sigma = newSigma = sqrt(newSigma / m_sections[k].section.size());

          if (newSigma > section_sigma_thres)
            continue;
          for (auto ptInfo : m_sections[k].section)
          {
            float dist = acvDistance(line_cand, ptInfo.pt);
            ptInfo.tmp = dist;
            s_points.push_back(ptInfo);
          }
        }

        float pdistThres = -1;
        for (int n = 0; n < s_points.size(); n++) //forward pass to link slow diviating points
        {
          s_points[n].curvature = 0;
          //this time curvature marks as consider flag for final line matching
          if (s_points[n].tmp < distThres)
          {
            pdistThres = s_points[n].tmp;
            s_points[n].curvature = 1;
            continue;
          }
          if (pdistThres == -1)
          {
            continue;
          }
          if (s_points[n].tmp > 2 * distThres) //If the diviation is too much still mark it as ignore
          {
            pdistThres = -1;
            continue;
          }

          //The distance is above the distThres
          float diff = pdistThres - s_points[n].tmp;
          if (diff < 0)
            diff = -diff;
          if (diff < distThres / 5)
          {
            pdistThres = s_points[n].tmp;
            s_points[n].curvature = 0.5;
            continue;
          }

          //The difference is too much mark ignore
          pdistThres = -1;
          continue;
        }

        pdistThres = -1;
        for (int n = s_points.size() - 1; n >= 0; n--) //backward pass to link slow diviating points
        {
          //this time curvature marks as weight weight for final line matching
          if (s_points[n].curvature == 1)
          {
            pdistThres = s_points[n].tmp;
            continue;
          }
          if (pdistThres == -1)
          {
            continue;
          }
          if (s_points[n].tmp > 2 * distThres) //If the diviation is too much still mark it as ignore
          {
            pdistThres = -1;
            continue;
          }

          //The distance is above the distThres
          float diff = pdistThres - s_points[n].tmp;
          if (diff < 0)
            diff = -diff;
          if (diff < distThres / 5)
          {
            pdistThres = s_points[n].tmp;
            s_points[n].curvature = 0.5;
            continue;
          }
          //The difference is too much mark ignore
          pdistThres = -1;
          continue;
        }

        for (int n = 0; n < s_points.size(); n++) //remove edgeRsp if the consideration weight is zero
        {

          s_points[n].edgeRsp *= s_points[n].curvature;
        }

        acvFitLine(
            &(s_points[0].pt), sizeof(ContourFetch::ptInfo),
            &(s_points[0].edgeRsp), sizeof(ContourFetch::ptInfo),
            s_points.size(), &line_cand, &sigma);
      }
    }

    if (usable_L == 0)
    {
      FeatureReport_lineReport lr = lr_NA;
      lr.def = line;
      return lr;
    }
  }
  else
  {
    acvFitLine(
        &(s_points[0].pt), sizeof(ContourFetch::ptInfo),
        &(s_points[0].edgeRsp), sizeof(ContourFetch::ptInfo),
        s_points.size(), &line_cand, &sigma);
  }

  /*acvDrawLine(buff_,
          line_cand.line_anchor.X-mult*line_cand.line_vec.X,
          line_cand.line_anchor.Y-mult*line_cand.line_vec.Y,
          line_cand.line_anchor.X+mult*line_cand.line_vec.X,
          line_cand.line_anchor.Y+mult*line_cand.line_vec.Y,
          20,255,128);*/

  if (acv2DDotProduct(line_cand.line_vec, target_vec) < 0)
  {
    LOGV("VEC INV::::");
    line_cand.line_vec = acvVecMult(line_cand.line_vec, -1);
  }

  // line_cand.line_anchor.X+=line_cand.line_vec.X;
  // line_cand.line_anchor.Y+=line_cand.line_vec.Y;
  // LOGI("L=%d===anchor:%f,%f vec:%f,%f ,sigma:%f target_vec:%f,%f,", j,
  //      line_cand.line_anchor.X,
  //      line_cand.line_anchor.Y,
  //      line_cand.line_vec.X,
  //      line_cand.line_vec.Y,
  //      sigma,
  //      target_vec.X, target_vec.Y);

  float rMIN, rMAX, rRMSE;
  roughness(s_points, 5, &rMIN, &rMAX, &rRMSE);

  LOGI("L===anchor:%f,%f vec:%f,%f ,sigma:%f rMin:%f rMax:%f rRMSE:%f",

       line_cand.line_anchor.X,
       line_cand.line_anchor.Y,
       line_cand.line_vec.X,
       line_cand.line_vec.Y,
       sigma,
       rMIN, rMAX, rRMSE);
  acv_LineFit lf;
  lf.line = line_cand;
  lf.matching_pts = s_points.size();
  lf.s = sigma;
  FeatureReport_lineReport lr;
  // lr.rough_RMSE=rRMSE;
  // lr.rough_MAX=rMAX;
  // lr.rough_MIN=rMIN;
  lr.line = lf;
  lr.def = line;
  lr.line.end_pt1 = acvClosestPointOnLine(line->p0, line_cand);
  lr.line.end_pt2 = acvClosestPointOnLine(line->p1, line_cand);
  if (lf.matching_pts < 5)
  {
    lr.status = FeatureReport_sig360_circle_line_single::STATUS_NA;
  }
  else
  {
    for (auto ptInfo : s_points)
    {
      line->tmp_pt.push_back(ptInfo);
    }
    lr.status = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
  }

  return lr;
}

#define TYPE_JUMP(type, ptr, jump) (*(type *)(((unsigned char *)ptr) + jump))

float CrossProduct(acv_XY p1, acv_XY p2, acv_XY p3)
{
  acv_XY v1 = (acvVecSub(p2, p1));
  acv_XY v2 = (acvVecSub(p2, p3));

  return acv2DCrossProduct(v1, v2);
}

float CrossProduct_norm(acv_XY p1, acv_XY p2, acv_XY p3)
{
  acv_XY v1 = acvVecNormalize(acvVecSub(p2, p1));
  acv_XY v2 = acvVecNormalize(acvVecSub(p2, p3));

  return acv2DCrossProduct(v1, v2);
}
void ConvexVertex(const acv_XY *polygon, const int L, const int p_step, std::vector<int> &ret_idxes, float convexT = 0, float mergeT = 0)
{
  // The polygon needs to have at least three points
  std::vector<int> &upperIdx = ret_idxes;
  upperIdx.resize(0);
  if (L < 3)
  {
    return;
  }

  upperIdx.push_back(0);
  upperIdx.push_back(1);
  /*
	We piecewise construct the convex hull and combine them at the end of the method. Note that this could be
	optimized by combing the while loops.
	*/
  for (size_t i = 2; i < L; i++)
  {

    while (upperIdx.size() >= 2)
    {

      acv_XY p1 = TYPE_JUMP(acv_XY, polygon, upperIdx[(upperIdx.size() - 2)] * p_step);
      acv_XY p2 = TYPE_JUMP(acv_XY, polygon, upperIdx[(upperIdx.size() - 1)] * p_step);
      acv_XY p3 = TYPE_JUMP(acv_XY, polygon, i * p_step);
      float crossP = CrossProduct_norm(p1, p2, p3);
      if (crossP >= -convexT)
      {
        // LOGI("pop: p1:%f %f  p2:%f %f  pc:%f %f      idx:%d",p1.X,p1.Y,p2.X,p2.Y,polygon[i].X,polygon[i].Y,upperIdx[(upperIdx.size() - 1)]);
        upperIdx.pop_back();
      }
      else
      {
        break;
      }
    }
    upperIdx.push_back(i);
  }

  if (mergeT > 0.0001)
    while (1)
    {

      int maxCP = -99999;
      int maxCP_idx = -1;
      for (size_t i = 1; i < upperIdx.size() - 1; i++)
      {
        acv_XY p1 = TYPE_JUMP(acv_XY, polygon, upperIdx[i - 1] * p_step);
        acv_XY pM = TYPE_JUMP(acv_XY, polygon, upperIdx[i - 0] * p_step);
        acv_XY p2 = TYPE_JUMP(acv_XY, polygon, upperIdx[i + 1] * p_step);
        float crossP = CrossProduct_norm(p1, pM, p2);
        LOGI("i:[%d]  -mergeT:%f crossP:%f", i, -mergeT, crossP);
        if (-mergeT < crossP && maxCP < crossP)
        {
          maxCP = crossP;
          maxCP_idx = i;
        }
      }

      if (maxCP_idx != -1)
      {
        for (size_t i = maxCP_idx; i < upperIdx.size() - 1; i++)
        {
          upperIdx[i] = upperIdx[i + 1];
        }
        upperIdx.resize(upperIdx.size() - 1);
        //do vertex reduction
      }

      LOGI("upperIdx.size():%d", upperIdx.size());
      if (maxCP_idx == -1 || upperIdx.size() <= 2)
        break;
    }

  for (size_t i = 1; i < upperIdx.size() - 1; i++)
  {
    acv_XY p1 = TYPE_JUMP(acv_XY, polygon, upperIdx[i - 1] * p_step);
    acv_XY pM = TYPE_JUMP(acv_XY, polygon, upperIdx[i - 0] * p_step);
    acv_XY p2 = TYPE_JUMP(acv_XY, polygon, upperIdx[i + 1] * p_step);
    float crossP = CrossProduct_norm(p1, pM, p2);
    LOGI(">>i:[%d]  -mergeT:%f crossP:%f", i, -mergeT, crossP);
  }
  // upperIdx.insert(upperIdx.end(), lowerIdx.begin(), lowerIdx.end());
  return;
}
float distRatioOnLine(acv_Line line, acv_XY sp1, acv_XY sp2, acv_XY px)
{ //  sp1...px......sp2 => return 3/9 => 0.3333

  sp1 = acvClosestPointOnLine(sp1, line);
  sp2 = acvClosestPointOnLine(sp2, line);
  px = acvClosestPointOnLine(px, line);

  acv_XY vecF = acvVecSub(sp2, sp1);
  acv_XY vecM = acvVecSub(px, sp1);

  float ratio = hypot(vecM.X, vecM.Y) / hypot(vecF.X, vecF.Y);
  if (acv2DDotProduct(vecF, vecM) < 0)
  {
    ratio = -ratio;
  }
  return ratio;
}
acv_XY ptOnLineRatio(acv_Line line, acv_XY sp1, acv_XY sp2, float ratio)
{ //  sp1...ret......sp2 <= 0.3333
  acv_XY ret;

  sp1 = acvClosestPointOnLine(sp1, line);
  sp2 = acvClosestPointOnLine(sp2, line);

  acv_XY vecF = acvVecSub(sp2, sp1);
  return acvVecAdd(acvVecMult(vecF, ratio), sp1);
}

acv_XY Image_mm_Domain_TO_TemplateDomain(acv_XY im_mm_pt, float sin, float cosin, float flip_f, acv_XY objCenter_pix, float mmpp)
{ //  sp1...ret......sp2 <= 0.3333
  acv_XY pt = acvVecSub(im_mm_pt,acvVecMult( objCenter_pix, mmpp));


  pt = acvRotation(-sin, cosin, pt);
  if (flip_f < 0)
    pt.Y = -pt.Y;

  return pt;
}
acv_XY PixDomain_TO_TemplateDomain(acv_XY pix_pt, float sin, float cosin, float flip_f, acv_XY objCenter_pix, float mmpp)
{ //  sp1...ret......sp2 <= 0.3333
  acv_XY pt = acvVecMult(acvVecSub(pix_pt, objCenter_pix), mmpp);
  pt = acvRotation(-sin, cosin, pt);
  if (flip_f < 0)
    pt.Y = -pt.Y;

  return pt;
}

acv_XY TemplateDomain_TO_PixDomain(acv_XY temp_pt, float sin, float cosin, float flip_f, acv_XY objCenter_pix, float mmpp)
{ //  sp1...ret......sp2 <= 0.3333

  acv_XY pt = acvRotation(sin, cosin, flip_f, temp_pt);

  return acvVecAdd(acvVecMult(pt, 1 / mmpp), objCenter_pix); //convert to pixel unit
}

acv_XY ConstrainMap::convert_polar(acv_XY from)
{
  // LOGI(">>>d  anchorPairs.size():%d", anchorPairs.size());
  float magSum = 0;
  float magWSum = 0;

  acv_XY angSum = {0, 0};
  float angWSum = 0;
  bool doAdj = false;
  for (int i = 0; i < anchorPairs.size(); i++)
  {
    anchorPair pair = anchorPairs[i];
    if (pair.to.X != pair.to.X) //NAN
    {
      continue;
    }
    doAdj = true;
    float distance = acvDistance(from, pair.from);
    if (distance < 0.01)
      distance = 0.01;
    float w = 1 / distance;
    // w*=w;
    acv_XY vec_a = acvVecSub(pair.to, center);

    acv_XY pFrom = pair.from;

    if (0) //try to use pre_projected point for directional adjustment, but it doesn't work very well
    {
      acv_XY v_to2from = acvVecSub(pair.from, pair.to);
      v_to2from = acvVecMult(pair.constrainVector, acv2DDotProduct(v_to2from, pair.constrainVector));
      acv_XY pFrom = acvVecAdd(v_to2from, pair.to);
    }

    //LOGI("pFrom:%f %f pair.to:%f %f pair.from:%f %f  cv:%f %f",pFrom.X,pFrom.Y,vec_a.X,vec_a.Y,pair.from.X,pair.from.Y,pair.constrainVector.X,pair.constrainVector.Y);
    // pair.constrainVector

    acv_XY vec_b = acvVecSub(pFrom, center);

    acv_XY shift = acvComplexDiv(vec_a, vec_b);

    float shiftMag = hypot(shift.X, shift.Y);
    // LOGI("from:%f %f c:%f %f  to:%f %f",pair.from.X,pair.from.Y,center.X,center.Y,pair.constrainVector.X,pair.constrainVector.Y);

    float magDotP = acv2DDotProduct(acvVecNormalize(vec_b), pair.constrainVector); //the magnitude
    if (magDotP < 0)
      magDotP = -magDotP;
    magWSum += magDotP * w;
    magSum += shiftMag * magDotP * w;

    acv_XY shiftAng = acvVecNormalize(shift);
    float angDotP = sqrt(1 - magDotP * magDotP);
    angWSum += angDotP * w;
    angSum = acvVecAdd(angSum, acvVecMult(shiftAng, angDotP * w));
  }
  if (!doAdj)
    return from;
  // magSum/=magWSum;
  acv_XY wshift = acvVecMult(angSum, magSum / angWSum / magWSum);

  // LOGI("angSum:%f %f magSum: %f  angWSum:%f  magWSum:%f",angSum.X,angSum.Y,magSum,angWSum,magWSum);

  // LOGI("vecAve:%f %f",vecSum.X,vecSum.Y);
  return acvComplexMult(from, wshift);
}

acv_XY ConstrainMap::convert(acv_XY from)
{
  return convert_polar(from);
}
acv_XY ConstrainMap::convert_vec(acv_XY from)
{
  acv_XY wvecSum = {0, 0};
  acv_XY vecSum = {0, 0};

  for (int i = 0; i < anchorPairs.size(); i++)
  {
    anchorPair pair = anchorPairs[i];
    if (pair.to.X != pair.to.X) //NAN
    {
      continue;
    }
    float distance = acvDistance(from, pair.from);
    if (distance < 0.01)
      distance = 0.01;
    float w = 1 / distance;

    // w*=w;
    acv_XY wvec = acvVecMult(pair.constrainVector, w);

    acv_XY pos_wvec = wvec;
    if (pos_wvec.X < 0)
      pos_wvec.X = -pos_wvec.X;
    if (pos_wvec.Y < 0)
      pos_wvec.Y = -pos_wvec.Y;

    wvecSum = acvVecAdd(wvecSum, pos_wvec);

    acv_XY vec = acvVecSub(pair.to, pair.from);
    float dotP = acv2DDotProduct(vec, pair.constrainVector);
    // LOGI("pos_wvec: %f %f  vec:%f %f",pos_wvec.X,pos_wvec.Y,vec.X,vec.Y);
    LOGI("[%d] w:%f dotP:%f from: %f %f   vec: %f %f  wvec:%f %f  wvecSum:%f %f", i, w, dotP, pair.from.X, pair.from.Y, vec.X, vec.Y, wvec.X, wvec.Y, wvecSum.X, wvecSum.Y);
    vecSum = acvVecAdd(vecSum, acvVecMult(wvec, dotP));
  }
  // LOGI("vecSum:%f %f wvecSum: %f %f",vecSum.X,vecSum.Y,wvecSum.X,wvecSum.Y);
  vecSum.X /= wvecSum.X;
  vecSum.Y /= wvecSum.Y;

  LOGI("vecSum:%f %f  wvecSum:%f %f", vecSum.X, vecSum.Y, wvecSum.X, wvecSum.Y);
  return acvVecAdd(vecSum, from);
}

inline int valueWarping(int v, int ringSize)
{
  v %= ringSize;
  return (v < 0) ? v + ringSize : v;
}
void matchingErrorMin_Refine(vector<acv_XY> &sigMatchErr, vector<acv_XY> &minMatchErr)
{
  minMatchErr.resize(0);
  for (int i = 0; i < sigMatchErr.size(); i++)
  {
    acv_XY pre = sigMatchErr[valueWarping(i - 1, sigMatchErr.size())];

    acv_XY cur = sigMatchErr[i];

    acv_XY post = sigMatchErr[valueWarping(i + 1, sigMatchErr.size())];

    if (pre.Y > cur.Y && post.Y > cur.Y)
    {
      minMatchErr.push_back(sigMatchErr[i]);
    }
  }
}

void minSegRefine(ContourSignature &tar, ContourSignature &src, float ScanW, int ScanCount, vector<acv_XY> &minMatchErr, bool flip, int fineStride = 1)
{
  vector<acv_XY> sigMatchErr(tar.signature_data.size());
  for (int i = 0; i < minMatchErr.size(); i++)
  {
    acv_XY cAng = minMatchErr[i];

    tar.match_span(src, cAng.X - ScanW, cAng.X + ScanW, ScanCount, sigMatchErr, fineStride, flip);

    acv_XY minErr = sigMatchErr[0];
    for (int j = 1; j < sigMatchErr.size(); j++)
    {
      if (minErr.Y > sigMatchErr[j].Y)
      {
        minErr = sigMatchErr[j];
      }
    }
    minMatchErr[i] = minErr;
  }
}

void xrefine(ContourSignature &tar, ContourSignature &src, float roughScanCount, vector<acv_XY> &minMatchErr, bool flip, int roughStride = 3, int fineStride = 1, float tarPrecision = 1)
{

  vector<acv_XY> sigMatchErr(360);
  tar.match_span(src, 0, 360, roughScanCount, sigMatchErr, roughStride, flip);

  // for(int i=0;i<sigMatchErr.size();i++)
  // {
  //   LOGI("%f,%f",sigMatchErr[i].X,sigMatchErr[i].Y);
  // }

  matchingErrorMin_Refine(sigMatchErr, minMatchErr);

  // for(int i=0;i<minMatchErr.size();i++)
  // {
  //   LOGI("%f>%f",minMatchErr[i].X,minMatchErr[i].Y);
  // }

  float ScanW = 1.5 * 360 / roughScanCount;
  float ScanCount = 3;
  for (int i = 0;; i++)
  {
    int doScanCount = ScanCount * 2;

    float prec = 2.0 * ScanW / doScanCount;
    minSegRefine(tar, src, ScanW, doScanCount, minMatchErr, flip, fineStride); //over scan

    // LOGI("ScanW:%f,ScanCount:%f,prec:%f",ScanW,ScanCount,prec);
    if (prec < tarPrecision)
    {
      break;
    }
    ScanW /= ScanCount; //shrink the scan width-> improve precision
  }

  // for(int i=0;i<tar.signature_data.size();i++)
  // {
  //   LOGI("[%d]:%f  %f",i,tar.signature_data[i].Y*180/M_PI,src.signature_data[i].Y*180/M_PI);
  // }

  // LOGI("tarAO:%f,srcAO:%f",tar.angleOffset*180/M_PI,src.angleOffset*180/M_PI);

  // for(int i=0;i<minMatchErr.size();i++)
  // {
  //   LOGI("-%f>%f",minMatchErr[i].X,minMatchErr[i].Y);
  // }
}

inline float angle_0_to_2pi(float ang)
{
  float _ang = fmod(ang, 2 * M_PI);
  return (_ang < 0) ? _ang + 2 * M_PI : _ang;
}
inline float angle_npi_to_pi(float ang)
{
  float _ang = angle_0_to_2pi(ang);
  if (_ang > M_PI)
    _ang -= 2 * M_PI;
  return _ang;
}

bool isAngleInRegion(float angle, float from, float to)
{
  angle = angle_0_to_2pi(angle);
  from = angle_0_to_2pi(from);
  to = angle_0_to_2pi(to);

  // LOGI("a:%f f:%f t:%f", angle * 180 / M_PI, from * 180 / M_PI, to * 180 / M_PI);
  if (to < from)
    to += 2 * M_PI;
  if (angle < from)
    angle += 2 * M_PI;
  return (angle >= to);
}

//normalized_thres default value is obtained threshold by test no absolute reason
bool isErrPass(float error,ContourSignature &sig,float normalized_thres=0.1)
{
  float errThres2nd=(error / sig.mean);
  
  LOGI("error:%f sig.mean:%f errThres2nd:%f",error,sig.mean,errThres2nd);
  if ((errThres2nd > normalized_thres))
  {
    return false;
  }

  return true;

}

FeatureReport_searchPointReport FeatureManager_sig360_circle_line::SPointMatching_ReportGen(
  featureDef_searchPoint *def,
  FeatureReport_sig360_circle_line_single &singleReport,
  edgeTracking &eT,
  acv_XY calibCen,float mmpp,float cached_cos,float cached_sin,float flip_f)

{
// thres, spoint

  featureDef_searchPoint spoint = *def;
  spoint.margin /= mmpp;
  spoint.width /= mmpp;
  spoint.data.anglefollow.position=cm.convert(spoint.data.anglefollow.position);

  if (spoint.subtype == featureDef_searchPoint::anglefollow)
  {

    spoint.data.anglefollow.position =
        TemplateDomain_TO_PixDomain(spoint.data.anglefollow.position,
                                    cached_sin, cached_cos, flip_f, calibCen, mmpp);
  }

  FeatureReport_searchPointReport report = searchPoint_process(singleReport, calibCen, cached_sin, cached_cos, flip_f, 0, spoint, eT);

  report.pt =
      acvVecMult(report.pt, mmpp);
  report.def = def;
  return report;

}



FeatureReport_circleReport FeatureManager_sig360_circle_line::CircleMatching_ReportGen(
  featureDef_circle *plineDef,edgeTracking &eT,
  acv_XY calibCen,float mmpp,float cached_cos,float cached_sin,float flip_f)
{

  featureDef_circle &cdef = *plineDef;

  float ppmm=1/mmpp;
  acv_XY m_pt1 = cm.convert(cdef.pt1);
  acv_XY m_pt2 = cm.convert(cdef.pt2);
  acv_XY m_pt3 = cm.convert(cdef.pt3);

  m_pt1 = acvRotation(cached_sin, cached_cos, flip_f, m_pt1);
  m_pt2 = acvRotation(cached_sin, cached_cos, flip_f, m_pt2);
  m_pt3 = acvRotation(cached_sin, cached_cos, flip_f, m_pt3);

  m_pt1 = acvVecMult(m_pt1, ppmm);
  m_pt2 = acvVecMult(m_pt2, ppmm);
  m_pt3 = acvVecMult(m_pt3, ppmm);

  m_pt1 = acvVecAdd(m_pt1, calibCen);
  m_pt2 = acvVecAdd(m_pt2, calibCen);
  m_pt3 = acvVecAdd(m_pt3, calibCen);

  arc_data arcD = convert3Pts2ArcData(m_pt1, m_pt2, m_pt3);

  float initMatchingMargin = cdef.initMatchingMargin * ppmm;

  int matching_tor = initMatchingMargin;

  //LOGV("flip_f:%f angle:%f sAngle:%f  eAngle:%f",flip_f,angle,cdef.sAngle,cdef.eAngle);

  acv_XY center = arcD.circleTar.circumcenter;
  float sAngle = arcD.sAngle, eAngle = arcD.eAngle;
  float radius = arcD.circleTar.radius;

  // acvDrawCrossX(originalImage,
  //               center.X, center.Y,
  //               4, 255,255,255);
  //LOGV("X:%f Y:%f r(%f)*ppmm(%f)=r(%f)",center.X,center.Y,cdef.circleTar.radius,ppmm,radius);
  edge_grid.getContourPointsWithInCircleContour(
      center.X,
      center.Y,
      radius,
      sAngle, eAngle, cdef.outter_inner,
      matching_tor, m_sections);

  LOGI("edge_grid:%p flip_f:%f radius:%f sAngle:%f  eAngle:%f   XY:%f,%f ppmm:%f mmpp:%f ",&edge_grid, flip_f, radius, sAngle, eAngle,center.X,
  center.Y,ppmm,mmpp);
  LOGI("matching_tor:%d initMatchingMargin:%f m_sections.size:%d",
  matching_tor,cdef.initMatchingMargin,m_sections.size());

  acv_CircleFit cf;
  FeatureReport_circleReport cr;

  cdef.tmp_pt.clear();
  cr.pt1 = cr.pt2 = cr.pt3 = (acv_XY){NAN, NAN};


  if (m_sections.size() == 0)
  {

    LOGE("Circle matching failed: resultR:%f defR:%f",
          cf.circle.radius, arcD.circleTar.radius);
    cr.def = plineDef;
    cr.status = FeatureReport_sig360_circle_line_single::STATUS_NA;
    return cr;
  }


  {
    float minFactor = 9999999;
    for (int tk = 0; tk < m_sections.size(); tk++)
    {
      float factor = m_sections[tk].dist;
      // if(flip_f<0)factor*=-1;
      // factor+=(m_sections[tk].sigma/100);

      LOGI("Sec[%d].dist:%f   f:%f",tk,m_sections[tk].dist,factor);

      if (minFactor > factor)
      {
        minFactor = factor;
      }
    }

    // LOGI("minFactor:%f",minFactor);
    for (int tk = 0; tk < m_sections.size(); tk++)
    {

      float factor = m_sections[tk].dist;
      // if(flip_f<0)factor*=-1;

      if (minFactor < factor) //only use the min one
      {
        m_sections[tk].section.clear();
      }
    }
  }






  // for (auto &secInfo : m_sections)
  // {

  //   eT.initTracking(secInfo,0);
  //   // eT.initTracking(secInfo,3);
  //   LOGI("secInfo.sigma:%f", secInfo.sigma);
  // }
  tmp_points.clear();
  for (int k = 0; k < m_sections.size(); k++)
  {

    eT.initTracking(m_sections[k], 0);
    vector<ContourFetch::ptInfo> &s_points = m_sections[k].section;
    // LOGI("secInfo[%d].sigma:%f size:%d",k, m_sections[k].sigma, m_sections[k].section.size());
    // for (int i = 0; i < s_points.size(); i++)
    // {
    //   acv_XY p = s_points[i].pt;
    //   bacpac->sampler->ideal2img(&p);
    //   int X = round(p.X);
    //   int Y = round(p.Y);
    //   {
    //     originalImage->CVector[Y][X * 3] = i*(k+1)*10;
    //     originalImage->CVector[Y][X * 3 + 1] = 255;
    //     originalImage->CVector[Y][X * 3 + 2] = 255;
    //   }
    // }
    for (int i = 0; i < s_points.size(); i++)
    {
      tmp_points.push_back(s_points[i]);
    }
  }
  vector<ContourFetch::ptInfo> &s_points = tmp_points;

  // LOGI("NAN:  C=%d==",j);
  // for(int m=0;m<s_points.size();m++)
  // {
  //   LOGI("XY:%f %f  >> %f %f   edgeRsp:%f",s_points[m].sobel.X,s_points[m].sobel.Y,s_points[m].pt.X,s_points[m].pt.Y,s_points[m].edgeRsp);
  // }

  circleRefine(s_points, s_points.size(), &cf);

  float minTor = matching_tor / 2;
  if (minTor < 1)
    minTor = 1;

  // LOGI("s_points.size():%d  r:%f", s_points.size(),cf.circle.radius);
  cr.circle = cf;
  cr.def = &cdef;

  // LOGI("C=%d===R:%f,pt:%f,%f , tarR:%f",
  //      j, cf.circle.radius,cf.circle.circumcenter.X ,cf.circle.circumcenter.Y,radius);

  if (cf.circle.radius != cf.circle.radius) //check NaN
  {
    cr.def = plineDef;
    // LOGI("Circle search failed: resultR:%f defR:%f",
    //      cf.circle.radius, cdef.circleTar.radius);
    cr.status = FeatureReport_sig360_circle_line_single::STATUS_NA;
    return cr;
  }

  // if (drawDBG_IMG)
  // {

  //   acvDrawCrossX(originalImage,
  //                 center.X, center.Y,
  //                 3, 3);

  //   acvDrawCrossX(originalImage,
  //                 cf.circle.circumcenter.X, cf.circle.circumcenter.Y,
  //                 5, 3);

  //   acvDrawCircle(originalImage,
  //                 cf.circle.circumcenter.X, cf.circle.circumcenter.Y,
  //                 cf.circle.radius,
  //                 20, 255, 0, 0);
  // }

  if (1)
  { //find the max & min  dist
    acv_XY center = cf.circle.circumcenter;
    const int angleRes = 360;
    float RArray[angleRes] = {-1};

    for (int k = 0; k < angleRes; k++)
    {
      RArray[k] = -1;
    }
    for (auto _pt : s_points)
    {
      // LOGI("pt:%0.2f,%0.2f,center:%0.2f,%0.2f",_pt.X,_pt.Y,center.X,center.Y);
      acv_XY pt = acvVecSub(_pt.pt, center);
      float theta_deg = atan2(pt.Y, pt.X) * 180 / M_PI;
      if (theta_deg < 0)
      {
        theta_deg += 360;
      }
      int thdegInt = (int)(theta_deg * angleRes / 360);
      if (thdegInt >= angleRes)
        thdegInt -= angleRes;

      float mag = hypot(pt.Y, pt.X);
      if (RArray[thdegInt] < mag)
      {
        RArray[thdegInt] = mag;
      }
    }

    const float angleStep = 2 * M_PI / angleRes;
    const int angleSweepHS = 2; //-2 -1 0 1 2
    // float cosTable[angleSweepHS+1];
    // for(int k=0;k<=angleSweepHS;k++)
    // {
    //   cosTable[k]=cos(M_PI-k*angleStep);
    // }

    float maxDArray[angleRes / 2] = {-1};
    float minDArray[angleRes / 2] = {-1};
    float maxD = -1;
    float minD = __FLT_MAX__;

    //c^2=a^2+b^2-2ab cos(theda)
    for (int k = 0; k < angleRes / 2; k++)
    {
      float a = RArray[k];
      if (a < 0)
        continue;
      float maxC = -1;
      float minC = __FLT_MAX__;
      float _b = RArray[k + angleRes / 2];
      for (int m = -angleSweepHS; m <= angleSweepHS; m++)
      {
        int absm = m;
        if (absm < 0)
          absm = -absm;
        int idx2 = k + angleRes / 2 + m;
        if (idx2 >= angleRes)
          idx2 -= angleRes;
        float b = RArray[idx2];
        if (b < 0)
          continue;
        // float c = sqrt(a*a+b*b+2*a*b*cosTable[absm]);
        float c = a + b;

        // LOGI("a:%f,b:%f,c:%f",a,b,c);
        if (maxC < c)
          maxC = c;
        if (minC > c)
          minC = c;
      }
      // LOGI("maxC[%d]:%f  a:%f,b:%f",k,maxC,a,_b);
      if (maxC != -1)
      {
        maxDArray[k] = maxC;
        if (maxD < maxC)
          maxD = maxC;
        if (minD > maxC)
          minD = maxC;
      }
      if (minC != -1)
      {
        minDArray[k] = minC;
      }
    }

    cr.maxD = (maxD == -1) ? NAN : maxD;
    cr.minD = (minD == __FLT_MAX__) ? NAN : minD;
    LOGI("C===maxD:%f minD:%f", cr.maxD, cr.minD);

    for (auto pt : s_points)
    {
      cdef.tmp_pt.push_back(pt);
    }
    float rMIN, rMAX, rRMSE;
    roughness(cdef.tmp_pt, 5, &rMIN, &rMAX, &rRMSE);
    cr.roughness_MIN = rMIN;
    cr.roughness_MAX = rMAX;
    cr.roughness_RMSE = rRMSE;
    LOGI("C===R:%f,pt:%f,%f , tarR:%f, rMIN:%f ,rMAX:%f ,rRMSE:%f",
          cf.circle.radius, cf.circle.circumcenter.X, cf.circle.circumcenter.Y, radius, rMIN, rMAX, rRMSE);
  }

  cr.pt1 = acvClosestPointOnCircle(m_pt1, cf.circle);
  cr.pt2 = acvClosestPointOnCircle(m_pt2, cf.circle);
  cr.pt3 = acvClosestPointOnCircle(m_pt3, cf.circle);

  if (cf.circle.radius < radius - initMatchingMargin ||
      cf.circle.radius > radius + initMatchingMargin)
  {
    cr.status = FeatureReport_sig360_circle_line_single::STATUS_FAILURE;
  }
  else
  {
    cr.status = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
  }
  cr.def = plineDef;


  {
    
    cr.circle.s *= mmpp;
    cr.circle.circle.circumcenter =
        acvVecMult(cr.circle.circle.circumcenter, mmpp);
    cr.circle.circle.radius *= mmpp;
    cr.maxD *= mmpp;
    cr.minD *= mmpp;

    cr.roughness_MAX *= mmpp;
    cr.roughness_MIN *= mmpp;
    cr.roughness_RMSE *= mmpp;
    cr.pt1 = acvVecMult(cr.pt1, mmpp);
    cr.pt2 = acvVecMult(cr.pt2, mmpp);
    cr.pt3 = acvVecMult(cr.pt3, mmpp);
  }




  return cr;
}

FeatureReport_lineReport FeatureManager_sig360_circle_line::LineMatching_ReportGen(
  featureDef_line *plineDef,edgeTracking &eT,
  acv_XY calibCen,float mmpp,float cached_cos,float cached_sin,float flip_f)
{
  featureDef_line lineDef=*plineDef;

  // LOGI("p0:%f %f , p1:%f %f",line.p0.X,line.p0.Y,line.p1.X,line.p1.Y);

  lineDef.p0=cm.convert(lineDef.p0);
  lineDef.p1=cm.convert(lineDef.p1);
  
  lineDef.p0 = TemplateDomain_TO_PixDomain(lineDef.p0, cached_sin, cached_cos, flip_f, calibCen, mmpp);
  lineDef.p1 = TemplateDomain_TO_PixDomain(lineDef.p1, cached_sin, cached_cos, flip_f, calibCen, mmpp);
  LOGI("p0:%f %f, p1:%f,%f",
    lineDef.p0.X,lineDef.p0.Y,
    lineDef.p1.X,lineDef.p1.Y);
      // acv_XY mid=acvVecMult(acvVecAdd(line.p0,line.p1), 0.5);//Just for testing
      // line.p0 = acvVecAdd(acvVecMult(acvVecSub(line.p0,mid), 0.5),mid);
      // line.p1 = acvVecAdd(acvVecMult(acvVecSub(line.p1,mid), 0.5),mid);

  lineDef = lineDefDataPrep(lineDef);
  
  acv_Line line_cand;
  line_cand.line_vec = lineDef.lineTar.line_vec;
  line_cand.line_anchor = lineDef.lineTar.line_anchor;
  lineDef.initMatchingMargin /= mmpp;

  // LOGI("initMatchingMargin:%f MatchingMarginX:%f",initMatchingMargin,MatchingMarginX);
  FeatureReport_lineReport Report = SingleMatching_line(
                      &lineDef, eT, 1/mmpp, 
                      line_cand, edge_grid, flip_f, tmp_points, m_sections);
  

  Report.line.end_pt1 = acvVecMult(Report.line.end_pt1, mmpp);
  Report.line.end_pt2 = acvVecMult(Report.line.end_pt2, mmpp);
  Report.line.line.line_anchor = acvVecMult(Report.line.line.line_anchor, mmpp);
  Report.line.s = Report.line.s * mmpp;
  Report.def=plineDef;
  return Report;
}



int FeatureManager_sig360_circle_line::TreeExecution(
  FeatureReport_sig360_circle_line_single &singleReport,
  edgeTracking &eT,
  acv_XY calibCen,float mmpp,float cached_cos,float cached_sin,float flip_f)
{
  
  for(int i=0;i<singleReport.detectedLines->size();i++)
  {
    TreeExecution((*singleReport.detectedLines)[i].def->id,
      singleReport,eT,
      calibCen, mmpp,cached_cos, cached_sin, flip_f);
  }
  
  for(int i=0;i<singleReport.detectedCircles->size();i++)
  {
    TreeExecution((*singleReport.detectedCircles)[i].def->id,
      singleReport,eT,
      calibCen, mmpp,cached_cos, cached_sin, flip_f);



  }
  
  for(int i=0;i<singleReport.detectedAuxPoints->size();i++)
  {
    TreeExecution((*singleReport.detectedAuxPoints)[i].def->id,
      singleReport,eT,
      calibCen, mmpp,cached_cos, cached_sin, flip_f);
      }
  for(int i=0;i<singleReport.detectedSearchPoints->size();i++)
  {
    TreeExecution((*singleReport.detectedSearchPoints)[i].def->id,
      singleReport,eT,
      calibCen, mmpp,cached_cos, cached_sin, flip_f);}
  
  for(int i=0;i<singleReport.judgeReports->size();i++)
  {
    TreeExecution((*singleReport.judgeReports)[i].def->id,
      singleReport,eT,
      calibCen, mmpp,cached_cos, cached_sin, flip_f);}
  return 0;
}

int FeatureManager_sig360_circle_line::TreeExecution(int id,
  FeatureReport_sig360_circle_line_single &singleReport,
  edgeTracking &eT,
  acv_XY calibCen,float mmpp,float cached_cos,float cached_sin,float flip_f)
{
  bool doPrintDBG=false;
  if(doPrintDBG)LOGI("ID:%d",id);

  
  for(int i=0;i<singleReport.detectedLines->size();i++)
  {
    FeatureReport_lineReport &rep=(*singleReport.detectedLines)[i];
    
    if(rep.def->id!=id )continue;
    if(rep.status!=FeatureReport_sig360_circle_line_single::STATUS_UNSET)
    {
      if(doPrintDBG)LOGI("=>LI:%d:%d",id,rep.status);
      return rep.status;
    }

    rep= LineMatching_ReportGen(
      rep.def,eT,
      calibCen, mmpp, cached_cos, cached_sin, flip_f);

    if(doPrintDBG)LOGI("LI:%d:%d",id,rep.status);
    return rep.status;
    
  }
  for(int i=0;i<singleReport.detectedCircles->size();i++)
  {
    FeatureReport_circleReport &rep=(*singleReport.detectedCircles)[i];

    if(rep.def->id!=id)continue;
    if(rep.status!=FeatureReport_sig360_circle_line_single::STATUS_UNSET)
    {
      if(doPrintDBG)LOGI("=>CI:%d:%d",id,rep.status);
      return rep.status;
    }

    rep=CircleMatching_ReportGen(
        rep.def,
        eT,
        calibCen,mmpp,cached_cos,cached_sin,flip_f);
    if(doPrintDBG)LOGI("CI:%d:%d",id,rep.status);
    return rep.status;
  
  }
  
  for(int i=0;i<singleReport.detectedAuxPoints->size();i++)
  {
    FeatureReport_auxPointReport &rep=(*singleReport.detectedAuxPoints)[i];

    if(rep.def->id!=id)continue;
    if(rep.status!=FeatureReport_sig360_circle_line_single::STATUS_UNSET)
    {
      if(doPrintDBG)LOGI("=>AP:%d:%d",id,rep.status);
      return rep.status;
    }

    switch(rep.def->subtype)
    {
      case featureDef_auxPoint::lineCross:
      {
        int id1_status=
          TreeExecution(rep.def->data.lineCross.line1_id,
            singleReport,eT,
            calibCen, mmpp,cached_cos, cached_sin, flip_f);

        int id2_status=
          TreeExecution(rep.def->data.lineCross.line2_id,
            singleReport,eT,
            calibCen, mmpp,cached_cos, cached_sin, flip_f);
        
        break;
      }
      
      case featureDef_auxPoint::centre:
      {
        int id1_status=
          TreeExecution(rep.def->data.centre.obj1_id,
            singleReport,eT,
            calibCen, mmpp,cached_cos, cached_sin, flip_f);
        break;
      }
    }



    rep = APointMatching_ReportGen(rep.def,singleReport, cached_sin, cached_cos, flip_f);
    if(doPrintDBG)LOGI("AP:%d:%d",id,rep.status);
    return rep.status;
  
  }
  
  for(int i=0;i<singleReport.detectedSearchPoints->size();i++)
  {
    FeatureReport_searchPointReport &rep=(*singleReport.detectedSearchPoints)[i];
    if(rep.def->id!=id)continue;
    if(rep.status!=FeatureReport_sig360_circle_line_single::STATUS_UNSET)
    {
      
      if(doPrintDBG)LOGI("=>SP:%d:%d",id,rep.status);
      return rep.status;
    }


    switch(rep.def->subtype)
    {
      case featureDef_searchPoint::anglefollow :
      {
        int id1_status=
          TreeExecution(rep.def->data.anglefollow.target_id,
            singleReport,eT,
            calibCen, mmpp,cached_cos, cached_sin, flip_f);

        break;
      }
    }


    rep=
      SPointMatching_ReportGen(
        rep.def,
        singleReport,
        eT,
        calibCen,mmpp,cached_cos,cached_sin,flip_f);


    if(doPrintDBG)LOGI("SP:%d:%d",id,rep.status);
    return rep.status;
    
  }
  
  for(int i=0;i<singleReport.judgeReports->size();i++)
  {
    FeatureReport_judgeReport &rep=(*singleReport.judgeReports)[i];
    if(rep.def->id!=id)continue;
    
    if(rep.status!=FeatureReport_sig360_circle_line_single::STATUS_UNSET)
    {
      if(doPrintDBG)LOGI("=>JU:%d:%d",id,rep.status);
      return rep.status;
    }

    if(rep.def->OBJ1_id>=0){
      TreeExecution(rep.def->OBJ1_id,
        singleReport,eT,
        calibCen, mmpp,cached_cos, cached_sin, flip_f);
    }
    if(rep.def->OBJ2_id>=0){
      TreeExecution(rep.def->OBJ2_id,
        singleReport,eT,
        calibCen, mmpp,cached_cos, cached_sin, flip_f);
    }


    if(rep.def->id==id)
    {
      switch(rep.def->measure_type )
      {
        case FeatureReport_judgeDef::ANGLE:
        break;
        case FeatureReport_judgeDef::DISTANCE:
        break;
        case FeatureReport_judgeDef::RADIUS:
        break;
        case FeatureReport_judgeDef::CALC:
          for (int j = 0; j < rep.def->data.CALC.post_exp.size(); j++)
          {
            const string post_exp = rep.def->data.CALC.post_exp[j];
            //LOGI("post_exp[%d]:%s",i,post_exp.c_str());

            int id_exp = parse_CALC_Id(post_exp.c_str());
            if (id_exp >= 0)
            {
              // LOGI("C_EXP[%d] __ID:%d  << %s",j,id_exp,post_exp.c_str());
              TreeExecution(id_exp,
                singleReport,eT,
                calibCen, mmpp,cached_cos, cached_sin, flip_f);
            }
          }
        break;
        case FeatureReport_judgeDef::ROUGHNESS:
          
        break;
        case FeatureReport_judgeDef::CIRCLE_INFO:
          
        break;
    //         NA,
    // AREA,
    // SIGMA,
    // DISTANCE,
    // RADIUS,
    // CALC,
    // CIRCLE_INFO,
    // ROUGHNESS,
      }





      rep= Judge_ReportGen(
        rep.def,
        singleReport, cached_sin, cached_cos, flip_f);
  
      // LOGI("JU:%d:%d",id,rep.status);

      return rep.status;




    }
  }
  return FeatureReport_sig360_circle_line_single::STATUS_UNSET;
}



void RESET_REPORT(FeatureReport_sig360_circle_line_single &srep)
{

    vector<FeatureReport_circleReport> &detectedCircles = *srep.detectedCircles;
    vector<FeatureReport_lineReport> &detectedLines = *srep.detectedLines;

    vector<FeatureReport_auxPointReport> &detectedAuxPoints = *srep.detectedAuxPoints;
    vector<FeatureReport_searchPointReport> &detectedSearchPoints = *srep.detectedSearchPoints;
    vector<FeatureReport_judgeReport> &judgeReports = *srep.judgeReports;
    for (int j = 0; j < detectedLines.size(); j++)
    {
      detectedLines[j].status=FeatureReport_sig360_circle_line_single::STATUS_UNSET;
    }
    for (int j = 0; j < detectedCircles.size(); j++)
    {
      detectedCircles[j].status=FeatureReport_sig360_circle_line_single::STATUS_UNSET;
    }
    for (int j = 0; j < judgeReports.size(); j++)
    {
      judgeReports[j].status=FeatureReport_sig360_circle_line_single::STATUS_UNSET;
    }
    for (int j = 0; j < detectedAuxPoints.size(); j++)
    {
      detectedAuxPoints[j].status=FeatureReport_sig360_circle_line_single::STATUS_UNSET;
    }
    for (int j = 0; j < detectedSearchPoints.size(); j++)
    {
      detectedSearchPoints[j].status=FeatureReport_sig360_circle_line_single::STATUS_UNSET;
    }
}

void SET_UNSET_REPORT_NA(FeatureReport_sig360_circle_line_single &srep)
{

    vector<FeatureReport_circleReport> &detectedCircles = *srep.detectedCircles;
    vector<FeatureReport_lineReport> &detectedLines = *srep.detectedLines;

    vector<FeatureReport_auxPointReport> &detectedAuxPoints = *srep.detectedAuxPoints;
    vector<FeatureReport_searchPointReport> &detectedSearchPoints = *srep.detectedSearchPoints;
    vector<FeatureReport_judgeReport> &judgeReports = *srep.judgeReports;
    for (int j = 0; j < detectedLines.size(); j++)
    {
      if(detectedLines[j].status==FeatureReport_sig360_circle_line_single::STATUS_UNSET)
        detectedLines[j].status=FeatureReport_sig360_circle_line_single::STATUS_NA;
    }
    for (int j = 0; j < detectedCircles.size(); j++)
    {
      if(detectedCircles[j].status==FeatureReport_sig360_circle_line_single::STATUS_UNSET)
        detectedCircles[j].status=FeatureReport_sig360_circle_line_single::STATUS_NA;
    }
    for (int j = 0; j < judgeReports.size(); j++)
    {
      if(judgeReports[j].status==FeatureReport_sig360_circle_line_single::STATUS_UNSET)
        judgeReports[j].status=FeatureReport_sig360_circle_line_single::STATUS_NA;
    }
    for (int j = 0; j < detectedAuxPoints.size(); j++)
    {
      if(detectedAuxPoints[j].status==FeatureReport_sig360_circle_line_single::STATUS_UNSET)
        detectedAuxPoints[j].status=FeatureReport_sig360_circle_line_single::STATUS_NA;
    }
    for (int j = 0; j < detectedSearchPoints.size(); j++)
    {
      if(detectedSearchPoints[j].status==FeatureReport_sig360_circle_line_single::STATUS_UNSET)
        detectedSearchPoints[j].status=FeatureReport_sig360_circle_line_single::STATUS_NA;
    }
}

int FeatureManager_sig360_circle_line::SingleMatching(acvImage *searchDistoriginalImage,
                                                      acvImage *labeledBuff, acvImage *binarizedBuff, acvImage *buffer_img,
                                                      int lableIdx, acv_LabeledData *ldData,
                                                      int grid_size, ContourFetch &edge_grid, int scanline_skip, FeatureManager_BacPac *bacpac,
                                                      FeatureReport_sig360_circle_line_single &singleReport,
                                                      vector<ContourFetch::ptInfo> &tmp_points, vector<ContourFetch::contourMatchSec> &m_sections)
{
  drawDraw = false;

  vector<FeatureReport_circleReport> &detectedCircles = *singleReport.detectedCircles;
  vector<FeatureReport_lineReport> &detectedLines = *singleReport.detectedLines;

  vector<FeatureReport_auxPointReport> &detectedAuxPoints = *singleReport.detectedAuxPoints;
  vector<FeatureReport_searchPointReport> &detectedSearchPoints = *singleReport.detectedSearchPoints;
  vector<FeatureReport_judgeReport> &judgeReports = *singleReport.judgeReports;

  edgeTracking eT(p_cropImg,cropOffset, bacpac);
  // acvDrawCrossX(originalImage,
  //   calibCen.X, calibCen.Y,
  //   3, 3);

  bool drawDBG_IMG = false;
  float thres = 80; //OTSU_Threshold(*smoothedImg, &ldData[i], 3);
  //LOGV("OTSU_Threshold:%f", thres);
  float minErr=__FLT_MAX__;
  float ignoreErr = 100000;
  bool isInv = false;
  float angle = NAN;
  vector<acv_XY> minMatchErr;
  vector<acv_XY> minMatchErr_bk;
  {

    int scanWidth = 10;
    int sparseScanLen = 360 / scanWidth;

    if (matching_face >= 0) //front
      xrefine(feature_signature, tmp_signature, 36 / 3, minMatchErr, false, 10, 5, 0.5);

    if (matching_face <= 0) //back
      xrefine(feature_signature, tmp_signature, 36 / 3, minMatchErr_bk, true, 10, 5, 0.5);

    float matching_angle_margin = this->matching_angle_margin;
    // if(matching_angle_margin>M_PI-0.0001)
    // {
    //   matching_angle_margin=M_PI-0.0001;
    // }
    // minMatchErr[0].Y=0.01;
    for (int i = 0; i < minMatchErr.size(); i++)
    {
      float preErr = minMatchErr[i].Y;
      if (matching_angle_margin < M_PI - 0.001 && isAngleInRegion(minMatchErr[i].X * M_PI / 180, this->matching_angle_offset - matching_angle_margin, this->matching_angle_offset + matching_angle_margin))
      {
        minMatchErr[i].Y = ignoreErr;
      }
      else
      {
        if(minErr>preErr)
        {
          minErr=preErr;
        }
      }
      LOGI("F %f: %f>%f", minMatchErr[i].X, preErr, minMatchErr[i].Y);

      // minMatchErr[i].Y+=0.005723;
    }

    for (int i = 0; i < minMatchErr_bk.size(); i++)
    {
      float preErr = minMatchErr_bk[i].Y;
      float angle = minMatchErr_bk[i].X * M_PI / 180 + M_PI; //the flip is along X axis, but in practice the flip should(in practice sense) be along Y axis
      if (matching_angle_margin < M_PI - 0.001 && isAngleInRegion(angle, this->matching_angle_offset - matching_angle_margin, this->matching_angle_offset + matching_angle_margin))
      {
        minMatchErr_bk[i].Y = ignoreErr;
      }
      else
      {
        if(minErr>preErr)
        {
          minErr=preErr;
        }
      }
      LOGI("B %f: %f>%f", minMatchErr_bk[i].X, preErr, minMatchErr_bk[i].Y);
    }
  }


  if(isErrPass(sqrt(minErr),feature_signature)==false)//if the minimum err still not pass, then, early stop the following procesure
  {
    return -30;
  }

  contourGridGrayLevelRefine(p_cropImg, edge_grid, bacpac);

  // for(int i=0;i<edge_grid.contourSections.size();i++)
  // {
  //   for(int j=0;j<edge_grid.contourSections[i].size();j++)
  //   {
  //     int X=edge_grid.contourSections[i][j].pt.X;
  //     int Y=edge_grid.contourSections[i][j].pt.Y;
  //     originalImage->CVector[Y][X*3]=255;
      
  //     // X=edge_grid.contourSections[i][j].pt_img.X;
  //     // Y=edge_grid.contourSections[i][j].pt_img.Y;
      
  //     // originalImage->CVector[Y][X*3]=0;
  //   }
  // }
    
  {//pre-allocate
    detectedCircles.resize(featureCircleList.size());
    detectedLines.resize(featureLineList.size());
    detectedAuxPoints.resize(auxPointList.size());
    detectedSearchPoints.resize(searchPointList.size());
    judgeReports.resize(judgeList.size());

    //assign def
    for (int j = 0; j < featureLineList.size(); j++)
    {
      detectedLines[j].def=&(featureLineList[j]);
    }
    for (int j = 0; j < featureCircleList.size(); j++)
    {
      detectedCircles[j].def=&(featureCircleList[j]);
    }
    for (int j = 0; j < judgeList.size(); j++)
    {
      judgeReports[j].def=&(judgeList[j]);
    }
    for (int j = 0; j < auxPointList.size(); j++)
    {
      detectedAuxPoints[j].def=&(auxPointList[j]);
    }
    for (int j = 0; j < searchPointList.size(); j++)
    {
      detectedSearchPoints[j].def=&(searchPointList[j]);
    }
    RESET_REPORT(singleReport);//Set to Unset state
  }

  cm.center = (acv_XY){0, 0};

  float error = NAN;

  static float angle_offset = 0;
  if (0) //test angle variation
  {
    float sigma;
    // angle += angle_offset * M_PI / 180;
    if (angle_offset >  5*M_PI/180)
    {
      angle_offset =  -5*M_PI/180;
    }
    else
    {
      angle_offset += 0.1*M_PI/180;
    }
  }


  int defaultRetryCountDown = 5;
  int retryCountDown = defaultRetryCountDown; //max retry
  while (true)
  {

    if (retryCountDown == 0)
    {
      LOGI("Retry count down(%d times) reached... giveup", defaultRetryCountDown);
      //No min orientation was found
      return -1;
    }

    {
      int targetIdx = -1;
      acv_XY minErr = {NAN, NAN};
      minErr.Y = ignoreErr;

      for (int i = 0; i < minMatchErr.size(); i++)
      {
        if (minErr.Y > minMatchErr[i].Y)
        {
          minErr = minMatchErr[i];
          isInv = false;
          targetIdx = i;
        }
      }

      for (int i = 0; i < minMatchErr_bk.size(); i++)
      {
        if (minErr.Y > minMatchErr_bk[i].Y)
        {
          minErr = minMatchErr_bk[i];
          isInv = true;
          targetIdx = i;
        }
      }
      if (targetIdx == -1)
      {
        LOGI("NO good angle was found!!");
        //No min orientation was found
        return -1;
      }
      if (isInv == false) //mark deletion
      {
        minMatchErr[targetIdx].Y = ignoreErr;
      }
      else
      {
        minMatchErr_bk[targetIdx].Y = ignoreErr;
      }

      angle = minErr.X * M_PI / 180;
      error = minErr.Y;
    }
    retryCountDown--;

    error = sqrt(error);
    //if(i<10)

    {
      LOGI("======%d===XY:%0.4f,%0.4f AREA:%d er:%f,inv:%d,angDeg:%f ",
           lableIdx, ldData[lableIdx].Center.X, ldData[lableIdx].Center.Y, ldData[lableIdx].area, error, isInv, angle * 180 / 3.14159);
    }

    if(isErrPass(error,feature_signature)==false)
    {
      LOGE("error:%f", error);
      return -3;
    }

    angle+=angle_offset;


    singleReport.rotate = angle;
    singleReport.isFlipped = isInv;

    //The angle we get from matching is current object rotates 'angle' to match target
    //But now, we want to rotate feature set to match current object, so opposite direction
    angle = -angle;
    float flip_f = 1;
    if (isInv)
    {
      flip_f = -1;
    }
    //Note, the flip_f here means to flip Y first then do rotation

    float mmpp = bacpac->sampler->mmpP_ideal(); //mm per pixel
    float ppmm = 1 / mmpp;                      //pixel per mm
    acv_XY calibCen = singleReport.Center;
    calibCen.X *= ppmm;
    calibCen.Y *= ppmm;
    float cached_cos = cos(angle);
    float cached_sin = sin(angle);

    LOGI("calibCen: %f %f", calibCen.X, calibCen.Y);


    
    for (int j = 0; j < detectedSearchPoints.size(); j++) //reset the ConstrainMap
    {
      cm.anchorPairs[j].to = (acv_XY){NAN, NAN};
    }
    for(int k=0;k<1;k++)//perform locating_anchor locating and adjust
    {

      RESET_REPORT(singleReport);//Set to Unset state
      for (int j = 0; j < searchPointList.size(); j++) 
      {
        // cm.anchorPairs[j].to = (acv_XY){NAN, NAN};
        if (searchPointList[j].data.anglefollow.locating_anchor == false)
        {
          continue;
        }
        LOGI("ID:%d", searchPointList[j].id);
        
        TreeExecution(searchPointList[j].id,singleReport,eT,
          calibCen, mmpp, cached_cos, cached_sin, flip_f);//find all necessary anchor first
      }


      for (int j = 0; j < detectedSearchPoints.size(); j++) //Fill the ConstrainMap
      {
        cm.anchorPairs[j].to = (acv_XY){NAN, NAN};
        if (detectedSearchPoints[j].def->data.anglefollow.locating_anchor == false)
        {
          continue;
        }

        // LOGI("ID:%d=>%d",detectedSearchPoints[j].def->id, detectedSearchPoints[j].status);
        if(detectedSearchPoints[j].status!= FeatureReport_sig360_circle_line_single::STATUS_SUCCESS)
        {
          continue;
        }

        acv_XY locatedPtOnTemplate = Image_mm_Domain_TO_TemplateDomain(
          detectedSearchPoints[j].pt, 
          cached_sin, 
          cached_cos, 
          flip_f, 
          calibCen, 
          mmpp);
        cm.anchorPairs[j].to = locatedPtOnTemplate;

        // acv_XY pos = ;
        // LOGI("XY:%f %f -> %f %f", cm.anchorPairs[j].from.X, cm.anchorPairs[j].from.Y, locatedPtOnTemplate.X, locatedPtOnTemplate.Y);

      }

    }

    RESET_REPORT(singleReport);//Set to Unset state, and perform official measurment with locating anchor calibration 

    bool reDo_orien=false;//check if retry the orientation needed,
    for (int j = 0; j < judgeReports.size(); j++)
    {
      FeatureReport_judgeDef &def = *judgeReports[j].def;
      if (def.orientation_essential != true)
        continue;
      int status =TreeExecution(def.id,singleReport,eT,
        calibCen, mmpp, cached_cos, cached_sin, flip_f);
      if(status!=FeatureReport_sig360_circle_line_single::STATUS_SUCCESS)
      {
        reDo_orien=true;
        break;
      }
    }



    if(reDo_orien)
    {
      LOGI(">>>>>REDO  REDO>>>>");
      LOGI(">>>>>REDO  REDO>>>>");
      LOGI(">>>>>REDO  REDO>>>>");
      LOGI(">>>>>REDO  REDO>>>>");
      LOGI(">>>>>REDO  REDO>>>>");
      continue;
    }
    //if the orientation is correct, do the rest of judge 


    bool ignore_unnecessary_shape=false;
    bool ignore_unnecessary_inspection=false;
    if(ignore_unnecessary_shape==false)
    {
      int status =TreeExecution(singleReport,eT,
        calibCen, mmpp, cached_cos, cached_sin, flip_f);
    }
    else
    {
      for (int j = 0; j < judgeReports.size(); j++)
      {
        FeatureReport_judgeDef &def = *judgeReports[j].def;
        if(ignore_unnecessary_inspection==true && def.quality_essential==false)
        {//not unnecessary
          continue;
        }
        int status =TreeExecution(def.id,singleReport,eT,
          calibCen, mmpp, cached_cos, cached_sin, flip_f);
      }
    }

    SET_UNSET_REPORT_NA(singleReport);

    break;
  }

  // acv_XY ttt = acvRotation(cached_sin, cached_cos, flip_f, (acv_XY){100,0});

  // acvDrawLine(originalImage,
  //             calibCen.X,
  //             calibCen.Y,
  //             calibCen.X + ttt.X,
  //             calibCen.Y + ttt.Y,
  //             20, 255, 128);
  // acvCloneImage(originalImage,originalImage,1);
  return 0;
}

bool convertContourGrid2Signature(acv_XY center, ContourFetch contour, std::vector<acv_XY> &o_signature, FeatureManager_BacPac *bacpac)
{
  if (contour.contourSections.size() == 0)
    return false;

  int preIdx = -1;
  int _1stIdx = -1;
  for (ContourFetch::ptInfo ptinfo : contour.contourSections[0])
  {
    acv_XY copos = ptinfo.pt;

    bacpac->sampler->img2ideal(&copos);
    float diffY = copos.Y - center.Y;
    float diffX = copos.X - center.X;
    if (diffX != diffX || diffY != diffY)
    {
      continue;
    }
    float theta = acvFAtan2(diffY, diffX); //-pi ~pi
    //if(theta<0)theta+=2*M_PI;
    int idx = round(o_signature.size() * theta / (2 * M_PI));
    if (idx < 0)
      idx += o_signature.size();
    //if(idx>=signature.size())idx-=signature.size();
    if (preIdx == -1)
    {
      _1stIdx = idx;
      preIdx = idx;
    }
    float R = hypot(diffX, diffY);
    if (o_signature[idx].X < R)
    {
      o_signature[idx].X = R;
      o_signature[idx].Y = theta;
      interpolateSignData(o_signature, preIdx, idx);
    }
    preIdx = idx;
  }

  return true;
}

FeatureManager_sig360_circle_line::~FeatureManager_sig360_circle_line()
{

  for (int i = 0; i < reportDataPool.size(); i++)
  {
    delete reportDataPool[i].detectedCircles;
    delete reportDataPool[i].detectedLines;
    delete reportDataPool[i].detectedAuxPoints;
    delete reportDataPool[i].detectedSearchPoints;
    delete reportDataPool[i].judgeReports;
  }
  reportDataPool.resize(0);
  reports.resize(0);
}

int FeatureManager_sig360_circle_line::FeatureMatching(acvImage *img)
{
  report.bacpac = bacpac;
  float ppmm = 1 / bacpac->sampler->mmpP_ideal();
  ; //pixel per mm

  acvImage *buff_ = &_buff;
  vector<acv_LabeledData> &ldData = *this->_ldData;
  int grid_size = 50;
  bool drawDBG_IMG = false;
  buff_->ReSize(img->GetWidth(), img->GetHeight());
  buff1.ReSize(img->GetWidth(), img->GetHeight());
  buff2.ReSize(img->GetWidth(), img->GetHeight());
  acvImage *labeledBuff = img;
  //acvImage *smoothedImg = &buff2;
  //acvCloneImage( img,buff_, -1);
  //acvCloneImage(img, labeledBuff, -1);
  //acvCloneImage(originalImage, smoothedImg, -1);
  //acvBoxFilter(buff_,smoothedImg,7);
  //acvBoxFilter(buff_,smoothedImg,7);
  //acvBoxFilter(buff_,smoothedImg,7);
  //acvBoxFilter(buff_,smoothedImg,2);
  //acvBoxFilter(buff_,smoothedImg,2);

  tmp_signature.RESET(feature_signature.signature_data.size());

  reports.resize(0);

  int onlyIdx = -1;
  if (single_result_area_ratio > 0 && ldData.size() >= 3)
  {
    onlyIdx = 0;
    int totalArea = ldData[1].area;

    LOGI("AREA 0:%d    1:%d", ldData[0].area, ldData[1].area);
    int maxArea = 0;
    for (int i = 2; i < ldData.size(); i++)
    {

      LOGI("AREA[%d]:%d", i, ldData[i].area);
      totalArea += ldData[i].area;
      if (maxArea < ldData[i].area)
      {
        onlyIdx = i;
        maxArea = ldData[i].area;
      }
    }

    float ratio = (float)maxArea / totalArea;

    LOGI("RATIO:maxArea:%d totalArea:%d  ratio:%f", maxArea, totalArea, ratio);
    if (ratio < single_result_area_ratio || onlyIdx == -1)
    {

      return -1;
    }
  }
  
  int avaliable_ld_size = 0;
  int maxArea = 0;
  {
    for (int i = 2; i < ldData.size(); i++)
    {
      if (maxArea < ldData[i].area)
      {
        maxArea = ldData[i].area;
      }
    }

    for (int i = 2; i < ldData.size(); i++)
    {
      if (ldData[i].area < maxArea * 7 / 10)
      {
        // ldData[i].area=0;
        ldData[i].misc = -1; //mark ignore
      }
      else
      {
        ldData[i].misc = 1;
        avaliable_ld_size++;
      }
    }
  }
  
  int scanline_skip = 15;

  float sigma;

  {
    if (reportDataPool.size() < avaliable_ld_size) //only expand
    {
      int oriSize = reportDataPool.size();
      reportDataPool.resize(avaliable_ld_size);

      for (int i = oriSize; i < reportDataPool.size(); i++)
      {
        reportDataPool[i].detectedCircles = new vector<FeatureReport_circleReport>(0);
        reportDataPool[i].detectedLines = new vector<FeatureReport_lineReport>(0);
        reportDataPool[i].detectedAuxPoints = new vector<FeatureReport_auxPointReport>(0);
        reportDataPool[i].detectedSearchPoints = new vector<FeatureReport_searchPointReport>(0);
        reportDataPool[i].judgeReports = new vector<FeatureReport_judgeReport>(0);
      }
    }
  }

  LOGV("ldData.size()=%d", ldData.size());
  for (int i = 2; i < ldData.size(); i++)
  {                           // idx 0 is not a label, idx 1 is for outer frame and connected objects
    if (ldData[i].area < 100*dsampLevel*dsampLevel) //HACK: no particular reason, just a hack filter
      continue;
    if (ldData[i].misc == -1) //the ignore mark
      continue;
    if (onlyIdx > 0 && i != onlyIdx)
    {
      continue;
    }

    bool doCropStyle=false;
    int cx=0;
    int cy=0;
    int cw=labeledBuff->GetWidth();
    int ch=labeledBuff->GetHeight();
    if(doCropStyle)
    {
      p_cropImg=&_cropImg;
      int margin=20;
      cx=ldData[i].LTBound.X-margin;
      cy=ldData[i].LTBound.Y-margin;
      cw=ldData[i].RBBound.X+margin-cx;
      ch=ldData[i].RBBound.Y+margin-cy;
      LOGI("CROP:%d  %d  %d  %d",cx,cy,cw,ch);
      acvCropImage(labeledBuff,p_cropImg,cx,cy,cw,ch);
    }
    else
    {
      p_cropImg=labeledBuff;
    }
    cropOffset.X=cx;
    cropOffset.Y=cy;
    acv_LabeledData curLableDat=(acv_LabeledData){
      .area= ldData[i].area,
      .Center=acvVecSub(ldData[i].Center,cropOffset),
      .LTBound=acvVecSub(ldData[i].LTBound,cropOffset),
      .RBBound=acvVecSub(ldData[i].RBBound,cropOffset),
    };
    edge_grid.RESET();

    extractLabeledContourDataToContourGrid(p_cropImg, i, curLableDat, edge_grid, scanline_skip);
    // edge_grid.ptMult(dsampLevel/2);
    

     if(doCropStyle)
    {

      acvCropImage(originalImage,p_cropImg,cx,cy,cw,ch);
    }
    else
    {
      p_cropImg=originalImage;
    }
    // acvSaveBitmapFile("CROPIMG.BMP", &p_cropImg);


    
    if (0)
    {
      static float rot = 0;
      rot += 10 * 3.14 / 180;
      float mag = 3;
      ldData[i].Center = acvVecAdd(ldData[i].Center, (acv_XY){mag * sin(rot), mag * cos(rot)});
    }

    acv_XY ideal_center = acvVecMult(curLableDat.Center,dsampLevel);

    bacpac->sampler->img2ideal(&ideal_center);

    ideal_center = acvVecMult(ideal_center,1.0/dsampLevel);
    LOGI(">>>>>%f  %f",ideal_center.X,ideal_center.Y);
    convertContourGrid2Signature(ideal_center, edge_grid, tmp_signature.signature_data, bacpac);

    //the tmp_signature is in Pixel unit, convert it to mm
    for (int j = 0; j < tmp_signature.signature_data.size(); j++)
    {
      tmp_signature.signature_data[j].X *=dsampLevel/ ppmm;
      
      // LOGI("%d=>%f",j,tmp_signature.signature_data[j].X);
    }
    
    // acvSaveBitmapFile("FFKFKJDK3.BMP", labeledBuff);
    // exit(-1);
    tmp_signature.CalcInfo();

    { //early intercept just to check mean and sigma
      float meanRatio = tmp_signature.mean / feature_signature.mean;
      if (meanRatio > 1)
        meanRatio = 1 / meanRatio;
      float sigmaRatio = tmp_signature.sigma / feature_signature.sigma;
      if (sigmaRatio > 1)
        sigmaRatio = 1 / sigmaRatio;

      float stage1Sim = meanRatio * sigmaRatio;
      LOGI("mR:%f  sR:%f stage1Sim:%f", meanRatio, sigmaRatio, stage1Sim);
      if (meanRatio < 0.8 || stage1Sim < 0.5)
        continue;
    }

    sign360_process(tmp_signature.signature_data, signature_data_buffer);

    // LOGI(">>feature_signature");
    // for(int i=0;i<feature_signature.size();i++)
    // {
    //   acv_XY xy=feature_signature[i];

    //   printf("%4.1f ",hypot(xy. X,xy.Y));
    // }
    // LOGI(">>=================");
    // for(int i=0;i<tmp_signature.size();i++)
    // {
    //   acv_XY xy=tmp_signature[i];

    //   printf("%4.1f ",hypot(xy.X,xy.Y));
    // }
    // LOGI(">>=================");

    float mmpp = bacpac->sampler->mmpP_ideal(); //mm per pixel
    FeatureReport_sig360_circle_line_single singleReport =
        {
            .detectedCircles = reportDataPool[reports.size()].detectedCircles,
            .detectedLines = reportDataPool[reports.size()].detectedLines,
            .detectedAuxPoints = reportDataPool[reports.size()].detectedAuxPoints,
            .detectedSearchPoints = reportDataPool[reports.size()].detectedSearchPoints,
            .judgeReports = reportDataPool[reports.size()].judgeReports,
            .LTBound = acvVecMult(curLableDat.LTBound,dsampLevel),
            .RBBound = acvVecMult(curLableDat.RBBound,dsampLevel),
            .Center = acvVecMult(curLableDat.Center,dsampLevel),
            .area = (float)curLableDat.area*dsampLevel*dsampLevel,
            .pix_area = curLableDat.area*dsampLevel*dsampLevel,
            .labeling_idx = i,
            // .rotate = angle,
            // .isFlipped = isInv,
            .scale = 1,
            .targetName = NULL};
    //singleReport.Center=acvVecRadialDistortionRemove(singleReport.Center,param);

    bacpac->sampler->img2ideal(&singleReport.Center);
    singleReport.Center = acvVecMult(singleReport.Center, mmpp);
    bacpac->sampler->img2ideal(&singleReport.LTBound);
    singleReport.LTBound = acvVecMult(singleReport.LTBound, mmpp);
    bacpac->sampler->img2ideal(&singleReport.RBBound);
    singleReport.RBBound = acvVecMult(singleReport.RBBound, mmpp);
    singleReport.area *= mmpp * mmpp;
    edge_grid.ptMult(dsampLevel);
    
    edge_grid.ptSubdivision(dsampLevel);
    //LOGV("======%d===er:%f,inv:%d,angDeg:%f",i,error,isInv,angle*180/3.14159);

    int ret = SingleMatching(originalImage, labeledBuff, img, buff_,
                             i, &(ldData[0]),
                             grid_size, edge_grid, scanline_skip, bacpac,
                             singleReport,
                             tmp_points, m_sections);
    if (ret == 0)
    {
      reports.push_back(singleReport);
    }
  }

  { //convert pixel unit to mm
    float mmpp = bacpac->sampler->mmpP_ideal();
  }

  //LOGI(">>>>>>>>");
  return 0;
}

int ContourSignature::RESET(int Len)
{
  mean = NAN;
  sigma = NAN;
  signature_data.resize(Len);
  return 0;
}

int ContourSignature::CalcInfo()
{
  mean = NAN;
  sigma = NAN;
  if (signature_data.size() == 0)
    return -1;
  float _mean = 0;
  float _sigma = 0;

  float _Angleoffset = 0;
  for (int i = 0; i < signature_data.size(); i++)
  {
    float x = signature_data[i].X;
    _mean += x;
    _sigma += x * x;

    float angleCenter = i * 2 * M_PI / signature_data.size();
    float angleDiff = signature_data[i].Y - angleCenter;

    if (angleDiff > M_PI)
      angleDiff -= 2 * M_PI;
    else if (angleDiff < -M_PI)
      angleDiff += 2 * M_PI;
    // LOGI("center:%f,angleDiff:%f",angleCenter,angleDiff);
    _Angleoffset += angleDiff;
  }

  _Angleoffset /= signature_data.size();
  _mean /= signature_data.size();
  _sigma /= signature_data.size();
  _sigma = sqrt(_sigma - _mean * _mean);
  angleOffset = _Angleoffset;
  mean = _mean;
  sigma = _sigma;
  return 0;
}

int ContourSignature::RELOAD(cJSON *sig_json)
{
  RESET(0);
  cJSON *signature_magnitude;
  cJSON *signature_angle;
  if (!(getDataFromJsonObj(sig_json, "magnitude", (void **)&signature_magnitude) & cJSON_Array))
  {
    LOGE("The signature_magnitude is not an cJSON_Array");
    return -1;
  }

  if (!(getDataFromJsonObj(sig_json, "angle", (void **)&signature_angle) & cJSON_Array))
  {
    LOGE("The signature_angle is not an cJSON_Array");
    return -1;
  }

  if (cJSON_GetArraySize(signature_magnitude) != cJSON_GetArraySize(signature_angle))
  {
    LOGE("The signature_angle and signature_magnitude doesn't have same length");
    return -1;
  }

  for (int i = 0; i < cJSON_GetArraySize(signature_magnitude); i++)
  {
    double *pnum_mag;
    if (!(getDataFromJsonObj(signature_magnitude, i, (void **)&pnum_mag) & cJSON_Number))
    {
      return -1;
    }

    double *pnum_ang;
    if (!(getDataFromJsonObj(signature_angle, i, (void **)&pnum_ang) & cJSON_Number))
    {
      return -1;
    }
    acv_XY dat = {.X = (float)*pnum_mag, .Y = (float)*pnum_ang};

    signature_data.push_back(dat);
  }

  CalcInfo();
  return 0;
}

ContourSignature::ContourSignature(cJSON *jsonObj)
{
  int ret_err = RELOAD(jsonObj);
  if (ret_err)
    throw std::invalid_argument("The json signature data isn't correct {magnitude:[N numbers],angle:[N numbers]}");
}

ContourSignature::ContourSignature(int len)
{
  RESET(len);
}

float ContourSignature::match_min_error(ContourSignature &s,
                                        float searchAngleOffset, float searchAngleRange, int facing,
                                        bool *ret_isInv, float *ret_angle)
{
  return SignatureMinMatching(s.signature_data, signature_data,
                              searchAngleOffset, searchAngleRange, facing,
                              ret_isInv, ret_angle);
}

void ContourSignature::match_span(ContourSignature &s,
                                  float offset1, float offset2, int count, vector<acv_XY> &error, float stride, bool flip)
{
  error.resize(0);
  for (int i = 0; i < count; i++)
  {
    float offset = offset1 + (offset2 - offset1) * i / (count);
    float err = SignatureMatchingError(&(signature_data[0]), offset, &(s.signature_data[0]), signature_data.size(), stride, flip);
    error.push_back((acv_XY){X : offset, Y : err});
  }

  return;
}
