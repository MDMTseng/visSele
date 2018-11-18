#include "FeatureManager_sig360_circle_line.h"
#include "logctrl.h"
#include <stdexcept>
#include <common_lib.h>
#include <MatchingCore.h>
#include <stdio.h>

/*
  FeatureManager_sig360_circle_line Section
*/
FeatureManager_sig360_circle_line::FeatureManager_sig360_circle_line(const char *json_str): FeatureManager_binary_processing(json_str)
{

  //LOGI(">>>>%s>>>>",json_str);
  root= NULL;
  int ret = reload(json_str);
  if(ret)
    throw std::invalid_argument( "Error:FeatureManager_sig360_circle_line failed... " );
}

bool FeatureManager_sig360_circle_line::check(cJSON *root)
{
  char *str;
  LOGI("FeatureManager_sig360_circle_line>>>");
  if(!(getDataFromJsonObj(root,"type",(void**)&str)&cJSON_String))
  {
    return false;
  }
  if (strcmp("sig360_circle_line",str) == 0)
  {
    return true;
  }
  return false;
}

double* json_get_num(cJSON *root,char* path, char* dbg_str)
{
  double *pnum;
  if((pnum=(double *)JFetch(root,path,cJSON_Number)) == NULL )
  {
    /*if(dbg_str)
    {
      LOGE("%s: Cannot get Number In path: %s",dbg_str,path);
    }
    else
    {
      LOGE("Cannot get Number In path: %s",path);
    }*/
    return NULL;
  }
  return pnum;
}

double* json_get_num(cJSON *root,char* path)
{
  return json_get_num(root,path,NULL);
}

#define JSON_GET_NUM(JROOT,PATH) json_get_num(JROOT,PATH,(char*)__func__)


char* json_find_str(cJSON *root, const char* key)
{
  char *str;
  if(!(getDataFromJsonObj(root,key,(void**)&str)&cJSON_String))
  {
    return NULL;
  }
  return str;
}

char* json_find_name(cJSON *root)
{
  return json_find_str(root,"name");;
}


int FeatureManager_sig360_circle_line::parse_arcData(cJSON * circle_obj)
{
  featureDef_circle cir;

  {
    char *tmpstr;
    cir.name[0] = '\0';
    if(tmpstr = json_find_name(circle_obj))
    {
      strcpy(cir.name,tmpstr);
    }
  }

  double *pnum;

  if((pnum=JSON_GET_NUM(circle_obj,"id")) == NULL )
  {
    LOGE("No id");
    return -1;
  }
  cir.id = (int)*pnum;
  LOGV("feature is an arc:%s %d",cir.name, cir.id);



  if((pnum=JSON_GET_NUM(circle_obj,"margin")) == NULL )
  {
    LOGE("No margin can be found");
    return -1;
  }


  cir.initMatchingMargin=*pnum;



  acv_XY pt1,pt2,pt3;
  if((pnum=JSON_GET_NUM(circle_obj,"pt1.x")) == NULL )
  {
    LOGE("No pt1.x can be found");
    return -1;
  }
  pt1.X=*pnum;


  int pt1y;
  if((pnum=JSON_GET_NUM(circle_obj,"pt1.y")) == NULL )
  {
    LOGE("No pt1.y can be found");
    return -1;
  }
  pt1.Y=*pnum;


  if((pnum=JSON_GET_NUM(circle_obj,"pt2.x")) == NULL )
  {
    LOGE("No pt2.x can be found");
    return -1;
  }
  pt2.X=*pnum;


  int pt2y;
  if((pnum=JSON_GET_NUM(circle_obj,"pt2.y")) == NULL )
  {
    LOGE("No pt2.y can be found");
    return -1;
  }
  pt2.Y=*pnum;


  int pt3x;
  if((pnum=JSON_GET_NUM(circle_obj,"pt3.x")) == NULL )
  {
    LOGE("No pt3.x can be found");
    return -1;
  }
  pt3.X=*pnum;


  int pt3y;
  if((pnum=JSON_GET_NUM(circle_obj,"pt3.y")) == NULL )
  {
    LOGE("No pt3.y can be found");
    return -1;
  }
  pt3.Y=*pnum;

  acv_XY circumcenter = acvCircumcenter(pt1,pt2,pt3);
  cir.circleTar.circumcenter=circumcenter;
  cir.circleTar.radius=hypot(circumcenter.X-pt2.X,circumcenter.Y-pt2.Y);

  LOGV("x:%f y:%f r:%f margin:%f",
  cir.circleTar.circumcenter.X,
  cir.circleTar.circumcenter.Y,
  cir.circleTar.radius,
  cir.initMatchingMargin);
  if(cir.name[0]=='\0')
  {
    sprintf(cir.name,"@CIRCLE_%d",featureCircleList.size());
  }
  featureCircleList.push_back(cir);
  return 0;
}

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
}


int FeatureManager_sig360_circle_line::FindFeatureDefIndex(vector<featureDef_circle> &list, char* name)
{
  if(!name)return -1;
  for(int i=0;i<list.size();i++)
  {
    if(strcmp(list[i].name, name)==0)
    {
      return i;
    }
  }
  return -1;
}

int FeatureManager_sig360_circle_line::FindFeatureDefIndex(vector<featureDef_line> &list, char* name)
{
  if(!name)return -1;
  for(int i=0;i<list.size();i++)
  {
    if(strcmp(list[i].name, name)==0)
    {
      return i;
    }
  }
  return -1;
}



FeatureReport_judgeReport FeatureManager_sig360_circle_line::measure_process(FeatureReport_sig360_circle_line_single &report,struct FeatureReport_judgeDef &judge)
{

  //vector<FeatureReport_judgeReport> &judgeReport = *report.judgeReports;
  FeatureReport_judgeReport judgeReport={0};
  judgeReport.def = &judge;
  LOGV("judge:%s  OBJ1:%s, OBJ2:%s type:%d, swapped:%d ",judge.name,judge.OBJ1,judge.OBJ2,judge.measure_type,judge.swap);
  LOGV("OBJ1_type:%d idx:%d   OBJ2_type:%d idx:%d ",judge.OBJ1_type,judge.OBJ1_idx,judge.OBJ2_type,judge.OBJ2_idx);
  LOGV("-val:%f  margin:%f",judge.targetVal,judge.targetVal_margin);

  if(judge.OBJ1_type == FeatureReport_judgeDef::LINE)
  {
    acv_LineFit OBJ1 = (*report.detectedLines)[judge.OBJ1_idx].line;
    if(judge.OBJ2_type == FeatureReport_judgeDef::NONE)
    {
      switch(judge.measure_type)
      {
        case FeatureReport_judgeDef::AREA:
          LOGI("AREA:%d",OBJ1.matching_pts);
          judgeReport.measured_val = OBJ1.matching_pts;
        break;
        case FeatureReport_judgeDef::SIGMA:
          LOGI("SIGMA:%f",OBJ1.s);
          judgeReport.measured_val = OBJ1.s;
        break;
        default:
          LOGE("judge.measure_type:%d is not supported",judge.measure_type);
      }
    }
    else if(judge.OBJ2_type == FeatureReport_judgeDef::LINE)
    {
      acv_LineFit OBJ2 = (*report.detectedLines)[judge.OBJ2_idx].line;
      switch(judge.measure_type)
      {
        case FeatureReport_judgeDef::ANGLE:
        {
          float angle = acvLineAngle(OBJ1.line,OBJ2.line);
          LOGI("angle:%f",angle*180/M_PI);
          judgeReport.measured_val = angle;
        }
        break;
        case FeatureReport_judgeDef::DISTANCE:
        {
          acv_XY vec= OBJ1.end_pos;
          vec = acvVecAdd(vec,OBJ1.end_neg);
          vec = acvVecAdd(vec,OBJ1.end_pos);
          vec = acvVecMult(vec,-1);
          vec = acvVecAdd(vec,OBJ2.end_neg);
          vec = acvVecAdd(vec,OBJ2.end_neg);
          float distance = hypot(vec.X,vec.Y)/2;
          LOGI("distance:%f",distance);
          judgeReport.measured_val = distance;
        }
        break;
        default:
          LOGE("judge.measure_type:%d is not supported",judge.measure_type);
      }
    }
    else if(judge.OBJ2_type == FeatureReport_judgeDef::CIRCLE)
    {
      acv_CircleFit OBJ2 = (*report.detectedCircles)[judge.OBJ2_idx].circle;
      switch(judge.measure_type)
      {
        case FeatureReport_judgeDef::DISTANCE:
        {
          acv_XY vec= OBJ1.end_pos;
          vec = acvVecAdd(vec,OBJ1.end_neg);
          vec = acvVecAdd(vec,OBJ1.end_pos);
          vec = acvVecMult(vec,-0.5);
          vec = acvVecAdd(vec,OBJ2.circle.circumcenter);
          float distance = hypot(vec.X,vec.Y);
          LOGI("distance:%f",distance);
          judgeReport.measured_val = distance;
        }
        break;
        default:
          LOGE("judge.measure_type:%d is not supported",judge.measure_type);
      }
    }
  }
  else if(judge.OBJ1_type == FeatureReport_judgeDef::CIRCLE)
  {
    acv_CircleFit OBJ1 = (*report.detectedCircles)[judge.OBJ1_idx].circle;
    if(judge.OBJ2_type == FeatureReport_judgeDef::NONE)
    {
      switch(judge.measure_type)
      {
        case FeatureReport_judgeDef::AREA:
          LOGI("AREA:%d",OBJ1.matching_pts);
          judgeReport.measured_val = OBJ1.matching_pts;
        break;
        case FeatureReport_judgeDef::SIGMA:
          LOGI("SIGMA:%f",OBJ1.s);
          judgeReport.measured_val = OBJ1.s;
        break;
        default:
          LOGE("judge.measure_type:%d is not supported",judge.measure_type);
      }
    }
    else if(judge.OBJ2_type == FeatureReport_judgeDef::CIRCLE)
    {
      acv_CircleFit OBJ2 = (*report.detectedCircles)[judge.OBJ2_idx].circle;
      switch(judge.measure_type)
      {
        case FeatureReport_judgeDef::DISTANCE:
        {
          float distance = acvDistance(OBJ1.circle.circumcenter,OBJ2.circle.circumcenter);
          LOGI("distance:%f",distance);
          judgeReport.measured_val = distance;
        }
        break;
        default:
          LOGE("judge.measure_type:%d is not supported",judge.measure_type);
      }
    }
  }






  LOGV("===================");

  return judgeReport;
}

int FeatureManager_sig360_circle_line::parse_search_key_points_Data(cJSON *kspArr_obj,vector<featureDef_line::searchKeyPoint> &skpsList)
{
  LOGI("It's key point search data");
  skpsList.resize(0);

  for (int i = 0 ; i < cJSON_GetArraySize(kspArr_obj) ; i++)
  {
    featureDef_line::searchKeyPoint skp;
    cJSON *jobj;
    if(!(getDataFromJsonObj(kspArr_obj,i,(void**)&jobj)&cJSON_Object))
    {
      return -1;
    }


    double *pnum;
    if((pnum=JSON_GET_NUM(jobj,"x")) == NULL )
    {
      return -1;
    }
    skp.searchStart.X=*pnum;

    if((pnum=JSON_GET_NUM(jobj,"y")) == NULL )
    {
      return -1;
    }
    skp.searchStart.Y=*pnum;

    if((pnum=JSON_GET_NUM(jobj,"vx")) == NULL )
    {
      return -1;
    }
    skp.searchVec.X=*pnum;

    if((pnum=JSON_GET_NUM(jobj,"vy")) == NULL )
    {
      return -1;
    }
    skp.searchVec.Y=*pnum;
    skp.searchVec=acvVecNormalize(skp.searchVec);

    if((pnum=JSON_GET_NUM(jobj,"searchDist")) == NULL )
    {
      return -1;
    }
    skp.searchDist=*pnum;


    LOGV("[%d]={x:%f,y:%f,vx:%f,vy:%f,sdist:%f}",
      i,
      skp.searchStart.X,skp.searchStart.Y,
      skp.searchVec.X,skp.searchVec.Y,
      skp.searchDist
    );
    skpsList.push_back(skp);
  }


}


int FeatureManager_sig360_circle_line::parse_searchPointData(cJSON * jobj)
{

  featureDef_searchPoint searchPoint;

  {
    char *tmpstr;
    searchPoint.name[0] = '\0';
    if(tmpstr = json_find_name(jobj))
    {
      strcpy(searchPoint.name,tmpstr);
    }
  }

  double *pnum;

  if((pnum=JSON_GET_NUM(jobj,"id")) == NULL )
  {
    LOGE("No id");
    return -1;
  }
  searchPoint.id = (int)*pnum;
  LOGV("feature is an searchPoint:%s %d",searchPoint.name, searchPoint.id);

  //The subtype might be determined by definition file 
  searchPoint.subtype = featureDef_searchPoint::anglefollow;

  switch(searchPoint.subtype)
  {
    case featureDef_searchPoint::anglefollow:
    {
      if((pnum=JSON_GET_NUM(jobj,"angle")) == NULL )
      {
        LOGE("No angle");
        return -1;
      }
      searchPoint.data.anglefollow.angle=*pnum;

      if((pnum=JSON_GET_NUM(jobj,"ref[0].id")) == NULL )
      {
        LOGE("No target_id");
        return -1;
      }
      searchPoint.data.anglefollow.target_id=(int)*pnum;


      if((pnum=JSON_GET_NUM(jobj,"pt1.x")) == NULL )
      {
        LOGE("No pt1.x");
        return -1;
      }
      searchPoint.data.anglefollow.position.X=*pnum;


      if((pnum=JSON_GET_NUM(jobj,"pt1.y")) == NULL )
      {
        LOGE("No pt1.y");
        return -1;
      }
      searchPoint.data.anglefollow.position.Y=*pnum;


      LOGV("searchPoint.X:%f searchPoint.Y:%f searchPoint.angle:%f searchPoint.tar_id:%d",
      searchPoint.data.anglefollow.position.X,
      searchPoint.data.anglefollow.position.Y,
      searchPoint.data.anglefollow.angle,
      searchPoint.data.anglefollow.target_id
      );



    }
    break;
    default:
      return -1;
  }
  
  searchPointList.push_back(searchPoint);



  return 0;
}

int FeatureManager_sig360_circle_line::parse_auxPointData(cJSON * jobj)
{
  featureDef_auxPoint auxPoint;

  {
    char *tmpstr;
    auxPoint.name[0] = '\0';
    if(tmpstr = json_find_name(jobj))
    {
      strcpy(auxPoint.name,tmpstr);
    }
  }

  double *pnum;

  if((pnum=JSON_GET_NUM(jobj,"id")) == NULL )
  {
    LOGE("No id");
    return -1;
  }
  auxPoint.id = (int)*pnum;
  LOGV("feature is an auxPoint:%s %d  skip...",auxPoint.name, auxPoint.id);


  //The subtype might be determined by definition file 
  auxPoint.subtype = featureDef_auxPoint::lineCross;



  switch(auxPoint.subtype)
  {
    case featureDef_auxPoint::lineCross:
    {
      if((pnum=JSON_GET_NUM(jobj,"ref[0].id")) == NULL )
      {
        LOGE("No target[0]_id");
        return -1;
      }
      auxPoint.data.lineCross.line1_id=(int)*pnum;

      if((pnum=JSON_GET_NUM(jobj,"ref[1].id")) == NULL )
      {
        LOGE("No target[1]_id");
        return -1;
      }
      auxPoint.data.lineCross.line2_id=(int)*pnum;


      LOGV("id_1:%d id_2:%d",
        auxPoint.data.lineCross.line1_id,auxPoint.data.lineCross.line2_id
      );



    }
    break;
    default:
      return -1;
  }
  
  auxPointList.push_back(auxPoint);







  return 0;
}
int FeatureManager_sig360_circle_line::parse_lineData(cJSON * line_obj)
{
  featureDef_line line;
  line.MatchingMarginX=0;
  double *pnum;

  {
    char *tmpstr;
    line.name[0] = '\0';
    if(tmpstr = json_find_name(line_obj))
    {
      strcpy(line.name,tmpstr);
    }
  }
  
  if((pnum=JSON_GET_NUM(line_obj,"id")) == NULL )
  {
    LOGE("No id");
    return -1;
  }
  line.id = (int)*pnum;

  LOGV("feature is a line:%s %d",line.name,line.id);

  if((pnum=JSON_GET_NUM(line_obj,"margin")) == NULL )
  {
    LOGE("No margin");

    return -1;
  }
  line.initMatchingMargin=*pnum;


  if((pnum=JSON_GET_NUM(line_obj,"MatchingMarginX")) == NULL )
  {
    LOGI("The MatchingMarginX isn't there will be generated later on...");
  }
  else
  {
    line.MatchingMarginX=*pnum;
  }

  cJSON *kspArr_obj=(cJSON *)JFetch(line_obj,"searchKeyPoints",cJSON_Array);
  if(kspArr_obj)
  {
    int ret = parse_search_key_points_Data(kspArr_obj,line.skpsList);
    line.MatchingMarginX=find_search_key_points_longest_distance(line.skpsList)/2;

    featureLineList.push_back(line);
    return 0;
  }




  acv_XY p0,p1;
  if((pnum=JSON_GET_NUM(line_obj,"pt1.x")) == NULL )
  {
    LOGE("No pt1.x");
    return -1;
  }
  p0.X=*pnum;

  if((pnum=JSON_GET_NUM(line_obj,"pt1.y")) == NULL )
  {
    LOGE("No pt1.y");
    return -1;
  }
  p0.Y=*pnum;

  if((pnum=JSON_GET_NUM(line_obj,"pt2.x")) == NULL )
  {
    LOGE("No pt2.x");
    return -1;
  }
  p1.X=*pnum;

  if((pnum=JSON_GET_NUM(line_obj,"pt2.y")) == NULL )
  {
    LOGE("No pt2.y");
    return -1;
  }
  p1.Y=*pnum;

  if(line.MatchingMarginX==0)
  {
    line.MatchingMarginX=hypot(p0.X-p1.X,p0.Y-p1.Y)/2;
  }
  line.lineTar.line_anchor.X=(p0.X+p1.X)/2;
  line.lineTar.line_anchor.Y=(p0.Y+p1.Y)/2;
  line.lineTar.line_vec.X=(p1.X-p0.X);
  line.lineTar.line_vec.Y=(p1.Y-p0.Y);
  line.lineTar.line_vec = acvVecNormalize(line.lineTar.line_vec);



  acv_XY normal = acvVecNormal(line.lineTar.line_vec);

  line.searchVec=normal;
  line.searchDist=line.initMatchingMargin*2;

  line.searchEstAnchor = line.lineTar.line_anchor;
  line.searchEstAnchor.X-=normal.X*line.initMatchingMargin;
  line.searchEstAnchor.Y-=normal.Y*line.initMatchingMargin;



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

  if(line.name[0]=='\0')
  {
    sprintf(line.name,"@LINE_%d",featureLineList.size());
  }


  featureLineList.push_back(line);

  return 0;
}

int FeatureManager_sig360_circle_line::parse_signatureData(cJSON * signature_obj)
{

  if( feature_signature.size()!=0 )
  {
    LOGE("feature_signature:size()=%d is set already. There can only be one signature feature.",feature_signature.size());
    return -1;
  }

  cJSON *param;
  if(!(getDataFromJsonObj(signature_obj,"param",(void**)&param)&cJSON_Object))
  {
    return -1;
  }

  cJSON *signature_magnitude;
  cJSON *signature_angle;
  if(!(getDataFromJsonObj(param,"magnitude",(void**)&signature_magnitude)&cJSON_Array))
  {
    LOGE("The signature_magnitude is not an cJSON_Array");
    return -1;
  }

  if(!(getDataFromJsonObj(param,"angle",(void**)&signature_angle)&cJSON_Array))
  {
    LOGE("The signature_angle is not an cJSON_Array");
    return -1;
  }

  if( cJSON_GetArraySize(signature_magnitude) != cJSON_GetArraySize(signature_angle) )
  {
    LOGE("The signature_angle and signature_magnitude doesn't have same length");
    return -1;
  }

  if( cJSON_GetArraySize(signature_magnitude) != 360 )
  {
    LOGE("The signature size(%d) is not equal to 360",cJSON_GetArraySize(signature_magnitude));
    return -1;
  }


  for (int i = 0 ; i < cJSON_GetArraySize(signature_magnitude) ; i++)
  {
    double *pnum_mag;
    if(!(getDataFromJsonObj(signature_magnitude,i,(void**)&pnum_mag)&cJSON_Number))
    {
      return -1;
    }

    double *pnum_ang;
    if(!(getDataFromJsonObj(signature_angle,i,(void**)&pnum_ang)&cJSON_Number))
    {
      return -1;
    }
    acv_XY dat={.X=*pnum_mag,.Y=*pnum_ang};

    feature_signature.push_back(dat);
    /*cJSON * feature = cJSON_GetArrayItem(signature, i);
    LOGI(" %f",type_str,ver_str,unit_str);*/
  }

  LOGV("feature is a signature");


  return 0;
}

int FeatureManager_sig360_circle_line::parse_judgeData(cJSON * judge_obj)
{

  FeatureReport_judgeDef judge={0};

  {
    char *tmpstr;
    judge.name[0] = '\0';
    if(tmpstr = json_find_name(judge_obj))
    {
      strcpy(judge.name,tmpstr);
    }
  }

  if(judge.name[0]=='\0')
  {
    sprintf(judge.name,"@JUDGE_%d",judgeList.size());
  }

  char * OBJ1_name = json_find_str(judge_obj, "OBJ1");
  if(OBJ1_name == NULL)
  {
    LOGE("OBJ1:Cannot find the name of it");
    return -1;
  }
  strcpy(judge.OBJ1,OBJ1_name);

  judge.OBJ1_type = FeatureReport_judgeDef::NONE;
  int idx1;
  if((idx1 = FindFeatureDefIndex(featureLineList, judge.OBJ1)) >= 0)
  {
    judge.OBJ1_type = FeatureReport_judgeDef::LINE;
  }
  else if((idx1 = FindFeatureDefIndex(featureCircleList, judge.OBJ1)) >= 0 )
  {
    judge.OBJ1_type = FeatureReport_judgeDef::CIRCLE;
  }
  else
  {
    LOGE("OBJ1:Cannot find the feature that has name:%s",judge.OBJ1);
    return -1;
  }
  judge.OBJ1_idx = idx1;



  char * OBJ2_name = json_find_str(judge_obj, "OBJ2");
  if(OBJ2_name)
  {
    strcpy(judge.OBJ2,OBJ2_name);
  }

  int idx2;
  judge.OBJ2_type = FeatureReport_judgeDef::NONE;
  if(judge.OBJ2[0]=='\0')
  {
    idx2 = -1;
    LOGI("OBJ2:Is NULL");
  }
  else if((idx2 = FindFeatureDefIndex(featureLineList, judge.OBJ2)) >=0)
  {
    judge.OBJ2_type = FeatureReport_judgeDef::LINE;
  }
  else if((idx2 = FindFeatureDefIndex(featureCircleList, judge.OBJ2)) >=0)
  {
    judge.OBJ2_type = FeatureReport_judgeDef::CIRCLE;
  }
  else
  {
    LOGE("OBJ2:Cannot find the feature that has name:%s",judge.OBJ2);
    return -1;
  }
  judge.OBJ2_idx = idx2;


  double * measure_val =NULL;

  if( measure_val = json_get_num(judge_obj,"sigma"))
  {
    judge.measure_type = FeatureReport_judgeDef::SIGMA;
    judge.targetVal = *measure_val;
    measure_val = json_get_num(judge_obj,"sigma_margin");
    if(measure_val == NULL)return -1;
    judge.targetVal_margin = *measure_val;
  }else
  if( measure_val = json_get_num(judge_obj,"angle"))
  {
    judge.measure_type = FeatureReport_judgeDef::ANGLE;
    judge.targetVal = *measure_val;
    measure_val = json_get_num(judge_obj,"angle_margin");
    if(measure_val == NULL)return -1;
    judge.targetVal_margin = *measure_val;
  }else
  if( measure_val = json_get_num(judge_obj,"distance"))
  {
    judge.measure_type = FeatureReport_judgeDef::DISTANCE;
    judge.targetVal = *measure_val;
    measure_val = json_get_num(judge_obj,"distance_margin");
    if(measure_val == NULL)return -1;
    judge.targetVal_margin = *measure_val;
  }else
  if( measure_val = json_get_num(judge_obj,"area"))
  {
    judge.measure_type = FeatureReport_judgeDef::AREA;
    judge.targetVal = *measure_val;
    measure_val = json_get_num(judge_obj,"area_margin");
    if(measure_val == NULL)return -1;
    judge.targetVal_margin = *measure_val;
  }
  else
  {
    return -1;
  }



  if( judge.OBJ2_type!= FeatureReport_judgeDef::NONE && judge.OBJ1_type>judge.OBJ2_type )
  {//Make sure the OBJ1_type is always less than OBJ2_type, so that we don't need to do reverse compatiable judgement: like line2circle and circle2line
    FeatureReport_judgeDef tmp_judge=judge;
    judge.swap=true;
    judge.OBJ1_type = tmp_judge.OBJ2_type;
    judge.OBJ2_type = tmp_judge.OBJ1_type;

    judge.OBJ1_idx = tmp_judge.OBJ2_idx;
    judge.OBJ2_idx = tmp_judge.OBJ1_idx;

    strcpy(judge.OBJ1,tmp_judge.OBJ2);
    strcpy(judge.OBJ2,tmp_judge.OBJ1);
  }

  judgeList.push_back(judge);
  return 0;
}



int FeatureManager_sig360_circle_line::parse_jobj()
{
  const char *type_str= (char *)JFetch(root,"type",cJSON_String);
  const char *ver_str = (char *)JFetch(root,"ver",cJSON_String);
  const char *unit_str =(char *)JFetch(root,"unit",cJSON_String);
  if(type_str==NULL||ver_str==NULL||unit_str==NULL)
  {
    LOGE("ptr: type:<%p>  ver:<%p>  unit:<%p>",type_str,ver_str,unit_str);
    return -1;
  }
  LOGI("type:<%s>  ver:<%s>  unit:<%s>",type_str,ver_str,unit_str);


  cJSON *featureList = cJSON_GetObjectItem(root,"features");
  cJSON *inherentfeatureList = cJSON_GetObjectItem(root,"inherentfeatures");

  if(featureList==NULL)
  {
    LOGE("features array does not exists");
    return -1;
  }

  if(!cJSON_IsArray(featureList))
  {
    LOGE("features is not an array");
    return -1;
  }

  for (int i = 0 ; i < cJSON_GetArraySize(featureList) ; i++)
  {
     cJSON * feature = cJSON_GetArrayItem(featureList, i);
     cJSON * tmp_obj = cJSON_GetObjectItem(feature, "type");
     if(tmp_obj == NULL)
     {
       LOGE("feature[%d] has no type...",i);
       return -1;
     }
     const char *feature_type =tmp_obj->valuestring;
     if(strcmp(feature_type, "arc")==0)
     {
       if(parse_arcData(feature)!=0)
       {
         LOGE("feature[%d] has error %s format",i,feature_type);
         return -1;
       }
     }
     else if(strcmp(feature_type, "line")==0)
     {
       if(parse_lineData(feature)!=0)
       {
         LOGE("feature[%d] has error %s format",i,feature_type);
         return -1;
       }
     }
     else if(strcmp(feature_type, "feature_signature")==0)
     {
       if(parse_signatureData(feature)!=0)
       {
         LOGE("feature[%d] has error %s format",i,feature_type);
         return -1;
       }
     }
     else if(strcmp(feature_type, "aux_point")==0)
     {
       
       if(parse_auxPointData(feature)!=0)
       {
         LOGE("feature[%d] has error %s format",i,feature_type);
         return -1;
       }
     }
     else if(strcmp(feature_type, "search_point")==0)
     {
       
       if(parse_searchPointData(feature)!=0)
       {
         LOGE("feature[%d] has error %s format",i,feature_type);
         return -1;
       }
     }
     else if(strcmp(feature_type, "judge")==0)
     {
     }
     else
     {
       LOGE("feature[%d] has unknown type:[%s]",i,feature_type);
       return -1;
     }
  }


  for (int i = 0 ; i < cJSON_GetArraySize(featureList) ; i++)
  {
     cJSON * feature = cJSON_GetArrayItem(featureList, i);
     cJSON * tmp_obj = cJSON_GetObjectItem(feature, "type");
     if(tmp_obj == NULL)
     {
       LOGE("feature[%d] has no type...",i);
       continue;
     }
     const char *feature_type =tmp_obj->valuestring;
     if(strcmp(feature_type, "judge")==0)
     {
       if(parse_judgeData(feature)!=0)
       {
         LOGE("feature[%d] has error %s format",i,feature_type);
         return -1;
       }
     }
     else
     {
       continue;
     }
  }


  {
    for(int i=0;i<featureLineList.size();i++)
    {
      LOGV("featureLineList[%d]:%s",i,featureLineList[i].name);
    }
    for(int i=0;i<featureCircleList.size();i++)
    {
      LOGV("featureCircleList[%d]:%s",i,featureCircleList[i].name);
    }
    for(int i=0;i<judgeList.size();i++)
    {
      LOGV("judgeList[%d]:%s  OBJ1:%s, OBJ2:%s type:%d",i,judgeList[i].name,judgeList[i].OBJ1,judgeList[i].OBJ2,judgeList[i].measure_type);
      LOGV("-val:%f  margin:%f",judgeList[i].targetVal,judgeList[i].targetVal_margin);
    }
  }


  if(feature_signature.size()==0)
  {
    LOGE("No signature data");
    return -1;
  }

  return 0;
}


int FeatureManager_sig360_circle_line::reload(const char *json_str)
{
  if(root)
  {
    cJSON_Delete(root);
  }
  featureCircleList.resize(0);
  featureLineList.resize(0);
  judgeList.resize(0);

  auxPointList.resize(0);
  searchPointList.resize(0);
  feature_signature.resize(0);



  root = cJSON_Parse(json_str);
  if(root==NULL)
  {
    LOGE("cJSON parse failed");
    return -1;
  }
  int ret_err = parse_jobj();
  if(ret_err!=0)
  {
    featureCircleList.resize(0);
    featureLineList.resize(0);
    feature_signature.resize(0);
    reload("");
    return -2;
  }
  return 0;
}

int searchP(acvImage *img, acv_XY *pos, acv_XY searchVec, float maxSearchDist)
{
  if(img ==NULL || pos==NULL )return -1;
  int X=(int)round(pos->X);
  int Y=(int)round(pos->Y);
  if(X<0 || Y<0 || X>=img->GetWidth() || Y>=img->GetHeight())
  {
    return -1;
  }

  searchVec = acvVecNormalize(searchVec);

  int tarX=0;
  if(img->CVector[Y][3*X]==255)
  {
    tarX=0;//Looking for non-255
  }
  else
  {
    tarX=255;//Looking for 255
    searchVec.X*=-1;//reverse search vector
    searchVec.Y*=-1;
  }

  for(int i=0;i<maxSearchDist;i++)
  {
    X=(int)round(pos->X+searchVec.X*i);
    Y=(int)round(pos->Y+searchVec.Y*i);
    if(X<0 || Y<0 || X>=img->GetWidth() || Y>=img->GetHeight())
    {
      return -1;
    }

    if(img->CVector[Y][3*X]==255)
    {
      if(tarX == 255)
      {
        pos->X=pos->X+searchVec.X*(i-1);//Get previous non-255
        pos->Y=pos->Y+searchVec.Y*(i-1);
        return 0;
      }
    }
    else
    {
      if(tarX != 255)
      {
        pos->X=pos->X+searchVec.X*(i);
        pos->Y=pos->Y+searchVec.Y*(i);
        return 0;
      }
    }

  }

  return -1;
}


const FeatureReport* FeatureManager_sig360_circle_line::GetReport()
{
  report.type = FeatureReport::sig360_circle_line;
  //report.error = FeatureReport_sig360_circle_line::NONE;
  report.data.sig360_circle_line.reports = &reports;
  return &report;
}

int FeatureManager_sig360_circle_line::FeatureMatching(acvImage *img,acvImage *buff,vector<acv_LabeledData> &ldData,acvImage *dbg)
{

  int grid_size = 50;
  inward_curve_grid.RESET(grid_size,img->GetWidth(),img->GetHeight());
  straight_line_grid.RESET(grid_size,img->GetWidth(),img->GetHeight());



  tmp_signature.resize(feature_signature.size());
  reports.resize(0);
  int scanline_skip=15;
  extractContourDataToContourGrid(buff,grid_size,inward_curve_grid, straight_line_grid,scanline_skip);


  if(1)//Draw debug image(curve and straight line)
  {
    for(int i=0;i<inward_curve_grid.dataSize();i++)
    {

      const acv_XY* p = inward_curve_grid.get(i);
      int X = round(p->X);
      int Y = round(p->Y);
      {
            buff->CVector[Y][X*3]=0;
            buff->CVector[Y][X*3+1]=100;
            buff->CVector[Y][X*3+2]=255;
      }


    }


    for(int i=0;i<straight_line_grid.dataSize();i++)
    {
        const acv_XY* p2 = straight_line_grid.get(i);
        int X = round(p2->X);
        int Y = round(p2->Y);
        {
              buff->CVector[Y][X*3]=0;
              buff->CVector[Y][X*3+1]=255;
              buff->CVector[Y][X*3+2]=100;
        }
    }
  }

  static vector<int> s_intersectIdxs;
  static vector<acv_XY> s_points;
  float sigma;
  int count = 0;

  {
      if(reportDataPool.size()<ldData.size())
      {
        int oriSize = reportDataPool.size();
        reportDataPool.resize(ldData.size());

        for(int i=oriSize;i<reportDataPool.size();i++)
        {
          reportDataPool[i].detectedCircles = new vector<FeatureReport_circleReport>(0);
          reportDataPool[i].detectedLines = new vector<FeatureReport_lineReport>(0);
          reportDataPool[i].detectedAuxLines = new vector<acv_Line>(0);
          reportDataPool[i].detectedAuxPoints = new vector<acv_XY>(0);
          reportDataPool[i].judgeReports = new vector<FeatureReport_judgeReport>(0);
        }
      }
  }


  for (int i = 1; i < ldData.size(); i++,count++)
  {
      if(ldData[i].area<120)continue;


      acvContourCircleSignature(img, ldData[i], i, tmp_signature);

      bool isInv;
      float angle;
      float error = SignatureMinMatching( tmp_signature,feature_signature,
        &isInv, &angle);

      LOGV("======%d===er:%f,inv:%d,ang:%f",i,error,isInv,angle*180/3.14159);

      FeatureReport_sig360_circle_line_single singleReport=
      {
          .detectedCircles=reportDataPool[count].detectedCircles,
          .detectedLines=reportDataPool[count].detectedLines,
          .detectedAuxLines=reportDataPool[count].detectedAuxLines,
          .detectedAuxPoints=reportDataPool[count].detectedAuxPoints,
          .judgeReports = reportDataPool[count].judgeReports,
          .LTBound=ldData[i].LTBound,
          .RBBound=ldData[i].RBBound,
          .Center=ldData[i].Center,
          .area=ldData[i].area,
          .rotate=angle,
          .isFlipped=isInv,
          .scale=1,
          .targetName=NULL,
      };
      reports.push_back(singleReport);

      vector<FeatureReport_circleReport> &detectedCircles = *singleReport.detectedCircles;
      vector<FeatureReport_lineReport> &detectedLines = *singleReport.detectedLines;

      vector<acv_Line> &detectedAuxLines = *singleReport.detectedAuxLines;
      vector<acv_XY> &detectedAuxPoints = *singleReport.detectedAuxPoints;
      vector<FeatureReport_judgeReport> &judgeReports = *singleReport.judgeReports;

      detectedCircles.resize(0);
      detectedLines.resize(0);
      detectedAuxLines.resize(0);
      detectedAuxPoints.resize(0);


      float cached_cos,cached_sin;
      //The angle we get from matching is current object rotates 'angle' to match target
      //But now, we want to rotate feature set to match current object, so opposite direction
      angle=-angle;
      cached_cos=cos(angle);
      cached_sin=sin(angle);
      float flip_f=1;
      if(isInv)
      {
        flip_f=-1;
      }


      acv_LineFit lf_zero = {0};
      for (int j = 0; j < featureLineList.size(); j++)
      {
        int mult=100;
        featureDef_line *line = &featureLineList[j];

        acv_Line line_cand;
        if(line->skpsList.size()!=0)
        {
          if(line->skpsList.size()<2)
          {
            LOGE("skpListSize:%d <2 not enough",line->skpsList.size());
            FeatureReport_lineReport lr;
            lr.line=lf_zero;
            lr.def=line;

            detectedLines.push_back(lr);
            continue;
            //Error
          }
          s_points.resize(0);
          for(int k=0;k<line->skpsList.size();k++)
          {
            featureDef_line::searchKeyPoint skp= line->skpsList[k];

            skp.searchStart= acvRotation(cached_sin,cached_cos,flip_f,skp.searchStart);
            skp.searchVec= acvRotation(cached_sin,cached_cos,flip_f,skp.searchVec);
            skp.searchStart.X+=ldData[i].Center.X;
            skp.searchStart.Y+=ldData[i].Center.Y;
            acvDrawCrossX(buff,
              skp.searchStart.X,skp.searchStart.Y,
              2,2);
            if(searchP(img, &skp.searchStart , skp.searchVec, 2*skp.searchDist)!=0)
            {
              continue;
            }
            acvDrawCrossX(buff,
              skp.searchStart.X,skp.searchStart.Y,
              4,4);
            s_points.push_back(skp.searchStart);
          }
          acvFitLine(&s_points[0], s_points.size(),&line_cand,&sigma);

          //  line.MatchingMarginX=30
        }
        else
        {
          //line.lineTar.line_anchor = acvRotation(cached_sin,cached_cos,flip_f,line.lineTar.line_anchor);
          line_cand.line_vec = acvRotation(cached_sin,cached_cos,flip_f,line->lineTar.line_vec);
          line_cand.line_anchor =acvRotation(cached_sin,cached_cos,flip_f,line->searchEstAnchor);
          acv_XY searchVec;
          searchVec = acvRotation(cached_sin,cached_cos,flip_f,line->searchVec);

          //Offet to real image and backoff searchDist distance along with the searchVec as start
          line_cand.line_anchor.X+=ldData[i].Center.X;
                                    //-line->searchDist*line->searchVec.X;
          line_cand.line_anchor.Y+=ldData[i].Center.Y;
                                    //-line->searchDist*line->searchVec.Y;

          acvDrawCrossX(buff,
            line_cand.line_anchor.X,line_cand.line_anchor.Y,
            2,2);
          //Search distance a 2*searchDist(since you go back off initMatchingMargin)
          if(searchP(img, &line_cand.line_anchor , searchVec, 2*line->searchDist)!=0)
          {
            FeatureReport_lineReport lr;
            lr.line=lf_zero;
            lr.def=line;
            detectedLines.push_back(lr);
            continue;
          }

          acvDrawCrossX(buff,
            line_cand.line_anchor.X,line_cand.line_anchor.Y,
            4,4);

        }

        s_points.resize(0);
        straight_line_grid.getContourPointsWithInLineContour(line_cand,
          line->MatchingMarginX+line->initMatchingMargin,
          line->initMatchingMargin,
          s_intersectIdxs,s_points);

        acvFitLine(&s_points[0], s_points.size(),&line_cand,&sigma);

        //LOGV("Matched points:%d",s_points.size());
        acvDrawLine(buff,
          line_cand.line_anchor.X-mult*line_cand.line_vec.X,
          line_cand.line_anchor.Y-mult*line_cand.line_vec.Y,
          line_cand.line_anchor.X+mult*line_cand.line_vec.X,
          line_cand.line_anchor.Y+mult*line_cand.line_vec.Y,
          20,255,128);

        LOGV("L=%d===anchor.X:%f anchor.Y:%f vec.X:%f vec.Y:%f ,sigma:%f",j,
        line_cand.line_anchor.X,
        line_cand.line_anchor.Y,
        line_cand.line_vec.X,
        line_cand.line_vec.Y,
        sigma);


        acv_XY *end_pos=findEndPoint(line_cand, 1, s_points);
        acv_XY *end_neg=findEndPoint(line_cand, -1, s_points);

        acv_LineFit lf;
        lf.line=line_cand;
        lf.matching_pts=s_points.size();
        lf.s=sigma;
        if(end_pos)lf.end_pos=*end_pos;
        if(end_neg)lf.end_neg=*end_neg;


        LOGV("end_pos.X:%f end_pos.Y:%f end_neg.X:%f end_neg.Y:%f",
          lf.end_pos.X,
          lf.end_pos.Y,
          lf.end_neg.X,
          lf.end_neg.Y);

        FeatureReport_lineReport lr;
        lr.line=lf;
        lr.def=line;
        detectedLines.push_back(lr);
      }

      acv_CircleFit cf_zero= {0};
      for (int j = 0; j < featureCircleList.size(); j++)
      {
        featureDef_circle &cdef= featureCircleList[j];
        acv_XY center = acvRotation(cached_sin,cached_cos,flip_f,cdef.circleTar.circumcenter);

        int matching_tor=cdef.initMatchingMargin;
        center.X+=ldData[i].Center.X;
        center.Y+=ldData[i].Center.Y;
        inward_curve_grid.getContourPointsWithInCircleContour(center.X,center.Y,cdef.circleTar.radius,matching_tor*2,
          s_intersectIdxs,s_points);

        acv_CircleFit cf;
        circleRefine(s_points,&cf);


        inward_curve_grid.getContourPointsWithInCircleContour(cf.circle.circumcenter.X,cf.circle.circumcenter.Y,cf.circle.radius,matching_tor,
          s_intersectIdxs,s_points);
        circleRefine(s_points,&cf);


        acvDrawCrossX(buff,
          center.X,center.Y,
          5,3);

        acvDrawCircle(buff,
        cf.circle.circumcenter.X, cf.circle.circumcenter.Y,
        cf.circle.radius,
        20,255, 0, 0);


        LOGV("C=%d===%f,%f   => %f,%f, dist:%f matching_pts:%d",
        j,cdef.circleTar.circumcenter.X,cdef.circleTar.circumcenter.Y,center.X,center.Y,
        hypot(cf.circle.circumcenter.X-center.X,cf.circle.circumcenter.Y-center.Y),
        cf.matching_pts);

        FeatureReport_circleReport cr;
        cr.circle=cf;
        cr.def = &cdef;


        detectedCircles.push_back(cr);

      }

      for(int i=0;i<judgeList.size();i++)
      {
        FeatureReport_judgeReport report= measure_process(singleReport,judgeList[i]);
        judgeReports.push_back(report);
      }

  }


  //LOGI(">>>>>>>>");
  return 0;
}
