#include "FM_Blank.h"
#include "logctrl.h"
#include <stdexcept>
#include <common_lib.h>
#include <MatchingCore.h>
#include <acvImage_SpDomainTool.hpp>
// #include <acvImage_.hpp>
#include <rotCaliper.h>
#include "SBM_if.hpp"

#include <algorithm>


// using namespace cv;

// Mat bg = Mat(800, 800, CV_8UC3, {0, 0, 0});

static SBM_if sbmif;

FM_Blank::FM_Blank(const char *json_str): FeatureManager(json_str)
{

  report.data.cjson_report.cjson=NULL;
  reload(json_str);
  backGroundTemplate.ReSize(1,1);
}

FM_Blank::~FM_Blank()
{
  ClearReport();
}
void FM_Blank::ClearReport()
{
  if(report.data.cjson_report.cjson!=NULL)
  {
    
    cJSON_Delete(report.data.cjson_report.cjson);

    report.data.cjson_report.cjson=NULL;
  }

  report.type=FeatureReport::cjson;



}

int FM_Blank::parse_jobj()
{
  LOGI(">>>parse_jobj>>>");
  SetParam(root);

  LOGI(">>>inspectionStage:%d>>>",inspectionStage);
  return 0;
}


int FM_Blank::reload(const char *json_str)
{
  LOGI(">>>reload>>>");
  report.data.cjson_report.cjson=NULL;


  root = cJSON_Parse(json_str);
  // if (root == NULL)
  // {
  //   LOGE("cJSON parse failed");
  //   return -1;
  // }
  // int ret_err = parse_jobj();
  // if (ret_err != 0)
  // {
  //   featureCircleList.resize(0);
  //   featureLineList.resize(0);
  //   feature_signature.RESET(0);
  //   reload("");
  //   return -2;
  // }
  parse_jobj();

  // cJSON_Delete(root);

  return 0;
}

#define SETSPARAM_NUMBER(json,structVarAssign,pName) {double *tmpN; if((tmpN=JFetch_NUMBER(json,pName))) structVarAssign*tmpN;}

#define RETSPARAM_NUMBER(json,structVar,pName) {cJSON_AddNumberToObject(json, pName, structVar);}


#define SETSPARAM_INT_NUMBER(json,structVar,pName) SETSPARAM_NUMBER(json,structVar=(int),pName)
#define SETSPARAM_DOU_NUMBER(json,structVar,pName) SETSPARAM_NUMBER(json,structVar=,pName)
#define SETPARAM_INT_NUMBER(json,pName) SETSPARAM_INT_NUMBER(json,this->pName,#pName)
#define SETPARAM_DOU_NUMBER(json,pName) SETSPARAM_DOU_NUMBER(json,this->pName,#pName)
#define RETPARAM_NUMBER(json,pName) RETSPARAM_NUMBER(json,this->pName,#pName)
  



static acv_XY readXY(cJSON *jsonParam)
{
  
  acv_XY xy={NAN,NAN};
  double* num=JFetch_NUMBER(jsonParam,"x");
  if(num!=NULL)xy.X=*num;
  else return xy;


  num=JFetch_NUMBER(jsonParam,"y");
  if(num!=NULL)xy.Y=*num;
  else{
    xy.X=xy.Y=NAN;
    return xy;
  }  
  return xy;
}

static double JFetch_NUMBER_V(cJSON *json, char* path)
{
  double* p_n=JFetch_NUMBER(json,path);
  if(p_n==NULL)return NAN;
  return *p_n;
}

cJSON * FM_Blank::SetParam0(cJSON *jsonParam)
{
  // SETPARAM_INT_NUMBER(jsonParam,inspectionStage);
  
  // SETPARAM_INT_NUMBER(jsonParam,HFrom);
  // SETPARAM_INT_NUMBER(jsonParam,HTo);
  // SETPARAM_INT_NUMBER(jsonParam,SMax);
  // SETPARAM_INT_NUMBER(jsonParam,SMin);
  // SETPARAM_INT_NUMBER(jsonParam,VMax);
  // SETPARAM_INT_NUMBER(jsonParam,VMin);



  // SETPARAM_INT_NUMBER(jsonParam,boxFilter1_Size);
  // SETPARAM_INT_NUMBER(jsonParam,boxFilter1_thres);
  // SETPARAM_INT_NUMBER(jsonParam,boxFilter2_Size);
  // SETPARAM_INT_NUMBER(jsonParam,boxFilter2_thres);



  // SETPARAM_DOU_NUMBER(jsonParam,targetHeadWHRatio);
  // SETPARAM_DOU_NUMBER(jsonParam,minHeadArea);
  // SETPARAM_DOU_NUMBER(jsonParam,targetHeadWHRatioMargin);
  // SETPARAM_DOU_NUMBER(jsonParam,FacingThreshold);
  

  // SETPARAM_DOU_NUMBER(jsonParam,cableSeachingRatio);
  // SETPARAM_INT_NUMBER(jsonParam,cableCount);
  // SETPARAM_INT_NUMBER(jsonParam,cableTableCount);

  // int vType = getDataFromJson(jsonParam, "backgroundFlag", NULL);
  // if(vType==cJSON_True)
  // {
  //   backgroundFlag=true;
  // }
  // else if(vType==cJSON_False)
  // {
  //   backgroundFlag=false;
  // }

  // acv_XY vec_btm =readXY(JFetch_OBJECT(jsonParam,"regionInfo.anchorInfo.vec_btm")); 
  // acv_XY vec_side =readXY(JFetch_OBJECT(jsonParam,"regionInfo.anchorInfo.vec_side")); 
  // acv_XY pt_cornor =readXY(JFetch_OBJECT(jsonParam,"regionInfo.anchorInfo.pt_cornor")); 


  // regionInfo.resize(0);
  // if(!isnan(vec_btm.X) && !isnan(vec_side.X) && !isnan(pt_cornor.X))
  // {
  //   LOGI("GO................");
    
  //   float L_btm=hypot(vec_btm.Y,vec_btm.X);
  //   for(int i=0;;i++)
  //   {
  //     char tmpPath[50];
  //     sprintf(tmpPath,"regionInfo.regions[%d]",i);
  //     cJSON *region = JFetch_OBJECT(jsonParam,tmpPath);
  //     if(region==NULL)break;
  //     acv_XY pt1 =readXY(JFetch_OBJECT(region,"pt1")); 
  //     acv_XY pt2 =readXY(JFetch_OBJECT(region,"pt2")); 
  //     double* p_margin = JFetch_NUMBER(region,"margin");
  //     double* p_id = JFetch_NUMBER(region,"id");
  //     double margin=p_margin==NULL?NAN:*p_margin;
  //     int id=p_id==NULL?-1:(int)*p_id;
  //     regionInfo_single rIs;
  //     rIs.normalized_pt1=rIs.pt1=pt1;
  //     rIs.normalized_pt1.X/=L_btm;
  //     rIs.normalized_pt1.Y/=L_btm;

  //     rIs.normalized_pt2=rIs.pt2=pt2;
  //     rIs.normalized_pt2.X/=L_btm;
  //     rIs.normalized_pt2.Y/=L_btm;
  //     rIs.margin=margin;
  //     rIs.id=id;

  //     rIs.RGBA[0]=(float)JFetch_NUMBER_V(region,"colorInfo.RGBA[0]");
  //     rIs.RGBA[1]=(float)JFetch_NUMBER_V(region,"colorInfo.RGBA[1]");
  //     rIs.RGBA[2]=(float)JFetch_NUMBER_V(region,"colorInfo.RGBA[2]");
  //     rIs.sgRGBA[0]=(float)JFetch_NUMBER_V(region,"colorInfo.sigma[0]");
  //     rIs.sgRGBA[1]=(float)JFetch_NUMBER_V(region,"colorInfo.sigma[1]");
  //     rIs.sgRGBA[2]=(float)JFetch_NUMBER_V(region,"colorInfo.sigma[2]");


  //     // regionInfo.push_back(rIs);
  //     // LOGI("[%d]   ID:%d, margin:%f   %f,%f> %f,%f",i,id,margin,pt1.X,pt1.Y,pt2.X,pt2.Y);
  //     LOGI("[%d]   ID:%d, margin:%f   %f,%f> %f,%f",i,id,margin,rIs.normalized_pt1.X,rIs.normalized_pt1.Y,rIs.normalized_pt2.X,rIs.normalized_pt2.Y);
  //   }


  // }


  // cJSON* ret_jobj = NULL;
  // if (getDataFromJson(jsonParam, "get_param", NULL) == cJSON_True)
  // {
  //   ret_jobj = cJSON_CreateObject();
  //   RETPARAM_NUMBER(ret_jobj,HFrom);
  //   RETPARAM_NUMBER(ret_jobj,HTo);
  //   RETPARAM_NUMBER(ret_jobj,SMax);
  //   RETPARAM_NUMBER(ret_jobj,SMin);
  //   RETPARAM_NUMBER(ret_jobj,VMax);
  //   RETPARAM_NUMBER(ret_jobj,VMin);

  //   RETPARAM_NUMBER(ret_jobj,boxFilter1_Size);
  //   RETPARAM_NUMBER(ret_jobj,boxFilter1_thres);
  //   RETPARAM_NUMBER(ret_jobj,boxFilter2_Size);
  //   RETPARAM_NUMBER(ret_jobj,boxFilter2_thres);

  //   RETPARAM_NUMBER(ret_jobj,targetHeadWHRatio);
  //   RETPARAM_NUMBER(ret_jobj,minHeadArea);
  //   RETPARAM_NUMBER(ret_jobj,targetHeadWHRatioMargin);
  //   RETPARAM_NUMBER(ret_jobj,FacingThreshold);

  //   RETPARAM_NUMBER(ret_jobj,cableSeachingRatio);
  //   RETPARAM_NUMBER(ret_jobj,cableCount);
  //   RETPARAM_NUMBER(ret_jobj,cableTableCount);
  // }
  // // float cableSeachingRatio=0.2;


  // const int cableCount=12;
  return NULL;
  // const int cableTableCount=2;
}

cJSON * FM_Blank::SetParam1(cJSON *jsonParam)
{
  
  // SETSPARAM_INT_NUMBER(jsonParam,insp02.inspectionType,"inspectionType");
  // SETSPARAM_NUMBER(jsonParam,insp02.pos.X=(int),"pos.X");
  // SETSPARAM_NUMBER(jsonParam,insp02.pos.Y=(int),"pos.Y");


  cJSON* ret_jobj = NULL;
  if (getDataFromJson(jsonParam, "get_param", NULL) == cJSON_True)
  {
    ret_jobj = cJSON_CreateObject();
  }
  return ret_jobj;
}

cJSON * FM_Blank::SetParam(cJSON *jsonParam)
{
  char* jsonStr = cJSON_Print(jsonParam);
  LOGI("%s..",jsonStr);
  delete jsonStr;
  // exit(-1);
  // double *tmpN;
  // if((tmpN=JFetch_NUMBER(jsonParam,"HFrom")))
  //   HFrom=(int)*tmpN;

  {
    double *tmpN=JFetch_NUMBER(jsonParam,"thres");
    if(tmpN!=NULL)
    {
      thres=(int)*tmpN;
    }
    else
    {
      thres=-1;
    }
  }



  SETSPARAM_INT_NUMBER(jsonParam,this->inspectionType,"inspectionType");
  switch(inspectionType)
  {
    case 0:
    return SetParam0(jsonParam);

    case 1:
    return SetParam1(jsonParam);


  }
  return NULL;
}


int FM_Blank::FeatureMatching(acvImage *img)
{

  ClearReport();
  cJSON *jsonRep=cJSON_CreateObject();
  
  cJSON_AddStringToObject(jsonRep, "type", GetFeatureTypeName());
  report.data.cjson_report.cjson=jsonRep;
  // LOGI("GOGOGOGOGOGGO....inspectionStage:%d",inspectionStage);

  if(thres>=0)
  {
    acvThreshold(img,thres);
  }

  return -1;
}

int FM_Blank::FeatureMatching1(acvImage *img)
{

  cJSON *jsonRep=report.data.cjson_report.cjson;

  return -1;
}

int FM_Blank::FeatureMatching0(acvImage *img)
{
  
  float cableRatio=0.074;
  float maxDiffMargin=24;


  // inspectionStage=1;

  return 0;
}
