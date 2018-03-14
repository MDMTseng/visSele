#include "VisSeleFeatureManager.h"
#include "logctrl.h"
#include <stdexcept>


VisSeleFeatureManager::VisSeleFeatureManager(const char *json_str)
{
  root= NULL;
  int ret = reload(json_str);
  if(ret)
    throw std::invalid_argument( "Error:DefineDocParser failed... " );
}


static int getDataFromJsonObj(cJSON * obj,void **ret_ptr)
{
  if(obj==NULL)
  {
    return cJSON_Invalid;
  }

  if(obj->type & cJSON_Number)
  {
    *ret_ptr=&obj->valuedouble;
    return cJSON_Number;
  }

  if(obj->type & cJSON_String)
  {
    *ret_ptr=&obj->valuestring;
    return obj->type;
  }

  if(obj->type & cJSON_Array)
  {
    *ret_ptr=obj;
    return obj->type;
  }

  if(obj->type & cJSON_Object)
  {
    *ret_ptr=obj;
    return obj->type;
  }

  return cJSON_Invalid;
}
static int getDataFromJsonObj(cJSON * obj,int idx,void **ret_ptr)
{
  cJSON *tmpObj = cJSON_GetArrayItem(obj,idx);
  return getDataFromJsonObj(tmpObj,ret_ptr);
}
static int getDataFromJsonObj(cJSON * obj,char *name,void **ret_ptr)
{

  cJSON *tmpObj = cJSON_GetObjectItem(obj,name);
  return getDataFromJsonObj(tmpObj,ret_ptr);
}
int VisSeleFeatureManager::parse_circleData(cJSON * circle_obj)
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
int VisSeleFeatureManager::parse_lineData(cJSON * line_obj)
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

int VisSeleFeatureManager::parse_signatureData(cJSON * signature_obj)
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

  cJSON *signature;
  if(!(getDataFromJsonObj(param,"signature",(void**)&signature)&cJSON_Array))
  {
    LOGE("The signature is not an cJSON_Array");
    return -1;
  }

  for (int i = 0 ; i < cJSON_GetArraySize(signature) ; i++)
  {
    double *pnum;
    if(!(getDataFromJsonObj(signature,i,(void**)&pnum)&cJSON_Number))
    {
      return -1;
    }
    contour_signature.push_back(*pnum);
    /*cJSON * feature = cJSON_GetArrayItem(signature, i);
    LOGI(" %f",type_str,ver_str,unit_str);*/
  }

  LOGV("feature is a signature");


  return 0;
}
int VisSeleFeatureManager::parse_jobj()
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


int VisSeleFeatureManager::reload(const char *json_str)
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
}
