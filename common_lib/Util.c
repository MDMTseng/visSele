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

int getDataFromJson(cJSON * obj,char *path,void **ret_ptr)
{
  char buff[128];
  strcpy(buff,path);
  char *paramH=buff;
  char *paramPtr=buff;
  cJSON * curobj=obj;
  for(;;paramPtr++)
  {
    char ch=*paramPtr;
    if(ch=='[')
    {//Not supported yet
      return cJSON_Invalid;
    }

    if(ch=='.')
    {
      *paramPtr='\0';//replace as termination
      cJSON * getobj=NULL;
      int obj_type = getDataFromJsonObj(curobj,paramH,(void**)&getobj);

      *paramPtr=ch;//restore value
      if( (ch='.' && !(obj_type&cJSON_Object)))
      {
        return cJSON_Invalid;
      }
      curobj=getobj;

      paramH=paramPtr+1;
    }

    if(ch=='\0')
    {
      return getDataFromJsonObj(curobj,paramH,ret_ptr);
    }
  }
}


void* JFetch(cJSON * obj,char *path,int type)
{
  void* tmp_ptr=NULL;
  if(0!=(getDataFromJson(obj,path,&tmp_ptr)&type))
  {
    return tmp_ptr;
  }
  return NULL;
}
