#include "FeatureManager.h"
#include "logctrl.h"
#include <stdexcept>
#include <MatchingCore.h>
#include <common_lib.h>



FeatureManager::FeatureManager(const char *json_str)
{
}

bool FeatureManager::check(cJSON *root)
{
  return false;
}

int FeatureManager::reload(const char *json_str)
{
  return -1;
}

int FeatureManager::FeatureMatching(acvImage *img,acvImage *buff,vector<acv_LabeledData> &ldData,acvImage *dbg)
{
  return 0;
}

int FeatureManager::parse_jobj()
{
  return 0;
}


FeatureManager_sig360_circle_line::FeatureManager_sig360_circle_line(const char *json_str): FeatureManager(json_str)
{
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

int FeatureManager_sig360_circle_line::parse_circleData(cJSON * circle_obj)
{
  featureDef_circle cir;
  /*char* str = cJSON_Print(circle_obj);
  LOGV("feature is a circle\n%s",str);
  free(str);*/

  double *pnum;
  if(!(getDataFromJsonObj(circle_obj,"MatchingMargin",(void**)&pnum)&cJSON_Number))
  {
    return -1;
  }
  cir.initMatchingMargin=*pnum;

  cJSON *param;
  if(!(getDataFromJsonObj(circle_obj,"param",(void**)&param)&cJSON_Object))
  {
    return -1;
  }

  if(!(getDataFromJsonObj(param,"x",(void**)&pnum)&cJSON_Number))
  {
    return -1;
  }
  cir.circleTar.circumcenter.X=*pnum;


  if(!(getDataFromJsonObj(param,"y",(void**)&pnum)&cJSON_Number))
  {
    return -1;
  }
  cir.circleTar.circumcenter.Y=*pnum;

  if(!(getDataFromJsonObj(param,"r",(void**)&pnum)&cJSON_Number))
  {
    return -1;
  }
  cir.circleTar.radius=*pnum;


  LOGV("feature is a circle");
  LOGV("x:%f y:%f r:%f margin:%f",
  cir.circleTar.circumcenter.X,
  cir.circleTar.circumcenter.Y,
  cir.circleTar.radius,
  cir.initMatchingMargin);
  featureCircleList.push_back(cir);
  return 0;
}
int FeatureManager_sig360_circle_line::parse_lineData(cJSON * line_obj)
{
  featureDef_line line;

  cJSON *param;
  double *pnum;
  if(!(getDataFromJsonObj(line_obj,"param",(void**)&param)&cJSON_Object))
  {
    return -1;
  }

  acv_XY p0,p1;
  if(!(getDataFromJsonObj(param,"x0",(void**)&pnum)&cJSON_Number))
  {
    return -1;
  }
  p0.X=*pnum;

  if(!(getDataFromJsonObj(param,"y0",(void**)&pnum)&cJSON_Number))
  {
    return -1;
  }
  p0.Y=*pnum;


  if(!(getDataFromJsonObj(param,"x1",(void**)&pnum)&cJSON_Number))
  {
    return -1;
  }
  p1.X=*pnum;

  if(!(getDataFromJsonObj(param,"y1",(void**)&pnum)&cJSON_Number))
  {
    return -1;
  }
  p1.Y=*pnum;

  line.lineTar.line_anchor.X=(p0.X+p1.X)/2;
  line.lineTar.line_anchor.Y=(p0.Y+p1.Y)/2;
  line.lineTar.line_vec.X=(p0.X-p1.X);
  line.lineTar.line_vec.Y=(p0.Y-p1.Y);
  line.lineTar.line_vec = acvVecNormalize(line.lineTar.line_vec);



  if(!(getDataFromJsonObj(line_obj,"searchVec",(void**)&param)&cJSON_Object))
  {
    return -1;
  }



  if(!(getDataFromJsonObj(param,"x",(void**)&pnum)&cJSON_Number))
  {
    return -1;
  }
  line.searchVec.X=*pnum;

  if(!(getDataFromJsonObj(param,"y",(void**)&pnum)&cJSON_Number))
  {
    return -1;
  }
  line.searchVec.Y=*pnum;







  LOGV("feature is a line");
  LOGV("anchor.X:%f anchor.Y:%f vec.X:%f vec.Y:%f sVX:%f sVY:%f",
  line.lineTar.line_anchor.X,
  line.lineTar.line_anchor.Y,
  line.lineTar.line_vec.X,
  line.lineTar.line_vec.Y,
  line.searchVec.X,
  line.searchVec.Y);
  featureLineList.push_back(line);



  return 0;
}

int FeatureManager_sig360_circle_line::parse_signatureData(cJSON * signature_obj)
{

  if( contour_signature.size()!=0 )
  {
    LOGE("contour_signature:size()=%d is set already. There can only be one signature feature.",contour_signature.size());
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

    contour_signature.push_back(dat);
    /*cJSON * feature = cJSON_GetArrayItem(signature, i);
    LOGI(" %f",type_str,ver_str,unit_str);*/
  }

  LOGV("feature is a signature");


  return 0;
}
int FeatureManager_sig360_circle_line::parse_jobj()
{
  cJSON *subObj = cJSON_GetObjectItem(root,"type");
  const char *type_str = subObj?subObj->valuestring:NULL;
  subObj = cJSON_GetObjectItem(root,"ver");
  const char *ver_str = subObj?subObj->valuestring:NULL;
  subObj = cJSON_GetObjectItem(root,"unit");
  const char *unit_str = subObj?subObj->valuestring:NULL;
  if(type_str==NULL||ver_str==NULL||unit_str==NULL)
  {
    LOGE("ptr: type:<%p>  ver:<%p>  unit:<%p>",type_str,ver_str,unit_str);
    return -1;
  }
  LOGI("type:<%s>  ver:<%s>  unit:<%s>",type_str,ver_str,unit_str);


  cJSON *featureList = cJSON_GetObjectItem(root,"features");

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
     if(strcmp(feature_type, "circle")==0)
     {
       if(parse_circleData(feature)!=0)
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
     else if(strcmp(feature_type, "contour signature")==0)
     {
       if(parse_signatureData(feature)!=0)
       {
         LOGE("feature[%d] has error %s format",i,feature_type);
         return -1;
       }
     }
     else
     {
       LOGE("feature[%d] has unknown type:[%s]",i,feature_type);
       return -1;
     }
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
  contour_signature.resize(0);

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
    contour_signature.resize(0);
    reload("");
    return -2;
  }
  return 0;
}

int FeatureManager_sig360_circle_line::FeatureMatching(acvImage *img,acvImage *buff,vector<acv_LabeledData> &ldData,acvImage *dbg)
{

  static vector<acv_XY> signature;
  signature.resize(contour_signature.size());

  for (int i = 1; i < ldData.size(); i++)
  {
      if(ldData[i].area<120)continue;
      acvContourCircleSignature(img, ldData[i], i, signature);

      bool isInv;
      float angle;
      float error = SignatureMinMatching( signature,contour_signature,
        &isInv, &angle);

      LOGV("======%d===%f,%d,%f",i,error,isInv,angle*180/3.14159);

      float cached_cos,cached_sin;
      cached_cos=cos(-angle);
      cached_sin=sin(-angle);
      float flip_f=1;
      if(isInv)
      {
        flip_f=-1;
      }

      /*
      acv_XY preXY={0};
      for (int j = 0; j < signature.size(); j++)
      {
        acv_XY curXY={
          .X=contour_signature[j].X*cos(contour_signature[j].Y),
          .Y=contour_signature[j].X*sin(contour_signature[j].Y)
        };
        curXY = XY_rotation(cached_sin,cached_cos,flip_f,curXY);
        curXY.X+=ldData[i].Center.X;
        curXY.Y+=ldData[i].Center.Y;
        if(j!=0)
        acvDrawLine(buff,
          preXY.X,preXY.Y,
          curXY.X,curXY.Y,
          255,60,48);
        preXY=curXY;
      }*/
      for (int j = 0; j < featureCircleList.size(); j++)
      {
        acv_XY center = XY_rotation(cached_sin,cached_cos,flip_f,featureCircleList[j].circleTar.circumcenter);
        LOGV("=%d===%f,%f   => %f,%f",j,featureCircleList[j].circleTar.circumcenter.X,featureCircleList[j].circleTar.circumcenter.Y,center.X,center.Y);

        center.X+=ldData[i].Center.X;
        center.Y+=ldData[i].Center.Y;
        acvDrawCrossX(buff,
          center.X,center.Y,
          5,3);
      }


  }


  LOGI(">>>>>>>>");
  return 0;
}
