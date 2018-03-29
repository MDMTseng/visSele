#include "common_lib.h"
#include "logctrl.h"
#include <stdexcept>


int getDataFromJsonObj(cJSON * obj,void **ret_ptr)
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
    *ret_ptr=obj->valuestring;
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
int getDataFromJsonObj(cJSON * obj,int idx,void **ret_ptr)
{
  cJSON *tmpObj = cJSON_GetArrayItem(obj,idx);
  return getDataFromJsonObj(tmpObj,ret_ptr);
}
int getDataFromJsonObj(cJSON * obj,char *name,void **ret_ptr)
{

  cJSON *tmpObj = cJSON_GetObjectItem(obj,name);
  return getDataFromJsonObj(tmpObj,ret_ptr);
}



acv_XY XY_rotation(float sine,float cosine,float flip_f,acv_XY input)
{
  acv_XY output;
  output.X = input.X*cosine-flip_f*input.Y*sine;
  output.Y = input.X*sine  +flip_f*input.Y*cosine;
  return output;
}
