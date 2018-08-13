#include "FeatureManager.h"
#include "logctrl.h"
#include <stdexcept>
#include <common_lib.h>
#include <MatchingCore.h>
/*
  FeatureManager_group_proto Section
*/
int FeatureManager_group_proto::reload(const char *json_str)
{
  if(root)
  {
    cJSON_Delete(root);
  }
  clearFeatureGroup();
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

int FeatureManager_group_proto::parse_jobj()
{
  const char *type_str= (char *)JFetch(root,"type",cJSON_String);
  if(type_str==NULL)
  {
    LOGE("ptr: type:<%p> ",type_str);
    return -1;
  }
  LOGI("type:<%s> ",type_str);


  cJSON *featureSetList = cJSON_GetObjectItem(root,"featureSet");

  if(featureSetList==NULL)
  {
    LOGE("featureSetList array does not exists");
    return -1;
  }

  if(!cJSON_IsArray(featureSetList))
  {
    LOGE("featureSetList is not an array");
    return -1;
  }

  for (int i = 0 ; i < cJSON_GetArraySize(featureSetList) ; i++)
  {
     cJSON * featureSet = cJSON_GetArrayItem(featureSetList, i);
     int ret = addSubFeature(featureSet);
     if(ret!=0)
     {
       LOGE("Add feature[%d] failed...",i);
       return -1;
     }
  }
  return 0;
}


/*
  FeatureManager_binary_processing_group Section
*/
FeatureManager_binary_processing_group::FeatureManager_binary_processing_group(const char *json_str):
  FeatureManager_group_proto(json_str)
{
  root= NULL;
  int ret = reload(json_str);
  if(ret)
    throw std::invalid_argument( "Error:FeatureManager_sig360_circle_line failed... " );
}

int FeatureManager_binary_processing_group::clearFeatureGroup()
{
    for(int i=0;i<binaryFeatureBundle.size();i++)
    {
      delete binaryFeatureBundle[i];
    }
    binaryFeatureBundle.resize(0);
    return 0;
}

int FeatureManager_binary_processing_group::addSubFeature(cJSON * subFeature)
{
  FeatureManager_binary_processing *newFeature=NULL;
  if(FeatureManager_sig360_circle_line::check(subFeature))
  {

    LOGI("FeatureManager_sig360_circle_line is the type...");
    newFeature = new FeatureManager_sig360_circle_line(cJSON_Print(subFeature));
  }
  else if(FeatureManager_sig360_extractor::check(subFeature))
  {
    LOGI("FeatureManager_sig360_extractor is the type...");
    newFeature = new FeatureManager_sig360_extractor(cJSON_Print(subFeature));
  }
  else
  {
    LOGE("Cannot find a corresponding type...");
    return -1;
  }
  binaryFeatureBundle.push_back(newFeature);
  return 0;
}

bool FeatureManager_binary_processing_group::check(cJSON *root)
{
    char *str;
    LOGI("FeatureManager_binary_processing_group>>>");
    if(!(getDataFromJsonObj(root,"type",(void**)&str)&cJSON_String))
    {
      return false;
    }
    if (strcmp("binary_processing_group",str) == 0)
    {
      return true;
    }
    return false;
}

int FeatureManager_binary_processing_group::FeatureMatching(acvImage *img,acvImage *buff,acvImage *dbg,cJSON *report)
{
    if (report==NULL)return -1;

    cJSON *report_obj = cJSON_CreateObject();
    if(report_obj == NULL)return -2;
    cJSON_AddItemToArray(report, report_obj );



    std::vector<acv_LabeledData> ldData;
    acvThreshold(img, 128, 0);
    acvDrawBlock(img, 1, 1, img->GetWidth() - 2, img->GetHeight() - 2);
    acvComponentLabeling(img);
    acvLabeledRegionInfo(img, &ldData);
    if(ldData.size()-1<1)
    {
      return 0;
    }
    ldData[1].area = 0;

    //Delete the object that has less than certain amount of area on ldData
    //acvRemoveRegionLessThan(img, &ldData, 120);


    cJSON_AddItemToObject(report_obj, "type", cJSON_CreateString("binary_processing_group"));
    cJSON *labeling_report = cJSON_CreateArray();
    cJSON_AddItemToObject(report_obj, "Label_data", labeling_report);
    for(int i=0;i<ldData.size();i++)
    {
      cJSON *obj = cJSON_CreateObject();
      //jobj
    }

    acvCloneImage( img,buff, -1);

    for(int i=0;i<binaryFeatureBundle.size();i++)
    {
      binaryFeatureBundle[i]->FeatureMatching(img,buff,ldData,dbg,report);
    }
  return 0;
}



/*
  FeatureManager_binary_processing_group Section
*/
FeatureManager_group::FeatureManager_group(const char *json_str):
  FeatureManager_group_proto(json_str)
{
  root= NULL;
  int ret = reload(json_str);
  if(ret)
    throw std::invalid_argument( "Error:FeatureManager_sig360_circle_line failed... " );
}

int FeatureManager_group::clearFeatureGroup()
{
    for(int i=0;i<featureBundle.size();i++)
    {
      delete featureBundle[i];
    }
    featureBundle.resize(0);
    return 0;
}

int FeatureManager_group::addSubFeature(cJSON * subFeature)
{
  FeatureManager *newFeature=NULL;
  if(FeatureManager_group::check(subFeature))
  {

    LOGI("FeatureManager_group is the type...:%s",cJSON_Print(subFeature));
    newFeature = new FeatureManager_group(cJSON_Print(subFeature));
  }
  else if(FeatureManager_binary_processing_group::check(subFeature))
  {

    LOGI("FeatureManager_binary_processing_group is the type...");
    newFeature = new FeatureManager_binary_processing_group(cJSON_Print(subFeature));
  }
  else
  {
    LOGE("Cannot find a corresponding type...");
    return -1;
  }
  featureBundle.push_back(newFeature);
  return 0;
}

bool FeatureManager_group::check(cJSON *root)
{
  char *str;
  LOGI("FeatureManager_group>>>");
  if(!(getDataFromJsonObj(root,"type",(void**)&str)&cJSON_String))
  {
    return false;
  }
  if (strcmp("processing_group",str) == 0)
  {
    return true;
  }
  return false;
}

int FeatureManager_group::FeatureMatching(acvImage *img,acvImage *buff,acvImage *dbg,cJSON *report)
{
  if (report==NULL)return -1;
  for(int i=0;i<featureBundle.size();i++)
  {
    featureBundle[i]->FeatureMatching(img,buff,dbg,report);
  }
  return 0;
}
