#include "VisSeleDefineDocParser.h"
#include "logctrl.h"
#include <stdexcept>


VisSeleDefineDocParser::VisSeleDefineDocParser(const char *json_str)
{
  root= NULL;
  int ret = reload(json_str);
  if(ret)
    throw std::invalid_argument( "Error:DefineDocParser failed... " );
}

int VisSeleDefineDocParser::parse_circleData(cJSON * circle_obj)
{
  LOGV("feature is a circle");
  return 0;
}
int VisSeleDefineDocParser::parse_lineData(cJSON * line_obj)
{
  LOGV("feature is a line");
  return 0;
}

int VisSeleDefineDocParser::parse_jobj()
{
  featureCircleList.resize(0);
  featureLineList.resize(0);

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
     else
     {
       LOGE("feature[%d] has unknown type:[%s]",i,feature_type);
       return -1;
     }
  }

  return 0;
}


int VisSeleDefineDocParser::reload(const char *json_str)
{
  if(root)
  {
    cJSON_Delete(root);
  }
  featureCircleList.resize(0);
  featureLineList.resize(0);

  root = cJSON_Parse(json_str);
  if(root==NULL)
  {
    LOGE("cJSON parse failed");
    return -1;
  }
  int ret_err = parse_jobj();
  if(ret_err!=0)
  {
    reload("");
    return -2;
  }
}
