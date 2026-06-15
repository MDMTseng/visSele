#include "FeatureManager_sig360_circle_line.h"
#include "Caliper.h"
#include "JudgeCALC.h"
#include "LabelingCV.h"

#include "SearchPointCV.h"
#include "logctrl.h"
LOG_MODULE("match.sig360");
#include <stdexcept>
#include <common_lib.h>
#include <algorithm>
#include <limits>
#include <chrono>
#include <MatchingCore.h>
#include <stdio.h>
#include "CvBridge.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include "polyfit.h"
#include "shape_matcher.h"   // sbm::ShapeMatcher / extractFeatures (line2Dup + ROI refine)


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
    to[i].x = (from[i - 1].x + from[i].x + from[i + 1].x) / 3;
    to[i].y = (from[i - 1].y + from[i].y + from[i + 1].y) / 3;
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
    //   LOGI("pt:%f,%f  dir:%f,%f",ptInfo[i].pt_img.x,ptInfo[i].pt_img.y,ptInfo[i].contourDir.x,ptInfo[i].contourDir.y);
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

// Bounded copy into a fixed-size featureDef/report name[FeatureManager_NAME_LENGTH]
// buffer. Source is def-JSON controlled and otherwise unbounded; snprintf truncates
// and always null-terminates, preventing buffer overflow on overlong names.
static void copyFeatureName(char *dst, const char *src)
{
  snprintf(dst, FeatureManager_NAME_LENGTH, "%s", src);
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
  retD.circleTar.radius = hypot(circumcenter.x - pt2.x, circumcenter.y - pt2.y);

  pt1.x -= circumcenter.x;
  pt1.y -= circumcenter.y;
  pt2.x -= circumcenter.x;
  pt2.y -= circumcenter.y;
  pt3.x -= circumcenter.x;
  pt3.y -= circumcenter.y;
  float angle1 = atan2(pt1.y, pt1.x);
  float angle2 = atan2(pt2.y, pt2.x);
  float angle3 = atan2(pt3.y, pt3.x);

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
      copyFeatureName(cir.name, tmpstr);
    }
  }

  double *pnum;

  if ((pnum = JSON_GET_NUM(circle_obj, "id")) == NULL) return -1;
  cir.id = (int)*pnum;
  LOGV("feature is an arc:%s %d", cir.name, cir.id);

  if ((pnum = JSON_GET_NUM(circle_obj, "margin")) == NULL) return -1;
  cir.initMatchingMargin = *pnum;

  // caliper/section locating (default contour). cal_width/length/step are in
  // DEF UNITS (mm); the per-primitive *_ReportGen function converts to px
  // (* ppmm) before constructing CaliperParams. Sentinels (-1) pass through
  // and trigger downstream fallbacks ("use initMatchingMargin" / "1px step").
  cir.locating = 0; cir.cal_count = 10; cir.cal_width = 0.5f; cir.cal_length = -1; cir.cal_step = -1;
  cir.cal_min_inliers = 0; cir.cal_max_error = 0;
  cir.fit_mode = 0;  // 0=ls, 1=outer, 2=inner (LS-center envelope variants)
  // default caliper edge: dominant FALLING edge (white->dark) silhouette; explicit overrides.
  cir.edge_method = EdgeSelectParams::STRONGEST; cir.edge_polarity = EdgeSelectParams::FALLING;
  cir.edge_nth = 0; cir.edge_min_strength = 0;
  {
    char *loc = (char *)JFetch(circle_obj, "locating", cJSON_String);
    if (loc && strcmp(loc, "caliper") == 0) cir.locating = 1;
    cJSON *calo = JFetch_OBJECT(circle_obj, "caliper");
    if (calo) { cir.cal_count = (int)JFetch_NUMBER_ex(calo, "count", 10);
                cir.cal_width = JFetch_NUMBER_ex(calo, "width", 0.5);
                cir.cal_length = JFetch_NUMBER_ex(calo, "length", -1);
                cir.cal_step = JFetch_NUMBER_ex(calo, "step", -1);
                cir.cal_min_inliers = (int)JFetch_NUMBER_ex(calo, "min_inliers", 0);
                cir.cal_max_error = JFetch_NUMBER_ex(calo, "max_error", 0);
                // Clamp against pathological caliper sizes that would DoS the
                // measurement loop (per-primitive cost ~ count * (2*width+1) *
                // length). Real-world calipers are tens; even at these caps
                // worst-case per primitive is ~10^8 ops -> completes in ~1s.
                // cal_length = -1 is the "use full" sentinel and passes through.
                if (cir.cal_count  >  512) cir.cal_count  =  512;
                if (cir.cal_width  >   64) cir.cal_width  =   64;
                if (cir.cal_length >  256) cir.cal_length =  256; }
    cJSON *edgeo = JFetch_OBJECT(circle_obj, "edge");
    if (edgeo) {
      cir.edge_method   = edge_method_from_string((char *)JFetch(edgeo, "method", cJSON_String));
      cir.edge_polarity = edge_polarity_from_string((char *)JFetch(edgeo, "polarity", cJSON_String));
      cir.edge_nth = (int)JFetch_NUMBER_ex(edgeo, "nth", 0);
      cir.edge_min_strength = JFetch_NUMBER_ex(edgeo, "min_strength", 0);
    }
    // Envelope fit mode — top-level on the arc def. "ls" (default) | "outer" | "inner".
    char *fm = (char *)JFetch(circle_obj, "fit_mode", cJSON_String);
    if (fm) {
      if      (strcmp(fm, "outer") == 0) cir.fit_mode = 1;
      else if (strcmp(fm, "inner") == 0) cir.fit_mode = 2;
      else                               cir.fit_mode = 0;
    }
  }

  acv_XY pt1, pt2, pt3;

  if ((pnum = JSON_GET_NUM(circle_obj, "pt1.x")) == NULL) return -1; pt1.x = *pnum;
  if ((pnum = JSON_GET_NUM(circle_obj, "pt1.y")) == NULL) return -1; pt1.y = *pnum;

  if ((pnum = JSON_GET_NUM(circle_obj, "pt2.x")) == NULL) return -1; pt2.x = *pnum;
  if ((pnum = JSON_GET_NUM(circle_obj, "pt2.y")) == NULL) return -1; pt2.y = *pnum;

  if ((pnum = JSON_GET_NUM(circle_obj, "pt3.x")) == NULL) return -1; pt3.x = *pnum;
  if ((pnum = JSON_GET_NUM(circle_obj, "pt3.y")) == NULL) return -1; pt3.y = *pnum;
  if ((pnum = JSON_GET_NUM(circle_obj, "direction")) == NULL) return -1;
  double direction = *pnum;

  cir.pt1 = pt1;
  cir.pt2 = pt2;
  cir.pt3 = pt3;

  cir.outter_inner = direction;

  LOGV("x:%f y:%f r:%f margin:%f",
       cir.circleTar.circumcenter.x,
       cir.circleTar.circumcenter.y,
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
    return acv_XY(NAN, NAN);
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
    return acv_XY(NAN, NAN);
  }

  acv_XY line_vec = acvVecSub(defLine->p1, defLine->p0);

  float angle = def_sp->data.anglefollow.angleDeg * M_PI / 180;

  LOGV("line_p1:%f %f  p0:%f %f", defLine->p1.x, defLine->p1.y, defLine->p0.x, defLine->p0.y);
  LOGV("line_vec:%f %f  angle:%f", line_vec.x, line_vec.y, angle);
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
      float sAngle = atan2(vec1.y, vec1.x);
      float eAngle = atan2(vec2.y, vec2.x);

      float midwayAngle = atan2(vec_mid.y, vec_mid.x);
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

      // LOGI("pt_: %f %f   %f %f",pt11.x,pt11.y,pt21.x,pt21.y);
      // LOGI("pt_mid:%f %f   interSectPt:%f %f",pt_mid.x,pt_mid.y,interSectPt.x,interSectPt.y);
      // LOGI("sAngle:%f eAngle:%f midwayAngle:%f",sAngle*180/M_PI,eAngle*180/M_PI,midwayAngle*180/M_PI);
      // LOGI("angleDiff:%f ",angleDiff*180/M_PI);
      // LOGI("angleDiff_mid:%f",angleDiff_mid*180/M_PI);

      // LOGI("vec_mid:%f %f",vec_mid.x,vec_mid.y);
      // LOGI("vec1:%f %f",vec1.x,vec1.y);
      // LOGI("vec2:%f %f",vec2.x,vec2.y);
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
    {
      // Cross-type ref (e.g. circle_info pointing at a line) must NA out,
      // not leave measured_val uninitialised -> garbage propagating through
      // the linear-normalize block and leaking nan/inf into the JSON output.
      judgeReport.measured_val = NAN;
      notNA = false;
      break;
    }

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
    auto DEF=judgeReport.def;


    judgeReport.measured_val = 
      (judgeReport.measured_val - DEF->value_A)
      *(DEF->value_Y - DEF->value_X)
      /(DEF->value_B - DEF->value_A)
      +DEF->value_X;
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

    
    if(judgeReport.status == FeatureReport_sig360_circle_line_single::STATUS_NA)
    {
      judgeReport.measured_val=NAN;
    }




  }

  int preSta=judgeReport.status;
  if(judgeReport.def->NAasNG)
  {
    if(judgeReport.status == FeatureReport_sig360_circle_line_single::STATUS_NA)
    {
      judgeReport.status=FeatureReport_sig360_circle_line_single::STATUS_FAILURE;
    }
  }
  if(judgeReport.def->NGasNA)
  {
    if(judgeReport.status == FeatureReport_sig360_circle_line_single::STATUS_FAILURE)
    {
      judgeReport.status=FeatureReport_sig360_circle_line_single::STATUS_NA;
    }
  }

  LOGV(">>>NAG:%d NGA:%d pre-sta:%d sta:%d",
    judgeReport.def->NAasNG,judgeReport.def->NGasNA,
    preSta,judgeReport.status);
  

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
      if (getenv("SP_LEGACY_DUMP"))
        fprintf(stderr, "[SPLEG] ENTER id=%d locating=%d target_id=%d ParseMainVector=%d vec=(%.4f,%.4f)\n",
                def.id, def.locating, def.data.anglefollow.target_id, ret_val, vec.x, vec.y);
      if (ret_val < 0)
      {
        break;
      }
      float angle = def.data.anglefollow.angleDeg * M_PI / 180;
      if (flip_f < 0)
      {
        angle = -angle; //depends on flip or not invert the angle
      }

      // LOGI("spoint id:%d  line vec:%f %f",def.id, vec.x, vec.y);
      vec = acvRotation(sin(angle), cos(angle), 1, vec); //No need to do the flip rotation
      // LOGI("Angle:%f", angle * 180 / M_PI);
      // LOGI("line vec:%f %f", vec.x, vec.y);
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
      vec.x *= -1;
      vec.y *= -1;
      searchDir = -1;
    }
    else
    {
    }
    // acvDrawCrossX(originalImage,
    //       (int)(pt_info.pt.x),(int)(pt_info.pt.y),
    //       2,255,100,255,2);

    // barVec    = parallel to the rendered width-bar (= `vec` from
    //             ParseMainVector after the angleDeg rotation).
    // searchVec = the scanline direction (axis the scan walks along to
    //             find the edge), perpendicular to barVec.
    // Historically these were `searchVec_nor` / `searchVec` — `_nor`
    // means "normal OF" (i.e. perpendicular), but it reads like
    // "normalized" and trips people up. Renamed for clarity.
    acv_XY barVec = vec;
    acv_XY searchVec = acvVecNormal(vec);

    LOGV("pt:%f %f", pt.x, pt.y);
    LOGV("barVec:%f %f", barVec.x, barVec.y);
    LOGV("searchVec:%f %f", searchVec.x, searchVec.y);

    // Caliper/section locating (docs/caliper_primitive_locating_design.md):
    // a single caliper straddling the expected edge along the search vector.
    // pt/margin/width are image-px here; eT image is offset-cropped. Default
    // (locating==0) keeps the legacy contour search below unchanged.
    if (def.locating == 1)
    {
      // A search point SCANS for the FIRST edge hit along the ray (CoreHub model:
      // remap region, gradient along scan, per-column first-hit, topmost). It must
      // NOT average across the width (that smooths/shifts the first hit). Lay
      // `width` parallel scan columns one-sided from pt over `margin` along
      // barVec (already flipped for search_far; SearchPointCV's "searchDir"
      // parameter is in fact the bar-direction axis along which the columns
      // are laid out); find first-hit edge per column; robustly combine
      // (median + inlier mean). default method = FIRST.
      acv_XY off = eT.getImgOffset();
      acv_XY out; float str;
      bool ok;
      // M2: robust CoreHub-ported first-hit scan (rectify + X-sobel + topmost).
      // Region is centered at pt spanning +/-margin along the search dir, so
      // pass margin as the half-depth. Polarity maps from the def edge_polarity
      // (RISING=dark->light). edge_surpress/blur/consider defaults match CoreHub.
      // Default to the OUTER silhouette edge: parts are dark objects on a bright
      // backlit background, so entering the object from outside is bright->dark
      // (LIGHT_TO_DARK). Explicit def polarity still overrides.
      SPEdgeType sp_et = SP_LIGHT_TO_DARK;
      if (def.edge_polarity == EdgeSelectParams::RISING)  sp_et = SP_DARK_TO_LIGHT;
      else if (def.edge_polarity == EdgeSelectParams::FALLING) sp_et = SP_LIGHT_TO_DARK;
      // labeled image shares eT image's frame only when cropOffset==0 (current
      // non-crop pipeline). Mask out background (dilated object label) so the scan
      // can't lock onto background specks; dilate ~8px to keep the boundary edge.
      // NOTE: labeled mask temporarily DISABLED for edge-finding debugging.
      cv::Mat labelImg; // (off.x == 0 && off.y == 0) ? m_labeledImg_cv : empty();
      float includeRangePx = (def.include_range > 0) ? def.include_range : 2.0f;
      // edgeSuppress = def's edge.min_strength (gradient floor). Fall back to
      // a small non-zero default when the user hasn't set one so noise specks
      // don't dominate the first-hit pick.
      float edgeSuppress = (def.edge_min_strength > 0) ? def.edge_min_strength : 10.0f;
      int   blur        = (def.blur > 0)        ? def.blur        : 3;
      float alphaKeep   = def.alpha_keep;            // 0 = none (algorithm default)
      int   maskDilate  = (def.mask_dilate > 0) ? def.mask_dilate : 8;
      ok = search_point_cv(eT.getImageCv(), acvVecSub(pt, off), barVec,
                           margin, width, sp_et, blur, edgeSuppress,
                           includeRangePx, alphaKeep,
                           eT.getBacpac(), labelImg, m_objLabel, maskDilate,
                           &out, &str, def.id, &rep.cal_hits);
      // Lens correction (full-image px). A search point is a single robust
      // centroid (no line/circle fit), so undistorting the final point is the
      // exact lens correction for it. The per-column display hits are
      // undistorted too so the overlay stays in the same frame. No-op unless a
      // valid lensCalib is present. The raw image (eT image) carries no other
      // distortion correction, so this is where it belongs.
      FeatureManager_BacPac *sp_bp = eT.getBacpac();
      bool sp_lens = sp_bp && sp_bp->lensCalib && sp_bp->lensCalib->ok;
      // cal_hits stay in raw (per-column) image-frame px -- the overlay just
      // visualizes where the search picked up, no measurement uses them, so
      // skip the lens-undistort cost. ONLY the final centroid (rep.pt) gets
      // the per-point lens correction below.
      acv_XY sOff = (sp_bp && sp_bp->sampler) ? sp_bp->sampler->getOriginOffset() : acv_XY{0.f, 0.f};
      for (auto &h : rep.cal_hits)
      {
        h.pt = acvVecAdd(h.pt, off);
      }
      if (ok)
      {
        rep.pt = acvVecAdd(out, off);
        if (sp_lens)
        {
          // Lift image-frame px → full-sensor px (sampler holds the user's
          // ROI offset), undistort with the full-sensor lens model, drop back.
          float x = rep.pt.x + sOff.x, y = rep.pt.y + sOff.y;
          lens_undistort_point(*sp_bp->lensCalib, x, y);
          rep.pt.x = x - sOff.x; rep.pt.y = y - sOff.y;
        }
        // Manual offset: shift along the scanline direction (searchVec).
        // Shifting along barVec would be cancelled by the WebUI's
        // closestPointOnLine projection (line runs along barVec
        // through inspAdjObj.x/y). Applied AFTER lens correction so the
        // operator nudge lands in the corrected measurement frame.
        if (def.manual_offset != 0)
          rep.pt = acvVecAdd(rep.pt, acvVecMult(searchVec, def.manual_offset));
        rep.status = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
      }
      else
      {
        rep.pt.x = NAN; rep.pt.y = NAN;
        rep.status = FeatureReport_sig360_circle_line_single::STATUS_NA;
      }
      LOGV("caliper spoint rep.pt:%f %f, status:%d", rep.pt.x, rep.pt.y, rep.status);
      break;
    }

    m_sections.resize(0);

    acv_Line line = {line_vec : barVec, line_anchor : pt};
    acv_Line start_line = line;

    start_line.line_anchor = acvVecMult(searchVec, -999);                         //back margin vector
    start_line.line_anchor = acvVecAdd(start_line.line_anchor, line.line_anchor); //add line_anchor

    // LOGI("line vec %f %f, search_far:%d",line.line_vec.x,line.line_vec.y,def.data.anglefollow.search_far);
    // LOGI("line_anchor %f %f",line.line_anchor.x,line.line_anchor.y);

    if (searchDir != searchDir_NA_FLAG)
      edge_grid.getContourPointsWithInLineContour(line,
                                                  width / 2,
                                                  margin,
                                                  searchDir, m_sections, 999999, 0);

    float nearestDist = 99999999;
    acv_XY nearestPt;

    // for( auto &section_info:m_sections)
    // {
    //   for( auto &pt_info:section_info.section)
    //   {

    //     acv_XY pt=pt_info.pt_img;
    //     // LOGI("[%d]:%.2f %.2f",j,pt.x,pt.y);

    //     // originalImage->CVector[(int)pt.y][(int)pt.x*3]=255;
    //     originalImage->CVector[(int)pt.y][(int)pt.x*3+0]=255;
    //     // originalImage->CVector[(int)pt.y][(int)pt.x*3+2]=255;
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

    if (getenv("SP_LEGACY_DUMP"))
    {
      int totPts = 0; for (auto &si : m_sections) totPts += si.section.size();
      fprintf(stderr, "[SPLEG] id=%d pt=(%.2f,%.2f) barVec=(%.4f,%.4f) searchVec=(%.4f,%.4f) margin=%.2f width=%.2f search_far=%d sections=%zu totPts=%d nearestIdx=%d nearestDist=%.3f\n",
              def.id, pt.x, pt.y, barVec.x, barVec.y, searchVec.x, searchVec.y,
              margin, width, (int)def.data.anglefollow.search_far, m_sections.size(), totPts, nearestPt_idx, nearestDist);
      if (best_section_info)
      {
        // scanline(=searchVec) and bar(=barVec) projection range of the selected section, relative to pt
        float pMin=1e9,pMax=-1e9,aMin=1e9,aMax=-1e9; acv_XY extPerpPt={0,0}; float extPerp=1e9;
        for (auto &pi : best_section_info->section){
          acv_XY d={pi.pt.x-pt.x, pi.pt.y-pt.y};
          float pr=d.x*searchVec.x+d.y*searchVec.y;
          float ar=d.x*barVec.x+d.y*barVec.y;
          if(pr<pMin)pMin=pr; if(pr>pMax)pMax=pr; if(ar<aMin)aMin=ar; if(ar>aMax)aMax=ar;
          if(pr<extPerp){extPerp=pr;extPerpPt=pi.pt;}
        }
        fprintf(stderr, "[SPLEG]   bestSec n=%zu perp[%.2f,%.2f] along[%.2f,%.2f] minPerpPt=(%.2f,%.2f)\n",
                best_section_info->section.size(), pMin,pMax,aMin,aMax, extPerpPt.x,extPerpPt.y);
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
          // float abs_sobel=hypot(pt_info.sobel.x,pt_info.sobel.y);
          // LOGI(">>>noise:%f  cur:%f |sobel|:%f",n,pt_info.curvature,n/abs_sobel);
          // if(abs(n/abs_sobel)>0.1)continue;

          float W = 1 - (dist - nearestDist) / reng;

          nearestPt = acvVecAdd(nearestPt, acvVecMult(pt_info.pt, W));
          accC += W;

          //acv_XY tmpPt = acvVecMult(nearestPt, 1.0 / accC);

          // LOGI("X:%f Y:%f D:%f W:%f",pt_info.pt.x,pt_info.pt.y,dist,W);
          // LOGI(">>>X:%f Y:%f noise:%f",tmpPt.x,tmpPt.y,n);
        }
      }

      if (accC == 0)
      {
        rep.pt.x = NAN;
        rep.pt.y = NAN;
        rep.status = FeatureReport_sig360_circle_line_single::STATUS_NA;
      }
      else
      {
        nearestPt = acvVecMult(nearestPt, 1.0 / accC);

        float ret_rsp;
        // LOGI("nearestPt:%f %f",nearestPt.x,nearestPt.y);
        //rep.pt = acvVecRadialDistortionRemove(nearestPt,param);
        rep.pt = nearestPt;
        rep.status = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
        if (getenv("SP_LEGACY_DUMP"))
          fprintf(stderr, "[SPLEG]   id=%d FINAL=(%.3f,%.3f) accC=%.2f  (delta from pt: %.2f,%.2f)\n",
                  def.id, nearestPt.x, nearestPt.y, accC, nearestPt.x-pt.x, nearestPt.y-pt.y);
      }
    }
    else
    {
      rep.status = FeatureReport_sig360_circle_line_single::STATUS_NA;
    }

    LOGV("found:%d,rep.pt:%f %f, status:%d", 0, rep.pt.x, rep.pt.y, rep.status);
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
      copyFeatureName(searchPoint.name, tmpstr);
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

  // caliper/section locating (default contour). docs/caliper_primitive_locating_design.md
  // A search point scans for the FIRST edge hit, so default method = FIRST
  // (not STRONGEST) to match the legacy nearest-edge semantics.
  searchPoint.locating = 0;
  searchPoint.edge_method = EdgeSelectParams::FIRST;
  searchPoint.edge_polarity = EdgeSelectParams::ANY;
  searchPoint.edge_nth = 0; searchPoint.edge_min_strength = 0;
  searchPoint.include_range = 0;
  searchPoint.manual_offset = 0;
  searchPoint.blur = 0;
  searchPoint.alpha_keep = 0;
  searchPoint.mask_dilate = 0;
  {
    char *loc = (char *)JFetch(jobj, "locating", cJSON_String);
    if (loc && strcmp(loc, "caliper") == 0) searchPoint.locating = 1;
    cJSON *edgeo = JFetch_OBJECT(jobj, "edge");
    if (edgeo) {
      searchPoint.edge_method   = edge_method_from_string((char *)JFetch(edgeo, "method", cJSON_String));
      searchPoint.edge_polarity = edge_polarity_from_string((char *)JFetch(edgeo, "polarity", cJSON_String));
      searchPoint.edge_nth = (int)JFetch_NUMBER_ex(edgeo, "nth", 0);
      searchPoint.edge_min_strength = JFetch_NUMBER_ex(edgeo, "min_strength", 0);
      searchPoint.include_range = JFetch_NUMBER_ex(edgeo, "include_range", 0);
      searchPoint.manual_offset = JFetch_NUMBER_ex(edgeo, "manual_offset", 0);
      searchPoint.blur        = (int)JFetch_NUMBER_ex(edgeo, "blur", 0);
      searchPoint.alpha_keep  = JFetch_NUMBER_ex(edgeo, "alpha_keep", 0);
      searchPoint.mask_dilate = (int)JFetch_NUMBER_ex(edgeo, "mask_dilate", 0);
    }
  }

  switch (searchPoint.subtype)
  {
  case featureDef_searchPoint::anglefollow:
  {
    if ((pnum = JSON_GET_NUM(jobj, "angleDeg")) == NULL) return -1;
    searchPoint.data.anglefollow.angleDeg = *pnum;
    if ((pnum = JSON_GET_NUM(jobj, "ref[0].id")) == NULL) return -1;
    int _tid = (int)*pnum;
    // Self-referential search_point (ref pointing back at its own id) causes a
    // SIGSEGV deep in the search-point processing path because the matching
    // pipeline follows the ref to read an in-progress / uninitialised report.
    // Reject at parse time -- real-world defs never self-reference.
    if (_tid == searchPoint.id)
    {
      LOGE("%s: search_point id:%d ref points to itself (self-reference rejected)",
           __func__, searchPoint.id);
      return -1;
    }
    searchPoint.data.anglefollow.target_id = _tid;
    if ((pnum = JSON_GET_NUM(jobj, "pt1.x")) == NULL) return -1;
    searchPoint.data.anglefollow.position.x = *pnum;
    if ((pnum = JSON_GET_NUM(jobj, "pt1.y")) == NULL) return -1;
    searchPoint.data.anglefollow.position.y = *pnum;

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

    // User tag: is this anchor a corner (2D-localized) or an edge (1D)? Drives the
    // per-anchor precision in the TPS morph (mode 2). Default edge.
    searchPoint.data.anglefollow.anchor_corner = false;
    if (getDataFromJson(jobj, "anchor_corner", &target) == cJSON_True)
      searchPoint.data.anglefollow.anchor_corner = true;

    LOGV("searchPoint.x:%f Y:%f angleDeg:%f tar_id:%d",
         searchPoint.data.anglefollow.position.x,
         searchPoint.data.anglefollow.position.y,
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

  auxPoint.name[0] = '\0';
  if (char *tmpstr = JFetEx_STRING(jobj, "name"))
    copyFeatureName(auxPoint.name, tmpstr);
  double *pnum;

  if ((pnum = JSON_GET_NUM(jobj, "id")) == NULL) return -1;
  auxPoint.id = (int)*pnum;
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

  line.lineTar.line_anchor.x = (line.p0.x + line.p1.x) / 2;
  line.lineTar.line_anchor.y = (line.p0.y + line.p1.y) / 2;
  line.lineTar.line_vec.x = (line.p1.x - line.p0.x);
  line.lineTar.line_vec.y = (line.p1.y - line.p0.y);
  line.MatchingMarginX = hypot(line.lineTar.line_vec.x, line.lineTar.line_vec.y) / 2;
  line.lineTar.line_vec = acvVecNormalize(line.lineTar.line_vec);

  // if (direction < 0)
  // {
  //   line.lineTar.line_vec = acvVecMult(line.lineTar.line_vec, -1);
  // }

  acv_XY normal = acvVecNormal(line.lineTar.line_vec);

  line.searchVec = normal;

  line.searchEstAnchor = line.lineTar.line_anchor;
  // line.searchEstAnchor.x -= normal.x * line.initMatchingMargin;
  // line.searchEstAnchor.y -= normal.y * line.initMatchingMargin;

  LOGV("anchor.x:%f anchor.y:%f vec.x:%f vec.y:%f ,MatchingMargin:%f",
       line.lineTar.line_anchor.x,
       line.lineTar.line_anchor.y,
       line.lineTar.line_vec.x,
       line.lineTar.line_vec.y,
       line.initMatchingMargin);
  LOGV("searchVec X:%f Y:%f vX:%f vY:%f,searchDist:%f",
       line.searchEstAnchor.x,
       line.searchEstAnchor.y,
       line.searchVec.x,
       line.searchVec.y,
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
      copyFeatureName(line.name, tmpstr);
    }
  }

  if ((pnum = JSON_GET_NUM(line_obj, "id")) == NULL) return -1;
  line.id = (int)*pnum;

  // LOGV("feature is a line:%s %d", line.name, line.id);

  if ((pnum = JSON_GET_NUM(line_obj, "margin")) == NULL) return -1;
  line.initMatchingMargin = (float)*pnum;

  // caliper/section locating (default contour). docs/caliper_primitive_locating_design.md
  // cal_width/length/step in DEF UNITS (mm); LineMatching_ReportGen converts
  // to px (/= mmpp) before the caliper engine consumes them.
  line.locating = 0; line.cal_count = 10; line.cal_width = 0.5f; line.cal_length = -1; line.cal_step = -1;
  line.cal_min_inliers = 0; line.cal_max_error = 0;
  // default caliper edge: dominant FALLING edge (white->dark), matching the backlit
  // dark-object-on-bright silhouette. Explicit def "edge.polarity" overrides.
  line.edge_method = EdgeSelectParams::STRONGEST; line.edge_polarity = EdgeSelectParams::FALLING;
  line.edge_nth = 0; line.edge_min_strength = 0;
  {
    char *loc = (char *)JFetch(line_obj, "locating", cJSON_String);
    if (loc && strcmp(loc, "caliper") == 0) line.locating = 1;
    cJSON *calo = JFetch_OBJECT(line_obj, "caliper");
    if (calo) { line.cal_count = (int)JFetch_NUMBER_ex(calo, "count", 10);
                line.cal_width = JFetch_NUMBER_ex(calo, "width", 0.5);
                line.cal_length = JFetch_NUMBER_ex(calo, "length", -1);
                line.cal_step = JFetch_NUMBER_ex(calo, "step", -1);
                line.cal_min_inliers = (int)JFetch_NUMBER_ex(calo, "min_inliers", 0);
                line.cal_max_error = JFetch_NUMBER_ex(calo, "max_error", 0);
                // Clamp pathological caliper sizes (see parse_arcData).
                if (line.cal_count  >  512) line.cal_count  =  512;
                if (line.cal_width  >   64) line.cal_width  =   64;
                if (line.cal_length >  256) line.cal_length =  256; }
    cJSON *edgeo = JFetch_OBJECT(line_obj, "edge");
    if (edgeo) {
      line.edge_method   = edge_method_from_string((char *)JFetch(edgeo, "method", cJSON_String));
      line.edge_polarity = edge_polarity_from_string((char *)JFetch(edgeo, "polarity", cJSON_String));
      line.edge_nth = (int)JFetch_NUMBER_ex(edgeo, "nth", 0);
      line.edge_min_strength = JFetch_NUMBER_ex(edgeo, "min_strength", 0);
    }
  }

  acv_XY p0, p1;
  if ((pnum = JSON_GET_NUM(line_obj, "pt1.x")) == NULL) return -1; p0.x = *pnum;
  if ((pnum = JSON_GET_NUM(line_obj, "pt1.y")) == NULL) return -1; p0.y = *pnum;
  if ((pnum = JSON_GET_NUM(line_obj, "pt2.x")) == NULL) return -1; p1.x = *pnum;
  if ((pnum = JSON_GET_NUM(line_obj, "pt2.y")) == NULL) return -1; p1.y = *pnum;
  line.p0 = p0;
  line.p1 = p1;

  if (line.name[0] == '\0')
  {
    sprintf(line.name, "@LINE_%d", featureLineList.size());
  }
  
  if(JFetch_TRUE(line_obj,"vertex_touch_searching"))
  {
    line.vertex_touch_searching=true;
  }
  else
  {
    line.vertex_touch_searching=false;
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
  //   float diff =1*buffer[i].x;
  //   LOGI("DIFF:%f-%f=%f ",target[i].x,diff,target[i].x-diff);
  //   target[i].x-=diff;
  // }
}

int FeatureManager_sig360_circle_line::parse_sign360(cJSON *signature_obj)
{

  double *pnum;
  if ((pnum = JSON_GET_NUM(signature_obj, "id")) == NULL) return -1;
  this->signature_feature_id = (int)*pnum;
  cJSON *signature;
  if (!(getDataFromJsonObj(signature_obj, "signature", (void **)&signature) & cJSON_Object))
  {
    LOGE("The signature is not an cJSON_Object");
    return -1;
  }

  feature_signature.RELOAD(signature);

  // Capture the raw radius signature (absolute mm) BEFORE the in-place high-pass
  // below -- the shape-based mask needs the true silhouette, not the detail band.
  this->raw_sig_radius = feature_signature.signature_data;

  // The signature's reference origin (pt1, mm/ideal) + orientation: the center
  // the signature was sampled around. The shape localizer uses this as its origin
  // ("original center location") instead of an Otsu-derived centroid.
  cJSON *pt1 = NULL;
  if (getDataFromJsonObj(signature_obj, "pt1", (void **)&pt1) & cJSON_Object)
  {
    this->ref_center_mm.x = (float)JFetch_NUMBER_ex(pt1, "x", 0);
    this->ref_center_mm.y = (float)JFetch_NUMBER_ex(pt1, "y", 0);
    this->has_ref_center = true;
  }
  this->ref_orientation = (float)JFetch_NUMBER_ex(signature_obj, "orientation", 0);

  sign360_process(feature_signature.signature_data, signature_data_buffer);
  // signature_data_buffer

  return 0;
}

int FeatureManager_sig360_circle_line::parse_judgeData(cJSON *judge_obj)
{

  FeatureReport_judgeDef judge = {0};
  double *pnum;

  char *tmpstr = JFetEx_STRING(judge_obj, "name");
  if (tmpstr)
    copyFeatureName(judge.name, tmpstr);

  if ((pnum = JSON_GET_NUM(judge_obj, "id")) == NULL) return -1;
  judge.id = (int)*pnum;

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

  judge.NGasNA=false;
  if(JFetch_TRUE(judge_obj, "NGasNA"))
  {
    judge.NGasNA=true;
  }

  
  judge.NAasNG=false;
  if(JFetch_TRUE(judge_obj, "NAasNG"))
  {
    judge.NAasNG=true;
  }

  char *subtype = JFetEx_STRING(judge_obj, "subtype");
  if (subtype == NULL)
  {
    LOGE("%s: judge id:%d missing required \"subtype\"", __func__, judge.id);
    return -1;
  }

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
    if (infotype == NULL)
    {
      LOGE("%s: judge id:%d circle_info missing required \"info_type\"", __func__, judge.id);
      return -1;
    }
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

    judge.data.ANGLE.pt.x = *JFetEx_NUMBER(judge_obj, "pt1.x");
    judge.data.ANGLE.pt.y = *JFetEx_NUMBER(judge_obj, "pt1.y");

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

  judge.value_A = JfetchStrNUM(judge_obj, "value_A");
  judge.value_B = JfetchStrNUM(judge_obj, "value_B");

  judge.value_X = JfetchStrNUM(judge_obj, "value_X");
  judge.value_Y = JfetchStrNUM(judge_obj, "value_Y");

  if( judge.value_A!=judge.value_A ||
  judge.value_B!=judge.value_B ||
  judge.value_X!=judge.value_X ||
  judge.value_Y!=judge.value_Y )
  {
    judge.value_A=0;
    judge.value_B=1;
    judge.value_X=0;
    judge.value_Y=1;
  }

  judge.USL_b = judge.USL = JfetchStrNUM(judge_obj, "USL");
  judge.LSL_b = judge.LSL = JfetchStrNUM(judge_obj, "LSL");

  void *target;
  int type = getDataFromJson(judge_obj, "back_value_setup", &target);
  if (type == cJSON_True)
  {
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
  this->sigRelativeMatchSimThres=0.8;
  this->sigMatchSimThres=0.9;

  this->sigRelativeMatchSimThres=JFetch_NUMBER_ex(root, "sig_relative_match_sim_thres",0.8);
  
  this->sigMatchSimThres=JFetch_NUMBER_ex(root, "sig_match_sim_thres",0.7);

  this->sig_st1_matching_sim_thres=JFetch_NUMBER_ex(root, "sig_st1_matching_sim_thres",0.3);


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

    // Orientation-signature method (backward compatible: absent => contour_sig).
    this->matching_method = 0;
    char *mm = (char *)JFetch(root, "matching_method", cJSON_String);
    if (mm != NULL && strcmp(mm, "edge_sig") == 0)
      this->matching_method = 1;
    double *emins = JFetch_NUMBER(root, "edge_sig_min_strength");
    if (emins != NULL)
      this->edge_sig_min_strength = (float)*emins;
    double *estep = JFetch_NUMBER(root, "edge_sig_ray_step");
    if (estep != NULL && *estep > 0)
      this->edge_sig_ray_step = (float)*estep;

    // Localization engine: backward compatible (absent => sig360 signature).
    // "shape_based" switches to line2Dup + ROI-refine, trained from the
    // <base>.png sidecar (see trainShapeMatcher, called at the end of parse).
    this->locating_engine = 0;
    char *le = (char *)JFetch(root, "locating_engine", cJSON_String);
    if (le != NULL && strcmp(le, "shape_based") == 0)
      this->locating_engine = 1;
    char *refimg = (char *)JFetch(root, "reference_image", cJSON_String);
    this->reference_image_name = (refimg != NULL) ? refimg : "";
    char *defp = (char *)JFetch(root, "_def_path", cJSON_String);
    this->def_path = (defp != NULL) ? defp : "";
    double *smin = JFetch_NUMBER(root, "shape_min_score");
    if (smin != NULL && *smin > 0) this->shape_min_score = (float)*smin;
    double *sastep = JFetch_NUMBER(root, "shape_angle_step_deg");
    if (sastep != NULL && *sastep > 0) this->shape_angle_step_deg = (float)*sastep;
    double *smscale = JFetch_NUMBER(root, "shape_match_scale");
    if (smscale != NULL && *smscale > 0 && *smscale <= 1.0) this->shape_match_scale = (float)*smscale;

    // line2Dup feature/pyramid tuning (all optional; defaults preserve behavior).
    double *snf = JFetch_NUMBER(root, "shape_num_features");
    if (snf != NULL && *snf >= 8) this->shape_num_features = (int)*snf;
    double *swk = JFetch_NUMBER(root, "shape_weak_thres");
    if (swk != NULL && *swk > 0) this->shape_weak_thres = (float)*swk;
    double *sst = JFetch_NUMBER(root, "shape_strong_thres");
    if (sst != NULL && *sst > 0) this->shape_strong_thres = (float)*sst;
    double *sbl = JFetch_NUMBER(root, "shape_blur");
    if (sbl != NULL && (int)*sbl >= 1) this->shape_blur = (int)*sbl | 1;  // odd kernel
    cJSON *spt = cJSON_GetObjectItem(root, "shape_pyramid_T");
    if (spt != NULL && cJSON_IsArray(spt) && cJSON_GetArraySize(spt) > 0)
    {
      std::vector<int> tv;
      cJSON *e = NULL;
      cJSON_ArrayForEach(e, spt)
        if (cJSON_IsNumber(e) && e->valueint > 0) tv.push_back(e->valueint);
      if (!tv.empty()) this->shape_pyramid_T = tv;
    }
    double *mmppn = JFetch_NUMBER(root, "mmpp");
    this->def_mmpp = (mmppn != NULL && *mmppn > 0) ? (float)*mmppn : 0.0f;

    // def_image_reg (registered pose of the part in the saved reference image).
    // Injected into the sub-feature by the def loader (it lives at def top level).
    this->has_reg = false;
    cJSON *reg = cJSON_GetObjectItem(root, "def_image_reg");
    if (reg != NULL && cJSON_IsObject(reg))
    {
      this->reg_center_mm.x = (float)JFetch_NUMBER_ex(reg, "cx", 0);
      this->reg_center_mm.y = (float)JFetch_NUMBER_ex(reg, "cy", 0);
      this->reg_angle_rad   = (float)JFetch_NUMBER_ex(reg, "angle", 0);
      cJSON *fl = cJSON_GetObjectItem(reg, "isFlipped");
      this->reg_flipped = (fl != NULL) && cJSON_IsTrue(fl);
      this->has_reg = true;
    }

    // Orientation matcher version: backward compatible (absent => v1).
    this->matching_version = 1;
    char *mv = (char *)JFetch(root, "matching_version", cJSON_String);
    if (mv != NULL && strcmp(mv, "v2") == 0)
      this->matching_version = 2;
    double *vmi = JFetch_NUMBER(root, "matching_v2_max_iter");
    if (vmi != NULL && *vmi >= 1) this->matching_v2_max_iter = (int)*vmi;
    double *vtl = JFetch_NUMBER(root, "matching_v2_tol_mm");
    if (vtl != NULL && *vtl > 0) this->matching_v2_tol_mm = (float)*vtl;

    // Locating-anchor morph (deformation correction). Backward compatible:
    // absent => mode 0 (legacy convert_polar). Opt in per def with
    // "wls_similarity"; "legacy"/"polar" are explicit no-ops for clarity.
    this->morph_mode = 1;
    char *morphm = (char *)JFetch(root, "morph_mode", cJSON_String);
    if (morphm != NULL)
    {
      if (strcmp(morphm, "wls_similarity") == 0)
        this->morph_mode = 1;
      else if (strcmp(morphm, "legacy") == 0 || strcmp(morphm, "polar") == 0)
        this->morph_mode = 0;
      else if (strcmp(morphm, "tps") == 0)
        this->morph_mode = 2;
    }
    double *mtl2 = JFetch_NUMBER(root, "morph_tps_lambda");
    if (mtl2 != NULL && *mtl2 >= 0) this->morph_tps_lambda = (float)*mtl2;
    double *mmi = JFetch_NUMBER(root, "morph_max_iter");
    if (mmi != NULL && *mmi >= 1) this->morph_max_iter = (int)*mmi;
    double *mtl = JFetch_NUMBER(root, "morph_tol_mm");
    if (mtl != NULL && *mtl > 0) this->morph_tol_mm = (float)*mtl;
    double *mok = JFetch_NUMBER(root, "morph_outlier_k");
    if (mok != NULL && *mok >= 0) this->morph_outlier_k = (float)*mok;
    double *mrg = JFetch_NUMBER(root, "morph_reg");
    if (mrg != NULL && *mrg >= 0) this->morph_reg = (float)*mrg;
    double *mma = JFetch_NUMBER(root, "morph_min_anchors");
    if (mma != NULL && *mma >= 0) this->morph_min_anchors = (int)*mma;

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
    LOGV("XY:%f %f", vec.x, vec.y);
    vec = acvVecNormal(vec);
    LOGV("XY:%f %f", vec.x, vec.y);
    // Corner anchors constrain both axes (w_minor = weight); edge anchors only
    // along their search normal (w_minor = 0). Used by the TPS morph (mode 2).
    float wmin = searchPointList[j].data.anglefollow.anchor_corner ? 1.0f : 0.0f;
    cm.add(pos, pos, vec, 1.0f, wmin);
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

  if (this->locating_engine != 1 &&
      this->matching_without_signature && feature_signature.signature_data.size() == 0)
  {
    LOGE("No signature data");
    return -1;
  }

  // Detect cycles in the search_point anglefollow target_id graph. A search_point
  // whose ref chain loops back on itself drives the matching pipeline into an
  // unbounded follow that SIGSEGVs. Direct self-refs are already rejected at
  // parse time (parse_searchPointData); this catches multi-node cycles
  // (A->B->A, longer rings).
  for (size_t i = 0; i < searchPointList.size(); i++)
  {
    if (searchPointList[i].subtype != featureDef_searchPoint::anglefollow) continue;
    int cur = searchPointList[i].data.anglefollow.target_id;
    for (int hops = 0; hops < (int)searchPointList.size() + 1 && cur >= 0; hops++)
    {
      if (cur == searchPointList[i].id)
      {
        LOGE("parse_jobj: search_point id:%d ref chain forms a cycle (rejected)",
             searchPointList[i].id);
        return -1;
      }
      int nxt = -1;
      for (size_t j = 0; j < searchPointList.size(); j++)
      {
        if (searchPointList[j].id == cur &&
            searchPointList[j].subtype == featureDef_searchPoint::anglefollow)
        {
          nxt = searchPointList[j].data.anglefollow.target_id;
          break;
        }
      }
      cur = nxt;
    }
  }

  // Train the shape-based localizer from the <base>.png sidecar. On failure we
  // log and leave shape_ready=false so FeatureMatching falls back to sig360.
  if (this->locating_engine == 1)
  {
    if (trainShapeMatcher() != 0)
      LOGW("[shape] training failed; falling back to sig360 for this def");
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


bool ptInfo_tmp_comp(const ContourFetch::ptInfo &a, const ContourFetch::ptInfo &b)
{
  return a.tmp < b.tmp;
}

const acv_LineFit lf_zero = {};
const FeatureReport_lineReport lr_NA = {line : lf_zero, status : FeatureReport_sig360_circle_line_single::STATUS_NA};

bool drawDraw = false;


int ptInfoFind(vector<ContourFetch::contourMatchSec> &ptGs,int mixedIdx,edgeTracking &eT,ContourFetch::ptInfo *ret_pt0)
{
  ContourFetch::contourMatchSec singPt;
  for(int i=0;i<ptGs.size();i++)
  {
    if(mixedIdx>=ptGs[i].section.size())
    {
      mixedIdx-=ptGs[i].section.size();
      continue;
    }
    // LOGI("ptGs[%d].section.size():%d>>>mixedIdx:%d",i,ptGs[i].section.size(),mixedIdx);
    singPt.dist=ptGs[i].dist;
    singPt.sigma=ptGs[i].sigma;
    singPt.contourIdx=ptGs[i].contourIdx;

    ContourFetch::ptInfo tarPt = ptGs[i].section[mixedIdx];
    int span=2;
    
    // LOGI("%d<=j<=%d   size:%d",mixedIdx-span,(mixedIdx+span+1 ),ptGs[i].section.size());
    for(int j=mixedIdx-span; 
      (j<(mixedIdx+span+1 )) && (j<(int)ptGs[i].section.size())   ;
      j++)
    {
      if(j<0)j=0;
      
      singPt.section.push_back(ptGs[i].section[j]);
    }
    
    eT.initTracking(singPt, 0);

    acv_XY ptSum={0};

    for(int j=0;j<singPt.section.size();j++)
    {
      acv_XY pt=singPt.section[j].pt;
      ptSum=acvVecAdd(ptSum,pt);
    }
    ptSum=acvVecMult(ptSum,1.0/singPt.section.size());



    if(ret_pt0)
    {
      tarPt.pt=ptSum;
      *ret_pt0=tarPt;
    }
    return 0;
  }
  return -1;
}

// void ComputeConvexHull(const acv_XY *polygon,const int L,std::vector<int> &ret_chIdxs);

int vertex_touch_searching(acv_Line line_cand,vector<ContourFetch::contourMatchSec> &ptGs,edgeTracking &eT,float flip,acv_Line *retLineMax,acv_XY *pt0,acv_XY *pt1)
{

  // LOGI("vertex_touch_searching  m_sections.size():%d",ptGs.size());
  // LOGI(">>line_vec>>%f,%f",line_cand.line_vec.x,line_cand.line_vec.y);


  acv_XY edgeVec=acvVecNormal(line_cand.line_vec);


  // LOGI(">edgeVec>>>%f,%f",edgeVec.x,edgeVec.y);

  float minDist=999999;
  int minDistIdx=-1;
  vector<acv_XY> pts;
  for(ContourFetch::contourMatchSec &sec:ptGs)
  {
     
    // LOGI("-----------");
    for(ContourFetch::ptInfo &pt:sec.section)
    {
      pts.push_back(pt.pt);
      // pt.
      acv_XY vec= acvVecSub(pt.pt,line_cand.line_anchor);
      float dist = acv2DDotProduct(vec,edgeVec)*flip;
      if(minDist>dist)
      {
        minDist=dist;
        minDistIdx=pts.size()-1;
      }
    }
  }
  std::vector<int> ret_chIdxs;
  //the following ComputeConvexHull is not a saft algo, that is, if the init stating point is not on convex surface
  //it will give false result
  //minDistIdx is the closest point to the target line, that is, the minDistIdx means it has to be on the ConvexHull point
  ComputeConvexHull(&pts[0],pts.size(),minDistIdx,ret_chIdxs);
  
  int maxDist=5;
  int maxDistIdxsIdx=-1;
  for(int i=0;i<ret_chIdxs.size();i++)
  {
    acv_XY pt0=pts[ret_chIdxs[i]];
    int nxti=i+1;
    if(nxti>=ret_chIdxs.size())
      nxti-=ret_chIdxs.size();

    acv_XY pt1=pts[ret_chIdxs[nxti]];
    acv_XY vec=acvVecSub(pt1,pt0);

    float dotP=acv2DDotProduct(vec, line_cand.line_vec);
    if(dotP*flip<0)
    {
      continue;
    }
    float dist = acvDistance(pt0,pt1);

    // LOGI("pt0:%f,%f  %f,%f  dotP:%f",pt0.x,pt0.y,  pt1.x,pt1.y,dotP);
    // LOGI("[%d~%d ]  dist:%f",ret_chIdxs[i],ret_chIdxs[nxti],dist);
    if(maxDist<dist)
    {
      maxDist=dist;
      maxDistIdxsIdx=i;
    }
  }



  if(maxDistIdxsIdx==-1)
  {
    return -1;
  }


  ContourFetch::ptInfo pti0,pti1;
  ptInfoFind(ptGs,ret_chIdxs[maxDistIdxsIdx],eT,&pti0);
  
  int nxti=maxDistIdxsIdx+1;
  if(nxti>=ret_chIdxs.size())
  {
    nxti-=ret_chIdxs.size();
  }
  ptInfoFind(ptGs,ret_chIdxs[nxti],eT,&pti1);

  LOGI("pti>>>:%f,%f  %f,%f  ",pti0.pt.x,pti0.pt.y, pti1.pt.x,pti1.pt.y);

  if(pt0)
  {
    *pt0=pti0.pt;
  }
  if(pt1)
  {
    *pt1=pti1.pt;
  }
  if(retLineMax)
  {
    retLineMax->line_anchor=pti0.pt;
    retLineMax->line_vec=acvVecMult(acvVecSub(pti1.pt,pti0.pt),flip);
  }

  return 0;
}

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
  //     line_cand.line_anchor.x-mult*line_cand.line_vec.x,
  //     line_cand.line_anchor.y-mult*line_cand.line_vec.y,
  //     line_cand.line_anchor.x+mult*line_cand.line_vec.x,
  //     line_cand.line_anchor.y+mult*line_cand.line_vec.y,
  //     20,255,128);
  // }
  float lineCurvatureMax=line->vertex_touch_searching?10 :0.15;
  float cosSim=line->vertex_touch_searching?0.3 :0.9;
  edge_grid.getContourPointsWithInLineContour(line_cand,
                                              MatchingMarginX,

                                              //HACK:Kinda hack... the initial Margin is for initial keypoints search,
                                              //But since we get the cadidate line already, no need for huge Margin
                                              initMatchingMargin,
                                              flip_f, m_sections,
                                              lineCurvatureMax,
                                              cosSim);

  if (m_sections.size() == 0)
  { //No match... FAIL
    LOGI("Not able to find matching contour");
    FeatureReport_lineReport lr = lr_NA;
    lr.def = line;
    return lr;
  }


  if(line->vertex_touch_searching)
  {
    acv_Line retLine;
    acv_XY pt0,pt1;

    if(vertex_touch_searching(line_cand,m_sections,eT,flip_f,&retLine,&pt0,&pt1)==0)
    {


      acv_LineFit lf = {};
      lf.line = retLine;
      lf.matching_pts = 2;
      lf.s = 1;
      FeatureReport_lineReport lr;
      // lr.rough_RMSE=rRMSE;
      // lr.rough_MAX=rMAX;
      // lr.rough_MIN=rMIN;
      lr.line = lf;
      lr.def = line;
      // lr.line.end_pt1 = pt0;
      // lr.line.end_pt2 = pt1;
      
      lr.line.end_pt1 = acvClosestPointOnLine(line->p0, retLine);
      lr.line.end_pt2 = acvClosestPointOnLine(line->p1, retLine);
      lr.status=FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
      return lr;
    }
    else
    {


      LOGI("MatchingMarginX:%f ,initMatchingMargin:%f",MatchingMarginX);
      FeatureReport_lineReport lr = lr_NA;
      lr.def = line;
      return lr;
    }
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
      //   LOGI("a:%d:%f,%f>",m,m_sections[k].section[m].pt.x,m_sections[k].section[m].pt.y);
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
  //       // LOGI("[%d]:%.2f %.2f",j,pt.x,pt.y);

  //       originalImage->CVector[(int)pt.y][(int)pt.x * 3 + 1] = 255;
  //     }
  //   }

  if (s_points.size() > 10 && false)
  {
    acv_XY lineNormal = {-line_cand.line_vec.y, line_cand.line_vec.x};

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
          line_cand.line_anchor.x-mult*line_cand.line_vec.x,
          line_cand.line_anchor.y-mult*line_cand.line_vec.y,
          line_cand.line_anchor.x+mult*line_cand.line_vec.x,
          line_cand.line_anchor.y+mult*line_cand.line_vec.y,
          20,255,128);*/

  if (acv2DDotProduct(line_cand.line_vec, target_vec) < 0)
  {
    LOGV("VEC INV::::");
    line_cand.line_vec = acvVecMult(line_cand.line_vec, -1);
  }

  // line_cand.line_anchor.x+=line_cand.line_vec.x;
  // line_cand.line_anchor.y+=line_cand.line_vec.y;
  // LOGI("L=%d===anchor:%f,%f vec:%f,%f ,sigma:%f target_vec:%f,%f,", j,
  //      line_cand.line_anchor.x,
  //      line_cand.line_anchor.y,
  //      line_cand.line_vec.x,
  //      line_cand.line_vec.y,
  //      sigma,
  //      target_vec.x, target_vec.y);

  float rMIN, rMAX, rRMSE;
  roughness(s_points, 5, &rMIN, &rMAX, &rRMSE);

  LOGI("L===anchor:%f,%f vec:%f,%f ,sigma:%f rMin:%f rMax:%f rRMSE:%f",

       line_cand.line_anchor.x,
       line_cand.line_anchor.y,
       line_cand.line_vec.x,
       line_cand.line_vec.y,
       sigma,
       rMIN, rMAX, rRMSE);
  acv_LineFit lf = {};
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
        // LOGI("pop: p1:%f %f  p2:%f %f  pc:%f %f      idx:%d",p1.x,p1.y,p2.x,p2.y,polygon[i].x,polygon[i].y,upperIdx[(upperIdx.size() - 1)]);
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
acv_XY Image_mm_Domain_TO_TemplateDomain(acv_XY im_mm_pt, float sin, float cosin, float flip_f, acv_XY objCenter_pix, float mmpp)
{ //  sp1...ret......sp2 <= 0.3333
  acv_XY pt = acvVecSub(im_mm_pt,acvVecMult( objCenter_pix, mmpp));


  pt = acvRotation(-sin, cosin, pt);
  if (flip_f < 0)
    pt.y = -pt.y;

  return pt;
}
acv_XY TemplateDomain_TO_PixDomain(acv_XY temp_pt, float sin, float cosin, float flip_f, acv_XY objCenter_pix, float mmpp)
{ //  sp1...ret......sp2 <= 0.3333

  acv_XY pt = acvRotation(sin, cosin, flip_f, temp_pt);

  return acvVecAdd(acvVecMult(pt, 1 / mmpp), objCenter_pix); //convert to pixel unit
}

// Inverse of TemplateDomain_TO_PixDomain: image-px → object-frame mm.
// Used to lift caliper-mode per-hit coordinates back into the def's own
// coordinate frame (centered around the object's nominal origin, no
// rotation), so consumers can compare them directly to def shape pt1/pt2.
acv_XY PixDomain_TO_TemplateDomain(acv_XY pix_pt, float sine, float cosine, float flip_f, acv_XY objCenter_pix, float mmpp)
{
  // 1) shift to object center, scale px → mm
  acv_XY pt = acvVecMult(acvVecSub(pix_pt, objCenter_pix), mmpp);
  // 2) inverse rotation: transpose of the forward rotation, then un-flip y.
  //    Forward (acvRotation): output = R(sine,cosine) · F(flip_f) · input
  //    Inverse:               input  = F(flip_f) · R^T · output
  acv_XY out;
  out.x =  pt.x * cosine + pt.y * sine;
  out.y = -pt.x * sine   + pt.y * cosine;
  out.y *= flip_f;
  return out;
}

acv_XY ConstrainMap::convert_polar(acv_XY from)
{
  // Local, distance-weighted SIMILARITY morph (Moving-Least-Squares form).
  //
  // LEGACY ASSUMPTION: locating anchors are CORNER points, so each anchor's
  // full 2D correspondence from_i -> to_i is trustworthy on BOTH axes (the
  // operator is told to constrain anchors to corners). We therefore fit, around
  // the query point and weighted by inverse distance, the local rotation+scale+
  // translation that best maps from_i -> to_i, and apply it to `from`.
  //
  // This keeps the local rotation/scale behaviour the old polar formulation was
  // reaching for, but in displacement space — without its coupling of the
  // correction to a point's absolute radius about the origin.
  //
  // Closed form (complex): subtract weighted centroids (handles translation),
  // then the similarity factor is  z = sum w (b * conj a) / sum w |a|^2  with
  // a_i = from_i - from_c, b_i = to_i - to_c, and  T(p) = to_c + z (x) (p-from_c).
  double wSum = 0;
  acv_XY fromC = {0, 0}, toC = {0, 0};
  bool doAdj = false;
  for (int i = 0; i < anchorPairs.size(); i++)
  {
    const anchorPair &pair = anchorPairs[i];
    if (pair.to.x != pair.to.x) // NAN
      continue;
    doAdj = true;
    float distance = acvDistance(from, pair.from);
    if (distance < 0.01)
      distance = 0.01;
    double w = (pair.weight > 0 ? pair.weight : 1.0) / distance;
    wSum  += w;
    fromC  = acvVecAdd(fromC, acvVecMult(pair.from, (float)w));
    toC    = acvVecAdd(toC,   acvVecMult(pair.to,   (float)w));
  }
  if (!doAdj || wSum <= 0)
    return from;
  fromC = acvVecMult(fromC, (float)(1.0 / wSum));
  toC   = acvVecMult(toC,   (float)(1.0 / wSum));

  // Weighted least-squares similarity factor z (complex scale*rotation).
  double numR = 0, numI = 0; // sum w * (b * conj(a))
  double den  = 0;           // sum w * |a|^2
  for (int i = 0; i < anchorPairs.size(); i++)
  {
    const anchorPair &pair = anchorPairs[i];
    if (pair.to.x != pair.to.x)
      continue;
    float distance = acvDistance(from, pair.from);
    if (distance < 0.01)
      distance = 0.01;
    double w = (pair.weight > 0 ? pair.weight : 1.0) / distance;
    double ax = pair.from.x - fromC.x, ay = pair.from.y - fromC.y;
    double bx = pair.to.x   - toC.x,   by = pair.to.y   - toC.y;
    numR += w * (bx * ax + by * ay); // Re(b * conj a)
    numI += w * (by * ax - bx * ay); // Im(b * conj a)
    den  += w * (ax * ax + ay * ay);
  }

  acv_XY p = acvVecSub(from, fromC);
  acv_XY mapped;
  if (den < 1e-12)
  {
    // Anchors (nearly) coincident -> rotation/scale under-determined; fall back
    // to a pure translation by the centroid offset (exact for a single anchor).
    mapped = p;
  }
  else
  {
    double zr = numR / den, zi = numI / den;
    mapped = acv_XY((float)(zr * p.x - zi * p.y),  // z (x) p  (complex multiply)
                    (float)(zi * p.x + zr * p.y));
  }
  return acvVecAdd(toC, mapped);
}

// Solve a small dense linear system M*x = b (n<=4) by Gauss elimination with
// partial pivoting. Returns false if M is singular. M is destroyed.
static bool solveDenseNxN(double M[4][4], double b[4], double x[4], int n)
{
  for (int col = 0; col < n; col++)
  {
    int piv = col;
    double best = fabs(M[col][col]);
    for (int r = col + 1; r < n; r++)
      if (fabs(M[r][col]) > best) { best = fabs(M[r][col]); piv = r; }
    if (best < 1e-18)
      return false;
    if (piv != col)
    {
      for (int c = 0; c < n; c++) { double t = M[col][c]; M[col][c] = M[piv][c]; M[piv][c] = t; }
      double t = b[col]; b[col] = b[piv]; b[piv] = t;
    }
    for (int r = col + 1; r < n; r++)
    {
      double f = M[r][col] / M[col][col];
      for (int c = col; c < n; c++) M[r][c] -= f * M[col][c];
      b[r] -= f * b[col];
    }
  }
  for (int r = n - 1; r >= 0; r--)
  {
    double s = b[r];
    for (int c = r + 1; c < n; c++) s -= M[r][c] * x[c];
    x[r] = s / M[r][r];
  }
  return true;
}

// Directional weighted least-squares similarity fit (morph mode 1).
//
// Each valid anchor contributes ONE scalar constraint: the displacement of its
// template point, measured only ALONG its constrainVector (searchVec) — the
// perpendicular (bar-direction) component is unreliable and intentionally left
// unconstrained. We fit a single similarity T(p)=[sa -sb; sb sa]p + [stx;sty]
// minimizing  sum_i w_i ( cv_i . (T(from_i) - to_i) )^2  + ridge toward identity.
// The ridge keeps the 4x4 well-conditioned and degrades gracefully to identity
// when the anchors do not constrain a parameter (few/collinear/degenerate).
int ConstrainMap::solve()
{
  // Count valid anchors regardless of mode (used by callers as a quality signal).
  int valid = 0;
  for (size_t i = 0; i < anchorPairs.size(); i++)
    if (anchorPairs[i].to.x == anchorPairs[i].to.x) valid++; // not-NAN
  valid_count = valid;

  // Mode 0 computes its warp per-call in convert_polar; nothing to cache.
  if (mode == 0)
    return valid;
  // Mode 2: anisotropic approximating-TPS (handles mixed edge/corner anchors).
  if (mode == 2)
    return solve_tps();
  // Mode 1: identity unless we have enough anchors to trust a fit.
  resetTransform();
  valid_count = valid;
  int need = min_anchors > 0 ? min_anchors : 2;
  if (valid < need)
  {
    if (valid < (int)anchorPairs.size())
      LOGI("ConstrainMap::solve mode1: only %d/%d valid anchors (<%d) -> identity morph",
           valid, (int)anchorPairs.size(), need);
    return valid;
  }

  // Active mask for optional MAD outlier rejection.
  std::vector<char> active(anchorPairs.size(), 1);
  int passes = (outlier_k > 0) ? 2 : 1;
  for (int pass = 0; pass < passes; pass++)
  {
    double M[4][4] = {{0}};
    double r[4] = {0};
    int used = 0;
    for (size_t i = 0; i < anchorPairs.size(); i++)
    {
      const anchorPair &p = anchorPairs[i];
      if (p.to.x != p.to.x) continue; // NAN
      if (!active[i]) continue;
      double cx = p.constrainVector.x, cy = p.constrainVector.y;
      double x = p.from.x, y = p.from.y;
      double u = p.to.x, v = p.to.y;
      double w = p.weight > 0 ? p.weight : 1.0;
      // A . theta = D, where theta = (sa, sb, stx, sty)
      double A[4] = { cx * x + cy * y, cy * x - cx * y, cx, cy };
      double D = cx * u + cy * v;
      for (int a = 0; a < 4; a++)
      {
        for (int b = 0; b < 4; b++) M[a][b] += w * A[a] * A[b];
        r[a] += w * A[a] * D;
      }
      used++;
    }
    if (used < need) { resetTransform(); valid_count = used; return used; }

    // Ridge toward identity target (sa,sb,stx,sty) = (1,0,0,0), scaled relative
    // to the data so it is negligible when the anchors fully constrain a param.
    double trace = M[0][0] + M[1][1] + M[2][2] + M[3][3];
    double lam = (double)reg * (trace / 4.0 + 1e-9);
    M[0][0] += lam; r[0] += lam * 1.0;
    M[1][1] += lam; /* target 0 */
    M[2][2] += lam; /* target 0 */
    M[3][3] += lam; /* target 0 */

    double theta[4];
    if (!solveDenseNxN(M, r, theta, 4))
    {
      resetTransform();
      valid_count = used;
      return used;
    }
    sa = theta[0]; sb = theta[1]; stx = theta[2]; sty = theta[3];

    if (outlier_k <= 0 || pass == passes - 1) break;

    // Residuals along each anchor's constrainVector, then MAD-gate.
    std::vector<double> res;
    std::vector<size_t> idx;
    for (size_t i = 0; i < anchorPairs.size(); i++)
    {
      const anchorPair &p = anchorPairs[i];
      if (p.to.x != p.to.x || !active[i]) continue;
      acv_XY t = convert(p.from); // uses cached sa,sb,stx,sty (mode 1)
      double e = p.constrainVector.x * (t.x - p.to.x) + p.constrainVector.y * (t.y - p.to.y);
      res.push_back(fabs(e));
      idx.push_back(i);
    }
    if (res.size() <= (size_t)need) break;
    std::vector<double> sorted = res;
    std::sort(sorted.begin(), sorted.end());
    double med = sorted[sorted.size() / 2];
    std::vector<double> dev(res.size());
    for (size_t i = 0; i < res.size(); i++) dev[i] = fabs(res[i] - med);
    std::sort(dev.begin(), dev.end());
    double mad = 1.4826 * dev[dev.size() / 2];
    if (mad < 1e-9) break; // residuals already tight
    int dropped = 0;
    for (size_t i = 0; i < res.size(); i++)
      if (res[i] > med + outlier_k * mad)
      {
        // Keep at least `need` anchors active.
        if ((int)idx.size() - dropped - 1 < need) break;
        active[idx[i]] = 0;
        dropped++;
      }
    if (dropped == 0) break; // converged, no need for second pass
  }
  return valid_count;
}

acv_XY ConstrainMap::convert(acv_XY from)
{
  if (mode == 0)
    return convert_polar(from);
  if (mode == 2)
    return convert_tps(from);
  // Mode 1: apply the cached directional-similarity transform.
  acv_XY out;
  out.x = (float)(sa * from.x - sb * from.y + stx);
  out.y = (float)(sb * from.x + sa * from.y + sty);
  return out;
}

// Thin-plate-spline radial basis: phi(r) = r^2 * log(r^2),  phi(0)=0.
static double tps_phi_r2(double r2)
{
  if (r2 < 1e-12) return 0.0;
  return r2 * std::log(r2);
}

// ---- Morph effectiveness test hook (diagnostics only) --------------------
// Injects a known REGIONAL deformation field D(p) onto detected anchor targets
// so the morph "sees" a ground-truth deformation on an undeformed frame. The
// metric is whether cm.convert(q) tracks q + D(q) (i.e. the inspection window
// stays on the feature). Zero overhead unless MORPH_FIELD_AMP is set.
//   MORPH_FIELD_AMP  amplitude: mm (type=trans) or DEGREES (type=rot); 0=>disabled
//   MORPH_FIELD_TYPE "trans" (gaussian translation, default) | "rot" (local rotation)
//   MORPH_FIELD_C    "cx,cy"  bump/rotation centre in object-frame mm (default arc)
//   MORPH_FIELD_SIG  gaussian sigma in mm (default 1.6)
//   MORPH_FIELD_DIR  "dx,dy"  translation direction (type=trans only; default 1,0)
// type=rot models the photo's case: the region near C rotates (the rod tilts), so a
// good morph must reproduce the LOCAL ROTATION to keep the caliper window aligned.
// p is the template/object-frame anchor position (cm.anchorPairs[].from).
static bool synth_field_enabled()
{
  static bool init = false, on = false;
  if (!init) { init = true; const char *a = getenv("MORPH_FIELD_AMP"); on = (a && atof(a) != 0.0); }
  return on;
}
static acv_XY synth_morph_field(acv_XY p)
{
  static bool init = false, is_rot = false;
  static double amp = 0, cx = 1.55, cy = -0.30, sig = 1.6, dx = 1, dy = 0;
  if (!init)
  {
    init = true;
    const char *a = getenv("MORPH_FIELD_AMP"); if (a) amp = atof(a);
    const char *c = getenv("MORPH_FIELD_C");   if (c) sscanf(c, "%lf,%lf", &cx, &cy);
    const char *s = getenv("MORPH_FIELD_SIG");  if (s) sig = atof(s);
    const char *d = getenv("MORPH_FIELD_DIR");  if (d) { sscanf(d, "%lf,%lf", &dx, &dy);
      double n = sqrt(dx * dx + dy * dy); if (n > 0) { dx /= n; dy /= n; } }
    const char *t = getenv("MORPH_FIELD_TYPE"); if (t && strcmp(t, "rot") == 0) is_rot = true;
  }
  acv_XY r = acv_XY(0, 0);
  if (amp == 0) return r;
  double ex = p.x - cx, ey = p.y - cy;
  double g = exp(-(ex * ex + ey * ey) / (2.0 * sig * sig));
  if (is_rot)
  {
    // Gaussian-enveloped local rotation: near C rotate by amp degrees, ->0 far away.
    double th = amp * (3.14159265358979323846 / 180.0) * g;
    double ct = cos(th), st = sin(th);
    r.x = (float)(ct * ex - st * ey - ex);   // (R*e - e).x
    r.y = (float)(st * ex + ct * ey - ey);   // (R*e - e).y
  }
  else
  {
    r.x = (float)(amp * g * dx);
    r.y = (float)(amp * g * dy);
  }
  return r;
}

// Similarity-base RBF fit (mode 2). The global part is a 4-DOF SIMILARITY
// (rotation+scale+translation, shared between x and y) instead of a 6-DOF affine
// -- removing the shear / anisotropic-scale freedom that made the old affine-base
// TPS wobble under sparse rank-1 anchors. A TPS-kernel RBF residual captures the
// REGIONAL (non-affine) part a global similarity can't. Each anchor contributes a
// 2x2 precision W = w_major*n n^T + w_minor*t t^T (n = constrainVector, t = perp):
// edge anchors (w_minor=0) constrain only along their normal, corner anchors
// (w_minor~w_major) constrain 2D -- all blended in one coupled solve. Unknowns:
//   u = [cx(N) | cy(N) | a | b | tx | ty],
//   fx = a*x - b*y + tx + sum cx_j phi(|p-from_j|)
//   fy = b*x + a*y + ty + sum cy_j phi(|p-from_j|)
// The result is stored into ax/ay as (tx,a,-b)/(ty,b,a) so convert_tps is unchanged.
int ConstrainMap::solve_tps()
{
  tps_valid = false;
  std::vector<int> idx;
  for (size_t i = 0; i < anchorPairs.size(); i++)
    if (anchorPairs[i].to.x == anchorPairs[i].to.x) idx.push_back((int)i); // not NAN
  int N = (int)idx.size();
  valid_count = N;
  int need = min_anchors > 0 ? min_anchors : 3;
  if (N < need) return N; // caller falls back to identity (tps_valid stays false)

  std::vector<acv_XY> X(N);
  for (int a = 0; a < N; a++) X[a] = anchorPairs[idx[a]].from;
  std::vector<std::vector<double>> K(N, std::vector<double>(N, 0.0));
  for (int a = 0; a < N; a++)
    for (int b = 0; b < N; b++)
    {
      double dx = X[a].x - X[b].x, dy = X[a].y - X[b].y;
      K[a][b] = tps_phi_r2(dx * dx + dy * dy);
    }

  // u = [cx(N) | cy(N) | a | b | tx | ty]
  const int M = 2 * N + 4, OA = 2 * N, OB = 2 * N + 1, OTX = 2 * N + 2, OTY = 2 * N + 3;
  cv::Mat S   = cv::Mat::zeros(M, M, CV_64F);
  cv::Mat rhs = cv::Mat::zeros(M, 1, CV_64F);
  std::vector<double> mx(M), my(M);

  for (int a = 0; a < N; a++)
  {
    const anchorPair &p = anchorPairs[idx[a]];
    double nx = p.constrainVector.x, ny = p.constrainVector.y;
    double nrm = std::sqrt(nx * nx + ny * ny);
    if (nrm > 1e-9) { nx /= nrm; ny /= nrm; } else { nx = 1; ny = 0; }
    double wMaj = (p.weight  > 0) ? p.weight  : 1.0;  // precision along normal
    double wMin = (p.w_minor > 0) ? p.w_minor : 0.0;  // precision along tangent
    double w11 = wMaj * nx * nx + wMin * ny * ny;
    double w22 = wMaj * ny * ny + wMin * nx * nx;
    double w12 = (wMaj - wMin) * nx * ny;
    double ux = p.to.x, uy = p.to.y;

    std::fill(mx.begin(), mx.end(), 0.0);
    std::fill(my.begin(), my.end(), 0.0);
    for (int j = 0; j < N; j++) { mx[j] = K[a][j]; my[N + j] = K[a][j]; }
    // similarity coupling: fx = a*x - b*y + tx ;  fy = a*y + b*x + ty
    mx[OA] = X[a].x; mx[OB] = -X[a].y; mx[OTX] = 1.0;
    my[OA] = X[a].y; my[OB] =  X[a].x; my[OTY] = 1.0;

    for (int r = 0; r < M; r++)
    {
      double mxr = mx[r], myr = my[r];
      if (mxr == 0.0 && myr == 0.0) continue;
      double *Srow = S.ptr<double>(r);
      for (int c = 0; c < M; c++)
        Srow[c] += w11 * mxr * mx[c] + w12 * (mxr * my[c] + myr * mx[c]) + w22 * myr * my[c];
      rhs.at<double>(r, 0) += w11 * mxr * ux + w12 * (mxr * uy + myr * ux) + w22 * myr * uy;
    }
  }

  // Bending energy lambda*(cx^T K cx + cy^T K cy): add lambda*K to the cx,cy blocks.
  // Penalises the RBF residual so the global similarity explains as much as it can
  // and the kernel only bends where a similarity genuinely cannot fit.
  double lam = (tps_lambda > 0) ? tps_lambda : 0.0;
  for (int a = 0; a < N; a++)
    for (int b = 0; b < N; b++)
    {
      S.at<double>(a,     b)     += lam * K[a][b];
      S.at<double>(N + a, N + b) += lam * K[a][b];
    }
  // Regularize the global similarity toward IDENTITY (a=1, b=0, tx=0, ty=0) and the
  // RBF coeffs toward 0. With rank-1 anchors the unobserved direction is a null space;
  // ridging to identity keeps that direction at "no deformation". CRITICAL: scale the
  // ridge to the similarity block's OWN data magnitude (its strongest diagonal), NOT
  // the global trace -- the bending block (lam*K) dominates the trace and would make
  // the ridge so strong it suppresses even a fully-observed translation. As a small
  // fraction of the strongest observed DOF, the ridge is negligible where a DOF is
  // observed (data wins -> full correction) yet still resolves the unobserved null
  // direction to identity.
  double sdmax = 0.0;
  for (int k : {OA, OB, OTX, OTY}) sdmax = std::max(sdmax, S.at<double>(k, k));
  double gamma = (double)((reg > 0 ? reg : 1e-3f)) * (sdmax + 1.0);
  S.at<double>(OA,  OA)  += gamma; rhs.at<double>(OA,  0) += gamma; // a  -> 1
  S.at<double>(OB,  OB)  += gamma;                                  // b  -> 0
  S.at<double>(OTX, OTX) += gamma;                                  // tx -> 0
  S.at<double>(OTY, OTY) += gamma;                                  // ty -> 0
  for (int j = 0; j < 2 * N; j++) S.at<double>(j, j) += 1e-6 * (sdmax + 1.0); // c -> 0 (numerical)

  cv::Mat theta;
  if (!cv::solve(S, rhs, theta, cv::DECOMP_SVD)) return N; // singular -> fall back

  double a = theta.at<double>(OA, 0), b = theta.at<double>(OB, 0);
  double tx = theta.at<double>(OTX, 0), ty = theta.at<double>(OTY, 0);
  tps_centers = X;
  tps_cx.assign(N, 0.0); tps_cy.assign(N, 0.0);
  for (int j = 0; j < N; j++) { tps_cx[j] = theta.at<double>(j, 0); tps_cy[j] = theta.at<double>(N + j, 0); }
  // Store the similarity as an affine row so convert_tps stays unchanged:
  //   fx = ax0 + ax1*x + ax2*y = tx + a*x + (-b)*y ;  fy = ty + b*x + a*y
  tps_ax[0] = tx; tps_ax[1] = a;  tps_ax[2] = -b;
  tps_ay[0] = ty; tps_ay[1] = b;  tps_ay[2] =  a;
  tps_valid = true;
  return N;
}

acv_XY ConstrainMap::convert_tps(acv_XY from)
{
  if (!tps_valid) return from;
  double fx = tps_ax[0] + tps_ax[1] * from.x + tps_ax[2] * from.y;
  double fy = tps_ay[0] + tps_ay[1] * from.x + tps_ay[2] * from.y;
  for (size_t j = 0; j < tps_centers.size(); j++)
  {
    double dx = from.x - tps_centers[j].x, dy = from.y - tps_centers[j].y;
    double ph = tps_phi_r2(dx * dx + dy * dy);
    fx += tps_cx[j] * ph;
    fy += tps_cy[j] * ph;
  }
  return acv_XY((float)fx, (float)fy);
}

inline int valueWarping(int v, int ringSize)
{
  v %= ringSize;
  return (v < 0) ? v + ringSize : v;
}
void matchingErrorLocalMin_Refine(vector<acv_XY> &sigMatchErr, vector<acv_XY> &minMatchErr)
{
  minMatchErr.resize(0);
  for (int i = 0; i < sigMatchErr.size(); i++)
  {
    acv_XY pre = sigMatchErr[valueWarping(i - 1, sigMatchErr.size())];

    acv_XY cur = sigMatchErr[i];

    acv_XY post = sigMatchErr[valueWarping(i + 1, sigMatchErr.size())];

    if (pre.y > cur.y && post.y > cur.y)
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

    tar.match_span(src, cAng.x - ScanW, cAng.x + ScanW, ScanCount, sigMatchErr, fineStride, flip);

    acv_XY minErr = sigMatchErr[0];
    for (int j = 1; j < sigMatchErr.size(); j++)
    {
      if (minErr.y > sigMatchErr[j].y)
      {
        minErr = sigMatchErr[j];
      }
    }
    minMatchErr[i] = minErr;
  }
}

// FFT-based circular normalized cross-correlation angle finder.
//
// Minimizing the brute-force SSD over rotation -- (1/N) sum_i (tmpl[off+i]-qry[i])^2
// -- is identical to MAXIMIZING the circular cross-correlation
//   c[k] = sum_i tmpl[i+k] * qry[i]
// (the sum-of-squares terms are shift-invariant). The DFT delivers c[] at ALL
// k at once in O(N log N): c = IDFT( DFT(tmpl) * conj(DFT(qry)) ), so there is
// no coarse stride that can step over the true peak. We mean-subtract first
// (normalized cross-correlation -> robust to overall part size / DC level),
// take the global peak, and parabola-interpolate for sub-bin precision.
//
//   tmpl, qry      : radius arrays length N. 'offset' convention matches
//                    SignatureMatchingError (tmpl[offset+i] vs qry[i]), so the
//                    returned degrees are directly comparable to minMatchErr.x.
//   flip           : back-facing -> mirror the query (qry[(N-i)%N]).
//   out_offset_deg : best offset in degrees (bin index, since N bins == 360 deg).
//   returns        : normalized correlation peak in [-1,1] (1 == identical shape).
static float matchAngleFFT(const std::vector<acv_XY> &tmpl,
                           const std::vector<acv_XY> &qry,
                           bool flip, float &out_offset_deg)
{
  int N = (int)tmpl.size();
  out_offset_deg = NAN;
  if (N < 4 || (int)qry.size() != N) return -1.0f;

  cv::Mat t(1, N, CV_32F), q(1, N, CV_32F);
  for (int i = 0; i < N; i++)
  {
    t.at<float>(i) = tmpl[i].x;
    int qi = flip ? ((N - i) % N) : i;
    q.at<float>(i) = qry[qi].x;
  }
  cv::Scalar mt = cv::mean(t), mq = cv::mean(q);
  t -= mt[0]; q -= mq[0];
  double nt = cv::norm(t), nq = cv::norm(q);
  if (nt < 1e-9 || nq < 1e-9) return -1.0f;

  cv::Mat Ft, Fq, X, c;
  cv::dft(t, Ft, cv::DFT_COMPLEX_OUTPUT);
  cv::dft(q, Fq, cv::DFT_COMPLEX_OUTPUT);
  cv::mulSpectrums(Ft, Fq, X, 0, /*conjB=*/true);            // Ft * conj(Fq)
  cv::dft(X, c, cv::DFT_INVERSE | cv::DFT_REAL_OUTPUT | cv::DFT_SCALE);
  // c[k] = sum_i tmpl[i+k]*qry[i]  -> k is the offset aligning tmpl onto qry.

  int kb = 0; float vb = c.at<float>(0);
  for (int k = 1; k < N; k++) { float v = c.at<float>(k); if (v > vb) { vb = v; kb = k; } }

  // Parabolic sub-bin interpolation around the (periodic) peak.
  float ym1 = c.at<float>((kb - 1 + N) % N), yp1 = c.at<float>((kb + 1) % N);
  float den = ym1 - 2.0f * vb + yp1;
  float d = (den != 0.0f) ? 0.5f * (ym1 - yp1) / den : 0.0f;
  if (d >  0.5f) d =  0.5f;
  if (d < -0.5f) d = -0.5f;

  out_offset_deg = (kb + d) * 360.0f / N;
  return (float)(vb / (nt * nq));   // normalized correlation peak
}

// Exact circular SSD curve at every integer offset, via one FFT correlation:
//   SSD[k] = (1/N)*( ||tar||^2 + ||src||^2 - 2*corr[k] ),  corr[k]=sum_i tar[i+k]*src[i]
// This is the SAME metric as SignatureMatchingError(stride=1), but evaluated at
// ALL N offsets at once in O(N log N) instead of a coarse sweep. `flip` mirrors
// tar (matching the reverse= path). Stores acv_XY(bin_index, ssd) so downstream
// code (which treats .x as a bin index == degrees for N=360) is unchanged.
static void fftSSDcurve(ContourSignature &tar, ContourSignature &src, bool flip,
                        std::vector<acv_XY> &ssd)
{
  int N = (int)tar.signature_data.size();
  ssd.resize(N);
  if (N < 4 || (int)src.signature_data.size() != N)
  {
    for (int k = 0; k < N; k++) ssd[k] = acv_XY((float)k, 0.f);
    return;
  }
  cv::Mat a(1, N, CV_32F), b(1, N, CV_32F);
  double aa = 0, bb = 0;
  for (int i = 0; i < N; i++)
  {
    float av = flip ? tar.signature_data[(N - i) % N].x : tar.signature_data[i].x;
    float bv = src.signature_data[i].x;
    a.at<float>(i) = av; b.at<float>(i) = bv;
    aa += (double)av * av; bb += (double)bv * bv;
  }
  cv::Mat Fa, Fb, X, corr;
  cv::dft(a, Fa, cv::DFT_COMPLEX_OUTPUT);
  cv::dft(b, Fb, cv::DFT_COMPLEX_OUTPUT);
  cv::mulSpectrums(Fa, Fb, X, 0, /*conjB=*/true);                 // Fa*conj(Fb)
  cv::dft(X, corr, cv::DFT_INVERSE | cv::DFT_REAL_OUTPUT | cv::DFT_SCALE);
  const float invN = 1.0f / N;
  for (int k = 0; k < N; k++)
    ssd[k] = acv_XY((float)k, (float)((aa + bb - 2.0 * (double)corr.at<float>(k)) * invN));
}

void xrefine(ContourSignature &tar, ContourSignature &src, float roughScanCount, vector<acv_XY> &minMatchErr, bool flip, int roughStride = 3, int fineStride = 1, float tarPrecision = 1)
{
  // FFT angle search (default): one full-resolution SSD curve, local minima as
  // candidates, parabolic sub-bin interpolation. Same metric as the brute-force
  // sweep but ~3-4x faster and it never steps over the true peak. The legacy
  // coarse sweep + minSegRefine is kept behind INSP_MATCH_BRUTEFORCE=1 for A/B
  // and rollback.
  if (!getenv("INSP_MATCH_BRUTEFORCE"))
  {
    // FFT finds the candidate angles at full resolution (no coarse stride can
    // step over the true peak); minSegRefine then does the sub-bin polish with
    // the EXACT interpolated metric -- identical to the brute-force refinement,
    // so .x/.y stay on the legacy scale (a parabola on the discrete SSD biased
    // the angle on near-degenerate self-matches).
    std::vector<acv_XY> ssd;
    fftSSDcurve(tar, src, flip, ssd);
    matchingErrorLocalMin_Refine(ssd, minMatchErr);          // local minima -> candidates
    float ScanW = 1.5f;                                       // candidates already at 1-deg res
    float ScanCount = 3;
    for (;;)
    {
      int doScanCount = ScanCount * 2;
      float prec = 2.0f * ScanW / doScanCount;
      minSegRefine(tar, src, ScanW, doScanCount, minMatchErr, flip, fineStride);
      if (prec < tarPrecision) break;
      ScanW /= ScanCount;
    }
    return;
  }

  // ---- legacy brute-force coarse sweep + local refine (INSP_MATCH_BRUTEFORCE=1) ----
  vector<acv_XY> sigMatchErr(360);
  tar.match_span(src, 0, 360, roughScanCount, sigMatchErr, roughStride, flip);
  matchingErrorLocalMin_Refine(sigMatchErr, minMatchErr);
  float ScanW = 1.5 * 360 / roughScanCount;
  float ScanCount = 3;
  for (int i = 0;; i++)
  {
    int doScanCount = ScanCount * 2;
    float prec = 2.0 * ScanW / doScanCount;
    minSegRefine(tar, src, ScanW, doScanCount, minMatchErr, flip, fineStride); //over scan
    if (prec < tarPrecision)
    {
      break;
    }
    ScanW /= ScanCount; //shrink the scan width-> improve precision
  }
}

// v2 centroid-iter wrapper around xrefine. After each xrefine call, picks the
// global-min angle, computes residual e(theta) = r_q - r_t(theta + best_angle),
// solves closed-form (dx, dy) = mean(e*cos), mean(e*sin) (scaled), updates the
// sample center, re-samples tmp_sig from the new center, repeats. Final
// minMatchErr is the result of the last xrefine call.
//   center_io   : initial centroid in (mm, ideal coord). Updated to final value.
//   max_iter    : centroid-iter passes (typical 3-4 enough).
//   tol_mm      : break when |(dx, dy)| < tol_mm.
//   ret_score   : (optional) FFT-correlation-style score of best match on final iter.
void xrefine_v2_centroidIter(ContourSignature &tar, ContourSignature &src,
                             float roughScanCount, vector<acv_XY> &minMatchErr,
                             bool flip, int roughStride, int fineStride, float tarPrecision,
                             acv_XY &center_io, int max_iter, float tol_mm,
                             float *ret_score)
{
  if (src.cartesian_ideal.empty())
  {
    // v2 requested but cartesian wasn't populated -- fall back to v1 behavior.
    xrefine(tar, src, roughScanCount, minMatchErr, flip, roughStride, fineStride, tarPrecision);
    if (ret_score) *ret_score = NAN;
    return;
  }

  // Experiment toggle: INSP_V2_NO_CENTROID=1 keeps the v2 morph-boundary dual-sig
  // signature but matches it with the plain fixed-center xrefine (no centroid
  // iteration) -- i.e. isolate "what does the v2 signature alone buy us?".
  if (getenv("INSP_V2_NO_CENTROID"))
  {
    xrefine(tar, src, roughScanCount, minMatchErr, flip, roughStride, fineStride, tarPrecision);
    if (ret_score) { float b = FLT_MAX; for (auto &mm : minMatchErr) if (mm.y < b) b = mm.y; *ret_score = b; }
    return;
  }

  const int N = (int)src.signature_data.size();
  const float dtheta = 2.0f * M_PI / N;
  acv_XY center = center_io;

  // Lobe lock: which rotational lobe (orientation) is decided ONCE by the first,
  // fixed-center xrefine -- byte-identical to v1, so robust to a part's near-
  // rotational symmetry. Centroid iteration then only REFINES within that lobe.
  // Without this, each iter's full 0..360 xrefine can re-select a symmetric lobe
  // and the LSQ centroid update reinforces it -> converges to a wrong-but-low-
  // residual orientation (e.g. golden self-match landing 86 deg off). The band
  // must be < half the smallest expected symmetry period; legitimate centroid
  // refinement moves the angle only a few degrees, so 20 deg is safe.
  bool  locked = false;
  float lockAngleDeg = 0.0f;
  const float lobeBandDeg = 20.0f;

  const acv_XY center0 = center_io;       // CCL centroid: the trusted anchor.
  std::vector<acv_XY> minMatchErr_iter0;  // v1-equivalent result -> guaranteed fallback.
  float prevStep = FLT_MAX;               // for divergence detection.
  float maxDrift = 0.0f;                   // centroid wander cap (set after iter 0).

  for (int it = 0; it < max_iter; it++)
  {
    // iter 0 reuses the signature already built by convertContourGrid2Signature
    // so the first xrefine is byte-identical to v1. Subsequent iters resample
    // from the LSQ-corrected center.
    if (it > 0)
    {
      src.resampleFromCenter(center);
      // Match the legacy pipeline: convertContourGrid2Signature -> *=mm conv
      // -> sign360_process (which applies SignatureSoften with the known
      // window^2 attenuation). The template (feature_signature) is already
      // softened once at load, so we must apply the same processing here for
      // the 1D correlation to be apples-to-apples.
      std::vector<acv_XY> _dummy_buf;
      sign360_process(src.signature_data, _dummy_buf);
      src.CalcInfo();
    }

    xrefine(tar, src, roughScanCount, minMatchErr, flip, roughStride, fineStride, tarPrecision);

    if (it == 0)
    {
      minMatchErr_iter0 = minMatchErr;        // == v1 result (fixed center): fallback.
      maxDrift = 0.2f * src.mean;             // cap centroid wander to ~1/5 mean radius.
      if (!(maxDrift > 0)) maxDrift = 1.0f;   // mm floor if mean is degenerate.
    }

    // Pick the best refinement among local minima. iter 0: global min (locks the
    // lobe, == v1). Later iters: best min that stays WITHIN the locked lobe band,
    // so the centroid update can't be computed against a jumped symmetric lobe.
    int best_idx = -1; float best_err = FLT_MAX;
    for (size_t i = 0; i < minMatchErr.size(); i++)
    {
      if (locked)
      {
        float dd = minMatchErr[i].x - lockAngleDeg;
        while (dd > 180) dd -= 360; while (dd < -180) dd += 360;
        if (fabsf(dd) > lobeBandDeg) continue;   // outside the locked lobe
      }
      if (minMatchErr[i].y < best_err) { best_err = minMatchErr[i].y; best_idx = (int)i; }
    }
    if (best_idx < 0) break;
    if (!locked) { lockAngleDeg = minMatchErr[best_idx].x; locked = true; }  // lock after iter 0
    float best_angle = minMatchErr[best_idx].x * (float)(M_PI / 180.0);

    // Closed-form (dx, dy): residual e(theta) at the matched angle, project
    // onto cos/sin. With uniform sampling the normal equations diagonalize.
    int kshift = (int)round(best_angle / dtheta);
    float sum_ec = 0, sum_es = 0;
    for (int i = 0; i < N; i++)
    {
      int j = (i + kshift) % N; if (j < 0) j += N;
      float r_q = src.signature_data[i].x;
      float r_t = flip
                  ? tar.signature_data[(N - j) % N].x
                  : tar.signature_data[j].x;
      float e = r_q - r_t;
      float th = (float)(2.0 * M_PI * i / N);
      sum_ec += e * cosf(th);
      sum_es += e * sinf(th);
    }
    // (dx, dy) = (2/N) * sum(e * cos), (2/N) * sum(e * sin)
    // Sign: model residual ~ -dx cos - dy sin so center update: c_new = c - (dx, dy).
    // But by derivation in lab notes: e ~ -Delta_x cos - Delta_y sin (linearized),
    // where Delta = (sample_center - true_center). LSQ recovers Delta. So:
    //   true_center = sample_center - Delta
    float Delta_x = -(2.0f / N) * sum_ec;
    float Delta_y = -(2.0f / N) * sum_es;
    acv_XY new_center;
    new_center.x = center.x - Delta_x;
    new_center.y = center.y - Delta_y;
    float step = hypotf(Delta_x, Delta_y);
    // Reject a runaway centroid: it must stay near the trusted CCL centroid and
    // the steps must shrink. Either guard tripping => the LSQ is diverging (the
    // resampled signature is being pulled toward a wrong-lobe fit); stop and keep
    // the last good center.
    float totalDrift = hypotf(new_center.x - center0.x, new_center.y - center0.y);
    if (totalDrift > maxDrift || step > prevStep * 1.5f) break;
    prevStep = step;
    center = new_center;
    if (step < tol_mm) break;
  }

  // Final pass with the converged center if we actually shifted it.
  if (hypotf(center.x - center_io.x, center.y - center_io.y) > 1e-7f)
  {
    src.resampleFromCenter(center);
    std::vector<acv_XY> _dummy_buf;
    sign360_process(src.signature_data, _dummy_buf);
    src.CalcInfo();
    xrefine(tar, src, roughScanCount, minMatchErr, flip, roughStride, fineStride, tarPrecision);
  }
  // Fence off out-of-lobe candidates so the caller's own global-min selection
  // can't jump to a symmetric lobe this iteration already rejected. The final
  // xrefine above ran a fresh 0..360 scan, so its minMatchErr may again contain
  // the competing lobes.
  bool haveValid = !locked;   // if never locked, all candidates are valid.
  if (locked)
  {
    for (acv_XY &mm : minMatchErr)
    {
      float dd = mm.x - lockAngleDeg;
      while (dd > 180) dd -= 360; while (dd < -180) dd += 360;
      if (fabsf(dd) > lobeBandDeg) mm.y = FLT_MAX;        // reject (never chosen as min)
      else if (mm.y < FLT_MAX)     haveValid = true;      // an in-lobe survivor
    }
  }

  // Never-worse-than-v1 guarantee: if centroid iteration diverged so that no
  // in-lobe candidate survived, fall back to the iter-0 (fixed-center == v1)
  // result and its center.
  if (!haveValid && !minMatchErr_iter0.empty())
  {
    minMatchErr = minMatchErr_iter0;
    center = center0;
  }
  center_io = center;

  if (ret_score)
  {
    float best_err = FLT_MAX;
    for (auto &mm : minMatchErr) if (mm.y < best_err) best_err = mm.y;
    *ret_score = best_err;
  }
}

inline float angle_0_to_2pi(float ang)
{
  float _ang = fmod(ang, 2 * M_PI);
  return (_ang < 0) ? _ang + 2 * M_PI : _ang;
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


float SigMatchErrorNormalize(float error,ContourSignature &sig)
{
  return (sqrt(error) / sig.mean);
  // if()
  // return (sqrt(error) / sig.sigma);
}

//normalized_thres default value is obtained threshold by test no absolute reason
// bool isErrPass(float error,ContourSignature &sig,float normalized_thres=0.1)
// {
//   float errThres2nd=(error / sig.mean);
  
//   LOGI("error:%f sig.mean:%f errThres2nd:%f",error,sig.mean,errThres2nd);
//   if ((errThres2nd > normalized_thres))
//   {
//     return false;
//   }

//   return true;

// }

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
  // include_range/manual_offset stored in def-mm; convert to px so
  // searchPoint_process can use them directly.
  spoint.include_range /= mmpp;
  spoint.manual_offset /= mmpp;
  acv_XY pos_raw = spoint.data.anglefollow.position;
  spoint.data.anglefollow.position=cm.convert(spoint.data.anglefollow.position);
  acv_XY pos_cm = spoint.data.anglefollow.position;

  if (spoint.subtype == featureDef_searchPoint::anglefollow)
  {

    spoint.data.anglefollow.position =
        TemplateDomain_TO_PixDomain(spoint.data.anglefollow.position,
                                    cached_sin, cached_cos, flip_f, calibCen, mmpp);
  }
  if (getenv("SP_PT_DUMP"))
    fprintf(stderr, "[SPPT] id=%d posRaw=(%.3f,%.3f) afterCM=(%.3f,%.3f) afterPose(px)=(%.2f,%.2f) calibCen=(%.1f,%.1f) sin=%.4f cos=%.4f flip=%.0f mmpp=%.6f\n",
            def->id, pos_raw.x, pos_raw.y, pos_cm.x, pos_cm.y,
            spoint.data.anglefollow.position.x, spoint.data.anglefollow.position.y,
            calibCen.x, calibCen.y, cached_sin, cached_cos, flip_f, mmpp);

  FeatureReport_searchPointReport report = searchPoint_process(singleReport, calibCen, cached_sin, cached_cos, flip_f, 0, spoint, eT);

  report.pt =
      acvVecMult(report.pt, mmpp);
  // Convert caliper-mode per-edge hits from image-px to OBJECT-FRAME mm so
  // the WebUI can overlay them at the def's own pt1 in editor mode (matches
  // line/arc convention; see CORE0_1_CAVEATS.md §H2 / §H4).
  for (auto &h : report.cal_hits) {
    h.pt = PixDomain_TO_TemplateDomain(h.pt, cached_sin, cached_cos, flip_f, calibCen, mmpp);
  }
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
  // Morph effectiveness test: report how well the morphed caliper points track
  // the ground-truth deformed position q + D(q). off_morph = |predicted-true|,
  // off_nomorph = |D(q)|; if off > caliper margin the feature is out of scope.
  if (synth_field_enabled()) {
    acv_XY q[3]   = {cdef.pt1, cdef.pt2, cdef.pt3};
    acv_XY pr[3]  = {m_pt1, m_pt2, m_pt3};
    const char *nm[3] = {"pt1", "pt2", "pt3"};
    for (int t = 0; t < 3; t++) {
      acv_XY d = synth_morph_field(q[t]);
      float truex = q[t].x + d.x, truey = q[t].y + d.y;
      float offm = sqrtf((pr[t].x-truex)*(pr[t].x-truex)+(pr[t].y-truey)*(pr[t].y-truey));
      float offn = sqrtf(d.x*d.x + d.y*d.y);
      fprintf(stderr, "[MORPHTEST] arc id=%d %s raw=(%.3f,%.3f) field=(%.3f,%.3f) pred=(%.3f,%.3f) off_nomorph=%.4f off_morph=%.4f\n",
              cdef.id, nm[t], q[t].x, q[t].y, d.x, d.y, pr[t].x, pr[t].y, offn, offm);
    }
  }

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
  //               center.x, center.y,
  //               4, 255,255,255);
  //LOGV("X:%f Y:%f r(%f)*ppmm(%f)=r(%f)",center.x,center.y,cdef.circleTar.radius,ppmm,radius);
  // Caliper path skips the contour-grid pre-scan: caliper_locate_circle does
  // its own image-space radial sweep and never reads m_sections.  The
  // maxD/minD/roughness block below already produced NAN when m_sections was
  // empty, so the report contract is unchanged for caliper consumers.
  if (cdef.locating != 1)
  {
    edge_grid.getContourPointsWithInCircleContour(
        center.x,
        center.y,
        radius,
        sAngle, eAngle, cdef.outter_inner,
        matching_tor, m_sections);
  }
  else
  {
    m_sections.clear();
  }

  LOGV("edge_grid:%p flip_f:%f radius:%f sAngle:%f  eAngle:%f   XY:%f,%f ppmm:%f mmpp:%f ",&edge_grid, flip_f, radius, sAngle, eAngle,center.x,
  center.y,ppmm,mmpp);
  LOGV("matching_tor:%d initMatchingMargin:%f m_sections.size:%d",
  matching_tor,cdef.initMatchingMargin,m_sections.size());

  acv_CircleFit cf = {};
  FeatureReport_circleReport cr;

  cdef.tmp_pt.clear();
  cr.pt1 = cr.pt2 = cr.pt3 = acv_XY(NAN, NAN);


  if (m_sections.size() == 0 && cdef.locating != 1)
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
    //   int X = round(p.x);
    //   int Y = round(p.y);
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
  //   LOGI("XY:%f %f  >> %f %f   edgeRsp:%f",s_points[m].sobel.x,s_points[m].sobel.y,s_points[m].pt.x,s_points[m].pt.y,s_points[m].edgeRsp);
  // }

  // Points that drove the fit, in image-px (same frame as cf.circle.circumcenter).
  // Populated by either branch below; used afterward for envelope fit modes.
  std::vector<acv_XY> envelope_pts;
  if (cdef.locating == 1) // caliper/section circle fit (radial calipers)
  {
    CaliperParams cal;
    // Caliper sub-params come in as mm (def-file unit, matches the rest of
    // the schema) and are consumed here in px. initMatchingMargin above
    // already crossed that boundary via `* ppmm`; do the same for the
    // caliper params. Sentinels (cal_length=-1, cal_step=-1) pass through.
    cal.length = (cdef.cal_length > 0) ? (cdef.cal_length * ppmm) : initMatchingMargin; // px radial search half-length
    cal.width  = cdef.cal_width * ppmm;
    cal.step   = (cdef.cal_step > 0) ? (cdef.cal_step * ppmm) : 1.0f;
    cal.min_inliers = cdef.cal_min_inliers;
    cal.max_error   = (cdef.cal_max_error > 0) ? (cdef.cal_max_error * ppmm) : 0;
    cal.edge.method       = cdef.edge_method;
    cal.edge.polarity     = cdef.edge_polarity;
    cal.edge.nth          = cdef.edge_nth;
    cal.edge.min_strength = cdef.edge_min_strength;
    acv_XY off = eT.getImgOffset();
    acv_XY cc = acvVecSub(center, off);
    CaliperCircleResult rr;
    if (getenv("INSP_CALIPER_PASSTHROUGH"))
    {
      int n = std::max(1, cdef.cal_count);
      rr.ok = true;
      rr.hits.reserve(n);
      for (int i = 0; i < n; i++) {
        float t = (n == 1) ? 0.5f : (float)i / (float)(n - 1);
        float a = sAngle + (eAngle - sAngle) * t;
        CaliperHit h{};
        h.pt = { cc.x + radius * std::cos(a), cc.y + radius * std::sin(a) };
        h.status = 2; h.strength = 0;
        rr.hits.push_back(h);
      }
      rr.center = cc; rr.radius = radius;
      rr.rms = 0; rr.nInlier = n; rr.confidence = 1.0f;
    }
    else
    {
      rr = caliper_locate_circle(eT.getImageCv(), cc, radius, sAngle, eAngle,
                                 cdef.cal_count, cal, eT.getBacpac(), cdef.name, off);
    }
    if (rr.ok) { cf.circle.circumcenter = acvVecAdd(rr.center, off); cf.circle.radius = rr.radius;
                 cf.s = rr.rms; cf.matching_pts = rr.nInlier; cf.confidence = rr.confidence; }
    else { cf.circle.radius = NAN; }
    // Per-caliper hits → cr.cal_hits directly in OBJECT-FRAME mm. Convert
    // here at push time (not in the later mm-conversion block) because the
    // function has early-return paths between here and that block (e.g. when
    // the fit produces a NaN radius) — those would leave cal_hits in raw
    // image-px and break the WebUI's object-frame overlay assumption.
    // All hits (including missed) carry valid coords — Caliper.cpp stashes
    // the caliper's nominal anchor on miss.
    cr.cal_hits.reserve(rr.hits.size());
    std::vector<acv_XY> dbg_pix_hits;  // image-absolute px, for debug overlay
    std::vector<int>    dbg_pix_st;
    for (const auto &h : rr.hits) {
      CaliperHit ih = h;
      acv_XY pix_pt = acvVecAdd(h.pt, off);   // image-relative → image-absolute px
      ih.pt = PixDomain_TO_TemplateDomain(pix_pt, cached_sin, cached_cos, flip_f, calibCen, mmpp);
      cr.cal_hits.push_back(ih);
      if (h.status == 2) envelope_pts.push_back(pix_pt);
      dbg_pix_hits.push_back(pix_pt);
      dbg_pix_st.push_back(h.status);
    }
    // Debug dump: cropped 4x view of the caliper search arc + hits + fitted
    // circle, identical pattern to LineMatching_caliper's overlay.
    if (getenv("INSP_DUMP_CALIPER_DEBUG"))
    {
      const cv::Mat &src = eT.getImageCv();
      if (!src.empty() && !dbg_pix_hits.empty()) {
        acv_XY ccPix = acvVecAdd(cc, off);
        double minX = ccPix.x - radius, maxX = ccPix.x + radius;
        double minY = ccPix.y - radius, maxY = ccPix.y + radius;
        for (const auto &p : dbg_pix_hits) {
          if (p.x < minX) minX = p.x; if (p.x > maxX) maxX = p.x;
          if (p.y < minY) minY = p.y; if (p.y > maxY) maxY = p.y;
        }
        const int margin = 30;
        int x0 = std::max(0, (int)std::floor(minX) - margin);
        int y0 = std::max(0, (int)std::floor(minY) - margin);
        int x1 = std::min(src.cols, (int)std::ceil(maxX) + margin);
        int y1 = std::min(src.rows, (int)std::ceil(maxY) + margin);
        if (x1 > x0 && y1 > y0) {
          cv::Mat roi = src(cv::Rect(x0, y0, x1 - x0, y1 - y0)).clone();
          cv::Mat dbg;
          if (roi.channels() == 1) cv::cvtColor(roi, dbg, cv::COLOR_GRAY2BGR);
          else                     dbg = roi;
          const int scale = 4;
          cv::Mat up;
          cv::resize(dbg, up, cv::Size(dbg.cols * scale, dbg.rows * scale), 0, 0, cv::INTER_NEAREST);
          auto mapPt = [&](const acv_XY &p) {
            return cv::Point2f((float)((p.x - x0) * scale), (float)((p.y - y0) * scale));
          };
          auto drawCross = [&](cv::Point2f c, cv::Scalar col) {
            cv::line(up, cv::Point(c.x - 10, c.y), cv::Point(c.x + 10, c.y), col, 2, cv::LINE_AA);
            cv::line(up, cv::Point(c.x, c.y - 10), cv::Point(c.x, c.y + 10), col, 2, cv::LINE_AA);
          };
          // Red nominal search arc (from sAngle to eAngle at given radius around ccPix).
          int N = 64;
          for (int i = 0; i < N; i++) {
            float a0 = sAngle + (eAngle - sAngle) * i / N;
            float a1 = sAngle + (eAngle - sAngle) * (i + 1) / N;
            acv_XY p0 = { ccPix.x + radius * std::cos(a0), ccPix.y + radius * std::sin(a0) };
            acv_XY p1 = { ccPix.x + radius * std::cos(a1), ccPix.y + radius * std::sin(a1) };
            cv::line(up, mapPt(p0), mapPt(p1), cv::Scalar(0,0,255), 1, cv::LINE_AA);
          }
          // Hit crosses.
          for (size_t i = 0; i < dbg_pix_hits.size(); i++) {
            cv::Scalar col = (dbg_pix_st[i] == 2) ? cv::Scalar(0,255,0) : cv::Scalar(140,140,140);
            drawCross(mapPt(dbg_pix_hits[i]), col);
          }
          // Cyan fitted circle.
          if (rr.ok && cf.circle.radius == cf.circle.radius) {
            for (int i = 0; i < 128; i++) {
              float a0 = (float)(2*M_PI * i / 128);
              float a1 = (float)(2*M_PI * (i + 1) / 128);
              acv_XY p0 = { cf.circle.circumcenter.x + cf.circle.radius * std::cos(a0),
                            cf.circle.circumcenter.y + cf.circle.radius * std::sin(a0) };
              acv_XY p1 = { cf.circle.circumcenter.x + cf.circle.radius * std::cos(a1),
                            cf.circle.circumcenter.y + cf.circle.radius * std::sin(a1) };
              cv::line(up, mapPt(p0), mapPt(p1), cv::Scalar(255,200,0), 1, cv::LINE_AA);
            }
          }
          char path[256];
          snprintf(path, sizeof(path), "/tmp/caliper_dbg_circle_%s.png",
                   (cdef.name[0] == '\0') ? "unnamed" : cdef.name);
          for (char *p = path + strlen("/tmp/caliper_dbg_circle_"); *p; p++)
            if (*p == '/' || *p == ' ') *p = '_';
          try { cv::imwrite(path, up); LOGI("caliper dbg dumped: %s (crop %dx%d ->4x)",
                                            path, x1 - x0, y1 - y0); }
          catch (const cv::Exception &ex) { LOGE("caliper dbg imwrite: %s", ex.what()); }
        }
      }
    }
  }
  else {
    circleRefine(s_points, s_points.size(), &cf);
    envelope_pts.reserve(s_points.size());
    for (const auto &p : s_points) envelope_pts.push_back(p.pt);
  }

  float minTor = matching_tor / 2;
  if (minTor < 1)
    minTor = 1;

  // Envelope fit (LS-center variant): keep cf.circle.circumcenter from the
  // LS fit, replace cf.circle.radius with max (outer) or min (inner) of
  // |center - pt| over the points that drove the fit. Same in both caliper
  // and contour modes; `envelope_pts` carries them in image-px.
  // No-op when fit_mode == 0, no points, or LS produced NaN.
  if (cdef.fit_mode != 0 && cf.circle.radius == cf.circle.radius && !envelope_pts.empty()) {
    const acv_XY cc = cf.circle.circumcenter;
    float rExt = (cdef.fit_mode == 2) ? FLT_MAX : 0.f;
    for (const auto &p : envelope_pts) {
      float dx = p.x - cc.x, dy = p.y - cc.y;
      float d  = sqrtf(dx*dx + dy*dy);
      if (cdef.fit_mode == 1) { if (d > rExt) rExt = d; }   // outer = max
      else                    { if (d < rExt) rExt = d; }   // inner = min
    }
    cf.circle.radius = rExt;
  }

  // LOGI("s_points.size():%d  r:%f", s_points.size(),cf.circle.radius);
  cr.circle = cf;
  cr.def = &cdef;

  // LOGI("C=%d===R:%f,pt:%f,%f , tarR:%f",
  //      j, cf.circle.radius,cf.circle.circumcenter.x ,cf.circle.circumcenter.y,radius);

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
  //                 center.x, center.y,
  //                 3, 3);

  //   acvDrawCrossX(originalImage,
  //                 cf.circle.circumcenter.x, cf.circle.circumcenter.y,
  //                 5, 3);

  //   acvDrawCircle(originalImage,
  //                 cf.circle.circumcenter.x, cf.circle.circumcenter.y,
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
      // LOGI("pt:%0.2f,%0.2f,center:%0.2f,%0.2f",_pt.x,_pt.y,center.x,center.y);
      acv_XY pt = acvVecSub(_pt.pt, center);
      float theta_deg = atan2(pt.y, pt.x) * 180 / M_PI;
      if (theta_deg < 0)
      {
        theta_deg += 360;
      }
      int thdegInt = (int)(theta_deg * angleRes / 360);
      if (thdegInt >= angleRes)
        thdegInt -= angleRes;

      float mag = hypot(pt.y, pt.x);
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
    LOGV("C===maxD:%f minD:%f", cr.maxD, cr.minD);

    for (auto pt : s_points)
    {
      cdef.tmp_pt.push_back(pt);
    }
    float rMIN, rMAX, rRMSE;
    roughness(cdef.tmp_pt, 5, &rMIN, &rMAX, &rRMSE);
    cr.roughness_MIN = rMIN;
    cr.roughness_MAX = rMAX;
    cr.roughness_RMSE = rRMSE;
    LOGV("C===R:%f,pt:%f,%f , tarR:%f, rMIN:%f ,rMAX:%f ,rRMSE:%f",
          cf.circle.radius, cf.circle.circumcenter.x, cf.circle.circumcenter.y, radius, rMIN, rMAX, rRMSE);
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
    // (cal_hits already in OBJECT-FRAME mm — converted at push-time above.)
  }




  return cr;
}

// Caliper/section line locating (docs/caliper_primitive_locating_design.md).
// lineDef.p0/p1 are in image px; initMatchingMargin already in px. Returns a
// px-unit report (caller converts to mm), matching the contour path's contract.
static FeatureReport_lineReport LineMatching_caliper(featureDef_line &lineDef, edgeTracking &eT, float flip_f)
{
  // Value-init (was memset — now UB because FeatureReport_lineReport carries
  // a std::vector<CaliperHit> cal_hits member).
  FeatureReport_lineReport Report = {};
  Report.status = FeatureReport_sig360_circle_line_single::STATUS_NA;

  // Flipped object: the def's p0->p1 ordering puts perp = rot90(p1-p0) on the
  // wrong side of the line in image space, breaking polarity (gradient sign)
  // and FIRST/LAST (scan order). Reversing p0/p1 reverses perp and the scan
  // range together, restoring both. The output report's line_vec and
  // end_pt1/end_pt2 are flipped back at the bottom so callers always see the
  // original p0->p1 convention.
  acv_XY p0_orig = lineDef.p0, p1_orig = lineDef.p1;
  if (flip_f < 0) std::swap(lineDef.p0, lineDef.p1);

  CaliperParams cal;
  cal.length = (lineDef.cal_length > 0) ? lineDef.cal_length : lineDef.initMatchingMargin; // px search half-length across edge
  cal.width  = lineDef.cal_width;
  cal.step   = (lineDef.cal_step > 0) ? lineDef.cal_step : 1.0f;
  cal.min_inliers = lineDef.cal_min_inliers;
  cal.max_error   = lineDef.cal_max_error;  // already mm→px via LineMatching_ReportGen
  cal.edge.method       = lineDef.edge_method;
  cal.edge.polarity     = lineDef.edge_polarity;
  cal.edge.nth          = lineDef.edge_nth;
  cal.edge.min_strength = lineDef.edge_min_strength;

  acv_XY off = eT.getImgOffset();
  acv_XY p0 = acvVecSub(lineDef.p0, off);
  acv_XY p1 = acvVecSub(lineDef.p1, off);
  CaliperLineResult r;
  if (getenv("INSP_CALIPER_PASSTHROUGH"))
  {
    // Debug bypass: don't run edge search; emit the nominal caliper centers
    // (evenly along p0→p1) as the hits. Useful for visualising "where the
    // caliper boxes sit" vs the actual edge, isolated from any matching
    // strength / polarity behaviour.
    int n = std::max(1, lineDef.cal_count);
    r.ok = true;
    r.hits.reserve(n);
    for (int i = 0; i < n; i++) {
      float t = (n == 1) ? 0.5f : (float)i / (float)(n - 1);
      CaliperHit h{};
      h.pt = { p0.x + (p1.x - p0.x) * t, p0.y + (p1.y - p0.y) * t };
      h.status = 2;
      h.strength = 0;
      r.hits.push_back(h);
    }
    // Synthesize a fit along p0->p1 so the downstream report doesn't NaN out.
    r.anchor = { (p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f };
    acv_XY d = { p1.x - p0.x, p1.y - p0.y };
    float L = std::sqrt(d.x*d.x + d.y*d.y);
    r.dir = (L > 0) ? acv_XY{ d.x / L, d.y / L } : acv_XY{ 1.f, 0.f };
    r.rms = 0;
    r.nInlier = n;
    r.confidence = 1.0f;
  }
  else
  {
    r = caliper_locate_line(eT.getImageCv(), p0, p1, lineDef.cal_count, cal, eT.getBacpac(), lineDef.name, off);
  }
  if (r.ok)
  {
    // Orient the fitted direction to the def's p0->p1 convention (the TLS fit's
    // sign is arbitrary). Without this the line vector can come out flipped vs the
    // legacy/contour path, which flips dependent search points' search angle and
    // shifts their measures. Mirrors SingleMatching_line's target_vec dot-flip.
    acv_XY tdir = acvVecSub(lineDef.p1, lineDef.p0);
    if (r.dir.x * tdir.x + r.dir.y * tdir.y < 0) { r.dir.x = -r.dir.x; r.dir.y = -r.dir.y; }

    acv_XY anchor = acvVecAdd(r.anchor, off); // back to image coords
    Report.line.line.line_anchor = anchor;
    Report.line.line.line_vec = r.dir;
    Report.line.s = r.rms;
    Report.line.matching_pts = r.nInlier;
    Report.line.confidence = r.confidence;
    float t0 = (lineDef.p0.x-anchor.x)*r.dir.x + (lineDef.p0.y-anchor.y)*r.dir.y;
    float t1 = (lineDef.p1.x-anchor.x)*r.dir.x + (lineDef.p1.y-anchor.y)*r.dir.y;
    Report.line.end_pt1 = acv_XY(anchor.x + t0*r.dir.x, anchor.y + t0*r.dir.y);
    Report.line.end_pt2 = acv_XY(anchor.x + t1*r.dir.x, anchor.y + t1*r.dir.y);
    // Report.status follows r.ok which is gated by the configurable
    // cal.min_inliers (default 2). Previously hardcoded `>= 5` which made
    // even a perfectly-fittable 3- or 4-inlier line fail unnecessarily.
    if (r.ok) Report.status = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
  }
  // Per-caliper hits → Report.cal_hits (rebased to image coords). All hits
  // are copied (including missed ones, which carry the caliper's nominal
  // anchor in pt — Caliper.cpp stashes it so the WebUI can show where
  // each miss happened).
  Report.cal_hits.reserve(r.hits.size());
  for (const auto &h : r.hits) {
    CaliperHit ih = h;
    ih.pt = acvVecAdd(h.pt, off);
    Report.cal_hits.push_back(ih);
  }
  // Debug: dump the caliper input image with the search line + hit dots
  // overlaid so the user can eyeball whether the green Xs they see in the
  // WebUI match what the caliper actually returned in image-pixel space.
  // Triggered only when env var INSP_DUMP_CALIPER_DEBUG is set (any value).
  if (getenv("INSP_DUMP_CALIPER_DEBUG"))
  {
    const cv::Mat &src = eT.getImageCv();
    if (!src.empty()) {
      // Bbox over search line + hits + fitted endpoints, then 20px margin.
      double minX = std::min(lineDef.p0.x, lineDef.p1.x);
      double maxX = std::max(lineDef.p0.x, lineDef.p1.x);
      double minY = std::min(lineDef.p0.y, lineDef.p1.y);
      double maxY = std::max(lineDef.p0.y, lineDef.p1.y);
      for (const auto &h : Report.cal_hits) {
        if (h.pt.x < minX) minX = h.pt.x; if (h.pt.x > maxX) maxX = h.pt.x;
        if (h.pt.y < minY) minY = h.pt.y; if (h.pt.y > maxY) maxY = h.pt.y;
      }
      const int margin = 30;
      int x0 = std::max(0, (int)std::floor(minX) - margin);
      int y0 = std::max(0, (int)std::floor(minY) - margin);
      int x1 = std::min(src.cols, (int)std::ceil(maxX) + margin);
      int y1 = std::min(src.rows, (int)std::ceil(maxY) + margin);
      if (x1 <= x0 || y1 <= y0) goto dbg_skip;
      cv::Mat roi = src(cv::Rect(x0, y0, x1 - x0, y1 - y0)).clone();
      cv::Mat dbg;
      if (roi.channels() == 1) cv::cvtColor(roi, dbg, cv::COLOR_GRAY2BGR);
      else                     dbg = roi;
      // 4x nearest-neighbor so individual pixels stay sharp under the marks.
      const int scale = 4;
      cv::Mat up;
      cv::resize(dbg, up, cv::Size(dbg.cols * scale, dbg.rows * scale), 0, 0, cv::INTER_NEAREST);
      auto mapPt = [&](const acv_XY &p) {
        return cv::Point2f((float)((p.x - x0) * scale), (float)((p.y - y0) * scale));
      };
      auto drawCross = [&](cv::Mat &im, cv::Point2f c, int sz, cv::Scalar col, int thick) {
        cv::line(im, cv::Point(c.x - sz, c.y), cv::Point(c.x + sz, c.y), col, thick, cv::LINE_AA);
        cv::line(im, cv::Point(c.x, c.y - sz), cv::Point(c.x, c.y + sz), col, thick, cv::LINE_AA);
      };
      // Red search line.
      cv::line(up, mapPt(lineDef.p0), mapPt(lineDef.p1), cv::Scalar(0,0,255), 1, cv::LINE_AA);
      // Hits: green cross (success) / gray cross (missed).
      for (const auto &h : Report.cal_hits) {
        cv::Scalar col = (h.status == 2) ? cv::Scalar(0,255,0) : cv::Scalar(140,140,140);
        drawCross(up, mapPt(h.pt), 10, col, 2);
      }
      // Fitted line in cyan.
      if (Report.status == FeatureReport_sig360_circle_line_single::STATUS_SUCCESS)
        cv::line(up, mapPt(Report.line.end_pt1), mapPt(Report.line.end_pt2),
                 cv::Scalar(255,200,0), 1, cv::LINE_AA);
      char path[256];
      snprintf(path, sizeof(path), "/tmp/caliper_dbg_line_%s.png",
               (lineDef.name[0] == '\0') ? "unnamed" : lineDef.name);
      for (char *p = path + strlen("/tmp/caliper_dbg_line_"); *p; p++)
        if (*p == '/' || *p == ' ') *p = '_';
      try { cv::imwrite(path, up); LOGI("caliper dbg dumped: %s (crop %dx%d ->4x)",
                                        path, x1 - x0, y1 - y0); }
      catch (const cv::Exception &ex) { LOGE("caliper dbg imwrite: %s", ex.what()); }
    }
    dbg_skip:;
  }
  // Restore lineDef p0/p1 to the original convention (we swapped them above
  // when flip_f<0) and flip the outputs derived from the swapped ordering so
  // callers see line_vec / end_pt1 / end_pt2 in the original p0->p1 frame.
  if (flip_f < 0) {
    lineDef.p0 = p0_orig; lineDef.p1 = p1_orig;
    Report.line.line.line_vec.x = -Report.line.line.line_vec.x;
    Report.line.line.line_vec.y = -Report.line.line.line_vec.y;
    std::swap(Report.line.end_pt1, Report.line.end_pt2);
  }
  return Report;
}

FeatureReport_lineReport FeatureManager_sig360_circle_line::LineMatching_ReportGen(
  featureDef_line *plineDef,edgeTracking &eT,
  acv_XY calibCen,float mmpp,float cached_cos,float cached_sin,float flip_f)
{
  featureDef_line lineDef=*plineDef;

  // LOGI("p0:%f %f , p1:%f %f",line.p0.x,line.p0.y,line.p1.x,line.p1.y);

  acv_XY l0_raw = lineDef.p0, l1_raw = lineDef.p1;
  lineDef.p0=cm.convert(lineDef.p0);
  lineDef.p1=cm.convert(lineDef.p1);
  // Morph effectiveness test (window follow): does the caliper window rotate/track
  // the deformed edge? Compares the morphed endpoints + line angle against the
  // ground-truth-deformed endpoints. off_* = endpoint error (mm); dang_* = line
  // ANGLE error (deg) -> directly measures the photo's "red box must tilt to follow".
  if (synth_field_enabled()) {
    acv_XY d0 = synth_morph_field(l0_raw), d1 = synth_morph_field(l1_raw);
    acv_XY t0 = acv_XY(l0_raw.x + d0.x, l0_raw.y + d0.y);
    acv_XY t1 = acv_XY(l1_raw.x + d1.x, l1_raw.y + d1.y);
    auto ang = [](acv_XY a, acv_XY b){ return (float)(atan2(b.y - a.y, b.x - a.x) * 180.0 / 3.14159265358979323846); };
    auto dabs = [](acv_XY a, acv_XY b){ return sqrtf((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)); };
    float a_true = ang(t0, t1), a_morph = ang(lineDef.p0, lineDef.p1), a_raw = ang(l0_raw, l1_raw);
    auto wrap = [](float e){ while(e>180)e-=360; while(e<-180)e+=360; return e; };
    fprintf(stderr, "[MORPHTEST] line id=%d off_nomorph=(%.4f,%.4f) off_morph=(%.4f,%.4f) "
            "dang_nomorph=%.3f dang_morph=%.3f (deg)\n",
            lineDef.id, dabs(l0_raw,t0), dabs(l1_raw,t1), dabs(lineDef.p0,t0), dabs(lineDef.p1,t1),
            wrap(a_raw - a_true), wrap(a_morph - a_true));
  }

  lineDef.p0 = TemplateDomain_TO_PixDomain(lineDef.p0, cached_sin, cached_cos, flip_f, calibCen, mmpp);
  lineDef.p1 = TemplateDomain_TO_PixDomain(lineDef.p1, cached_sin, cached_cos, flip_f, calibCen, mmpp);
  LOGV("p0:%f %f, p1:%f,%f",
    lineDef.p0.x,lineDef.p0.y,
    lineDef.p1.x,lineDef.p1.y);
      // acv_XY mid=acvVecMult(acvVecAdd(line.p0,line.p1), 0.5);//Just for testing
      // line.p0 = acvVecAdd(acvVecMult(acvVecSub(line.p0,mid), 0.5),mid);
      // line.p1 = acvVecAdd(acvVecMult(acvVecSub(line.p1,mid), 0.5),mid);

  lineDef = lineDefDataPrep(lineDef);
  
  acv_Line line_cand;
  line_cand.line_vec = lineDef.lineTar.line_vec;
  line_cand.line_anchor = lineDef.lineTar.line_anchor;
  lineDef.initMatchingMargin /= mmpp;
  // Caliper sub-params (def-file unit: mm) -> px for the matching engine,
  // matching the rest of the def schema (pt1/pt2/margin are also mm-in-def
  // and px-after-conversion). Sentinels (cal_length=-1 "use margin",
  // cal_step=-1 "use 1px") are left alone.
  if (lineDef.locating == 1) {
    lineDef.cal_width /= mmpp;
    if (lineDef.cal_length > 0) lineDef.cal_length /= mmpp;
    if (lineDef.cal_step   > 0) lineDef.cal_step   /= mmpp;
    if (lineDef.cal_max_error > 0) lineDef.cal_max_error /= mmpp;
  }

  // LOGI("initMatchingMargin:%f MatchingMarginX:%f",initMatchingMargin,MatchingMarginX);
  FeatureReport_lineReport Report;
  if (lineDef.locating == 1) // caliper/section locating (px-unit report)
    Report = LineMatching_caliper(lineDef, eT, flip_f);
  else
    Report = SingleMatching_line(
                      &lineDef, eT, 1/mmpp,
                      line_cand, edge_grid, flip_f, tmp_points, m_sections);
  

  Report.line.end_pt1 = acvVecMult(Report.line.end_pt1, mmpp);
  Report.line.end_pt2 = acvVecMult(Report.line.end_pt2, mmpp);
  Report.line.line.line_anchor = acvVecMult(Report.line.line.line_anchor, mmpp);
  Report.line.s = Report.line.s * mmpp;
  // Caliper hits go all the way back to OBJECT-FRAME mm (def coord system),
  // unlike end_pt1/pt2 which stay in image-mm. This lets consumers compare
  // hit positions directly to def shape.pt1/pt2 without applying the
  // inspection cx/cy/rotate inverse themselves. Apply to ALL hits — missed
  // entries carry the caliper's nominal anchor (stored by Caliper.cpp) so
  // the WebUI overlay can mark where each miss happened.
  for (auto &h : Report.cal_hits)
    h.pt = PixDomain_TO_TemplateDomain(h.pt, cached_sin, cached_cos, flip_f, calibCen, mmpp);
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
    rep.status = FeatureReport_sig360_circle_line_single::STATUS_NA; // in-progress: cyclic re-entry returns NA

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
    rep.status = FeatureReport_sig360_circle_line_single::STATUS_NA; // in-progress: cyclic re-entry returns NA

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
    rep.status = FeatureReport_sig360_circle_line_single::STATUS_NA; // in-progress: cyclic re-entry returns NA

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
    rep.status = FeatureReport_sig360_circle_line_single::STATUS_NA; // in-progress: cyclic re-entry returns NA


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
    rep.status = FeatureReport_sig360_circle_line_single::STATUS_NA; // in-progress: cyclic re-entry returns NA

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

int FeatureManager_sig360_circle_line::SingleMatching(int lableIdx, acv_LabeledData *ldData,
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

  cv::Mat _eT_cv = p_cropImg_cv;
  edgeTracking eT(_eT_cv, cropOffset, bacpac);
  // acvDrawCrossX(originalImage,
  //   calibCen.x, calibCen.y,
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
  float globeMinErr_ALL=ignoreErr;



  {

    int scanWidth = 10;
    int sparseScanLen = 360 / scanWidth;

    // Both v1 and v2 use the same FFT-based angle search (xrefine). The v2
    // centroid iteration (xrefine_v2_centroidIter) is disabled: it added no
    // final-output benefit (the downstream anchor/morph already refines
    // orientation), was the source of the symmetric-lobe jump, and was the
    // slowest path. v2's only remaining distinction is the morph-boundary
    // dual-sig signature. (The centroid-iter function is kept for reference.)
    if (matching_face >= 0)
      xrefine(feature_signature, tmp_signature, 36 / 3, minMatchErr, false, 10, 5, 0.5);
    if (matching_face <= 0)
      xrefine(feature_signature, tmp_signature, 36 / 3, minMatchErr_bk, true, 10, 5, 0.5);

    float matching_angle_margin = this->matching_angle_margin;
    // if(matching_angle_margin>M_PI-0.0001)
    // {
    //   matching_angle_margin=M_PI-0.0001;
    // }
    // minMatchErr[0].y=0.01;

    const bool dbgSig = (getenv("SIGCAND_DUMP") != NULL);
    if (dbgSig) fprintf(stderr, "[SIGCAND] === front candidates (matching_face=%d) ===\n", this->matching_face);
    float globeMinErr=ignoreErr;
    for (int i = 0; i < minMatchErr.size(); i++)
    {
      float preErr = minMatchErr[i].y;

      
      if(globeMinErr>preErr)
      {
        globeMinErr=preErr;
      }
      if (matching_angle_margin < M_PI - 0.001 && isAngleInRegion(minMatchErr[i].x * M_PI / 180, this->matching_angle_offset - matching_angle_margin, this->matching_angle_offset + matching_angle_margin))
      {
        minMatchErr[i].y = ignoreErr;
      }
      else
      {
        if(minErr>preErr)
        {
          minErr=preErr;
        }
      }
      LOGV("F %f: %f>%f", minMatchErr[i].x, preErr, minMatchErr[i].y);
      if (dbgSig) {
        float sim = 1 - SigMatchErrorNormalize(preErr, feature_signature);
        fprintf(stderr, "[SIGCAND] F[%2d] ang=%7.2f deg  err=%.6f  sim=%.6f  %s\n",
                i, minMatchErr[i].x, preErr, sim,
                (minMatchErr[i].y >= ignoreErr) ? "(rejected by angle_margin)" : "");
      }
      // minMatchErr[i].y+=0.005723;
    }

    if (dbgSig) fprintf(stderr, "[SIGCAND] === back candidates (skipped when matching_face=1) ===\n");
    float globeMinErr_bk=ignoreErr;
    for (int i = 0; i < minMatchErr_bk.size(); i++)
    {
      float preErr = minMatchErr_bk[i].y;
      float angle = minMatchErr_bk[i].x * M_PI / 180 + M_PI; //the flip is along X axis, but in practice the flip should(in practice sense) be along Y axis
      
      
      
      if(globeMinErr_bk>preErr)
      {
        globeMinErr_bk=preErr;
      }
      if (matching_angle_margin < M_PI - 0.001 && isAngleInRegion(angle, this->matching_angle_offset - matching_angle_margin, this->matching_angle_offset + matching_angle_margin))
      {
        minMatchErr_bk[i].y = ignoreErr;
      }
      else
      {
        if(minErr>preErr)
        {
          minErr=preErr;
        }
      }
      LOGV("B %f: %f>%f", minMatchErr_bk[i].x, preErr, minMatchErr_bk[i].y);
      if (dbgSig) {
        float sim = 1 - SigMatchErrorNormalize(preErr, feature_signature);
        fprintf(stderr, "[SIGCAND] B[%2d] ang=%7.2f deg  err=%.6f  sim=%.6f  %s\n",
                i, minMatchErr_bk[i].x, preErr, sim,
                (minMatchErr_bk[i].y >= ignoreErr) ? "(rejected by angle_margin)" : "");
      }
    }
    if (dbgSig) fprintf(stderr, "[SIGCAND] minErr=%.6f globeMinErr_front=%.6f globeMinErr_back=%.6f → globeMinErr_ALL_pick=%.6f\n",
                        minErr, globeMinErr, globeMinErr_bk, std::min(globeMinErr, globeMinErr_bk));
    
    globeMinErr_ALL=globeMinErr;
    if(globeMinErr_ALL>globeMinErr_bk)
    {
      globeMinErr_ALL=globeMinErr_bk;
    }
    
  }
  float globeMaxSimF_ALL=1-SigMatchErrorNormalize(globeMinErr_ALL,feature_signature);
  
  
  if(sigRelativeMatchSimThres==sigRelativeMatchSimThres)//Not NaN
  {
    //check if the first avaliable error factor still not too far from the best match
    
    float sqrt_MaxSimF=1-SigMatchErrorNormalize(minErr,feature_signature);
    if(sqrt_MaxSimF/globeMaxSimF_ALL<sigRelativeMatchSimThres)
    {
      // LOGE("gErr:%f cur_Err:%f",globeMinErr_ALL,minErr);
      return -40;
    }

    for (int i = 0; i < minMatchErr.size(); i++)
    {
      
      float sqrt_SimF=1-SigMatchErrorNormalize(minMatchErr[i].y,feature_signature);
      
      // LOGE("minMatchErr[%d].y:%f sqrt_SimF:%f",i,minMatchErr[i].y,sqrt_SimF);
      if(sqrt_SimF/globeMaxSimF_ALL<sigRelativeMatchSimThres)
      {
          minMatchErr[i].y = ignoreErr;
      }
    }

    for (int i = 0; i < minMatchErr_bk.size(); i++)
    {
      float sqrt_SimF=1-SigMatchErrorNormalize(minMatchErr_bk[i].y,feature_signature);
      // LOGE("b_minMatchErr[%d].y:%f sqrt_SimF:%f",i,minMatchErr[i].y,sqrt_SimF);
      if(sqrt_SimF/globeMaxSimF_ALL<sigRelativeMatchSimThres)
      {
          minMatchErr_bk[i].y = ignoreErr;
      }
    }
  }




  LOGV("minErr:%f globeMinErr_ALL:%f mean:%f sigma:%f",
    minErr,globeMinErr_ALL,feature_signature.mean,feature_signature.sigma
  );
  
  float minAvaSimilar=(1-SigMatchErrorNormalize(minErr,feature_signature));
  LOGV("globeMaxSimF_ALL:%f sigRelativeMatchSimThres:%f minAvaSimilar:%f  sigMatchSimThres:%f ",
    globeMaxSimF_ALL,sigRelativeMatchSimThres,minAvaSimilar,sigMatchSimThres
  );
  if(minAvaSimilar<(sigMatchSimThres))//if the minimum err still not pass, then, early stop the following procesure
  {
    return -30;
  }

  contourGridGrayLevelRefine(p_cropImg_cv, edge_grid, bacpac);

  // for(int i=0;i<edge_grid.contourSections.size();i++)
  // {
  //   for(int j=0;j<edge_grid.contourSections[i].size();j++)
  //   {
  //     int X=edge_grid.contourSections[i][j].pt.x;
  //     int Y=edge_grid.contourSections[i][j].pt.y;
  //     originalImage->CVector[Y][X*3]=255;
      
  //     // X=edge_grid.contourSections[i][j].pt_img.x;
  //     // Y=edge_grid.contourSections[i][j].pt_img.y;
      
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

  cm.center = acv_XY(0, 0);
  // Propagate the def-configured morph model to the ConstrainMap (mode 0 == legacy).
  cm.mode        = this->morph_mode;
  cm.reg         = this->morph_reg;
  cm.outlier_k   = this->morph_outlier_k;
  cm.min_anchors = this->morph_min_anchors;
  cm.tps_lambda  = this->morph_tps_lambda;

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
      angle_offset += 0.5*M_PI/180;
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
      minErr.y = ignoreErr;

      for (int i = 0; i < minMatchErr.size(); i++)
      {
        if (minErr.y > minMatchErr[i].y)
        {
          minErr = minMatchErr[i];
          isInv = false;
          targetIdx = i;
        }
      }

      for (int i = 0; i < minMatchErr_bk.size(); i++)
      {
        if (minErr.y > minMatchErr_bk[i].y)
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
        minMatchErr[targetIdx].y = ignoreErr;
      }
      else
      {
        minMatchErr_bk[targetIdx].y = ignoreErr;
      }

      angle = minErr.x * M_PI / 180;
      error = minErr.y;
      if (getenv("SIGCAND_DUMP")) {
        float sim = 1 - SigMatchErrorNormalize(error, feature_signature);
        fprintf(stderr, "[SIGCAND] pick attempt %d: %s ang=%.2f deg  err=%.6f  sim=%.6f\n",
                defaultRetryCountDown - retryCountDown, isInv?"BACK":"FRONT", minErr.x, error, sim);
      }
    }
    retryCountDown--;

    error = sqrt(error);
    //if(i<10)

    {
      LOGV("======%d===XY:%0.4f,%0.4f AREA:%d er:%f,inv:%d,angDeg:%f ",
           lableIdx, ldData[lableIdx].Center.x, ldData[lableIdx].Center.y, ldData[lableIdx].area, error, isInv, angle * 180 / 3.14159);
    }

    float similarFactor=(1-SigMatchErrorNormalize(error,feature_signature));
    if(similarFactor<(sigMatchSimThres))//if the minimum err still not pass, then, early stop the following procesure
    {
      LOGE("similarFactor:%f < %f", similarFactor,sigMatchSimThres);
      return -3;
    }
    singleReport.similarity=similarFactor;



    angle+=angle_offset;
    // angle=0;

    // Report rotate in the canonical (-pi, pi] range (smallest-magnitude
    // representative) so it's consistent across matchers and reads ~0 near the
    // 0/360 boundary instead of ~2*pi. The geometry below uses sin/cos and is
    // wrap-agnostic, so only the reported number is affected.
    float rot_report = angle;
    while (rot_report >   (float)M_PI) rot_report -= (float)(2 * M_PI);
    while (rot_report <= -(float)M_PI) rot_report += (float)(2 * M_PI);
    singleReport.rotate = rot_report;
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
    calibCen.x *= ppmm;
    calibCen.y *= ppmm;
    float cached_cos = cos(angle);
    float cached_sin = sin(angle);

    LOGV("calibCen: %f %f,angle_offset:%f", calibCen.x, calibCen.y,angle_offset);


    
    for (int j = 0; j < detectedSearchPoints.size(); j++) //reset the ConstrainMap
    {
      cm.anchorPairs[j].to = acv_XY(NAN, NAN);
    }
    // Start each frame on an un-morphed pose so iteration 1 reproduces the legacy
    // single-pass result; morph_max_iter>1 then re-locates anchors through the
    // morph built from the previous iteration until they converge.
    cm.resetTransform();
    std::vector<acv_XY> prev_to(cm.anchorPairs.size(), acv_XY(NAN, NAN));
    for(int k=0;k<morph_max_iter;k++)//perform locating_anchor locating and adjust
    {

      RESET_REPORT(singleReport);//Set to Unset state
      for (int j = 0; j < searchPointList.size(); j++)
      {
        // cm.anchorPairs[j].to = acv_XY(NAN, NAN);
        if (searchPointList[j].data.anglefollow.locating_anchor == false)
        {
          continue;
        }
        LOGV("ID:%d", searchPointList[j].id);

        TreeExecution(searchPointList[j].id,singleReport,eT,
          calibCen, mmpp, cached_cos, cached_sin, flip_f);//find all necessary anchor first
      }


      for (int j = 0; j < detectedSearchPoints.size(); j++) //Fill the ConstrainMap
      {
        cm.anchorPairs[j].to = acv_XY(NAN, NAN);

        bool isAnchor = false;
        if(detectedSearchPoints[j].def->data.anglefollow.locating_anchor == true)
        {
          isAnchor = true;
        }
        LOGV("ID:%d=>%d  isAnchor:%d",detectedSearchPoints[j].def->id, detectedSearchPoints[j].status, isAnchor);
        if (isAnchor == false)
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
        // Morph effectiveness test: add a known regional field to the anchor
        // target so the morph sees a ground-truth deformation (diagnostics).
        if (synth_field_enabled()) {
          acv_XY d = synth_morph_field(cm.anchorPairs[j].from);
          cm.anchorPairs[j].to.x += d.x;
          cm.anchorPairs[j].to.y += d.y;
        }

        // acv_XY pos = ;
        // LOGI("XY:%f %f -> %f %f", cm.anchorPairs[j].from.x, cm.anchorPairs[j].from.y, locatedPtOnTemplate.x, locatedPtOnTemplate.y);

      }

      // Rebuild the cached morph from this iteration's anchors (no-op for mode 0).
      cm.solve();

      // Converged once every valid anchor moved less than the tolerance since the
      // previous iteration. (Single-iteration default never reaches this.)
      if (k + 1 < morph_max_iter)
      {
        float maxd = 0;
        bool any_prev = false;
        for (size_t j = 0; j < cm.anchorPairs.size(); j++)
        {
          acv_XY cur = cm.anchorPairs[j].to;
          if (cur.x != cur.x) continue; // NAN
          if (prev_to[j].x == prev_to[j].x) // had a previous value
          {
            float d = acvDistance(cur, prev_to[j]);
            if (d > maxd) maxd = d;
            any_prev = true;
          }
          prev_to[j] = cur;
        }
        if (any_prev && maxd < morph_tol_mm)
          break;
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
      if (getenv("SIGCAND_DUMP")) fprintf(stderr, "[SIGCAND] REJECTED: orientation_essential judge failed → retry next candidate\n");
      LOGV(">>>>>REDO  REDO>>>>");
      LOGV(">>>>>REDO  REDO>>>>");
      LOGV(">>>>>REDO  REDO>>>>");
      LOGV(">>>>>REDO  REDO>>>>");
      LOGV(">>>>>REDO  REDO>>>>");
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



  // for(int i=0;;i++)
  // {
  //   const ContourFetch::ptInfo* pinfo= edge_grid.get(i);
  //   if(pinfo==NULL)break;
  //   int X =(int)pinfo->pt.x;
  //   int Y =(int)pinfo->pt.y;
  //   originalImage->CVector[Y][3*X]=255;
  // }
  // acv_XY ttt = acvRotation(cached_sin, cached_cos, flip_f, acv_XY(100, 0));

  // acvDrawLine(originalImage,
  //             calibCen.x,
  //             calibCen.y,
  //             calibCen.x + ttt.x,
  //             calibCen.y + ttt.y,
  //             20, 255, 128);
  // acvCloneImage(originalImage,originalImage,1);
  return 0;
}

// Grayscale gradient-edge signature (matching_method="edge_sig"). Mirrors
// convertContourGrid2Signature's units/binning, but sources the per-angle edge
// from the GRAYSCALE image (ray-cast from the centroid, outermost strong |grad|
// edge) instead of the binary silhouette -> exposure/tint robust.
//   center_lb : centroid in labeled-buffer coords
//   ideal_center : same point in ideal coords (as the binary path uses)
//   grayOrig : full-res grayscale image (sampled at coord*dsampLevel)
static float graySampleBilinear(const cv::Mat &im, float x, float y)
{
  const int W = im.cols, H = im.rows;
  if (x < 0 || y < 0 || x >= W - 1 || y >= H - 1) return -1;
  int x0 = (int)x, y0 = (int)y; float fx = x - x0, fy = y - y0;
  const int cn = im.channels();   // 1 (native gray) or 3 (B=G=R, sample ch0)
  const unsigned char *r0 = im.ptr<unsigned char>(y0);
  const unsigned char *r1 = im.ptr<unsigned char>(y0 + 1);
  float a = r0[x0*cn], b = r0[(x0+1)*cn], c = r1[x0*cn], d = r1[(x0+1)*cn];
  return (a*(1-fx)+b*fx)*(1-fy) + (c*(1-fx)+d*fx)*fy;
}
bool convertGrayEdges2Signature(acv_XY center_lb, acv_XY ideal_center, const cv::Mat &grayOrig,
                                float dsampLevel, float searchRadius_lb,
                                std::vector<acv_XY> &o_signature, FeatureManager_BacPac *bacpac,
                                float minStrength, float step = 1.5f)
{
  int N = (int)o_signature.size();
  if (N == 0 || grayOrig.empty()) return false;
  const float rInner = 2.0f;
  if (step <= 0) step = 1.5f;
  std::vector<char> found(N, 0);
  for (int b = 0; b < N; b++) { o_signature[b].x = 0; o_signature[b].y = 0; }

  for (int b = 0; b < N; b++)
  {
    float th = 2.0f * M_PI * b / N;
    float dx = cosf(th), dy = sinf(th);
    float bestR = 0, bestStr = 0; float gP2 = 0, gP = 0, rP = 0; bool h1 = false, h2 = false;
    for (float r = rInner; r <= searchRadius_lb; r += step)
    {
      float iIn  = graySampleBilinear(grayOrig, (center_lb.x + dx*(r-step))*dsampLevel, (center_lb.y + dy*(r-step))*dsampLevel);
      float iOut = graySampleBilinear(grayOrig, (center_lb.x + dx*(r+step))*dsampLevel, (center_lb.y + dy*(r+step))*dsampLevel);
      if (iIn < 0 || iOut < 0) { h1 = h2 = false; continue; }
      float g = fabsf(iOut - iIn);
      if (h2 && gP >= gP2 && gP >= g && gP >= minStrength) { bestR = rP; bestStr = gP; } // outermost peak
      gP2 = gP; gP = g; rP = r; h2 = h1; h1 = true;
    }
    if (bestStr > 0)
    {
      acv_XY E = { center_lb.x + dx*bestR, center_lb.y + dy*bestR };
      bacpac->sampler->img2ideal(&E); // same transform as the contour path
      float diffX = E.x - ideal_center.x, diffY = E.y - ideal_center.y;
      float theta = acvFAtan2(diffY, diffX);
      int idx = round(N * theta / (2 * M_PI)); if (idx < 0) idx += N; if (idx >= N) idx -= N;
      float R = hypot(diffX, diffY);
      if (o_signature[idx].x < R) { o_signature[idx].x = R; o_signature[idx].y = theta; found[idx] = 1; }
    }
  }
  // gap-fill empty bins (no zeros -> the existing correlation matcher stays valid)
  for (int b = 0; b < N; b++)
  {
    if (found[b]) continue;
    int lft = -1, rgt = -1;
    for (int d = 1; d <= N; d++) { int j = ((b-d)%N+N)%N; if (found[j]) { lft = j; break; } }
    for (int d = 1; d <= N; d++) { int j = (b+d)%N;       if (found[j]) { rgt = j; break; } }
    if (lft >= 0 && rgt >= 0) o_signature[b].x = 0.5f * (o_signature[lft].x + o_signature[rgt].x);
    else if (lft >= 0)        o_signature[b].x = o_signature[lft].x;
    else if (rgt >= 0)        o_signature[b].x = o_signature[rgt].x;
    o_signature[b].y = 2.0f * M_PI * b / N;
  }
  return true;
}

// v2 helper: extract the ordered cartesian boundary points (ideal coord) the
// signature builder would have consumed. Used so ContourSignature can be
// re-sampled from a shifted centroid without re-walking the grid.
bool extractContourGridCartesian(ContourFetch contour, std::vector<acv_XY> &o_cartesian_ideal, FeatureManager_BacPac *bacpac)
{
  o_cartesian_ideal.clear();
  if (contour.contourSections.size() == 0) return false;
  for (ContourFetch::ptInfo ptinfo : contour.contourSections[0])
  {
    acv_XY copos = ptinfo.pt;
    bacpac->sampler->img2ideal(&copos);
    if (copos.x != copos.x || copos.y != copos.y) continue;
    o_cartesian_ideal.push_back(copos);
  }
  return !o_cartesian_ideal.empty();
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
    float diffY = copos.y - center.y;
    float diffX = copos.x - center.x;
    if (diffX != diffX || diffY != diffY)
    {
      continue;
    }
    if (diffY<0.1 && diffY>-0.1)//prevent error/sensitive condition
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
    if (o_signature[idx].x < R)
    {
      o_signature[idx].x = R;
      o_signature[idx].y = theta;
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

int FeatureManager_sig360_circle_line::FeatureMatching(cv::Mat &img_cv)
{
  // Shape-based localizer path: bypass binarize/CCL/signature entirely and
  // locate via line2Dup + ROI refine on the original grayscale. Falls through
  // to the legacy sig360 path when shape training wasn't available.
  if (locating_engine == 1 && shape_ready)
  {
    return FeatureMatching_shape();
  }
  if (img_cv.empty()) return -1;
  if (!img_cv.isContinuous()) img_cv = img_cv.clone();
  // img_cv is the LABELED image (binary_processing_group's binary_img_storage
  // after acvComponentLabeling_cv). originalImage was independently set via
  // setOriginalImage(cv::Mat&) to the SOURCE image and is used by the body's
  // grayscale samplers (caliper / search_point_cv / edgeTracking).
  report.bacpac = bacpac;
  float ppmm = 1 / bacpac->sampler->mmpP_ideal();
  ; //pixel per mm

  vector<acv_LabeledData> &ldData = *this->_ldData;
  int grid_size = 50;
  bool drawDBG_IMG = false;
  cv::Mat &labeledBuff_cv = img_cv;
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

  // Phase 2: when matching_version=2 AND group exposed the CV_8UC1 binary,
  // build all per-label signatures + cartesian boundaries in a single pass
  // up-front (morph boundary + dual-sig diffusion), then later look them up
  // by label index instead of calling the legacy walker per label.
  std::vector<std::vector<acv_XY>> phase2_perLabelSig;
  std::vector<std::vector<acv_XY>> phase2_perLabelCart;
  bool phase2_active = (this->matching_version == 2)
                       && (matching_method == 0)   // contour_sig only for now
                       && bacpac && bacpac->binary_uc1_for_phase2
                       && !bacpac->binary_uc1_for_phase2->empty();
  if (phase2_active)
  {
    std::vector<acv_LabeledData> phase2_ld_unused;  // group's ldData is the truth
    buildLabeledSignatures_phase2(*bacpac->binary_uc1_for_phase2,
                                  phase2_ld_unused,
                                  phase2_perLabelSig,
                                  phase2_perLabelCart,
                                  (int)feature_signature.signature_data.size(),
                                  /*K_min=*/2,
                                  /*connectivity=*/8);
  }

  int onlyIdx = -1;
  if (single_result_area_ratio > 0 && ldData.size() >= 3)
  {
    onlyIdx = 0;
    int totalArea = ldData[1].area;

    LOGV("AREA 0:%d    1:%d", ldData[0].area, ldData[1].area);
    int maxArea = 0;
    for (int i = 2; i < ldData.size(); i++)
    {

      LOGV("AREA[%d]:%d", i, ldData[i].area);
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
  int areaThres = 0;
  if(0)
  {
    for (int i = 2; i < ldData.size(); i++)
    {
      //LOGI("ldData[%d].area=%d",i,ldData[i].area);
      if (areaThres < ldData[i].area)
      {
        areaThres = ldData[i].area;
      }
    }
    areaThres=areaThres*7/10;
  }
  else
  {
    areaThres= 100*dsampLevel*dsampLevel; //HACK:100 no particular reason, just a hack filter
  }

  for (int i = 2; i < ldData.size(); i++)
  {
    if (ldData[i].area < areaThres )
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

  LOGV("ldData.size()=%d onlyIdx:%d", ldData.size(),onlyIdx);
  for (int i = 2; i < ldData.size(); i++)
  {                           // idx 0 is not a label, idx 1 is for outer frame and connected objects(with the outter frame)
    if (ldData[i].misc == -1) //the ignore mark
      continue;
    if (onlyIdx > 0 && i != onlyIdx)
    {
      continue;
    }

    LOGV("ldData[%d].area=%d",i,ldData[i].area);
    p_cropImg_cv = labeledBuff_cv;
    cropOffset.x=0;
    cropOffset.y=0;
    m_objLabel = i;
    acv_LabeledData curLableDat=(acv_LabeledData){
      .LTBound=acvVecSub(ldData[i].LTBound,cropOffset),
      .RBBound=acvVecSub(ldData[i].RBBound,cropOffset),
      .Center=acvVecSub(ldData[i].Center,cropOffset),
      .area= ldData[i].area,
    };
    edge_grid.RESET();
    cv::Mat _crop_cv = p_cropImg_cv;
    int contGridRet=extractLabeledContourDataToContourGrid(_crop_cv, i, curLableDat, edge_grid, scanline_skip);
    // edge_grid.ptMult(dsampLevel/2);
    if(contGridRet!=0)
    {
      LOGI("contGridRet:%d",contGridRet);
      continue;
    }

    p_cropImg_cv = originalImage_cv;


    
    if (0)
    {
      static float rot = 0;
      rot += 10 * 3.14 / 180;
      float mag = 3;
      ldData[i].Center = acvVecAdd(ldData[i].Center, acv_XY(mag * sin(rot), mag * cos(rot)));
    }

    acv_XY ideal_center = acvVecMult(curLableDat.Center,dsampLevel);

    bacpac->sampler->img2ideal(&ideal_center);

    ideal_center = acvVecMult(ideal_center,1.0/dsampLevel);
    LOGV(">>>>>%f  %f",ideal_center.x,ideal_center.y);
    if (matching_method == 1) // edge_sig: grayscale gradient edge per ray (lighting robust)
    {
      float ex = fmax(fabs(curLableDat.RBBound.x-curLableDat.Center.x), fabs(curLableDat.Center.x-curLableDat.LTBound.x));
      float ey = fmax(fabs(curLableDat.RBBound.y-curLableDat.Center.y), fabs(curLableDat.Center.y-curLableDat.LTBound.y));
      float searchRadius_lb = hypot(ex, ey) * 1.25f + 10;
      convertGrayEdges2Signature(curLableDat.Center, ideal_center, originalImage_cv, dsampLevel,
                                 searchRadius_lb, tmp_signature.signature_data, bacpac, edge_sig_min_strength,
                                 edge_sig_ray_step);
    }
    else if (phase2_active && i >= 0 && i < (int)phase2_perLabelSig.size()
             && !phase2_perLabelSig[i].empty())
    {
      // Phase 2: copy the pre-built dual-sig signature for this label. Units
      // are still input-px (group's binary_img_storage pixel space); the
      // *=dsampLevel/ppmm scaling below converts to mm exactly as the legacy
      // convertContourGrid2Signature output would have.
      tmp_signature.signature_data = phase2_perLabelSig[i];
    }
    else // contour_sig: binary silhouette contour (default, backward compatible)
    {
      convertContourGrid2Signature(ideal_center, edge_grid, tmp_signature.signature_data, bacpac);
    }

    //the tmp_signature is in Pixel unit, convert it to mm
    for (int j = 0; j < tmp_signature.signature_data.size(); j++)
    {
      tmp_signature.signature_data[j].x *=dsampLevel/ ppmm;

      // LOGI("%d=>%f",j,tmp_signature.signature_data[j].x);
    }

    // v2 path: also store the cartesian boundary (mm, ideal-coord) and the
    // sample center, so xrefine_v2_centroidIter can re-sample after each
    // centroid LSQ step. Skipped for v1 to keep its hot path untouched.
    tmp_signature.cartesian_ideal.clear();
    tmp_signature.sample_center = acv_XY(0, 0);
    if (this->matching_version == 2 && matching_method == 0)
    {
      const float k = dsampLevel / ppmm;
      // Phase 2 path: cartesian comes directly from buildLabeledSignatures_phase2
      // (input-px units, no img2ideal applied since group's binary_img_storage
      // already lives in the ideal-coord-equivalent grid).
      // Legacy path: walk edge_grid -> px -> mm via extractContourGridCartesian.
      std::vector<acv_XY> cart_pix;
      bool have_cart = false;
      if (phase2_active && i >= 0 && i < (int)phase2_perLabelCart.size()
          && !phase2_perLabelCart[i].empty())
      {
        cart_pix = phase2_perLabelCart[i];
        have_cart = true;
      }
      else if (extractContourGridCartesian(edge_grid, cart_pix, bacpac))
      {
        have_cart = true;
      }
      if (have_cart)
      {
        tmp_signature.cartesian_ideal.reserve(cart_pix.size());
        for (acv_XY &p : cart_pix)
          tmp_signature.cartesian_ideal.push_back(acv_XY(p.x * k, p.y * k));
        tmp_signature.sample_center = acv_XY(ideal_center.x * k, ideal_center.y * k);
        // Legacy guard was |diffY| < 0.1 dsamp-px. Scale into mm to keep the
        // same behavior on the resampled signature.
        tmp_signature.dy_skip_thres = 0.1f * k;
      }
    }

    // acvSaveBitmapFile("FFKFKJDK3.BMP", labeledBuff);
    // exit(-1);
    tmp_signature.CalcInfo();

    // ---- DEBUG: v1-vs-v2 signature + angle comparison (headless --insp) ------
    // Set INSP_CMP_V1V2=1 to, per label, build BOTH the legacy v1 contour
    // signature (from the same edge_grid) and the v2 morph-boundary dual-sig
    // (already in tmp_signature), then run BOTH refiners against the SAME
    // feature_signature and log:
    //   (a) whether the two signature CONTOURS agree (per-bin radius diff), and
    //   (b) whether both matchers land on a similar best ANGLE.
    // Requires a v2 def (so the phase2 sig exists); the v1 sig is rebuilt here.
    // Optional INSP_CMP_V1V2_DUMP=<file.csv> appends both signatures (for plot).
    if (getenv("INSP_CMP_V1V2") && phase2_active)
    {
      const float k_mm = dsampLevel / ppmm;
      const int N = (int)feature_signature.signature_data.size();

      // v2 signature == the one just built into tmp_signature (mm units).
      ContourSignature sig_v2 = tmp_signature;

      // Rebuild the v1 legacy contour signature from the same edge_grid.
      ContourSignature sig_v1; sig_v1.RESET(N);
      convertContourGrid2Signature(ideal_center, edge_grid, sig_v1.signature_data, bacpac);
      for (acv_XY &p : sig_v1.signature_data) p.x *= k_mm;   // px -> mm (main-path scale)
      sig_v1.CalcInfo();

      // (a) contour agreement: per-bin radius diff (.x holds radius in mm).
      int nBin = (int)sig_v1.signature_data.size();
      if ((int)sig_v2.signature_data.size() < nBin) nBin = (int)sig_v2.signature_data.size();
      double sumAbs = 0, maxAbs = 0; int nCmp = 0;
      for (int b = 0; b < nBin; b++) {
        float r1 = sig_v1.signature_data[b].x, r2 = sig_v2.signature_data[b].x;
        if (!(r1 > 0) || !(r2 > 0)) continue;               // skip empty bins
        double d = fabs(r1 - r2); sumAbs += d; if (d > maxAbs) maxAbs = d; nCmp++;
      }
      double meanAbs = nCmp ? sumAbs / nCmp : NAN;

      // (b) run both refiners against the SAME feature_signature (front facing).
      // Copy the sources: xrefine_v2_centroidIter mutates src (resamples).
      std::vector<acv_XY> e_v1, e_v2;
      ContourSignature s1 = sig_v1, s2 = sig_v2;
      xrefine(feature_signature, s1, 36 / 3, e_v1, false, 10, 5, 0.5);
      acv_XY c2 = s2.sample_center;
      xrefine_v2_centroidIter(feature_signature, s2, 36 / 3, e_v2, false, 10, 5, 0.5,
                              c2, this->matching_v2_max_iter, this->matching_v2_tol_mm, NULL);
      float a1 = NAN, er1 = INFINITY, a2 = NAN, er2 = INFINITY;
      for (acv_XY &e : e_v1) if (e.y < er1) { er1 = e.y; a1 = e.x; }
      for (acv_XY &e : e_v2) if (e.y < er2) { er2 = e.y; a2 = e.x; }
      float dAng = a1 - a2;
      while (dAng > 180) dAng -= 360; while (dAng < -180) dAng += 360;

      // (c) FFT circular normalized cross-correlation (global peak, sub-bin) on
      // the same v1 and v2 signatures vs the template -- no sweep, no iteration.
      float aF1 = NAN, aF2 = NAN;
      float scF1 = matchAngleFFT(feature_signature.signature_data, sig_v1.signature_data, false, aF1);
      float scF2 = matchAngleFFT(feature_signature.signature_data, sig_v2.signature_data, false, aF2);
      float dFv1 = aF2 - a1; while (dFv1 > 180) dFv1 -= 360; while (dFv1 < -180) dFv1 += 360;

      LOGI("[CMP] label %d c(%.0f,%.0f) bins=%d mean|dR|=%.4f max|dR|=%.4f | "
           "v1=%.3f(e%.4f)  v2=%.3f(e%.4f) dAng=%.2f | "
           "fft_v1=%.3f(ncc%.4f) fft_v2=%.3f(ncc%.4f)  fftv2-v1=%.2f",
           i, ldData[i].Center.x, ldData[i].Center.y, nCmp, meanAbs, maxAbs,
           a1, er1, a2, er2, dAng,
           aF1, scF1, aF2, scF2, dFv1);

      // (d) speed: per-call cost of each angle finder (INSP_TIME_MATCH=<reps>).
      if (const char *tm = getenv("INSP_TIME_MATCH"))
      {
        int R = atoi(tm); if (R < 1) R = 500;
        using clk = std::chrono::high_resolution_clock;
        volatile float sink = 0;   // defeat dead-code elimination
        // v1: plain brute-force xrefine (no mutation of src).
        auto t0 = clk::now();
        for (int r = 0; r < R; r++) { std::vector<acv_XY> e; ContourSignature s = sig_v1; xrefine(feature_signature, s, 36/3, e, false, 10,5,0.5); sink += e.empty()?0:e[0].x; }
        auto t1 = clk::now();
        // v2: centroid-iter (fresh copy each rep; it resamples src in place).
        for (int r = 0; r < R; r++) { std::vector<acv_XY> e; ContourSignature s = sig_v2; acv_XY c = s.sample_center; xrefine_v2_centroidIter(feature_signature, s, 36/3, e, false, 10,5,0.5, c, this->matching_v2_max_iter, this->matching_v2_tol_mm, NULL); sink += e.empty()?0:e[0].x; }
        auto t2 = clk::now();
        // fft: single global correlation.
        for (int r = 0; r < R; r++) { float a; sink += matchAngleFFT(feature_signature.signature_data, sig_v2.signature_data, false, a); }
        auto t3 = clk::now();
        // bare copy cost (subtract to isolate algo).
        for (int r = 0; r < R; r++) { ContourSignature s = sig_v2; sink += s.signature_data.empty()?0:s.signature_data[0].x; }
        auto t4 = clk::now();
        auto us = [&](clk::time_point a, clk::time_point b){ return std::chrono::duration_cast<std::chrono::nanoseconds>(b-a).count()/1000.0/R; };
        LOGI("[TIME] label %d reps=%d  v1=%.2fus  v2=%.2fus  fft=%.2fus  (copy=%.2fus)  -> v1-copy=%.2f v2-copy=%.2f fft-copy=%.2f us  [sink=%.1f]",
             i, R, us(t0,t1), us(t1,t2), us(t2,t3), us(t3,t4),
             us(t0,t1)-us(t3,t4), us(t1,t2)-us(t3,t4), us(t2,t3)-us(t3,t4), (float)sink);
      }

      if (const char *dp = getenv("INSP_CMP_V1V2_DUMP")) {
        FILE *fp = fopen(dp, "a");
        if (fp) {
          fprintf(fp, "# label,%d,center,%.2f,%.2f,v1ang,%.3f,v2ang,%.3f\n",
                  i, ldData[i].Center.x, ldData[i].Center.y, a1, a2);
          for (int b = 0; b < nBin; b++)
            fprintf(fp, "%d,%d,%.5f,%.5f\n", i, b,
                    sig_v1.signature_data[b].x, sig_v2.signature_data[b].x);
          fclose(fp);
        }
      }
    }
    // ---- END DEBUG ---------------------------------------------------------

    { //early intercept just to check mean and sigma

      LOGV("mCur:%f  mFea:%f",tmp_signature.mean, feature_signature.mean);
      
      float meanRatio = tmp_signature.mean / feature_signature.mean;
      if (meanRatio > 1)
        meanRatio = 1 / meanRatio;
      float sigmaRatio = tmp_signature.sigma / feature_signature.sigma;
      if (sigmaRatio > 1)
        sigmaRatio = 1 / sigmaRatio;

      float stage1Sim = meanRatio * sigmaRatio;
      LOGV("mR:%f  sR:%f stage1Sim:%f", meanRatio, sigmaRatio, stage1Sim);
      if (meanRatio < 0.5 || stage1Sim < sig_st1_matching_sim_thres)
        continue;
    }

    sign360_process(tmp_signature.signature_data, signature_data_buffer);

    // LOGI(">>feature_signature");
    // for(int i=0;i<feature_signature.size();i++)
    // {
    //   acv_XY xy=feature_signature[i];

    //   printf("%4.1f ",hypot(xy. X,xy.y));
    // }
    // LOGI(">>=================");
    // for(int i=0;i<tmp_signature.size();i++)
    // {
    //   acv_XY xy=tmp_signature[i];

    //   printf("%4.1f ",hypot(xy.x,xy.y));
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

    int ret = SingleMatching(i, &(ldData[0]),
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

// ===========================================================================
// Shape-based localizer (line2Dup + ROI refine) -- opt-in alternative to the
// sig360 contour signature. Locates the object pose, then reuses the SAME
// anchor-morph + caliper measurement as the sig360 path.
// ===========================================================================

int FeatureManager_sig360_circle_line::trainShapeMatcher()
{
  shape_ready = false;
  shapeMatcher.reset();
  shapeFeatureSet.reset();

  // Resolve the template image path: explicit def "reference_image" (relative to
  // the def's dir) wins; otherwise derive "<def-base>.png" from the def path
  // (def "_def_path", else env VISSELE_DEF_PATH set by the --insp entry).
  std::string base = !def_path.empty() ? def_path
                     : (getenv("VISSELE_DEF_PATH") ? std::string(getenv("VISSELE_DEF_PATH")) : std::string());
  std::string png;
  if (!reference_image_name.empty())
  {
    std::string dir;
    size_t slash = base.find_last_of("/\\");
    if (slash != std::string::npos) dir = base.substr(0, slash + 1);
    png = dir + reference_image_name;
  }
  else if (!base.empty())
  {
    size_t dot = base.find_last_of('.');
    png = (dot == std::string::npos ? base : base.substr(0, dot)) + ".png";
  }
  if (png.empty())
  {
    LOGE("[shape] no template path (set def \"reference_image\" or VISSELE_DEF_PATH)");
    return -1;
  }

  cv::Mat templ = cv::imread(png, cv::IMREAD_GRAYSCALE);
  if (templ.empty()) { LOGE("[shape] cannot read template image: %s", png.c_str()); return -1; }

  // Derive the object mask + centroid via Otsu. The part is the largest
  // connected component that does NOT touch the image border -- the backlit
  // field / cage frame spans the image and touches the border, so it must be
  // excluded (otherwise the frame wins and the origin collapses to the image
  // centre). We try both polarities (part may be bright or dark) and keep the
  // largest interior blob. Its centroid is the registration origin, so the
  // reported pose shares the sig360 silhouette-centroid basis.
  cv::Mat bw;
  cv::threshold(templ, bw, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
  cv::Mat bwInv; cv::bitwise_not(bw, bwInv);

  auto pickInterior = [](const cv::Mat &m, cv::Mat &o_labels, cv::Mat &o_cents,
                         int &o_best) -> int {
    cv::Mat stats;
    int n = cv::connectedComponentsWithStats(m, o_labels, stats, o_cents, 8, CV_32S);
    int best = -1, bestA = 0;
    for (int c = 1; c < n; c++)
    {
      int x = stats.at<int>(c, cv::CC_STAT_LEFT), y = stats.at<int>(c, cv::CC_STAT_TOP);
      int w = stats.at<int>(c, cv::CC_STAT_WIDTH), h = stats.at<int>(c, cv::CC_STAT_HEIGHT);
      int a = stats.at<int>(c, cv::CC_STAT_AREA);
      if (x <= 0 || y <= 0 || x + w >= m.cols || y + h >= m.rows) continue; // border
      if (a > bestA) { bestA = a; best = c; }
    }
    o_best = best;
    return bestA;
  };

  cv::Mat l1, c1, l2, c2; int b1 = -1, b2 = -1;
  int a1 = pickInterior(bw,    l1, c1, b1);
  int a2 = pickInterior(bwInv, l2, c2, b2);

  cv::Point2f originPx(templ.cols * 0.5f, templ.rows * 0.5f);
  cv::Mat blob;
  int blobArea = 0;
  if (a1 >= a2 && b1 > 0)
  {
    blob = (l1 == b1); blobArea = a1;
    originPx = cv::Point2f((float)c1.at<double>(b1, 0), (float)c1.at<double>(b1, 1));
  }
  else if (b2 > 0)
  {
    blob = (l2 == b2); blobArea = a2;
    originPx = cv::Point2f((float)c2.at<double>(b2, 0), (float)c2.at<double>(b2, 1));
  }
  // Registration origin + angle. Priority: def_image_reg (the registered pose of
  // the part in the saved reference image) > sign360 pt1 (the signature center) >
  // Otsu interior-blob centroid (fallback). The reg angle (and flip) also rotate
  // the silhouette mask and become the reported-angle offset, so matches land in
  // the def's reference frame.
  const char *origin_src = "otsu_blob";
  float reg_sin = 0.0f, reg_cos = 1.0f, reg_flip_f = 1.0f, angle_offset_deg = 0.0f;
  if (has_reg && def_mmpp > 0)
  {
    originPx = cv::Point2f(reg_center_mm.x / def_mmpp, reg_center_mm.y / def_mmpp);
    reg_sin = sinf(reg_angle_rad);
    reg_cos = cosf(reg_angle_rad);
    reg_flip_f = reg_flipped ? -1.0f : 1.0f;
    angle_offset_deg = reg_angle_rad * 180.0f / (float)M_PI;
    origin_src = "def_image_reg";
  }
  else if (has_ref_center && def_mmpp > 0)
  {
    originPx = cv::Point2f(ref_center_mm.x / def_mmpp, ref_center_mm.y / def_mmpp);
    origin_src = "sign360_pt1";
  }

  cv::Mat mask;
  const char *mask_src = "none";

  // Preferred feature mask: the def's sig360 silhouette. It is the exact part
  // region (so it precisely excludes the cage frame) and is rendered around the
  // origin via the measurement pipeline's own template->pixel transform at
  // identity pose -- so the coordinate frame matches automatically.
  if (!raw_sig_radius.empty() && def_mmpp > 0)
  {
    std::vector<cv::Point> poly;
    const int N = (int)raw_sig_radius.size();
    poly.reserve(N);
    for (int i = 0; i < N; i++)
    {
      float R  = raw_sig_radius[i].x;   // absolute radius (mm)
      float th = raw_sig_radius[i].y;   // absolute angle (rad)
      if (!(R > 1e-4f)) continue;       // skip empty bins
      acv_XY tp = { R * cosf(th), R * sinf(th) };
      // Rotate/flip by the registered pose so the silhouette overlays the part as
      // it actually sits in the saved image (identity when there is no reg).
      acv_XY px = TemplateDomain_TO_PixDomain(tp, reg_sin, reg_cos, reg_flip_f,
                                              acv_XY(originPx.x, originPx.y), def_mmpp);
      poly.push_back(cv::Point((int)lround(px.x), (int)lround(px.y)));
    }
    if (poly.size() >= 3)
    {
      cv::Mat m = cv::Mat::zeros(templ.size(), CV_8U);
      std::vector<std::vector<cv::Point>> polys{poly};
      cv::fillPoly(m, polys, cv::Scalar(255));
      // Grow by ~5px so the silhouette boundary gradient sits inside the mask.
      cv::dilate(m, m, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(11, 11)));
      mask = m;
      mask_src = "signature";
    }
  }

  // Fallback: dilate the Otsu interior blob by ~its equivalent radius so it grows
  // over the connected part body while still excluding the far cage frame.
  if (mask.empty() && !blob.empty() && blobArea > 0)
  {
    int r = (int)std::sqrt((double)blobArea / M_PI);
    int k = std::max(5, r);
    cv::Mat kern = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(2 * k + 1, 2 * k + 1));
    cv::dilate(blob, mask, kern);
    mask_src = "otsu_blob";
  }
  if (getenv("SHAPE_DBG"))
    fprintf(stderr, "[SHAPE_DBG] train: templ %dx%d origin=(%.1f,%.1f)[%s] areaPos=%d areaInv=%d blobArea=%d mask=%s maskPx=%d\n",
            templ.cols, templ.rows, originPx.x, originPx.y, origin_src, a1, a2, blobArea, mask_src,
            mask.empty() ? 0 : cv::countNonZero(mask));

  // Tight-crop the template to the part (mask bbox + margin). The whole-image
  // template rotates about the IMAGE centre, which mis-maps the pose of a part
  // that sits far off-centre (breaks rotation/flip). Cropping puts the part at
  // the template centre so rotation/flip pose recovery is accurate -- and
  // matching/training run on a small ROI instead of the full 5MP frame.
  cv::Mat templ_use = templ;
  cv::Mat mask_use  = mask;
  cv::Point2f origin_use(originPx.x, originPx.y);
  cv::Rect cropRect(0, 0, templ.cols, templ.rows);
  if (!mask.empty())
  {
    cv::Rect bb = cv::boundingRect(mask);
    // Square crop CENTRED ON THE ORIGIN so the template centre == origin. Then the
    // matcher's rotation (about the template centre) is exactly the part's rotation
    // about the registration point -> pose recovery is correct at any angle. The
    // half-size covers the part's farthest extent from the origin so it stays
    // inside the crop at all rotations, plus a margin for the ROI windows.
    float hx = std::max(originPx.x - bb.x, (bb.x + bb.width) - originPx.x);
    float hy = std::max(originPx.y - bb.y, (bb.y + bb.height) - originPx.y);
    int half = (int)std::ceil(std::max(hx, hy)) + std::max(24, (int)(0.15f * std::max(bb.width, bb.height)));
    cv::Rect r((int)originPx.x - half, (int)originPx.y - half, 2 * half, 2 * half);
    r &= cv::Rect(0, 0, templ.cols, templ.rows);
    if (r.width >= 16 && r.height >= 16)
    {
      cropRect = r;
      templ_use = templ(r).clone();
      mask_use  = mask(r).clone();
      origin_use = cv::Point2f(originPx.x - r.x, originPx.y - r.y);
    }
  }
  if (getenv("SHAPE_DBG"))
    fprintf(stderr, "[SHAPE_DBG] crop: [%d,%d %dx%d] origin_in_crop=(%.1f,%.1f)\n",
            cropRect.x, cropRect.y, cropRect.width, cropRect.height, origin_use.x, origin_use.y);

  try
  {
    sbm::FeatureSet fset = sbm::extractFeatures(templ_use, mask_use, shape_num_features,
                                                shape_pyramid_T, shape_weak_thres, shape_strong_thres);
    // If the masked region is too sparse to model, retry on the whole crop so
    // we never feed line2Dup a degenerate (crash-prone) template.
    if (fset.numFeatures() < 16)
    {
      LOGW("[shape] masked features=%d too few; retrying without mask", fset.numFeatures());
      fset = sbm::extractFeatures(templ_use, cv::Mat(), shape_num_features,
                                  shape_pyramid_T, shape_weak_thres, shape_strong_thres);
    }
    if (fset.numFeatures() < 16)
    {
      LOGE("[shape] only %d features extractable; aborting shape training", fset.numFeatures());
      return -1;
    }
    fset.setOrigin(origin_use.x, origin_use.y);
    // Report matched angles in the def reference frame: add back the registered
    // orientation of the saved image (0 when there is no def_image_reg).
    fset.setAngleOffset(angle_offset_deg);
    shapeFeatureSet = std::make_shared<sbm::FeatureSet>(fset);

    sbm::MatchConfig mc;
    mc.min_score        = shape_min_score;
    mc.refine           = sbm::RefineMode::ROI;
    mc.T_levels         = shape_pyramid_T;
    mc.weak_threshold   = shape_weak_thres;
    mc.strong_threshold = shape_strong_thres;
    mc.blur_kernel_size = shape_blur;
    mc.match_scale      = shape_match_scale;   // <1 = downscaled coarse match (ROI refine keeps accuracy)
    shapeMatcher = std::make_shared<sbm::ShapeMatcher>(mc);

    sbm::ModelConfig modc;
    modc.angle.start = 0; modc.angle.end = 360; modc.angle.step = shape_angle_step_deg;
    modc.flip = (matching_face == 0);   // match both faces when face==both
    int nv = shapeMatcher->addModel("def", *shapeFeatureSet, modc);
    if (nv <= 0) { LOGE("[shape] addModel failed (%d)", nv); return -1; }

    shape_ready = true;

    // Optional: render the ROI refine sample points (+ their search windows), the
    // signature silhouette mask, and the origin onto the template for inspection.
    //   SHAPE_DRAW_ROI=1 -> writes "<base>_roi.png" next to the template.
    if (getenv("SHAPE_DRAW_ROI"))
    {
      cv::Mat vis;                                  // drawn on the cropped template
      cv::cvtColor(templ_use, vis, cv::COLOR_GRAY2BGR);
      // silhouette mask outline (the part region the features were extracted from)
      if (!mask_use.empty())
      {
        std::vector<std::vector<cv::Point>> cts;
        cv::findContours(mask_use, cts, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        cv::drawContours(vis, cts, -1, cv::Scalar(255, 128, 0), 2);
      }
      const float tcx = templ_use.cols / 2.0f, tcy = templ_use.rows / 2.0f;
      // linemod (line2Dup) gradient features, finest pyramid level: pixel = f.{x,y}+tl
      if (!shapeFeatureSet->levels.empty())
      {
        auto &lv = shapeFeatureSet->levels[0];
        for (auto &f : lv.features)
          cv::circle(vis, cv::Point(f.x + lv.tl_x, f.y + lv.tl_y), 2, cv::Scalar(255, 255, 0), -1); // cyan
      }
      std::vector<cv::Point2f> rpts = shapeFeatureSet->selectOptimizedPoints(8);
      for (size_t i = 0; i < rpts.size(); i++)
      {
        cv::Point2f p(rpts[i].x + tcx, rpts[i].y + tcy);   // template-center relative -> pixel
        cv::rectangle(vis, cv::Rect((int)p.x - 15, (int)p.y - 15, 30, 30), cv::Scalar(0, 255, 0), 1); // roi_half window
        cv::circle(vis, p, 6, cv::Scalar(0, 0, 255), -1);
        cv::putText(vis, std::to_string((int)i), p + cv::Point2f(16, -10),
                    cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(0, 255, 255), 3);
      }
      cv::drawMarker(vis, origin_use, cv::Scalar(255, 0, 255),
                     cv::MARKER_CROSS, 40, 3);   // registration origin
      size_t dot = png.find_last_of('.');
      std::string outp = (dot == std::string::npos ? png : png.substr(0, dot)) + "_roi.png";
      cv::imwrite(outp, vis);
      fprintf(stderr, "[SHAPE_DRAW_ROI] wrote %s (%d roi points)\n", outp.c_str(), (int)rpts.size());
    }

    LOGI("[shape] trained from %s origin(%.1f,%.1f) variants=%d min_score=%.1f",
         png.c_str(), originPx.x, originPx.y, nv, shape_min_score);
    return 0;
  }
  catch (const std::exception &e)
  {
    LOGE("[shape] training exception: %s", e.what());
    return -1;
  }
}

int FeatureManager_sig360_circle_line::FeatureMatching_shape()
{
  // Debug toggle read once (process-wide), so the per-frame path costs nothing
  // when off -- no getenv in the hot loop.
  static const bool dbg = (getenv("SHAPE_DBG") != nullptr);
  // Test hook: add a deliberate offset (deg) to the located angle, to check how
  // well the anchor-warp re-registers and keeps measurements stable (env INSP_ANG_OFFSET).
  static const double angOffDeg = getenv("INSP_ANG_OFFSET") ? atof(getenv("INSP_ANG_OFFSET")) : 0.0;

  report.bacpac = bacpac;
  reports.resize(0);
  if (!shapeMatcher || originalImage_cv.empty()) return -1;

  cv::Mat scene;
  // Source is BGR-replicated grayscale (B=G=R), so pull channel 0 -- much cheaper
  // than a weighted cvtColor on a multi-megapixel frame.
  if (originalImage_cv.channels() == 1) scene = originalImage_cv;
  else cv::extractChannel(originalImage_cv, scene, 0);
  if (!scene.isContinuous()) scene = scene.clone();

  // Match the full scene. Scene downscaling for speed is the def's shape_match_scale
  // (handled inside ShapeMatcher); ROI refine restores full-res accuracy.
  std::vector<sbm::MatchResult> ms;
  try { ms = shapeMatcher->match(scene); }
  catch (const std::exception &e) { LOGE("[shape] match exception: %s", e.what()); return -1; }
  LOGI("[shape] matches=%d", (int)ms.size());
  if (dbg)
  {
    fprintf(stderr, "[SHAPE_DBG] scene %dx%d matches=%d mmpp=%.6f\n",
            scene.cols, scene.rows, (int)ms.size(), bacpac->sampler->mmpP_ideal());
    for (int z = 0; z < (int)ms.size() && z < 5; z++)
      fprintf(stderr, "[SHAPE_DBG]  m[%d] x=%.2f y=%.2f ang=%.3f scale=%.3f flip=%d score=%.1f\n",
              z, ms[z].x, ms[z].y, ms[z].angle, ms[z].scale, (int)ms[z].flipped, ms[z].score);
  }
  if (ms.empty()) return 0;

  float mmpp = bacpac->sampler->mmpP_ideal();

  // Grow the per-instance report pool (mirrors the CCL path's allocation).
  int want = (int)ms.size();
  if ((int)reportDataPool.size() < want)
  {
    int oriSize = reportDataPool.size();
    reportDataPool.resize(want);
    for (int i = oriSize; i < (int)reportDataPool.size(); i++)
    {
      reportDataPool[i].detectedCircles     = new vector<FeatureReport_circleReport>(0);
      reportDataPool[i].detectedLines       = new vector<FeatureReport_lineReport>(0);
      reportDataPool[i].detectedAuxPoints   = new vector<FeatureReport_auxPointReport>(0);
      reportDataPool[i].detectedSearchPoints= new vector<FeatureReport_searchPointReport>(0);
      reportDataPool[i].judgeReports        = new vector<FeatureReport_judgeReport>(0);
    }
  }

  // Calipers measure on the full-res original (same source sig360's eT uses).
  p_cropImg_cv = originalImage_cv;
  cropOffset.x = 0; cropOffset.y = 0;

  for (int mi = 0; mi < (int)ms.size(); mi++)
  {
    sbm::MatchResult &m = ms[mi];
    FeatureReport_sig360_circle_line_single singleReport =
        {
            .detectedCircles      = reportDataPool[reports.size()].detectedCircles,
            .detectedLines        = reportDataPool[reports.size()].detectedLines,
            .detectedAuxPoints    = reportDataPool[reports.size()].detectedAuxPoints,
            .detectedSearchPoints = reportDataPool[reports.size()].detectedSearchPoints,
            .judgeReports         = reportDataPool[reports.size()].judgeReports,
            .LTBound = acv_XY(0, 0),
            .RBBound = acv_XY(0, 0),
            .Center  = acv_XY(m.x, m.y),   // origin point in scene px (full-res)
            .area = 0,
            .pix_area = 0,
            .labeling_idx = mi,
            .scale = m.scale,
            .targetName = NULL};

    // Scene px -> ideal mm, exactly as the sig360 path converts its centroid.
    bacpac->sampler->img2ideal(&singleReport.Center);
    singleReport.Center = acvVecMult(singleReport.Center, mmpp);
    singleReport.LTBound = singleReport.Center;
    singleReport.RBBound = singleReport.Center;

    // line2Dup's rotation sign is inverted relative to the sig360 measurement
    // convention (TreeExecution/calipers). Flip the rotation-from-reference back
    // to that convention; the reference offset (reg angle, baked in via
    // setAngleOffset) is preserved so the upright case reads identically. The
    // matcher's flip path already negates internally, so flipped matches use the
    // angle as-is.
    float reg_deg = reg_angle_rad * 180.0f / (float)M_PI;
    float corr_deg = (m.flipped ? m.angle : (2.0f * reg_deg - m.angle)) + (float)angOffDeg;
    float ang = corr_deg * (float)M_PI / 180.0f;
    if (dbg)
      fprintf(stderr, "[SHAPE_DBG]  -> Center_mm=(%.4f,%.4f) m.angle=%.2f corr=%.2f flip=%d\n",
              singleReport.Center.x, singleReport.Center.y, m.angle, corr_deg, (int)m.flipped);
    int ret = SingleMatching_shape(bacpac, singleReport, ang, m.flipped, m.score / 100.0f);
    if (dbg)
      fprintf(stderr, "[SHAPE_DBG]     ret=%d rotate=%.4f\n", ret, singleReport.rotate);
    if (ret == 0) reports.push_back(singleReport);
  }
  return 0;
}

int FeatureManager_sig360_circle_line::SingleMatching_shape(
    FeatureManager_BacPac *bacpac,
    FeatureReport_sig360_circle_line_single &singleReport,
    float matched_angle, bool isInv, float similarity)
{
  vector<FeatureReport_circleReport>      &detectedCircles      = *singleReport.detectedCircles;
  vector<FeatureReport_lineReport>        &detectedLines        = *singleReport.detectedLines;
  vector<FeatureReport_auxPointReport>    &detectedAuxPoints    = *singleReport.detectedAuxPoints;
  vector<FeatureReport_searchPointReport> &detectedSearchPoints = *singleReport.detectedSearchPoints;
  vector<FeatureReport_judgeReport>       &judgeReports         = *singleReport.judgeReports;

  cv::Mat _eT_cv = p_cropImg_cv;                 // full-res original (set by caller)
  edgeTracking eT(_eT_cv, cropOffset, bacpac);

  { // pre-allocate + bind defs (mirror SingleMatching)
    detectedCircles.resize(featureCircleList.size());
    detectedLines.resize(featureLineList.size());
    detectedAuxPoints.resize(auxPointList.size());
    detectedSearchPoints.resize(searchPointList.size());
    judgeReports.resize(judgeList.size());
    for (int j = 0; j < featureLineList.size(); j++)   detectedLines[j].def        = &(featureLineList[j]);
    for (int j = 0; j < featureCircleList.size(); j++) detectedCircles[j].def      = &(featureCircleList[j]);
    for (int j = 0; j < judgeList.size(); j++)         judgeReports[j].def         = &(judgeList[j]);
    for (int j = 0; j < auxPointList.size(); j++)      detectedAuxPoints[j].def    = &(auxPointList[j]);
    for (int j = 0; j < searchPointList.size(); j++)   detectedSearchPoints[j].def = &(searchPointList[j]);
    RESET_REPORT(singleReport);
  }

  cm.center      = acv_XY(0, 0);
  cm.mode        = this->morph_mode;
  cm.reg         = this->morph_reg;
  cm.outlier_k   = this->morph_outlier_k;
  cm.min_anchors = this->morph_min_anchors;
  cm.tps_lambda  = this->morph_tps_lambda;

  singleReport.similarity = similarity;

  // ---- pose setup (mirror SingleMatching 5137-5167) ----
  float angle = matched_angle;
  float rot_report = angle;
  while (rot_report >   (float)M_PI) rot_report -= (float)(2 * M_PI);
  while (rot_report <= -(float)M_PI) rot_report += (float)(2 * M_PI);
  singleReport.rotate = rot_report;
  singleReport.isFlipped = isInv;

  angle = -angle;                                // rotate template to match object
  float flip_f = isInv ? -1 : 1;
  float mmpp = bacpac->sampler->mmpP_ideal();
  float ppmm = 1 / mmpp;
  acv_XY calibCen = singleReport.Center;
  calibCen.x *= ppmm;
  calibCen.y *= ppmm;
  float cached_cos = cos(angle);
  float cached_sin = sin(angle);

  // ---- locating-anchor morph loop (mirror SingleMatching 5172-5259) ----
  for (int j = 0; j < detectedSearchPoints.size(); j++)
    cm.anchorPairs[j].to = acv_XY(NAN, NAN);
  cm.resetTransform();
  std::vector<acv_XY> prev_to(cm.anchorPairs.size(), acv_XY(NAN, NAN));
  for (int k = 0; k < morph_max_iter; k++)
  {
    RESET_REPORT(singleReport);
    for (int j = 0; j < searchPointList.size(); j++)
    {
      if (searchPointList[j].data.anglefollow.locating_anchor == false) continue;
      TreeExecution(searchPointList[j].id, singleReport, eT,
                    calibCen, mmpp, cached_cos, cached_sin, flip_f);
    }
    for (int j = 0; j < detectedSearchPoints.size(); j++)
    {
      cm.anchorPairs[j].to = acv_XY(NAN, NAN);
      if (detectedSearchPoints[j].def->data.anglefollow.locating_anchor != true) continue;
      if (detectedSearchPoints[j].status != FeatureReport_sig360_circle_line_single::STATUS_SUCCESS) continue;
      acv_XY locatedPtOnTemplate = Image_mm_Domain_TO_TemplateDomain(
          detectedSearchPoints[j].pt, cached_sin, cached_cos, flip_f, calibCen, mmpp);
      cm.anchorPairs[j].to = locatedPtOnTemplate;
      if (synth_field_enabled()) {
        acv_XY d = synth_morph_field(cm.anchorPairs[j].from);
        cm.anchorPairs[j].to.x += d.x;
        cm.anchorPairs[j].to.y += d.y;
      }
    }
    cm.solve();
    if (k + 1 < morph_max_iter)
    {
      float maxd = 0; bool any_prev = false;
      for (size_t j = 0; j < cm.anchorPairs.size(); j++)
      {
        acv_XY cur = cm.anchorPairs[j].to;
        if (cur.x != cur.x) continue;
        if (prev_to[j].x == prev_to[j].x)
        {
          float d = acvDistance(cur, prev_to[j]);
          if (d > maxd) maxd = d;
          any_prev = true;
        }
        prev_to[j] = cur;
      }
      if (any_prev && maxd < morph_tol_mm) break;
    }
  }

  // ---- official measurement (mirror SingleMatching 5261-5314) ----
  RESET_REPORT(singleReport);
  for (int j = 0; j < judgeReports.size(); j++)
  {
    FeatureReport_judgeDef &def = *judgeReports[j].def;
    if (def.orientation_essential != true) continue;
    int status = TreeExecution(def.id, singleReport, eT,
                               calibCen, mmpp, cached_cos, cached_sin, flip_f);
    if (status != FeatureReport_sig360_circle_line_single::STATUS_SUCCESS)
      return -2;   // orientation-essential judge failed -> reject this detection
  }

  TreeExecution(singleReport, eT, calibCen, mmpp, cached_cos, cached_sin, flip_f);
  SET_UNSET_REPORT_NA(singleReport);
  return 0;
}

int ContourSignature::RESET(int Len)
{
  mean = NAN;
  sigma = NAN;
  signature_data.resize(Len);
  return 0;
}

bool ContourSignature::resampleFromCenter(acv_XY center)
{
  // v2 helper: rebuild signature_data from cartesian_ideal as seen from
  // `center`. Mirrors convertContourGrid2Signature exactly (same binning,
  // same per-segment interpolateSignData between consecutive walk-order
  // points, same max-R update). Critical for v2 -- different gap-fill
  // semantics produce a 180-deg-shifted signature that breaks rotation match.
  const int N = (int)signature_data.size();
  if (N == 0 || cartesian_ideal.empty()) return false;
  for (int i = 0; i < N; i++) { signature_data[i].x = 0; signature_data[i].y = 0; }
  int preIdx = -1;
  for (const acv_XY &p : cartesian_ideal)
  {
    float dx = p.x - center.x;
    float dy = p.y - center.y;
    if (dx != dx || dy != dy) continue;
    // Numerical-noise guard mirroring convertContourGrid2Signature's
    // `|diffY| < 0.1` px skip, scaled to the cartesian's unit (mm here) by
    // the caller via dy_skip_thres.
    if (dy_skip_thres > 0 && dy < dy_skip_thres && dy > -dy_skip_thres) continue;
    float theta = acvFAtan2(dy, dx);
    int idx = (int)round(N * theta / (2 * M_PI));
    if (idx < 0) idx += N;
    if (idx >= N) idx -= N;
    if (preIdx == -1) preIdx = idx;
    float R = hypot(dx, dy);
    if (signature_data[idx].x < R)
    {
      signature_data[idx].x = R;
      signature_data[idx].y = theta;
      interpolateSignData(signature_data, preIdx, idx);
    }
    preIdx = idx;
  }
  sample_center = center;
  return true;
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
    float x = signature_data[i].x;
    _mean += x;
    _sigma += x * x;

    float angleCenter = i * 2 * M_PI / signature_data.size();
    float angleDiff = signature_data[i].y - angleCenter;

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
    acv_XY dat((float)*pnum_mag, (float)*pnum_ang);

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
    error.push_back(acv_XY(offset, err));
  }

  return;
}
