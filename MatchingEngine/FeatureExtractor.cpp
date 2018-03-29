#include "FeatureManager.h"
#include "logctrl.h"
#include <stdexcept>
#include <MatchingCore.h>
#include <common_lib.h>



FeatureManager_sig360_extractor::FeatureManager_sig360_extractor(const char *json_str): FeatureManager(json_str)
{
  root= NULL;
  int ret = reload(json_str);
  if(ret)
    throw std::invalid_argument( "Error:FeatureManager_sig360_extractor failed... " );
}



bool FeatureManager_sig360_extractor::check(cJSON *root)
{
  char *str;
  if(!(getDataFromJsonObj(root,"type",(void**)&str)&cJSON_String))
  {
    return false;
  }
  if (strcmp("sig360_extractor",str) == 0)
  {
    return true;
  }
  return false;
}

int FeatureManager_sig360_extractor::parse_jobj()
{
  cJSON *subObj = cJSON_GetObjectItem(root,"type");
  const char *type_str = subObj?subObj->valuestring:NULL;
  subObj = cJSON_GetObjectItem(root,"ver");
  const char *ver_str = subObj?subObj->valuestring:NULL;
  subObj = cJSON_GetObjectItem(root,"mmpp");//mm per pixel
  const char *mmpp_str = subObj?subObj->valuestring:NULL;
  if(type_str==NULL||ver_str==NULL||mmpp_str==NULL)
  {
    LOGE("ptr: type:<%p>  ver:<%p>  mmpp_str:<%p>",type_str,ver_str,mmpp_str);
    return -1;
  }
  LOGI("type:<%s>  ver:<%s>  mmpp_str:<%s>",type_str,ver_str,mmpp_str);



  return 0;
}


int FeatureManager_sig360_extractor::reload(const char *json_str)
{
  if(root)
  {
    cJSON_Delete(root);
  }

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
  return 0;
}

int FeatureManager_sig360_extractor::FeatureMatching(acvImage *img,acvImage *buff,vector<acv_LabeledData> &ldData,acvImage *dbg)
{
  vector<acv_XY> signature(360);
  int idx=-1;
  for(int i=1;i<ldData.size();i++)
  {
    if(ldData[i].area<=0)continue;
    LOGV("[%d]: X:%f Y:%f...",i,ldData[i].Center.X,ldData[i].Center.Y);
    if(idx!=-1)
    {
      LOGE("Only one component is allowed for extractor");
      return -1;
    }
    idx=i;
  }
  if(idx==-1)
  {
    LOGE("Cannot find one component for extractor");
    return -1;
  }

  LOGI("Find the component => idx:%d",idx);
  LOGI(">>>Center X:%f Y:%f...",ldData[idx].Center.X,ldData[idx].Center.Y);
  LOGI(">>>LTBound X:%f Y:%f...",ldData[idx].LTBound.X,ldData[idx].LTBound.Y);
  LOGI(">>>RBBound X:%f Y:%f...",ldData[idx].RBBound.X,ldData[idx].RBBound.Y);
  vector<acv_CircleFit> detectedCircles;
  vector<acv_LineFit> detectedLines;
  acvContourCircleSignature(img, ldData[idx], idx, signature);
  MatchingCore_CircleLineExtraction(img,buff,detectedCircles,detectedLines);

  LOGI(">>>detectedCircles:%d",detectedCircles.size());
  LOGI(">>>detectedLines:%d",detectedLines.size());


  return 0;
}
